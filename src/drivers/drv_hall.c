#include "drv_hall.h"

#include "board.h"


#include "hpm_soc.h"
#include "hpm_trgm_drv.h"
#include "hpm_clock_drv.h"
#include "hpm_adc16_drv.h"
#include "hpm_dmav2_drv.h"
#include "hpm_dmamux_drv.h"
#include "hpm_mchtmr_drv.h"
#ifdef HPMSOC_HAS_HPMSDK_GPTMRV2
#include "hpm_gptmrv2_drv.h"
#else
#include "hpm_gptmr_drv.h"
#endif

#include "_pin_defs.h"
#include "gpio.h"

#include "app_config.h"

extern uint32_t core_freq_mhz;

#if defined(HPM_HDMA)
#    define MMX_HDMA HPM_HDMA
#    define MMX_HDMA_IRQ IRQn_HDMA
#elif defined(HPM_HDMA0)
#     define MMX_HDMA HPM_HDMA0
#     define MMX_HDMA_IRQ IRQn_HDMA0
#else
#     error "Unsupported HPM DMA controller"
#endif

#ifdef HPMSOC_HAS_HPMSDK_GPTMRV2
#    define SENSOR_MATRIX_TMR_TYPE GPTMRV2_Type
/* GPTMR reload max */
#    define SENSOR_MATRIX_TMR_RLD_MAX GPTMRV2_CHANNEL_RLD_RLD_MASK
#else
#    define SENSOR_MATRIX_TMR_TYPE GPTMR_Type
#    define SENSOR_MATRIX_TMR_RLD_MAX GPTMR_CHANNEL_RLD_RLD_MASK
#endif


#ifdef POWER_MANAGER_ENABLE
#    error "SENSOR_MATRIX_DRIVER mux_io not support POWER_MANAGER"
#endif

#if (SENSOR_MATRIX_ROUND_SELS_MAX < 1)
#    error "SENSOR_MATRIX_ROUND_SELS_MAX must be at least 1"
#endif


/* usb sof sync setup time (sof to tmr irq margin time) */
#ifndef SENSOR_MATRIX_SOF_ST_TIME
#    define SENSOR_MATRIX_SOF_ST_TIME 0
#endif

/* tmr soft sync setup time default 700ns */
#ifndef SENSOR_MATRIX_TMR_ST_TIME
#    define SENSOR_MATRIX_TMR_ST_TIME 70
#endif

/* gpio addr set */
#ifndef SENSOR_MATRIX_SEL_HT_TIME1
#    define SENSOR_MATRIX_SEL_HT_TIME1 1
#endif

/* seq buffer (2*2*7 word) burst1 615ns burst2 515ns burst4 415ns */
#ifndef SENSOR_MATRIX_SEL_HT_TIME2
#    define SENSOR_MATRIX_SEL_HT_TIME2 110
#endif

/* sample setup default 3us */
#ifndef SENSOR_MATRIX_ADC_ST_TIME
#    define SENSOR_MATRIX_ADC_ST_TIME 300
#endif

/* gptmr */
#define SENSOR_MATRIX_TMR_BASE INST_CONCAT3(HPM, SENSOR_MATRIX_TMR_INST, BASE)
#define SENSOR_MATRIX_TMR ((SENSOR_MATRIX_TMR_TYPE *)SENSOR_MATRIX_TMR_BASE)
#define SENSOR_MATRIX_TMR_SHT_CHANNEL 0
#define SENSOR_MATRIX_TMR_DMA_CHANNEL 1
#define SENSOR_MATRIX_TMR_ADC_CHANNEL 2
#define SENSOR_MATRIX_TMR_SOF_CHANNEL 3

/* dma ch */
#ifndef SENSOR_MATRIX_DMA_SET_CHANNEL
#    define SENSOR_MATRIX_DMA_SET_CHANNEL 6
#endif
#ifndef SENSOR_MATRIX_DMA_SEL_CHANNEL
#    define SENSOR_MATRIX_DMA_SEL_CHANNEL 7
#endif

/* dma src */
#define SENSOR_MATRIX_DMA_SET_INPUT_SRC INST_CONCAT3(HPM_DMA_SRC, SENSOR_MATRIX_TMR_INST, SENSOR_MATRIX_TMR_SHT_CHANNEL)
#define SENSOR_MATRIX_DMA_SEL_INPUT_SRC INST_CONCAT3(HPM_DMA_SRC, SENSOR_MATRIX_TMR_INST, SENSOR_MATRIX_TMR_DMA_CHANNEL)

/* trgm */
#ifndef SENSOR_MATRIX_SOF_USB
#    define SENSOR_MATRIX_SOF_USB INST_CONCAT(USB0, SOF)
#endif
#define SENSOR_MATRIX_TRGM_BASE HPM_TRGM0_BASE
#define SENSOR_MATRIX_TRGM ((TRGM_Type *)SENSOR_MATRIX_TRGM_BASE)
#define SENSOR_MATRIX_TRGM_SEL_INPUT_SRC INST_CONCAT3(HPM_TRGM0_INPUT_SRC, SENSOR_MATRIX_TMR_INST, OUT2)     /* SENSOR_MATRIX_TMR_ADC_CHANNEL */
#define SENSOR_MATRIX_TRGM_SOF_USB_INPUT_SRC INST_CONCAT(HPM_TRGM0_INPUT_SRC, SENSOR_MATRIX_SOF_USB)         /* sof from usb sof */
#define SENSOR_MATRIX_TRGM_SOF_EXT_INPUT_SRC INST_CONCAT(HPM_TRGM0_INPUT_SRC, SENSOR_MATRIX_SOF_EXT)         /* sof from ext io */
#define SENSOR_MATRIX_TRGM_SOF_TMR_INPUT_SRC INST_CONCAT3(HPM_TRGM0_INPUT_SRC, SENSOR_MATRIX_TMR_INST, OUT3) /* sof from gptmr, SENSOR_MATRIX_TMR_SOF_CHANNEL */
#define SENSOR_MATRIX_TRGM_SOF_NONE_INPUT_SRC INST_CONCAT(HPM_TRGM0_INPUT_SRC, VSS)
#define SENSOR_MATRIX_TRGM_SOF_OUTPUT_SRC INST_CONCAT3(HPM_TRGM0_OUTPUT_SRC, SENSOR_MATRIX_TMR_INST, SYNCI) /* sof to gptmr synci */

