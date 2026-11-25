#ifndef TX_TASK_CONFIG_H
#define TX_TASK_CONFIG_H


/* 以下栈大小按 -fstack-usage 的静态峰值 + 约 20% 余量确定 */
#define DB_THREAD_STACK         (1536U)     /* 峰值约 0.9~1.0KB，主要是 printf / flash */
#define ROTCC_THREAD_STACK      (3072U)     /* 峰值约 2.2KB，rotgen_send 组帧占 1KB */
#define CST_THREAD_STACK        (1024U)     /* 峰值约 0.98KB，已被 hal_dt_printf 占满，勿再降 */
#define CORE_THREAD_STACK       (1024U)     /* 峰值约 0.5KB */
#define DT_THREAD_STACK         (1024U)     /* 峰值约 0.6KB，含 USB 发送链 */
#define CALIB_THREAD_STACK      (768U)      /* 峰值约 0.4KB */
#define HID_THREAD_STACK        (1024U)     /* 峰值约 0.7KB */

#define HID_THREAD_PRIORITY     (TX_MAX_PRIORITIES - 1U)
#define CORE_THREAD_PRIORITY    (TX_MAX_PRIORITIES - 1U)
#define CALIB_THREAD_PRIORITY   (TX_MAX_PRIORITIES - 2U)
#define ROTCC_THREAD_PRIORITY   (TX_MAX_PRIORITIES - 3U)
#define DB_THREAD_PRIORITY      (TX_MAX_PRIORITIES - 4U)
#define DT_THREAD_PRIORITY      (TX_MAX_PRIORITIES - 5U)
#define CST_THREAD_PRIORITY     (TX_MAX_PRIORITIES - 2U)


#endif

