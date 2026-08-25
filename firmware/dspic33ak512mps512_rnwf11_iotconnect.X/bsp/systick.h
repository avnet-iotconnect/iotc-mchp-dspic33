/**
 * @file systick.h
 * @brief 1 millisecond system tick, driven by TMR1 (mcc_generated_files/timer/tmr1.c).
 *
 * TMR1 is already configured by Melody for exactly a 1 ms period on this
 * project's 100 MHz peripheral clock (see tmr1.c: PR1 = 99999). This module
 * just registers a callback that counts milliseconds, giving the rest of
 * the firmware (delay.c, the main loop's telemetry-publish interval) a
 * single time base without pulling in the OOB demo's full cooperative task
 * scheduler (bsp/task.c), which this trimmed-down project does not need.
 */
#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void SYSTICK_Initialize(void);
uint32_t SYSTICK_GetMillis(void);

#endif // SYSTICK_H
