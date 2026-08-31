#ifndef IOTCONNECT_RNWF11_H
#define IOTCONNECT_RNWF11_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint16_t motorRunning;
	uint16_t state;
	uint16_t sector;
	uint16_t requestedSpeedRpm;
	uint16_t measuredSpeedRpm;
	int16_t requestedCurrent;
	int16_t measuredCurrent;
	int16_t dutyCycle;
	int16_t dcBusAdc;
} IOTC_RNWF11_Telemetry_t;

void IOTC_RNWF11_Initialize(void);
void IOTC_RNWF11_Tick1ms(void);
void IOTC_RNWF11_SetTelemetry(const IOTC_RNWF11_Telemetry_t *telemetry);
void IOTC_RNWF11_Task(void);
bool IOTC_RNWF11_IsConnected(void);

#endif
