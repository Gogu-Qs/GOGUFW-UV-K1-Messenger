#ifndef APP_MESSENGER_RF_H
#define APP_MESSENGER_RF_H

#include <stdbool.h>
#include <stdint.h>

void MSG_RF_Open(void);
void MSG_RF_Close(void);
void MSG_RF_Tick10ms(void);
void MSG_RF_OnRadioInterrupt(uint16_t status);
bool MSG_RF_SendText(const char *text);
bool MSG_RF_SendRangePing(void);
void MSG_RF_HardRestoreVoicePath(void);
void MSG_RF_PrepareVoxVoiceTx(void);
void MSG_RF_OnVoxModeChanged(bool enabled);
void MSG_RF_OnRadioSetupRegisters(void);
bool MSG_RF_RxChannelLockActive(void);

#endif
