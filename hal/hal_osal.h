#ifndef HAL_OSAL_H
#define HAL_OSAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__)
#define HAL_OSAL_INLINE static inline __attribute__((always_inline))
#else
#define HAL_OSAL_INLINE static inline
#endif

#define HAL_OSAL_WAIT_FOREVER UINT32_MAX

typedef void (*hal_osal_thread_entry_t)(void *argument);

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
#include "tx_api.h"

typedef struct {
    TX_MUTEX native;
    bool created;
} hal_osal_mutex_t;

typedef struct {
    TX_THREAD native;
    TX_SEMAPHORE notification;
    hal_osal_thread_entry_t entry;
    void *argument;
    bool created;
    bool notification_created;
} hal_osal_thread_t;

typedef struct {
    TX_SEMAPHORE native;
    uint32_t max_count;
    bool created;
} hal_osal_sem_t;

#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

typedef struct {
    StaticSemaphore_t storage;
    SemaphoreHandle_t native;
    bool created;
} hal_osal_mutex_t;

typedef struct {
    StaticTask_t storage;
    TaskHandle_t native;
    bool created;
} hal_osal_thread_t;

typedef struct {
    StaticSemaphore_t storage;
    SemaphoreHandle_t native;
    bool created;
} hal_osal_sem_t;

#else

typedef struct {
    bool created;
} hal_osal_mutex_t;

typedef struct {
    bool created;
} hal_osal_thread_t;

typedef struct {
    uint32_t max_count;
    bool created;
} hal_osal_sem_t;

#endif

HAL_OSAL_INLINE void hal_osal_init(void)
{
}