/* gptmr clock */
#if SENSOR_MATRIX_TMR_BASE == HPM_GPTMR0_BASE
#    define SENSOR_MATRIX_TMR_CLOCK clock_gptmr0
#elif SENSOR_MATRIX_TMR_BASE == HPM_GPTMR1_BASE
#    define SENSOR_MATRIX_TMR_CLOCK clock_gptmr1
#elif SENSOR_MATRIX_TMR_BASE == HPM_GPTMR2_BASE
#    define SENSOR_MATRIX_TMR_CLOCK clock_gptmr2
#elif SENSOR_MATRIX_TMR_BASE == HPM_GPTMR3_BASE
#    define SENSOR_MATRIX_TMR_CLOCK clock_gptmr3
#endif

static pin_t   ana_pins[SENSOR_MATRIX_ANAS]  = SENSOR_MATRIX_ANA_PINS;
static uint8_t ana_func[SENSOR_MATRIX_ANAS]  = SENSOR_MATRIX_ANA_FUNC;
static pin_t   mux_pins[SENSOR_MATRIX_ADDRS] = SENSOR_MATRIX_ADDR_PINS;
static const uint8_t adc_channel_counts[SENSOR_MATRIX_ADC_INSTANCES] = SENSOR_MATRIX_ADC_CHANNEL_COUNTS;
static hall_recv_cb_t hall_recv_cb = NULL;
volatile sensor_matrix_timing_t g_sensor_matrix_timing;


/* dma set chain */
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 32)
static dma_linked_descriptor_t dma_set_chain_descriptors[9];

/* dma sel chain: switch_chain(1) + (SELS_MAX-1)*(mux+copy) + stop_dma + stop_adc + copy_last + active_end + switch_preload(5) */
enum {
    dma_sel_chain_buffer0 = 0,
    dma_sel_chain_buffer1 = 1,
};
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 32)
static dma_linked_descriptor_t dma_sel_chain_descriptors[2][1 + 2 * (SENSOR_MATRIX_ROUND_SELS_MAX - 1) + 5];

/* mux addr control */
enum {
    mux_addr_preload = 0,
    mux_addr_shadow  = 1,
};
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 4)
static volatile uint32_t MUX_ADDR_REG[2][SENSOR_MATRIX_ROUND_SELS_MAX * 2];

/* scan array control */
enum {
    sensor_scan_array_preload = 0,
    sensor_scan_array_shadow  = 1,
};
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 4)
struct {
    volatile uint32_t count;
    volatile uint8_t  array[CEILING(SENSOR_MATRIX_ROUND_SELS_MAX, 4) * 4];
} SENSOR_SCAN_ARRAY_REG[2];

/* dummy reg */
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 4)
static volatile uint32_t DUMMY_REG[2];

/* tmr channel control */
enum {
    tmr_dma_channel_cr_disable = 0,
    tmr_dma_channel_cr_enable  = 1,
    tmr_adc_channel_cr_disable = 2,
    tmr_adc_channel_cr_enable  = 3,
    tmr_channel_gcr            = 4,
};
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 4)
static volatile uint32_t TMR_CHANNEL_REG[5];

/* dma sel control */
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 4)
struct {
    volatile uint32_t CTRL;
    volatile uint32_t TRANSIZE;
    volatile uint32_t SRCADDR;
    volatile uint32_t CHANREQCTRL;
    volatile uint32_t DSTADDR;
    volatile uint32_t SWAPTABLE;
    volatile uint32_t LLPOINTER;
} DMA_SEL_REG;

/* dma sel llpointer */
enum {
    llpointer_buffer0 = 0,
    llpointer_buffer1 = 1,
};
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 4)
volatile uint32_t LLPOINTER[2];

#define STATUS_MAGIC_ACT0 0x41435430
#define STATUS_MAGIC_ACT1 0x31544341

/* status reg */
enum {
    status_reg_active_beg  = 0, /* same as active_end: not active, different from active_end: active */
    status_reg_active_end  = 1,
    status_reg_preload_beg = 2,
    status_reg_preload_end = 3,
    status_reg_magic0      = 4,
    status_reg_magic1      = 5,
};
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 4)
static volatile uint32_t STATUS_REG[6];

/* adc seq buffer，按各 ADC 实际通道数紧密排列 */
ATTR_PLACE_AT_WITH_ALIGNMENT(".ahb_sram", 4)
static uint32_t seq_buff[SENSOR_MATRIX_ADC_TOTAL_CHANNELS * SENSOR_MATRIX_ADC_SAMPLE_ROUNDS];

/* SENSOR_MATRIX_ROUND_SELS_MAX scan each sel */
/* each sel with SENSOR_MATRIX_ADC_TOTAL_CHANNELS channels */
ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(4)
static uint32_t raw_buff[2][SENSOR_MATRIX_ROUND_SELS_MAX * SENSOR_MATRIX_ADC_TOTAL_CHANNELS * SENSOR_MATRIX_ADC_SAMPLE_ROUNDS];
/* raw_buff 每个 sel 的布局（32 位 word，低 16 位为采样值）：
 * 按“通道优先”排列，每通道 SAMPLE_ROUNDS 个采样：
 * [ch0 r0, ch0 r1, ch1 r0, ch1 r1, ..., ch9 r0, ch9 r1]
 */

static uint8_t  sensor_sof_src_saved;
static uint8_t  sensor_sof_src;
static float    sensor_scan_adjust;
static uint32_t sensor_scan_interval;
static uint32_t mux_addr_mask;

#if ((SENSOR_MATRIX_ADC_CHANNELS) > 8)
#    error "SENSOR_MATRIX_ADC_CHANNELS can't exceed 8"
#endif

#if ((SENSOR_MATRIX_ADC_SAMPLES) > 1) && ((SENSOR_MATRIX_ADC_SAMPLES) & 1)
#    error "SENSOR_MATRIX_ADC_SAMPLES must be 1 or even"
#endif


