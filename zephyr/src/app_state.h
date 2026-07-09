/*
 * Shared controller state between the encoder task and the main control loop.
 * Values are in "inHg * 10" (e.g. 200 == 20.0 inHg), same convention as the
 * original Arduino firmware.
 */
#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>

/* Pump-ON (red) and pump-OFF (green) setpoints. Green is always kept >= red+5
 * by the encoder task. */
extern volatile int16_t app_red;
extern volatile int16_t app_grn;

/* Start the rotary-encoder polling thread. */
void encoder_start(void);

#endif /* APP_STATE_H */
