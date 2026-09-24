#ifndef APP_CALLTX_STORE_H
#define APP_CALLTX_STORE_H

#include <stdint.h>

extern uint8_t gCallTxTone;
extern uint8_t gCallTxVol;

void CALLTX_STORE_Init(void);
void CALLTX_STORE_Save(void);

#endif