static void isr_hdma(void)
{
    uint64_t cycle = hpm_csr_get_core_mcycle();
    //uint32_t sof_now = gptmr_channel_get_counter(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL, gptmr_counter_type_normal);

    /* clear isr status */
    dma_clear_transfer_status(MMX_HDMA, SENSOR_MATRIX_DMA_SEL_CHANNEL);
    uint8_t bufidx = STATUS_REG[status_reg_active_beg] == STATUS_MAGIC_ACT1;

    if (hall_recv_cb) {
        hall_recv_cb(raw_buff[bufidx]);
    }

    if (g_sensor_matrix_timing.isr_count != 0U) {
        g_sensor_matrix_timing.scan_period_us = (cycle - g_sensor_matrix_timing.isr_tick) /  (core_freq_mhz);
        g_sensor_matrix_timing.scan_time_us = (cycle - g_sensor_matrix_timing.sof_start_tick) / (core_freq_mhz);
    }
    g_sensor_matrix_timing.isr_tick = cycle;
    g_sensor_matrix_timing.sof_tick = cycle;
    g_sensor_matrix_timing.isr_count++;
}

SDK_DECLARE_EXT_ISR_M(MMX_HDMA_IRQ, isr_hdma)

static void sensor_matrix_init_pins(void)
{
    uint32_t fast_padctrl = IOC_PAD_PAD_CTL_SPD_SET(3) | IOC_PAD_PAD_CTL_DS_SET(3);
    /* mux addr gpio init */
    for (uint32_t i = 0; i < SENSOR_MATRIX_ADDRS; i++) {
        gpio_write_pin_low(mux_pins[i]);
        gpio_set_pin_output(mux_pins[i]);
        gpio_set_pin_alternate(mux_pins[i], IOC_FUNC_CTL_GPIO, fast_padctrl);
    }
    /* adc gpio init */
    for (uint32_t i = 0; i < SENSOR_MATRIX_ANAS; i++) {
        gpio_set_pin_alternate(ana_pins[i], IOC_PAD_FUNC_CTL_ANALOG_MASK, 0);
    }

    /* pwr pin */
#ifdef SENSOR_MATRIX_PWR_PIN
    gpio_write_pin_high(SENSOR_MATRIX_PWR_PIN);
    gpio_set_pin_output(SENSOR_MATRIX_PWR_PIN);
#endif

    /* pwr left pin */
#ifdef SENSOR_MATRIX_PWRL_PIN
    gpio_write_pin_high(SENSOR_MATRIX_PWRL_PIN);
    gpio_set_pin_output(SENSOR_MATRIX_PWRL_PIN);
#endif

    /* pwr right pin */
#ifdef SENSOR_MATRIX_PWRR_PIN
    gpio_write_pin_high(SENSOR_MATRIX_PWRR_PIN);
    gpio_set_pin_output(SENSOR_MATRIX_PWRR_PIN);
#endif

    /* tmr pin */
#ifdef SENSOR_MATRIX_TMR_SHT_PIN
    gpio_set_pin_alternate(SENSOR_MATRIX_TMR_SHT_PIN, SENSOR_MATRIX_TMR_SHT_FUNC, 0);
#endif
#ifdef SENSOR_MATRIX_TMR_DMA_PIN
    gpio_set_pin_alternate(SENSOR_MATRIX_TMR_DMA_PIN, SENSOR_MATRIX_TMR_DMA_FUNC, 0);
#endif
#ifdef SENSOR_MATRIX_TMR_ADC_PIN
    gpio_set_pin_alternate(SENSOR_MATRIX_TMR_ADC_PIN, SENSOR_MATRIX_TMR_ADC_FUNC, 0);
#endif
#ifdef SENSOR_MATRIX_TMR_SOF_PIN
    gpio_set_pin_alternate(SENSOR_MATRIX_TMR_SOF_PIN, SENSOR_MATRIX_TMR_SOF_FUNC, 0);
#endif
}

static void sensor_matrix_set_mux(uint8_t val)
{
    uint32_t set = 0;
    uint32_t clr = 0;

    for (uint8_t i = 0; i < SENSOR_MATRIX_ADDRS; i++) {
        if (val & 1) {
            set |= 1 << mux_pins[i].npin;
        }
        else {
            clr |= 1 << mux_pins[i].npin;
        }
        val >>= 1;
    }

    mux_pins[0].ngpio->DO[mux_pins[0].nport].CLEAR = clr;
    mux_pins[0].ngpio->DO[mux_pins[0].nport].SET   = set;
}

static void sensor_matrix_init_mux(void)
{
    uint8_t nport = mux_pins[0].nport;
    (void)nport;

    mux_addr_mask = 0;

    /* check port */
    for (uint8_t i = 0; i < SENSOR_MATRIX_ADDRS; i++) {
        mux_addr_mask |= 1 << mux_pins[i].npin;
    }

    sensor_matrix_set_mux(0);
}

static uint32_t sensor_matrix_init_adc_clock(ADC16_Type *ptr, bool clk_src_ahb)
{
    return board_init_adc_clock(ptr, clk_src_ahb);
}

static void sensor_matrix_init_adc_common(ADC16_Type *ptr, bool clk_src_ahb)
{
    adc16_config_t cfg;

    (void)clk_src_ahb;
    adc16_get_default_config(&cfg);
    cfg.res          = SENSOR_MATRIX_ADC_CONVERT_CYCLE;
    cfg.conv_mode    = adc16_conv_mode_sequence;
    cfg.adc_clk_div  = (adc16_clock_divider_t)SENSOR_MATRIX_ADC_DIV;
#if !defined(HPM_IP_FEATURE_ADC16_FORCE_SYNC_AHB) || !HPM_IP_FEATURE_ADC16_FORCE_SYNC_AHB
    cfg.sel_sync_ahb = clk_src_ahb;
#endif
    cfg.adc_ahb_en   = true;

    adc16_init(ptr, &cfg);
}


