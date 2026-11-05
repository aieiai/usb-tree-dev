#include "rot_cc.h"
#include "hal_usb.h"
#include "fdb.h"

#include "board.h"
#include "hpm_ppor_drv.h"
#include "hpm_gpio_drv.h"

#include "tx_api.h"
#include "tx_config.h"


#define TAG "rot_cc: "
#define ROT_CC_PACKET_SIZE      (64U)
#define ROT_CC_RX_QUEUE_DEPTH   (8U)

typedef struct {
    uint16_t len;
    uint8_t data[ROT_CC_PACKET_SIZE];
} rot_cc_packet_t;

static TX_THREAD thread_rot_cc;
static uint8_t thread_rot_cc_stack[ROT_CC_TASK_STACK] __attribute__((aligned(8U)));
static TX_SEMAPHORE cc_rx_sem;

static rot_cc_packet_t cc_rx_queue[ROT_CC_RX_QUEUE_DEPTH];
static volatile uint32_t cc_rx_head;
static volatile uint32_t cc_rx_tail;
static volatile uint32_t cc_rx_dropped;
static volatile bool cc_tx_done_pending;
static volatile rot_state_t cc_tx_result;
static volatile uint16_t cc_tx_len;
static bool rot_cc_initialized;

_Static_assert((ROT_CC_RX_QUEUE_DEPTH & (ROT_CC_RX_QUEUE_DEPTH - 1U)) == 0U, "ROT_CC_RX_QUEUE_DEPTH must be a power of two");

static void rot_cc_rx_callback(const uint8_t *data, uint16_t len, void *ctx)
{
    uint32_t index;
    bool queued = false;

    (void)ctx;
    if ((data == NULL) || (len == 0U) || (len > ROT_CC_PACKET_SIZE)) {
        return;
    }

    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    if ((cc_rx_tail - cc_rx_head) < ROT_CC_RX_QUEUE_DEPTH) {
        index = cc_rx_tail & (ROT_CC_RX_QUEUE_DEPTH - 1U);
        cc_rx_queue[index].len = len;
        memcpy(cc_rx_queue[index].data, data, len);
        cc_rx_tail++;
        queued = true;
    } else {
        cc_rx_dropped += len;
    }
    TX_RESTORE

    if (queued) {
        (void)tx_semaphore_put(&cc_rx_sem);
    }
}

static void rot_cc_tx_complete_callback(rot_state_t state, uint16_t len, void *ctx)
{
    (void)ctx;

    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    cc_tx_result = state;
    cc_tx_len = len;
    cc_tx_done_pending = true;
    TX_RESTORE

    (void)tx_semaphore_put(&cc_rx_sem);
}

//更新rot_cc_on_rx_packet函数签名，将context参数改为ctx，并移除int result参数
__attribute__((weak)) void rot_cc_on_rx_packet(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)data;
    (void)len;
    (void)ctx;
}
__attribute__((weak)) void rot_cc_on_tx_complete(rot_state_t state, uint16_t len, void *ctx)
{
    (void)state;
    (void)len;
    (void)ctx;
}

static bool rot_cc_rx_pop(rot_cc_packet_t *packet)
{
    uint32_t index;
    bool popped = false;

    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    if (cc_rx_head != cc_rx_tail) {
        index = cc_rx_head & (ROT_CC_RX_QUEUE_DEPTH - 1U);
        *packet = cc_rx_queue[index];
        cc_rx_head++;
        popped = true;
    }
    TX_RESTORE

    return popped;
}


