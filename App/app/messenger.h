#ifndef APP_MESSENGER_H
#define APP_MESSENGER_H
#include <stdbool.h>
#include "driver/keyboard.h"

void MSG_Init(void);
void MSG_Open(void);
void MSG_RangeOpen(void);
bool MSG_IsHomeOpen(void);
void MSG_Tick(void);
void MSG_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
bool MSG_HasUnread(void);
void MSG_RangeOnPong(const char *callsign, int8_t rssi_dbm, uint16_t battery_cv);
void MSG_HeardUpdate(const char *callsign, int8_t rssi_dbm, uint8_t packet_type);
bool MSG_RangeIsOpen(void);

#endif
