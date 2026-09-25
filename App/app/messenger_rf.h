#ifndef APP_MESSENGER_RF_H
#define APP_MESSENGER_RF_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MSG_RF_BLOCK_NONE = 0,
    MSG_RF_BLOCK_NO_FSK,
    MSG_RF_BLOCK_TX_FREQUENCY,
    MSG_RF_BLOCK_MODULATION,
    MSG_RF_BLOCK_CONFIG,
    MSG_RF_BLOCK_BATTERY,
    MSG_RF_BLOCK_UNKNOWN,
} MSG_RF_BlockReason_t;

void MSG_RF_Open(void);
void MSG_RF_Close(void);
void MSG_RF_Tick10ms(void);
void MSG_RF_OnRadioInterrupt(uint16_t status);
bool MSG_RF_SendText(const char *text);
bool MSG_RF_SendRangePing(void);
bool MSG_RF_LastSendWasBlocked(void);
MSG_RF_BlockReason_t MSG_RF_LastSendBlockReason(void);
void MSG_RF_HardRestoreVoicePath(void);
void MSG_RF_PrepareVoxVoiceTx(void);
void MSG_RF_OnVoxModeChanged(bool enabled);
void MSG_RF_OnRadioSetupRegisters(void);
bool MSG_RF_RxChannelLockActive(void);
bool MSG_RF_TransactionActive(void);

#endif
