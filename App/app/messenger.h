#ifndef APP_MESSENGER_H
#define APP_MESSENGER_H
#include <stdbool.h>
#include "driver/keyboard.h"
#include "app/messenger_store.h"

/* Runtime HEARD/Range records only; never serialized as a packet or EEPROM data. */
typedef struct {
    bool used;
    char callsign[MSG_CALLSIGN_EDIT_LEN + 1];
    int8_t rssi;
    uint8_t packet_type;
    uint16_t battery_cv;
    uint16_t age_seconds;
    uint16_t range_session;
} MSG_RangeFound_t;

#define MSG_RANGE_MAX_FOUND 6u
_Static_assert(sizeof(MSG_RangeFound_t) == 16u, "unexpected HEARD record layout");
extern MSG_RangeFound_t gMsgRangeFound[MSG_RANGE_MAX_FOUND];

void MSG_Init(void);
void MSG_Open(void);
void MSG_RangeOpen(void);
bool MSG_IsHomeOpen(void);
#ifdef ENABLE_FEAT_F4HWN_ACTION_PICKER
bool MSG_ActionPickerAllowed(void);
#endif
void MSG_Tick(void);
void MSG_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
bool MSG_HasUnread(void);
void MSG_RangeOnPong(const char *callsign, int8_t rssi_dbm, uint16_t battery_cv);
void MSG_HeardUpdate(const char *callsign, int8_t rssi_dbm, uint8_t packet_type);
bool MSG_RangeIsOpen(void);

#endif
