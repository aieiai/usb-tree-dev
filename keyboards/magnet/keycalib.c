#include "keycalib.h"
#include "keyprop.h"
#include "keyevt.h"
#include "db.h"
#include "rotcc_proto.h"

#include <string.h>

#include "tx_api.h"
#include "tx_config.h"


#define TAG "keycalib: "
#define KCALIB_EVT_FRAME        (0x1UL)

static TX_THREAD calib_thread;
static uint8_t calib_thread_stack[CALIB_THREAD_STACK] __attribute__((aligned(8U)));
static TX_EVENT_FLAGS_GROUP calib_events;
/* Packed sensor entries, split across frames when one frame cannot hold them all. */
static uint8_t sensor_pack_buf[ROTCC_SENSOR_DATA_LEN * KBD_KEY_MAX] __attribute__((aligned(4)));

keycal_sta_t g_keycal_sta = {0};

static void _calib_accumulate(const uint16_t *adc)
{
    keycal_sta_t *p_cal = &g_keycal_sta;
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        uint16_t point = adc[ki];
        keymag_lmt_t *p_acc = &p_cal->acc[ki];

        if (point > p_acc->max) {
            p_acc->max = point;
        }
        if (point < p_acc->min) {
            p_acc->min = point;
        }
    }
}

static uint32_t _calib_span(uint8_t ki)
{
    keycal_sta_t *p_cal = &g_keycal_sta;
    return (p_cal->acc[ki].max > p_cal->acc[ki].min) ? (uint32_t)(p_cal->acc[ki].max - p_cal->acc[ki].min) : 0U;
}

/*
 * A pressed key spans a large part of the ADC range; resting keys only show
 * sensor noise, orders of magnitude below this threshold, and keep their limits.
 */
#define KCALIB_MIN_SPAN         ((uint32_t)((0x01UL << KBD_ADC_BIT) / 8U))

/* Keep the stored limits of keys that never moved during calibration. */
static rotsta_t _calib_commit(void)
{
    rotsta_t state;
    uint32_t updated = 0U;
    uint32_t skipped = 0U;
    uint32_t max_span = 0U;
    keycal_sta_t *p_cal = &g_keycal_sta;

    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        uint32_t span = _calib_span(ki);

        if (span > max_span) {
            max_span = span;
        }
        if (span < KCALIB_MIN_SPAN) {
            skipped++;
            continue;
        }
        printf(TAG "K%u top=%u bot=%u dif=%u\r\n", (unsigned)ki, (unsigned)p_cal->acc[ki].top, (unsigned)p_cal->acc[ki].bot, (unsigned)span);
        g_obmag.cal.lmt[ki] = p_cal->acc[ki];
        keymag_update_factor(ki);
        keyevt_reload();
        updated++;
    }

    if (updated == 0U) {
        printf(TAG "no key moved, max span=%u (need %u), keep stored limits\r\n", (unsigned)max_span, (unsigned)KCALIB_MIN_SPAN);
        return rotsta_fail;
    }
    state = db_update(DB_ITEM_OBMAG, DB_SAVE_TY_NOW);
    printf(TAG "saved %u keys, skipped %u, result=0x%08x\r\n", (unsigned)updated, (unsigned)skipped, (unsigned)state);
    return state;
}

/*
 * Always call resume: a task blocked on the event group was only marked for a
 * delayed suspend by tx_thread_suspend(), and resume is what lifts that mark
 * (TX_SUSPEND_LIFTED). TX_RESUME_ERROR just means it is already waiting.
 */
static void _calib_task_resume(void)
{
    UINT status = tx_thread_resume(&calib_thread);
    if ((status != TX_SUCCESS) && (status != TX_SUSPEND_LIFTED) && (status != TX_RESUME_ERROR)) {
        printf(TAG "resume failed: 0x%02x, state=%u\r\n", (unsigned)status, (unsigned)calib_thread.tx_thread_state);
    }
}

static void _calib_task_suspend(void)
{
    if (calib_thread.tx_thread_state != TX_SUSPENDED) {
        (void)tx_thread_suspend(&calib_thread);
    }
}