static void sensor_matrix_init_adc_config(ADC16_Type *ptr, uint8_t idx)
{
    adc16_seq_config_t     seq_cfg;
    adc16_dma_config_t     dma_cfg;
    adc16_channel_config_t ch_cfg;
    const uint8_t          adc_index = idx;
    const uint8_t          channel_count = (adc_index < SENSOR_MATRIX_ADC_INSTANCES) ? adc_channel_counts[adc_index] : 0U;
    uint32_t               channel_offset = 0U;
    uint8_t                index = 0U;

    if ((adc_index >= SENSOR_MATRIX_ADC_INSTANCES) || (channel_count == 0U)) {
        return;
    }

    /* 前面几路 ADC 的通道数（含采样轮数）之和，就是本路在 seq_buff 中的起始偏移 */
    for (uint8_t i = 0; i < adc_index; i++) {
        channel_offset += (uint32_t)adc_channel_counts[i] * SENSOR_MATRIX_ADC_SAMPLE_ROUNDS;
    }

    memset(&seq_cfg, 0, sizeof(seq_cfg));
    memset(&dma_cfg, 0, sizeof(dma_cfg));
    adc16_get_channel_default_config(&ch_cfg);
    ch_cfg.sample_cycle = SENSOR_MATRIX_ADC_SAMPLE_CYCLE;

    /*
     * 每个通道连续放入 SAMPLE_ROUNDS 个序列项，使 seq_buff 按“通道优先”排列：
     * ch0 r0, ch0 r1, ch1 r0, ch1 r1, ...，这样 raw_buff 的线性下标就是
     * (sel * 通道数 + ch) * 轮数 + 采样序号，矩阵侧不需要额外的通道偏移表。
     */
    for (uint32_t i = 0; i < SENSOR_MATRIX_ANAS; i++) {
        const uint8_t inst = ana_func[i] / 16U;
        const uint8_t channel = ana_func[i] % 16U;

        if ((inst == adc_index) && (index < (channel_count * SENSOR_MATRIX_ADC_SAMPLE_ROUNDS))) {
            ch_cfg.ch = channel;
            adc16_init_channel(ptr, &ch_cfg);

            for (uint8_t s = 0; s < SENSOR_MATRIX_ADC_SAMPLE_ROUNDS; s++) {
                seq_cfg.queue[index].seq_int_en = false;
                seq_cfg.queue[index].ch = channel;
                index++;
            }
        }
    }

    if (index == 0U) {
        return;
    }

    /* 序列长度 = 通道数 × 采样轮数，各 ADC 的 seq_buff 首尾相接 */
    seq_cfg.seq_len    = index;

    if (index > g_sensor_matrix_timing.adc_seq_len) {
        g_sensor_matrix_timing.adc_seq_len = index;
    }
    seq_cfg.restart_en = false;
    seq_cfg.cont_en    = true;
    seq_cfg.hw_trig_en = true;
    seq_cfg.sw_trig_en = false;

    adc16_set_seq_config(ptr, &seq_cfg);

    dma_cfg.start_addr = (uint32_t *)core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&seq_buff[channel_offset]);
    dma_cfg.buff_len_in_4bytes = index;
    dma_cfg.stop_en = false;
    dma_cfg.stop_pos = 0;

    adc16_init_seq_dma(ptr, &dma_cfg);
}

static void sensor_matrix_init_adc(void)
{
    uint32_t freq;
    (void)freq;
    ADC16_Type *const adc_instances[SENSOR_MATRIX_ADC_INSTANCES] = SENSOR_MATRIX_ADC_PORT;

    for (uint8_t i = 0; i < SENSOR_MATRIX_ADC_INSTANCES; i++) {
        freq = sensor_matrix_init_adc_clock(adc_instances[i], true);
        sensor_matrix_init_adc_common(adc_instances[i], true);
        sensor_matrix_init_adc_config(adc_instances[i], i);
    }
}

static void sensor_matrix_init_trgm(void)
{
    /* Trigger mux initialization */
    trgm_output_t trgm_sel_cfg;

    trgm_sel_cfg.invert = false;
    trgm_sel_cfg.type   = trgm_output_same_as_input;
    trgm_sel_cfg.input  = SENSOR_MATRIX_TRGM_SEL_INPUT_SRC;

    /* trigger adc sampling */
    const uint8_t trgm_tag_src[SENSOR_MATRIX_ADC_INSTANCES] = SENSOR_MATRIX_ADC_OUTPUT_SOURCES;

    for (uint8_t i = 0; i < SENSOR_MATRIX_ADC_INSTANCES; i++) {
        trgm_output_config(SENSOR_MATRIX_TRGM, trgm_tag_src[i], &trgm_sel_cfg);
    }

    sensor_matrix_set_sof_src(sensor_sof_src);
}

static void sensor_matrix_init_dma_set(void)
{
    dma_channel_config_t dma_ch_config;
    uint32_t             idx = 0;

    dma_default_channel_config(MMX_HDMA, &dma_ch_config);
    memset(dma_set_chain_descriptors, 0, sizeof(dma_set_chain_descriptors));

    STATUS_REG[status_reg_magic0] = STATUS_MAGIC_ACT0;
    STATUS_REG[status_reg_magic1] = STATUS_MAGIC_ACT1;

    STATUS_REG[status_reg_preload_beg] = STATUS_MAGIC_ACT0;
    STATUS_REG[status_reg_preload_end] = STATUS_MAGIC_ACT0;
    STATUS_REG[status_reg_active_beg]  = STATUS_MAGIC_ACT1;
    STATUS_REG[status_reg_active_end]  = STATUS_MAGIC_ACT1;

    /* 1. set active_beg = preload_beg, handshake */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&STATUS_REG[status_reg_preload_beg]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&STATUS_REG[status_reg_active_beg]);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_HANDSHAKE;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* config but disable transfer */
        dma_setup_channel(MMX_HDMA, SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config, false);

        /* next link */
        idx++;
    } while (0);

    /* 2. set dma tmr ch reset, disable */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&TMR_CHANNEL_REG[tmr_dma_channel_cr_disable]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_DMA_CHANNEL].CR);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 3. set adc tmr ch reset, disable */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&TMR_CHANNEL_REG[tmr_adc_channel_cr_disable]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_ADC_CHANNEL].CR);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 4. load scan mux buffer */
    do {
        dma_ch_config.size_in_byte   = 4 * SENSOR_MATRIX_ROUND_SELS_MAX * 2;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&MUX_ADDR_REG[mux_addr_preload][0]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&MUX_ADDR_REG[mux_addr_shadow][0]);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 5. load scan array */
    do {
        dma_ch_config.size_in_byte   = (4 + CEILING(SENSOR_MATRIX_ROUND_SELS_MAX, 4) * 4);
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_SCAN_ARRAY_REG[sensor_scan_array_preload]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_SCAN_ARRAY_REG[sensor_scan_array_shadow]);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 6. setup dma_sel */
    do {
        dma_ch_config.size_in_byte   = 7 * 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&DMA_SEL_REG.LLPOINTER);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&(MMX_HDMA->CHCTRL[SENSOR_MATRIX_DMA_SEL_CHANNEL].LLPOINTER));
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = 7 - 1;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_DECREMENT;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_DECREMENT;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_CUSTOM_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 7. clr dma tmr ch reset, enable */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&TMR_CHANNEL_REG[tmr_dma_channel_cr_enable]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_DMA_CHANNEL].CR);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 8. clr adc tmr ch reset, enable */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&TMR_CHANNEL_REG[tmr_adc_channel_cr_enable]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_ADC_CHANNEL].CR);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 9. set tmr adc + dma sync */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&TMR_CHANNEL_REG[tmr_channel_gcr]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_MATRIX_TMR->GCR);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        /* loop chain */
        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_set_chain_descriptors[0]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_set_chain_descriptors[idx], SENSOR_MATRIX_DMA_SET_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    dmamux_config(HPM_DMAMUX, DMA_SOC_CHN_TO_DMAMUX_CHN(MMX_HDMA, SENSOR_MATRIX_DMA_SET_CHANNEL), SENSOR_MATRIX_DMA_SET_INPUT_SRC, true);
    dma_enable_channel(MMX_HDMA, SENSOR_MATRIX_DMA_SET_CHANNEL);
}

