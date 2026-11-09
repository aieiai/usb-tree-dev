#ifndef UPGRADE_TASK_H
#define UPGRADE_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "rot_state.h"
#include "rotcc_protocol.h"


typedef enum {
    ROT_UPGRADE_CMD_START = 0,
    ROT_UPGRADE_CMD_DATA,
    ROT_UPGRADE_CMD_END,
    ROT_UPGRADE_CMD_ABORT,
    ROT_UPGRADE_CMD_QUERY,
} rot_upgrade_command_t;


typedef struct {
    uint8_t command;
    uint8_t reserved0;
    uint16_t seq;
    uint16_t payload_len;
    uint16_t reserved1;
    uint32_t session_id;
    uint32_t file_size;
    uint8_t payload[ROTCC_FW_MAX_PAYLOAD_SIZE];
} rot_upgrade_job_t;


/* Creates the queue and the dedicated upgrade worker thread. */
rot_state_t upgrade_task_init(void);

/* Copies one validated command into the worker queue without blocking. */
rot_state_t upgrade_task_submit(const rot_upgrade_job_t *job);

bool upgrade_task_is_running(void);
uint32_t upgrade_task_get_init_error(void);


#endif
