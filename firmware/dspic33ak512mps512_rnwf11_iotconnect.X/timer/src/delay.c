#include "../delay.h"
#include "../../bsp/systick.h"

void DELAY_milliseconds(uint16_t milliseconds)
{
    uint32_t start = SYSTICK_GetMillis();
    while ((uint32_t)(SYSTICK_GetMillis() - start) < (uint32_t)milliseconds)
    {
    }
}

void DELAY_microseconds(uint16_t microseconds)
{
    // Calibrated busy loop, not a hardware timer - the only caller (RNWF11
    // driver's transceiver turn-around delay in rnwf_system_service.c) needs
    // a short settle time, not precise timing. ~100MHz Fcy on this project's
    // clock config (see mcc_generated_files/system/src/clock.c) gives
    // roughly 10 instruction cycles per microsecond; this loop body is a
    // handful of cycles per iteration. If you need verified microsecond
    // accuracy, replace this with a free-running hardware timer readout and
    // check it with an oscilloscope/logic analyzer.
    volatile uint32_t loops = (uint32_t)microseconds * 20U;
    while (loops--)
    {
    }
}