static void sensor_matrix_init_dma_sel(void)
{
    dma_channel_config_t dma_ch_config;
    uint32_t             idx = 0;
    const uint32_t       sel_copy_words = SENSOR_MATRIX_ADC_TOTAL_CHANNELS * SENSOR_MATRIX_ADC_SAMPLE_ROUNDS;

    dma_default_channel_config(MMX_HDMA, &dma_ch_config);
    memset(dma_sel_chain_descriptors, 0, sizeof(dma_sel_chain_descriptors));
    memset((void*)DUMMY_REG, 0, sizeof(DUMMY_REG));

    LLPOINTER[llpointer_buffer0] = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer0][0]) & ~0x7;
    LLPOINTER[llpointer_buffer1] = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer1][0]) & ~0x7;

    /* 1. pre (SEL 0), with handshake, mux addr (loaded directly into DMA_SEL_REG, executes in Slot 0) */
    do {
        dma_ch_config.size_in_byte   = 8;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&MUX_ADDR_REG[mux_addr_shadow][1]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&(mux_pins[0].ngpio->DO[mux_pins[0].nport].CLEAR));
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_2T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_HANDSHAKE;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_DECREMENT;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_DECREMENT;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;
        dma_ch_config.linked_ptr     = LLPOINTER[llpointer_buffer0];

        dma_setup_channel(MMX_HDMA, SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config, false); /* disable transfer */
        memcpy(&DMA_SEL_REG, &MMX_HDMA->CHCTRL[SENSOR_MATRIX_DMA_SEL_CHANNEL], sizeof(DMA_SEL_REG));

        /* enable transfer */
        DMA_SEL_REG.CTRL |= DMAV2_CHCTRL_CTRL_ENABLE_MASK;
    } while (0);

    /* 2. switch chain for next frame, no handshake */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.dst_addr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&DMA_SEL_REG.LLPOINTER);
        /* buffer 0 */
        dma_ch_config.src_addr   = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&LLPOINTER[llpointer_buffer1]);
        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);
        /* buffer 1 */
        dma_ch_config.src_addr   = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&LLPOINTER[llpointer_buffer0]);
        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 3. loop sel 0 .. (SELS_MAX - 2): switch mux to (sel + 1), then copy seq_buff of sel */
    for (uint32_t sel = 0; sel < (SENSOR_MATRIX_ROUND_SELS_MAX - 1); sel++) {
        /* 3.1 switch mux addr to (sel + 1), with handshake */
        do {
            dma_ch_config.size_in_byte   = 8;
            dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&MUX_ADDR_REG[mux_addr_shadow][(sel + 1) * 2 + 1]);
            dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&(mux_pins[0].ngpio->DO[mux_pins[0].nport].CLEAR));
            dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
            dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
            dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_2T;
            dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_HANDSHAKE;
            dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
            dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_DECREMENT;
            dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_DECREMENT;
            dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
            dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
            dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
            dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

            dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx + 1]);
            dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

            dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx + 1]);
            dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

            /* next link */
            idx++;
        } while (0);

        /* 3.2 copy adc data of sel, no handshake */
        do {
            dma_ch_config.size_in_byte   = sel_copy_words * sizeof(uint32_t);
            dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&seq_buff);
            dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
            dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
            dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
            dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
            dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
            dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
            dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
            dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
            dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
            dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
            dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

            dma_ch_config.dst_addr   = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&raw_buff[dma_sel_chain_buffer0][sel_copy_words * sel]);
            dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx + 1]);
            dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

            dma_ch_config.dst_addr   = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&raw_buff[dma_sel_chain_buffer1][sel_copy_words * sel]);
            dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx + 1]);
            dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

            /* next link */
            idx++;
        } while (0);
    }

    /* 4.1 disable dma tmr, with handshake (triggers after last sel ADC finishes) */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&TMR_CHANNEL_REG[tmr_dma_channel_cr_disable]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_DMA_CHANNEL].CR);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_HANDSHAKE;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 4.2 disable adc tmr immediately, no handshake (prevents redundant ADC trigger) */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&TMR_CHANNEL_REG[tmr_adc_channel_cr_disable]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_ADC_CHANNEL].CR);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 4.3 copy adc data of last sel (SELS_MAX - 1), no handshake */
    do {
        dma_ch_config.size_in_byte   = sel_copy_words * sizeof(uint32_t);
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&seq_buff);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.dst_addr   = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&raw_buff[dma_sel_chain_buffer0][sel_copy_words * (SENSOR_MATRIX_ROUND_SELS_MAX - 1)]);
        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        dma_ch_config.dst_addr   = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&raw_buff[dma_sel_chain_buffer1][sel_copy_words * (SENSOR_MATRIX_ROUND_SELS_MAX - 1)]);
        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 4.4 active_end = preload_end, no handshake */
    do {
        dma_ch_config.size_in_byte   = 4;
        dma_ch_config.src_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&STATUS_REG[status_reg_preload_end]);
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&STATUS_REG[status_reg_active_end]);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        dma_ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx + 1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    /* 4.5 switch preload & trigger TC interrupt, no handshake */
    do {
        dma_ch_config.size_in_byte   = 8;
        dma_ch_config.dst_addr       = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&STATUS_REG[status_reg_preload_beg]);
        dma_ch_config.src_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.dst_width      = DMA_TRANSFER_WIDTH_WORD;
        dma_ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_2T;
        dma_ch_config.src_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.dst_mode       = DMA_HANDSHAKE_MODE_NORMAL;
        dma_ch_config.src_addr_ctrl  = DMA_ADDRESS_CONTROL_FIXED;
        dma_ch_config.dst_addr_ctrl  = DMA_ADDRESS_CONTROL_INCREMENT;
        dma_ch_config.priority       = DMA_CHANNEL_PRIORITY_HIGH;
        dma_ch_config.interrupt_mask = DMA_INTERRUPT_MASK_ALL & ~DMA_INTERRUPT_MASK_TERMINAL_COUNT;
        dma_ch_config.handshake_opt  = DMA_HANDSHAKE_OPT_ALL_TRANSIZE;
        dma_ch_config.burst_opt      = DMA_SRC_BURST_OPT_STANDAND_SIZE;
        dma_ch_config.linked_ptr     = 0;

        dma_ch_config.src_addr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&STATUS_REG[status_reg_magic1]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer0][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        dma_ch_config.src_addr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&STATUS_REG[status_reg_magic0]);
        dma_config_linked_descriptor(MMX_HDMA, &dma_sel_chain_descriptors[dma_sel_chain_buffer1][idx], SENSOR_MATRIX_DMA_SEL_CHANNEL, &dma_ch_config);

        /* next link */
        idx++;
    } while (0);

    dmamux_config(HPM_DMAMUX, DMA_SOC_CHN_TO_DMAMUX_CHN(MMX_HDMA, SENSOR_MATRIX_DMA_SEL_CHANNEL), SENSOR_MATRIX_DMA_SEL_INPUT_SRC, true);
}

