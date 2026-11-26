#ifndef KEYMONITOR_H
#define KEYMONITOR_H

#include <stdint.h>

void keymonitor_lmt(uint8_t ki, uint16_t data, uint16_t data_pre);
void keymonitor_rst(void);

#endif  /* KEYMONITOR_H */