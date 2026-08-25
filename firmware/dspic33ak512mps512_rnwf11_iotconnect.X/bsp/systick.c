#include "systick.h"
#include "../mcc_generated_files/timer/tmr1.h"

static volatile uint32_t g_millis = 0;

static void SYSTICK_Tick(void)
{
    g_millis++;
}

void SYSTICK_Initialize(void)
{
    TMR1_Initialize();
    TMR1_TimeoutCallbackRegister(&SYSTICK_Tick);
    TMR1_Start();
}

uint32_t SYSTICK_GetMillis(void)
{
    uint32_t ms;
    // TMR1's ISR can preempt this read on the low byte(s) of the 32-bit
    // counter mid-update; disable T1IE for the single-instruction read.
    IEC1bits.T1IE = 0;
    ms = g_millis;
    IEC1bits.T1IE = 1;
    return ms;
}