static void sensor_matrix_init_timer(void)
{
    float    ssof_freq;
    float    scan_freq;
    uint32_t tmr_clock;
    uint32_t adc_clock;

    gptmr_channel_config_t config_sht;
    gptmr_channel_config_t config_dma;
    gptmr_channel_config_t config_adc;
    gptmr_channel_config_t config_sof;

    /* default set to 1 */
    sensor_scan_interval = 1;

    /* default set to 0 */
    sensor_scan_adjust = 0.0f;

    tmr_clock = clock_get_frequency(SENSOR_MATRIX_TMR_CLOCK);
    // adc max clock is 50MHz
    adc_clock = clock_get_frequency(clock_adc0) / SENSOR_MATRIX_ADC_DIV;

    gptmr_channel_get_default_config(SENSOR_MATRIX_TMR, &config_sht);
    gptmr_channel_get_default_config(SENSOR_MATRIX_TMR, &config_dma);
    gptmr_channel_get_default_config(SENSOR_MATRIX_TMR, &config_adc);
    gptmr_channel_get_default_config(SENSOR_MATRIX_TMR, &config_sof);

    if (sensor_sof_src == sensor_matrix_sof_usb) {
        config_sht.synci_edge = gptmr_synci_edge_both;
    }
    else {
        config_sht.synci_edge = gptmr_synci_edge_rising;
    }

    /* shot channel */
    config_sht.cmp[0]                    = 1 + SENSOR_MATRIX_SOF_ST_TIME;
    config_sht.cmp[1]                    = 1 + SENSOR_MATRIX_SOF_ST_TIME + 5; /* keep 50ns high */
    config_sht.reload                    = SENSOR_MATRIX_TMR_RLD_MAX;
    config_sht.enable_cmp_output         = true;
    config_sht.cmp_initial_polarity_high = false;
    config_sht.enable_software_sync      = false;
    config_sht.dma_request_event         = gptmr_dma_request_on_cmp0;
    config_sht.debug_mode                = true;
    gptmr_channel_config(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL, &config_sht, false);
    gptmr_channel_enable_dma_request(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL, true);

    /* dma channel */
    scan_freq                            = SENSOR_MATRIX_TMR_SEL_FREQ;
    config_dma.cmp[0]                    = SENSOR_MATRIX_TMR_ST_TIME;
    config_dma.cmp[1]                    = SENSOR_MATRIX_TMR_ST_TIME + SENSOR_MATRIX_SEL_HT_TIME1 + SENSOR_MATRIX_SEL_HT_TIME2; /* keep working high */
    config_dma.reload                    = (uint32_t)(tmr_clock / scan_freq);
    scan_freq                            = (tmr_clock / (float)config_dma.reload);
    config_dma.enable_cmp_output         = true;
    config_dma.cmp_initial_polarity_high = false;
    config_dma.enable_software_sync      = true;
    config_dma.dma_request_event         = gptmr_dma_request_on_cmp0;
    config_dma.debug_mode                = true;
    gptmr_channel_config(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_DMA_CHANNEL, &config_dma, false);
    gptmr_channel_enable_dma_request(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_DMA_CHANNEL, true);

#ifdef HPMSOC_HAS_HPMSDK_GPTMRV2
    TMR_CHANNEL_REG[tmr_dma_channel_cr_disable] = SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_DMA_CHANNEL].CR & ~GPTMRV2_CHANNEL_CR_CEN_MASK;
    TMR_CHANNEL_REG[tmr_dma_channel_cr_enable]  = TMR_CHANNEL_REG[tmr_dma_channel_cr_disable] | GPTMRV2_CHANNEL_CR_CEN_MASK;
#else
    TMR_CHANNEL_REG[tmr_dma_channel_cr_disable] = SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_DMA_CHANNEL].CR & ~GPTMR_CHANNEL_CR_CEN_MASK;
    TMR_CHANNEL_REG[tmr_dma_channel_cr_enable]  = TMR_CHANNEL_REG[tmr_dma_channel_cr_disable] | GPTMR_CHANNEL_CR_CEN_MASK;
#endif

    /* adc channel */
#if ((SENSOR_MATRIX_ADC_SAMPLES) > 1)
    uint32_t cycle = (uint64_t)tmr_clock * (SENSOR_MATRIX_ADC_SAMPLE_CYCLE + SENSOR_MATRIX_ADC_CONVERT_CYCLE) * (SENSOR_MATRIX_ADC_CHANNELS) * 2 / (uint64_t)adc_clock;
#else
    uint32_t cycle = (uint64_t)tmr_clock * (SENSOR_MATRIX_ADC_SAMPLE_CYCLE + SENSOR_MATRIX_ADC_CONVERT_CYCLE) * (SENSOR_MATRIX_ADC_CHANNELS) / (uint64_t)adc_clock;