static uint32_t _sensor_period_ticks(void)
{
    uint32_t freq = g_keycal_sta.rpt_freq;

    if (freq == 0U) {
        freq = KCALIB_SENSOR_FREQ_DFT;
    }
    return (TX_TIMER_TICKS_PER_SECOND + freq - 1U) / freq;
}

/* Wake up in time for the next report, or wait for an ADC frame only. */
static ULONG _sensor_wait_ticks(void)
{
    uint32_t period;
    uint32_t elapsed;

    if (g_keycal_sta.rpt_en == 0U) {
        return TX_WAIT_FOREVER;
    }

    period = _sensor_period_ticks();
    elapsed = tx_time_get() - g_keycal_sta.rpt_last;
    return (elapsed >= period) ? TX_NO_WAIT : (ULONG)(period - elapsed);
}

static bool _sensor_report_due(void)
{
    if (g_keycal_sta.rpt_en == 0U) {
        return false;
    }
    return (uint32_t)(tx_time_get() - g_keycal_sta.rpt_last) >= _sensor_period_ticks();
}

static uint16_t _sensor_dist_um(uint8_t ki)
{
    float dist = g_keyevt[ki].dst;

    if (!(dist > 0.0f)) {
        return 0U;
    }
    if (dist >= 65535.0f) {
        return 65535U;
    }
    return (uint16_t)(dist + 0.5f);
}

/*
 * One report period: pack all keys into as few frames as the CC endpoint allows.
 * The period itself does not depend on calibration.
 */
static void _sensor_report(void)
{
    uint16_t max_payload = rotcc_proto_notify_payload_max();
    uint16_t offset = 0U;

    g_keycal_sta.rpt_last = tx_time_get();
    if (max_payload < ROTCC_SENSOR_DATA_LEN) {
        return;
    }

    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        if ((uint32_t)offset + ROTCC_SENSOR_DATA_LEN > max_payload) {
            if (rotcc_proto_notify(ROTCC_CMD_NOTIFY, ROTCC_NOTIFY_SENSOR, sensor_pack_buf, offset) != rotsta_success) {
                return;
            }
            offset = 0U;
        }

        sensor_pack_buf[offset + 0U] = ki;
        sensor_pack_buf[offset + 1U] = EVT_IS_D(g_keyevt[ki].trg) ? 1U : 0U;
        rotutil_put_le16(&sensor_pack_buf[offset + 2U], g_keyevt[ki].val);
        rotutil_put_le16(&sensor_pack_buf[offset + 4U], _sensor_dist_um(ki));
        offset = (uint16_t)(offset + ROTCC_SENSOR_DATA_LEN);
    }

    if (offset != 0U) {
        (void)rotcc_proto_notify(ROTCC_CMD_NOTIFY, ROTCC_NOTIFY_SENSOR, sensor_pack_buf, offset);
    }
}

/*
 * Thread is suspended when neither calibration nor sensor upload needs it. While
 * a calibration session is open the frame event drives sampling; sensor upload
 * uses the timeout to keep its own fixed period in both cases.
 */
static void calib_thread_entry(ULONG id)
{
    (void)id;

    for (;;) {
        ULONG events = 0U;
        UINT status = tx_event_flags_get(&calib_events, KCALIB_EVT_FRAME, TX_OR_CLEAR, &events, _sensor_wait_ticks());

        if ((status == TX_SUCCESS) && _keycal_active()) {
            keycal_sta_t *p_cal = &g_keycal_sta;
            _calib_accumulate((const uint16_t *)p_cal->frame);
        }
        if (_sensor_report_due()) {
            _sensor_report();
        }
    }
}


void keycal_post_adc(uint16_t *p_buf)
{
    if (!_keycal_active()) {
        return;
    }
    /*
     * The core thread hands the frame over here and only signals the task; the
     * calibration copy stays private to the consumer. Calibration only needs a
     * steady stream, the newest frame wins.
     */
    keycal_sta_t *p_cal = &g_keycal_sta;
    p_cal->frame = p_buf;
    (void)tx_event_flags_set(&calib_events, KCALIB_EVT_FRAME, TX_OR);
}

