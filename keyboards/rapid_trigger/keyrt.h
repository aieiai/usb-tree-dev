/**
 * @Author: wj
 * @Brief:  key rapid trigger.
 * @Date:   2024/07/02
 */
#ifndef KEY_RAPID_TRIGGER_H
#define KEY_RAPID_TRIGGER_H

#include <stdint.h>
#include <stdbool.h>

bool keyrt_enabled(uint8_t ki);
void keyrt_init(void);
void keyrt_reset(uint8_t ki);
void keyrt_process(uint8_t ki);

#endif  /* KEY_RAPID_TRIGGER_H */
