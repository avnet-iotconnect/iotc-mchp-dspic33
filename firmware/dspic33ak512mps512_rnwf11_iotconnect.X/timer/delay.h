/**
 * DELAY Driver API Header File
 *
 * @file delay.h
 *
 * @brief Blocking millisecond/microsecond delay used by the ported RNWF11
 *        driver (rnwf11/rnwf_system_service.c, rnwf11/rnwf_mqtt_service.c).
 *
 * This is a hand-written replacement for the equivalent file in Microchip's
 * AVR128DB48/SAME54 RNWF11 reference firmware, matching its API exactly so
 * the ported rnwf11/*.c files did not need to change their #include line.
 * DELAY_milliseconds() is backed by the 1 ms SYSTICK_ tick (see
 * bsp/systick.c). DELAY_microseconds() is a calibrated busy loop - it is
 * only used for one non-critical ~100us settle delay in the RNWF11 driver,
 * so its precision has not been verified on hardware with an oscilloscope.
 */
#ifndef _DELAY_H
#define _DELAY_H

#include <stdint.h>

void DELAY_milliseconds(uint16_t milliseconds);
void DELAY_microseconds(uint16_t microseconds);

#endif // _DELAY_H