HAL_OSAL_INLINE uint32_t hal_osal_timeout_to_ticks(uint32_t timeout_ms)
{
#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    if (timeout_ms == HAL_OSAL_WAIT_FOREVER) {
        return TX_WAIT_FOREVER;
    }
    return (uint32_t)(((uint64_t)timeout_ms * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    if (timeout_ms == HAL_OSAL_WAIT_FOREVER) {
        return portMAX_DELAY;
    }
    uint32_t ticks = pdMS_TO_TICKS(timeout_ms);
    return ((timeout_ms > 0U) && (ticks == 0U)) ? 1U : ticks;
#else
    return timeout_ms;
#endif
}

HAL_OSAL_INLINE void hal_osal_delay_ms(uint32_t ms)
{
#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    #if TX_TIMER_TICKS_PER_SECOND != 1000
    #error "TX_TIMER_TICKS_PER_SECOND must be 1000"
    uint32_t ticks = (uint32_t)(((uint64_t)ms * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U);
    #endif
    tx_thread_sleep(ms);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    vTaskDelay(pdMS_TO_TICKS(ms));
#else
    (void)ms;
#endif
}

HAL_OSAL_INLINE bool hal_osal_mutex_create(hal_osal_mutex_t *mutex, const char *name)
{
#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    if ((mutex == NULL) || (TX_SUCCESS != tx_mutex_create(&mutex->native, (CHAR *)name, TX_INHERIT))) {
        return false;
    }
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    (void)name;
    if (mutex == NULL) {
        return false;
    }
    mutex->native = xSemaphoreCreateMutexStatic(&mutex->storage);
    if (mutex->native == NULL) {
        return false;
    }
#else
    (void)name;
    if (mutex == NULL) {
        return false;
    }
#endif

    mutex->created = true;
    return true;
}

HAL_OSAL_INLINE void hal_osal_mutex_destroy(hal_osal_mutex_t *mutex)
{
    if ((mutex == NULL) || !mutex->created) {
        return;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    (void)tx_mutex_delete(&mutex->native);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    vSemaphoreDelete(mutex->native);
    mutex->native = NULL;
#endif

    mutex->created = false;
}

HAL_OSAL_INLINE bool hal_osal_mutex_lock(hal_osal_mutex_t *mutex)
{
    if ((mutex == NULL) || !mutex->created) {
        return false;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    return TX_SUCCESS == tx_mutex_get(&mutex->native, TX_WAIT_FOREVER);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    return pdTRUE == xSemaphoreTake(mutex->native, portMAX_DELAY);
#else
    return true;
#endif
}

HAL_OSAL_INLINE bool hal_osal_mutex_unlock(hal_osal_mutex_t *mutex)
{
    if ((mutex == NULL) || !mutex->created) {
        return false;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    return TX_SUCCESS == tx_mutex_put(&mutex->native);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    return pdTRUE == xSemaphoreGive(mutex->native);
#else
    return true;
#endif
}

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
static void __attribute__((unused)) hal_osal_threadx_entry(ULONG context)
{
    hal_osal_thread_t *thread = (hal_osal_thread_t *)(uintptr_t)context;
    thread->entry(thread->argument);
}
#endif

/*
 * Thread storage and stack are caller-owned and must remain valid for the
 * thread lifetime. The stack must be 8-byte aligned. Smaller priority values
 * represent higher priority on both supported RTOS backends.
 */
HAL_OSAL_INLINE bool hal_osal_thread_create(hal_osal_thread_t *thread,
                                             const char *name,
                                             void *stack,
                                             uint32_t stack_size,
                                             uint32_t priority,
                                             hal_osal_thread_entry_t entry,
                                             void *argument)
{
    if ((thread == NULL) || (stack == NULL) || (entry == NULL) || (stack_size == 0U)) {
        return false;
    }

    memset(thread, 0, sizeof(*thread));

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    if ((priority >= TX_MAX_PRIORITIES) || (((uintptr_t)stack & 0x7U) != 0U)) {
        return false;
    }
    if (TX_SUCCESS != tx_semaphore_create(&thread->notification, (CHAR *)name, 0U)) {
        return false;
    }
    thread->notification_created = true;
    thread->entry = entry;
    thread->argument = argument;
    thread->created = true;
    if (TX_SUCCESS != tx_thread_create(&thread->native,
                                       (CHAR *)name,
                                       hal_osal_threadx_entry,
                                       (ULONG)(uintptr_t)thread,
                                       (ULONG *)stack,
                                       stack_size,
                                       priority,
                                       priority,
                                       TX_NO_TIME_SLICE,
                                       TX_AUTO_START)) {
        (void)tx_semaphore_delete(&thread->notification);
        thread->notification_created = false;
        thread->created = false;
        return false;
    }
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    if (priority >= configMAX_PRIORITIES) {
        return false;
    }
    uint32_t stack_words = stack_size / sizeof(StackType_t);
    if (stack_words < configMINIMAL_STACK_SIZE) {
        return false;
    }
    thread->created = true;
    thread->native = xTaskCreateStatic((TaskFunction_t)entry,
                                       name,
                                       stack_words,
                                       argument,
                                       (configMAX_PRIORITIES - 1U) - priority,
                                       (StackType_t *)stack,
                                       &thread->storage);
    if (thread->native == NULL) {
        thread->created = false;
        return false;
    }
#else
    (void)name;
    (void)stack;
    (void)stack_size;
    (void)priority;
    (void)entry;
    (void)argument;
    thread->created = true;
#endif

    return true;
}

HAL_OSAL_INLINE void hal_osal_thread_destroy(hal_osal_thread_t *thread)
{
    if ((thread == NULL) || !thread->created) {
        return;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    if (thread->notification_created) {
        (void)tx_semaphore_delete(&thread->notification);
        thread->notification_created = false;
    }
    (void)tx_thread_delete(&thread->native);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    vTaskDelete(thread->native);
    thread->native = NULL;
#endif

    thread->created = false;
}

HAL_OSAL_INLINE void hal_osal_thread_yield(void)
{
#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    tx_thread_relinquish();
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    taskYIELD();
#endif
}

/*
 * max_count == 1 creates a binary semaphore; larger values create a counting
 * semaphore. Timeouts are expressed in milliseconds.
 */
HAL_OSAL_INLINE bool hal_osal_sem_create(hal_osal_sem_t *sem,
                                          const char *name,
                                          uint32_t initial_count,
                                          uint32_t max_count)
{
    if ((sem == NULL) || (max_count == 0U) || (initial_count > max_count)) {
        return false;
    }

    memset(sem, 0, sizeof(*sem));
    sem->max_count = max_count;

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    if (TX_SUCCESS != tx_semaphore_create(&sem->native, (CHAR *)name, initial_count)) {
        return false;
    }
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    (void)name;
    sem->native = xSemaphoreCreateCountingStatic(max_count, initial_count, &sem->storage);
    if (sem->native == NULL) {
        return false;
    }
#else
    (void)name;
#endif

    sem->created = true;
    return true;
}

HAL_OSAL_INLINE void hal_osal_sem_destroy(hal_osal_sem_t *sem)
{
    if ((sem == NULL) || !sem->created) {
        return;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    (void)tx_semaphore_delete(&sem->native);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    vSemaphoreDelete(sem->native);
    sem->native = NULL;
#endif

    sem->created = false;
}

HAL_OSAL_INLINE bool hal_osal_sem_wait(hal_osal_sem_t *sem, uint32_t timeout_ms)
{
    if ((sem == NULL) || !sem->created) {
        return false;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    return TX_SUCCESS == tx_semaphore_get(&sem->native, hal_osal_timeout_to_ticks(timeout_ms));
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    return pdTRUE == xSemaphoreTake(sem->native, hal_osal_timeout_to_ticks(timeout_ms));
#else
    (void)timeout_ms;
    return false;
#endif
}

HAL_OSAL_INLINE bool hal_osal_sem_post(hal_osal_sem_t *sem)
{
    if ((sem == NULL) || !sem->created) {
        return false;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    return TX_SUCCESS == tx_semaphore_ceiling_put(&sem->native, sem->max_count);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    return pdTRUE == xSemaphoreGive(sem->native);
#else
    return true;
#endif
}

HAL_OSAL_INLINE bool hal_osal_sem_post_from_isr(hal_osal_sem_t *sem, bool *higher_priority_task_woken)
{
    if ((sem == NULL) || !sem->created) {
        return false;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = false;
    }
    return TX_SUCCESS == tx_semaphore_ceiling_put(&sem->native, sem->max_count);
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    BaseType_t woken = pdFALSE;
    BaseType_t result = xSemaphoreGiveFromISR(sem->native, &woken);
    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = (woken == pdTRUE);
    }
    return result == pdTRUE;
#else
    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = false;
    }
    return true;
#endif
}

/* Call after all from-ISR post/notify operations, at the end of the ISR. */
HAL_OSAL_INLINE void hal_osal_isr_exit(bool higher_priority_task_woken)
{
#if defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    portYIELD_FROM_ISR(higher_priority_task_woken ? pdTRUE : pdFALSE);
#else
    (void)higher_priority_task_woken;
#endif
}

HAL_OSAL_INLINE void hal_osal_sem_reset(hal_osal_sem_t *sem)
{
    if ((sem == NULL) || !sem->created) {
        return;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    while (TX_SUCCESS == tx_semaphore_get(&sem->native, TX_NO_WAIT)) {
    }
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    while (pdTRUE == xSemaphoreTake(sem->native, 0U)) {
    }
#endif
}

HAL_OSAL_INLINE bool hal_osal_thread_notify(hal_osal_thread_t *thread)
{
    if ((thread == NULL) || !thread->created) {
        return false;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    return thread->notification_created &&
           (TX_SUCCESS == tx_semaphore_put(&thread->notification));
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    return pdPASS == xTaskNotifyGive(thread->native);
#else
    return true;
#endif
}

HAL_OSAL_INLINE bool hal_osal_thread_notify_from_isr(hal_osal_thread_t *thread,
                                                      bool *higher_priority_task_woken)
{
    if ((thread == NULL) || !thread->created) {
        return false;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = false;
    }
    return thread->notification_created &&
           (TX_SUCCESS == tx_semaphore_put(&thread->notification));
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(thread->native, &woken);
    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = (woken == pdTRUE);
    }
    return true;
#else
    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = false;
    }
    return true;
#endif
}

/* Wait from the target thread itself. Returns the number of pending notifications. */
HAL_OSAL_INLINE uint32_t hal_osal_thread_notification_wait(hal_osal_thread_t *thread,
                                                            uint32_t timeout_ms,
                                                            bool clear_on_exit)
{
    if ((thread == NULL) || !thread->created) {
        return 0U;
    }

#if defined(CONFIG_THREADX) && (CONFIG_THREADX == 1)
    if (!thread->notification_created || (tx_thread_identify() != &thread->native)) {
        return 0U;
    }
    if (TX_SUCCESS != tx_semaphore_get(&thread->notification, hal_osal_timeout_to_ticks(timeout_ms))) {
        return 0U;
    }

    CHAR *name = NULL;
    TX_THREAD *first_suspended = NULL;
    TX_SEMAPHORE *next_semaphore = NULL;
    ULONG suspended_count = 0U;
    ULONG count = 0U;
    uint32_t notification_count = 1U;

    if (TX_SUCCESS == tx_semaphore_info_get(&thread->notification,
                                            &name,
                                            &count,
                                            &first_suspended,
                                            &suspended_count,
                                            &next_semaphore)) {
        notification_count += (uint32_t)count;
    }

    if (clear_on_exit) {
        while (TX_SUCCESS == tx_semaphore_get(&thread->notification, TX_NO_WAIT)) {
        }
    }
    return notification_count;
#elif defined(CONFIG_FREERTOS) && (CONFIG_FREERTOS == 1)
    if (xTaskGetCurrentTaskHandle() != thread->native) {
        return 0U;
    }
    return (uint32_t)ulTaskNotifyTake(clear_on_exit ? pdTRUE : pdFALSE,
                                      hal_osal_timeout_to_ticks(timeout_ms));
#else
    (void)timeout_ms;
    (void)clear_on_exit;
    return 0U;
#endif
}

#endif