#endif

    g_sensor_matrix_timing.adc_window_ticks = (uint16_t)cycle;
    g_sensor_matrix_timing.copy_words = (uint16_t)(SENSOR_MATRIX_ADC_TOTAL_CHANNELS * SENSOR_MATRIX_ADC_SAMPLE_ROUNDS);

    config_adc.cmp[0]                    = SENSOR_MATRIX_TMR_ST_TIME + SENSOR_MATRIX_SEL_HT_TIME1 + SENSOR_MATRIX_ADC_ST_TIME;
    config_adc.cmp[1]                    = SENSOR_MATRIX_TMR_ST_TIME + SENSOR_MATRIX_SEL_HT_TIME1 + SENSOR_MATRIX_ADC_ST_TIME + cycle; /* keep high when sample */

    /* 防止实际外设时钟与配置宏不一致导致 reload <= cmp[1] 溢出 */
    uint32_t min_reload = config_adc.cmp[1] + (uint32_t)SENSOR_MATRIX_SEL_MARGIN_TICKS;
    if (config_dma.reload < min_reload) {
        config_dma.reload = min_reload;
        gptmr_channel_config_update_reload(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_DMA_CHANNEL, config_dma.reload);
        scan_freq = (tmr_clock / (float)config_dma.reload);
    }

    config_adc.reload                    = config_dma.reload;
    config_adc.enable_cmp_output         = true;
    config_adc.cmp_initial_polarity_high = false;
    config_adc.enable_software_sync      = true;
    config_adc.dma_request_event         = gptmr_dma_request_disabled;
    config_adc.debug_mode                = true;
    gptmr_channel_config(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_ADC_CHANNEL, &config_adc, false);

#ifdef HPMSOC_HAS_HPMSDK_GPTMRV2
    TMR_CHANNEL_REG[tmr_adc_channel_cr_disable] = SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_ADC_CHANNEL].CR & ~GPTMRV2_CHANNEL_CR_CEN_MASK;
    TMR_CHANNEL_REG[tmr_adc_channel_cr_enable]  = TMR_CHANNEL_REG[tmr_adc_channel_cr_disable] | GPTMRV2_CHANNEL_CR_CEN_MASK;
#else
    TMR_CHANNEL_REG[tmr_adc_channel_cr_disable] = SENSOR_MATRIX_TMR->CHANNEL[SENSOR_MATRIX_TMR_ADC_CHANNEL].CR & ~GPTMR_CHANNEL_CR_CEN_MASK;
    TMR_CHANNEL_REG[tmr_adc_channel_cr_enable]  = TMR_CHANNEL_REG[tmr_adc_channel_cr_disable] | GPTMR_CHANNEL_CR_CEN_MASK;
#endif

    TMR_CHANNEL_REG[tmr_channel_gcr] = GPTMR_CH_GCR_SWSYNCT_MASK(SENSOR_MATRIX_TMR_ADC_CHANNEL) | GPTMR_CH_GCR_SWSYNCT_MASK(SENSOR_MATRIX_TMR_DMA_CHANNEL);

    /* sof channel */
    ssof_freq                            = ((SENSOR_MATRIX_TMR_SHOT_FREQ) / sensor_scan_interval) + sensor_scan_adjust;
    config_sof.cmp[0]                    = 1;
    config_sof.cmp[1]                    = 1 + 5; /* keep 50ns high @100MHz */
    config_sof.reload                    = (uint32_t)(tmr_clock / ssof_freq);
    ssof_freq                            = (tmr_clock / (float)config_sof.reload);
    config_sof.enable_cmp_output         = true;
    config_sof.cmp_initial_polarity_high = false;
    config_sof.enable_software_sync      = false;
    config_sof.dma_request_event         = gptmr_dma_request_disabled;
    config_sof.debug_mode                = true;
    gptmr_channel_config(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SOF_CHANNEL, &config_sof, false);

    printf("sensor matrix update sel freq %d Hz, reload %d\n", (uint32_t)scan_freq, config_dma.reload);
    printf("sensor matrix update sof freq %d Hz, reload %d\n", (uint32_t)ssof_freq, config_sof.reload);
}

__attribute__((weak)) void sensor_matrix_post_init(void)
{
    sensor_matrix_set_config_t config = {0};

    /* must be change all in first init */
    config.valid_scan  = true;
    config.valid_timer = true;

    const uint8_t scan_order[SENSOR_MATRIX_ROUND_SELS_MAX] = SENSOR_MATRIX_SELS_SEQUENCE;
    memcpy(config.list, scan_order, sizeof(scan_order));

    config.count    = SENSOR_MATRIX_ROUND_SELS_MAX;
    config.interval = 1;
    config.adjust   = 0;

    sensor_matrix_set_config(config);
}

void sensor_matrix_recv_cb(hall_recv_cb_t recv_cb)
{
    hall_recv_cb = recv_cb;
}

void sensor_matrix_init(void)
{
    (void)sensor_sof_src_saved;
    sensor_sof_src = sensor_matrix_sof_usb;

    /* clear buffer */
    memset(raw_buff, 0xff, sizeof(raw_buff));

    /* enable gptmr clock */
    clock_add_to_group(SENSOR_MATRIX_TMR_CLOCK, 0);

    sensor_matrix_init_pins();

    sensor_matrix_init_mux();

    sensor_matrix_init_adc();

    sensor_matrix_init_timer();

    sensor_matrix_init_trgm();

    sensor_matrix_init_dma_set();

    sensor_matrix_init_dma_sel();

    sensor_matrix_post_init();

    intc_m_enable_irq_with_priority(MMX_HDMA_IRQ, HDMA_IRQ_PRIORITY);
}

bool sensor_matrix_scan_is_busy(void)
{
    uint32_t beg0 = STATUS_REG[status_reg_active_beg];
    uint32_t end0 = STATUS_REG[status_reg_active_end];
    uint32_t beg1 = STATUS_REG[status_reg_active_beg];
    uint32_t end1 = STATUS_REG[status_reg_active_end];

    if ((beg0 == beg1) && (end0 == end1) && (beg0 == end0)) {
        return false;
    }

    return true;
}