static void rot_cc_task(ULONG id)
{
    (void)id;
    uint32_t tick = 0;
    uint8_t rx[64];
    rot_cc_packet_t packet;

    uint32_t cnt = 0;
    while (1) {
        if (tx_semaphore_get(&cc_rx_sem, 1U) == TX_SUCCESS) {
            
            hal_dt_printf(TAG "host -> dev, %d bytes\r\n", packet.len);
            bool tx_done = false;
            rot_state_t tx_state = rot_state_success;
            uint16_t tx_len = 0U;

            TX_INTERRUPT_SAVE_AREA
            TX_DISABLE
            if (cc_tx_done_pending) {
                tx_done = true;
                tx_state = cc_tx_result;
                tx_len = cc_tx_len;
                cc_tx_done_pending = false;
            }
            TX_RESTORE

            if (tx_done) {
                rot_cc_on_tx_complete(tx_state, tx_len, NULL);
            }
            while (rot_cc_rx_pop(&packet)) {
                rot_cc_on_rx_packet(packet.data, packet.len, NULL);
            }
        }

        if ( cnt >= 1000 )
        {
            /* DT OUT 端点：上位机下发的数据，这里回显一下，方便验证双向通道 */
            int n = hal_dt_read(rx, sizeof(rx), 0);
            if (n > 0) {
                hal_dt_printf("[ray] host -> dev, %d bytes\r\n", n);
            }
            cnt = 0;
            hal_dt_printf("[ray] alive, tick=%u, dropped=%u, kbd_led=0x%02x\r\n", (unsigned)tick++, (unsigned)hal_dt_dropped(), hal_kbd_get_leds());
        }
        cnt++;

        static uint8_t btn_st_pre = !BOARD_BUTTON_PRESSED_VALUE;
        uint8_t btn_st = gpio_read_pin(BOARD_APP_GPIO_CTRL, BOARD_APP_GPIO_INDEX, BOARD_APP_GPIO_PIN);
        if (btn_st != btn_st_pre) {
            tx_thread_sleep(20);
            btn_st = gpio_read_pin(BOARD_APP_GPIO_CTRL, BOARD_APP_GPIO_INDEX, BOARD_APP_GPIO_PIN);
        }
        if (btn_st != btn_st_pre) {
            btn_st_pre = btn_st;
            if (btn_st == BOARD_BUTTON_PRESSED_VALUE) {
                // fdb_data.bif.gp_mode++;
                // fdb_data.bif.gp_mode %= HAL_GAMEPAD_MODE_MAX;
                
                if (rot_state_success == fdb_update(FDB_ITEM_TY_SOBI, FDB_SAVE_TY_NOW)) {
                    tx_thread_sleep(100);
                    disable_global_irq(CSR_MSTATUS_MIE_MASK);
                    (*(volatile uint32_t *)(0xF40D4000UL + 8U)) = 0; // warning: this is rom reset flag,
                    ppor_sw_reset(HPM_PPOR, 1000);
                    while (1)
                    {
                    }
                }

                hal_dt_printf("btn down (level=%u, gp_mode=%u)\r\n", (unsigned)btn_st, (unsigned)g_gamepad_mode);
            } 
            else {
                hal_dt_printf("btn up (level=%u)\r\n", (unsigned)btn_st);
            }
        }

        tx_thread_sleep(1);
    }
}

void rot_cc_init(void)
{
    static const hal_cc_callbacks_t callbacks = {
        .rx = rot_cc_rx_callback,
        .tx_complete = rot_cc_tx_complete_callback,
        .ctx = NULL,
    };
    UINT ret;

    if (rot_cc_initialized) {
        return;
    }
    if (tx_semaphore_create(&cc_rx_sem, "rot_cc_rx", 0U) != TX_SUCCESS) {
        printf(TAG "semaphore create failed\r\n");
        return;
    }
    (void)hal_cc_register_callbacks(&callbacks);

    ret = tx_thread_create(&thread_rot_cc, "thread_rot_cc", rot_cc_task, 0U, thread_rot_cc_stack, sizeof(thread_rot_cc_stack), ROT_CC_TASK_PRIORITY, ROT_CC_TASK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ret != TX_SUCCESS) {
        perror("thread_rot_cc creation failed!.\n");
        for (;;) {
            ;
        }
    }
    rot_cc_initialized = true;
}

int rot_cc_send(const void *data, uint32_t len)
{
    return hal_cc_send(data, len);
}

bool rot_cc_is_busy(void)
{
    return hal_cc_is_busy();
}

uint32_t rot_cc_dropped(void)
{
    return cc_rx_dropped;
}