rotsta_t keycal_set_status(uint8_t sta)
{
    rotsta_t state;
    keycal_sta_t *p_cal = &g_keycal_sta;

    if (sta == KCAL_STA_START) {
        for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
            p_cal->acc[ki].max = 0U;
            p_cal->acc[ki].min = UINT16_MAX;
        }
        (void)tx_event_flags_set(&calib_events, 0U, TX_AND);
        p_cal->sta = KCAL_STA_START;
        _calib_task_resume();
        printf(TAG "start\r\n");
        return rotsta_success;
    }

    /* 0x80 aborts the session, 0x82 commits the collected range. */
    if ((sta != KCAL_STA_STOP) && (sta != KCAL_STA_FINISH)) {
        return rotsta_invalid_argument;
    }
    if (!_keycal_active()) {
        p_cal->sta = KCAL_STA_STOP;
        printf(TAG "no active session, ignore 0x%02x\r\n", (unsigned)sta);
        return (sta == KCAL_STA_STOP) ? rotsta_success : rotsta_fail;
    }

    p_cal->sta = sta;
    if (sta == KCAL_STA_STOP) {
        printf(TAG "stop, samples discarded\r\n");
        state = rotsta_success;
    }
    else {
        state = _calib_commit();
        p_cal->sta = (state == rotsta_success) ? KCAL_STA_FINISH : KCAL_STA_STOP;
    }
    if (p_cal->rpt_en == 0U) {
        _calib_task_suspend();
    }
    return state;
}

rotsta_t keycal_sensor_report_set(uint8_t en)
{
    keycal_sta_t *p_cal = &g_keycal_sta;

    if (en > 1U) {
        return rotsta_invalid_argument;
    }

    if (en != 0U) {
        if (p_cal->rpt_en == 0U) {
            /* Report the next period immediately, then keep a fixed cadence. */
            p_cal->rpt_last = tx_time_get() - _sensor_period_ticks();
        }
        p_cal->rpt_en = 1U;
        _calib_task_resume();
        printf(TAG "sensor report on, %u Hz\r\n", (unsigned)p_cal->rpt_freq);
        return rotsta_success;
    }

    if (p_cal->rpt_en == 0U) {
        return rotsta_success;
    }
    p_cal->rpt_en = 0U;
    if (!_keycal_active()) {
        _calib_task_suspend();
    }
    printf(TAG "sensor report off\r\n");
    return rotsta_success;
}

rotsta_t keycal_sensor_freq_set(uint16_t freq)
{
    keycal_sta_t *p_cal = &g_keycal_sta;

    if ((freq < KCALIB_SENSOR_FREQ_MIN) || (freq > KCALIB_SENSOR_FREQ_MAX)) {
        return rotsta_invalid_argument;
    }

    p_cal->rpt_freq = freq;
    p_cal->rpt_last = tx_time_get() - _sensor_period_ticks();
    printf(TAG "sensor report freq=%u Hz\r\n", (unsigned)freq);
    return rotsta_success;
}

rotsta_t keycal_init(void)
{
    rotsta_t result = rotsta_success;
    keycal_sta_t *p_cal = &g_keycal_sta;
    
    p_cal->sta = KCAL_STA_STOP;
    p_cal->frame = NULL;
    p_cal->rpt_en = 0U;
    p_cal->rpt_freq = KCALIB_SENSOR_FREQ_DFT;
    p_cal->rpt_last = 0U;
    for (uint8_t ki = 0U; ki < KBD_KEY_MAX; ki++) {
        p_cal->acc[ki].max = 0U;
        p_cal->acc[ki].min = UINT16_MAX;
    }
    
    result = keymag_init();
    if (result != rotsta_success) {
        printf(TAG "keymag init failed: 0x%08x, using defaults\r\n", (unsigned)result);
    }

    if (tx_event_flags_create(&calib_events, "calib_evt") != TX_SUCCESS) {
        printf(TAG "event group create failed\r\n");
        return rotsta_fail;
    }

    if (tx_thread_create(&calib_thread, "calib_thread", calib_thread_entry, 0U, calib_thread_stack, sizeof(calib_thread_stack), CALIB_THREAD_PRIORITY, CALIB_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START) != TX_SUCCESS) {
        printf(TAG "thread create failed\r\n");
        return rotsta_fail;
    }
    printf(TAG "task ready, state=%u\r\n", (unsigned)calib_thread.tx_thread_state);

    return result;
}
