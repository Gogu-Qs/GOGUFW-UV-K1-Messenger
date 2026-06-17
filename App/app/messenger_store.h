#ifndef APP_MESSENGER_STORE_H
#define APP_MESSENGER_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define MSG_CALLSIGN_LEN       8
#define MSG_CALLSIGN_EDIT_LEN  6
#define MSG_TEXT_LEN           36
#define MSG_INBOX_CAPACITY     16
#define MSG_OUTBOX_CAPACITY    8
#define MSG_DRAFT_CAPACITY     8
#define MSG_ACK_SOURCE_MAX     3
#define MSG_ACK_ID_LEN         MSG_CALLSIGN_EDIT_LEN

#define MSG_STATUS_NONE        0u
#define MSG_STATUS_PENDING     1u
#define MSG_STATUS_ACKED       2u
#define MSG_STATUS_FAILED      3u

typedef struct {
    bool     used;
    bool     unread;
    uint16_t id;
    uint8_t  ttl_init;
    uint8_t  ttl_remain;
    uint8_t  status;
    uint16_t age_seconds;
    char     from[MSG_CALLSIGN_LEN + 1];
    char     to[MSG_CALLSIGN_LEN + 1];
    char     text[MSG_TEXT_LEN + 1];
    uint8_t  ack_count;
    char     ack_from[MSG_ACK_SOURCE_MAX][MSG_ACK_ID_LEN];
} MSG_Message_t;

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint8_t msg_rx;
    uint8_t callsign_tx;
    uint8_t msg_ack;
    uint8_t msg_hop;
    uint8_t msg_beep;
    uint8_t msg_led;
    uint8_t msg_debug;
    uint16_t next_msg_id;
    char    callsign[MSG_CALLSIGN_LEN + 1];
    char    drafts[MSG_DRAFT_CAPACITY][MSG_TEXT_LEN + 1];
    // Added at the END only. Do not insert new fields before next_msg_id/callsign/drafts;
    // older builds store these fields by raw struct offset.
    uint8_t call_tone;   // 0..4, CllTon menu
    uint8_t call_vol;    // 0=LOW, 1=HIGH, CllVol REG40 deviation trim
    uint8_t rng_rsp;     // 0=OFF, 1=ON automatic Range Check PONG response
} MSG_Config_t;

extern MSG_Config_t gMessengerConfig;
extern MSG_Message_t gMessengerInbox[MSG_INBOX_CAPACITY];
extern MSG_Message_t gMessengerOutbox[MSG_OUTBOX_CAPACITY];

void MSG_STORE_Init(void);
void MSG_STORE_SaveConfig(void);
uint16_t MSG_STORE_NextMsgId(void);
bool MSG_STORE_IsDuplicateInbox(const char *from, uint16_t id);
void MSG_STORE_AddInboxMessage(const char *text, const char *from, const char *to, uint16_t id, uint8_t ttl_init, uint8_t ttl_remain, bool unread);
void MSG_STORE_AddOutboxMessage(const char *text, const char *from, const char *to, uint16_t id, uint8_t ttl_init, uint8_t ttl_remain);
void MSG_STORE_SetOutboxStatusById(uint16_t id, uint8_t status);
void MSG_STORE_AddOutboxAckSourceById(uint16_t id, const char *from);
void MSG_STORE_AddInboxDemo(const char *text);
void MSG_STORE_AddOutboxDemo(const char *text);
bool MSG_STORE_InjectNativePacket(const char *text);
void MSG_STORE_DeleteInbox(uint8_t index);
void MSG_STORE_DeleteOutbox(uint8_t index);
void MSG_STORE_MarkInboxRead(uint8_t index);
uint8_t MSG_STORE_CountInbox(void);
uint8_t MSG_STORE_CountOutbox(void);
uint8_t MSG_STORE_CountDrafts(void);
bool MSG_STORE_HasUnread(void);
void MSG_STORE_SetDraft(uint8_t index, const char *text);

#endif