void sensor_matrix_set_sof_src(uint8_t sof_src)
{
    static const char     *src_str[4] = {"usb", "ext", "tmr", "none"};
    trgm_output_t          trgm_sof_cfg;
    gptmr_channel_config_t config_sht;

    gptmr_channel_get_default_config(SENSOR_MATRIX_TMR, &config_sht);

    /* shot channel */
    config_sht.cmp[0] = 10 + SENSOR_MATRIX_SOF_ST_TIME;
    config_sht.cmp[1] = 10 + SENSOR_MATRIX_SOF_ST_TIME + 50; /* keep 500ns high */

    config_sht.reload                    = SENSOR_MATRIX_TMR_RLD_MAX;
    config_sht.synci_edge                = gptmr_synci_edge_rising;
    config_sht.enable_cmp_output         = true;
    config_sht.cmp_initial_polarity_high = false;
    config_sht.enable_software_sync      = false;
    config_sht.dma_request_event         = gptmr_dma_request_on_cmp0;
    config_sht.debug_mode                = true;

    trgm_sof_cfg.invert = false;
    trgm_sof_cfg.type   = trgm_output_same_as_input;

    switch (sof_src) {
        case sensor_matrix_sof_usb:
            config_sht.synci_edge = gptmr_synci_edge_both;
            sensor_sof_src        = sensor_matrix_sof_usb;
            trgm_sof_cfg.input    = SENSOR_MATRIX_TRGM_SOF_USB_INPUT_SRC;
            break;
        case sensor_matrix_sof_ext:
            sensor_sof_src     = sensor_matrix_sof_ext;
            trgm_sof_cfg.input = SENSOR_MATRIX_TRGM_SOF_EXT_INPUT_SRC;
            break;
        case sensor_matrix_sof_tmr:
            sensor_sof_src     = sensor_matrix_sof_tmr;
            trgm_sof_cfg.input = SENSOR_MATRIX_TRGM_SOF_TMR_INPUT_SRC;
            break;
        default:
            sensor_sof_src     = sensor_matrix_sof_none;
            trgm_sof_cfg.input = SENSOR_MATRIX_TRGM_SOF_NONE_INPUT_SRC;
            break;
    }

    gptmr_channel_config(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL, &config_sht, false);
    gptmr_channel_enable_dma_request(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL, true);

    trgm_output_config(SENSOR_MATRIX_TRGM, SENSOR_MATRIX_TRGM_SOF_OUTPUT_SRC, &trgm_sof_cfg);

    (void)src_str;
    printf("sensor matrix sof src %s\n", src_str[sensor_sof_src]);
}

void sensor_matrix_stop_sof_timer(void)
{
    gptmr_stop_counter(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL);
    gptmr_stop_counter(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SOF_CHANNEL);
    gptmr_channel_reset_count(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL);
    gptmr_channel_reset_count(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SOF_CHANNEL);
}

void sensor_matrix_start_sof_timer(void)
{
    /* reserve 1s time to config */
    gptmr_channel_update_count(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL, 0xffffffff - 100000000);
    gptmr_channel_reset_count(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SOF_CHANNEL);
    gptmr_start_counter(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SHT_CHANNEL);
    gptmr_start_counter(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SOF_CHANNEL);
}

void sensor_matrix_restart_sof_timer(void)
{
    sensor_matrix_stop_sof_timer();
    sensor_matrix_start_sof_timer();
}

void sensor_matrix_stop_sel_timer(void)
{
    gptmr_stop_counter(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_ADC_CHANNEL);
    gptmr_stop_counter(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_DMA_CHANNEL);
    gptmr_channel_reset_count(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_ADC_CHANNEL);
    gptmr_channel_reset_count(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_DMA_CHANNEL);
}

void sensor_matrix_set_sof_timer(uint32_t interval, float adjust)
{
    uint32_t tmr_clock;
    uint32_t tmr_reload;
    float    ssof_freq;

    /* set to new interval and adjust */
    sensor_scan_interval = interval;
    sensor_scan_adjust   = adjust;

    ssof_freq = ((SENSOR_MATRIX_TMR_SHOT_FREQ) / sensor_scan_interval) + sensor_scan_adjust;

    tmr_clock  = clock_get_frequency(SENSOR_MATRIX_TMR_CLOCK);
    tmr_reload = (uint32_t)(tmr_clock / ssof_freq);
    ssof_freq  = (tmr_clock / (float)tmr_reload);

    gptmr_channel_config_update_reload(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_SOF_CHANNEL, tmr_reload);

    printf("sensor matrix update sof freq %d Hz, reload %d\n", (uint32_t)ssof_freq, tmr_reload);
}

void sensor_matrix_set_next_scan(uint32_t count, const uint8_t *list)
{
    uint32_t *data_buffer = (void *)MUX_ADDR_REG[mux_addr_preload];
    uint32_t  dummy;

    if (count > SENSOR_MATRIX_ROUND_SELS_MAX) {
        count = SENSOR_MATRIX_ROUND_SELS_MAX;
    }

    dummy = SENSOR_MATRIX_ROUND_SELS_MAX - count;

    /* dummy select */
    for (uint8_t i = 0; i < dummy; i++) {
        data_buffer[0] = 0;
        data_buffer[1] = mux_addr_mask;
        data_buffer += 2;
    }

    /* true select */
    for (uint32_t sel = 0; sel < count; sel++) {
        uint32_t clr_mask = mux_addr_mask;
        uint32_t set_mask = 0;
        uint8_t  true_sel = list[sel];

        for (uint8_t i = 0; i < SENSOR_MATRIX_ADDRS; i++) {
            set_mask |= (true_sel & 1) << mux_pins[i].npin;
            true_sel >>= 1;
        }

        data_buffer[0] = set_mask;
        data_buffer[1] = clr_mask;
        data_buffer += 2;
    }

    SENSOR_SCAN_ARRAY_REG[sensor_scan_array_preload].count = count;
    memcpy((void *)SENSOR_SCAN_ARRAY_REG[sensor_scan_array_preload].array, list, count);
}

void sensor_matrix_set_config(sensor_matrix_set_config_t config)
{
    sensor_matrix_stop_sof_timer();

    sensor_matrix_stop_sel_timer();

    /* step.1 set scan mode */
    if (config.valid_mode) {
        sensor_matrix_init_timer();
        printf("sensor matrix update to [%s] mode\n", config.mode ? "low-power" : "high-performance");
    }

    /* step.2 set scan count */
    if (config.valid_scan) {
        sensor_matrix_set_next_scan(config.count, config.list);
        printf("sensor matrix update scan count [%d]\n", config.count);
        printf("sensor matrix update default scan list ");
        for (uint8_t i = 0; i < config.count; i++) {
            printf("[%d] ", config.list[i]);
        }
        printf("\n");
    }

    /* step.3 set timer */
    if (config.valid_timer) {
        sensor_matrix_set_sof_timer(config.interval, config.adjust);
    }

    sensor_matrix_start_sof_timer();
}
