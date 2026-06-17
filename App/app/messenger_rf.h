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
void MSG_RF_OnRadioSetupRegisters(void);
bool MSG_RF_RxChannelLockActive(void);

uint16_t MSG_RF_GetAckDbgPendingId(void);
uint16_t MSG_RF_GetAckDbgSentId(void);
uint16_t MSG_RF_GetAckDbgRxId(void);
uint8_t MSG_RF_GetAckDbgSentCount(void);
uint8_t MSG_RF_GetAckDbgRxCount(void);
uint8_t MSG_RF_GetAckDbgMatchCount(void);
uint8_t MSG_RF_GetAckDbgMissCount(void);
uint8_t MSG_RF_GetAckDbgWaitActive(void);
uint8_t MSG_RF_GetAckDbgRetryCount(void);

uint8_t MSG_RF_GetTxCount(void);
uint8_t MSG_RF_GetSyncCount(void);
uint8_t MSG_RF_GetFifoCount(void);
uint8_t MSG_RF_GetDecodeCount(void);
uint8_t MSG_RF_GetRestoreCount(void);
uint8_t MSG_RF_GetSidecarCount(void);
uint8_t MSG_RF_GetOpenTicks(void);
uint8_t MSG_RF_GetLastDecodeOpen(void);
uint16_t MSG_RF_GetDbg02(void);
uint16_t MSG_RF_GetDbg0B(void);
uint16_t MSG_RF_GetDbg0C(void);
uint16_t MSG_RF_GetDbg30(void);
uint16_t MSG_RF_GetDbg3F(void);
uint16_t MSG_RF_GetDbg47(void);
uint16_t MSG_RF_GetDbg58(void);
uint16_t MSG_RF_GetDbg59(void);
uint16_t MSG_RF_GetDbg67(void);

#endif
