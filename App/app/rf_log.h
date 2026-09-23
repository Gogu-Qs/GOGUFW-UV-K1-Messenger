#ifndef APP_RF_LOG_H
#define APP_RF_LOG_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/keyboard.h"
#include "functions.h"
#include "radio.h"

void GOGU_RFLOG_BeginRx(const VFO_Info_t *vfo, FUNCTION_Type_t function);
void GOGU_RFLOG_BeginTx(const VFO_Info_t *vfo);
void GOGU_RFLOG_EndActive(void);
void GOGU_RFLOG_Tick500ms(void);
void GOGU_RFLOG_Open(void);
void GOGU_RFLOG_ProcessKeys(KEY_Code_t key, bool isPressed, bool isHeld);
void UI_DisplayGoguRfLog(void);

#endif
