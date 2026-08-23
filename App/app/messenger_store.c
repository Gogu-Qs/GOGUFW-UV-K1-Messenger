#include <stddef.h>
#include <string.h>
#include "app/messenger_store.h"
#include "app/messenger_packet.h"
#include "driver/py25q16.h"
#include "audio.h"
#include "misc.h"

#define MSG_CFG_MAGIC 0x47u
#define MSG_CFG_VERSION 7u
#define MSG_CFG_FLASH_ADDR 0x012000u
#define MSG_CFG_FLASH_SIZE 0x1000u

MSG_Config_t gMessengerConfig;
MSG_InboxMessage_t gMessengerInbox[MSG_INBOX_CAPACITY];
MSG_OutboxMessage_t gMessengerOutbox[MSG_OUTBOX_CAPACITY];

/* Persistent layout shared with older firmware and the GOGUFW CHIRP module.
 * Keep this private: only active settings stay resident in RAM. */
typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t version;
    uint8_t msg_rx;
    uint8_t callsign_tx;
    uint8_t msg_ack;
    uint8_t msg_hop;
    uint8_t msg_beep;
    uint8_t msg_led;
    uint8_t msg_debug;
    uint8_t alignment_pad;
    uint16_t next_msg_id;
    char callsign[MSG_CALLSIGN_LEN + 1];
    char drafts[MSG_DRAFT_CAPACITY][MSG_TEXT_LEN + 1];
    uint8_t call_tone;
    uint8_t call_vol;
    uint8_t rng_rsp;
} MSG_ConfigFlash_t;

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t version;
    uint8_t msg_rx;
    uint8_t callsign_tx;
    uint8_t msg_ack;
    uint8_t msg_hop;
    uint8_t msg_beep;
    uint8_t msg_led;
    uint8_t msg_debug;
    uint8_t alignment_pad;
    uint16_t next_msg_id;
    char callsign[MSG_CALLSIGN_LEN + 1];
} MSG_ConfigPrefix_t;

_Static_assert(sizeof(MSG_ConfigFlash_t) == 320u, "Messenger flash layout changed");
_Static_assert(sizeof(MSG_ConfigPrefix_t) == 21u, "Messenger config prefix changed");
_Static_assert(offsetof(MSG_ConfigFlash_t, next_msg_id) == 10u, "next_msg_id offset changed");
_Static_assert(offsetof(MSG_ConfigFlash_t, callsign) == 12u, "callsign offset changed");
_Static_assert(offsetof(MSG_ConfigFlash_t, drafts) == 21u, "draft offset changed");
_Static_assert(offsetof(MSG_ConfigFlash_t, call_tone) == 317u, "call_tone offset changed");
_Static_assert(offsetof(MSG_ConfigFlash_t, call_vol) == 318u, "call_vol offset changed");
_Static_assert(offsetof(MSG_ConfigFlash_t, rng_rsp) == 319u, "rng_rsp offset changed");

static void MSG_STORE_DefaultConfig(void)
{
    memset(&gMessengerConfig, 0, sizeof(gMessengerConfig));
    gMessengerConfig.msg_rx = 1;
    gMessengerConfig.msg_ack = 0;
    gMessengerConfig.msg_beep = 1;
    gMessengerConfig.msg_led = 1;
    gMessengerConfig.call_tone = 0;
    gMessengerConfig.call_vol = 1;
    gMessengerConfig.rng_rsp = 1;
    gMessengerConfig.next_msg_id = 1;
    strncpy(gMessengerConfig.callsign, "UVK1", MSG_CALLSIGN_EDIT_LEN);
}

static void flash_read_struct(uint32_t addr, void *dst, uint16_t size)
{
    PY25Q16_ReadBuffer(addr, dst, size);
}

static void flash_write_struct(uint32_t addr, const void *src, uint16_t size)
{
    // Store Messenger data in a dedicated GOGUFW flash sector.
    // Do not use EEPROM-compatible addresses here: the old 0x1E80 location
    // overlaps the MR/channel memory compatibility area.
    if (size > MSG_CFG_FLASH_SIZE) return;
    PY25Q16_WriteBuffer(addr, src, size, false);
}

static void MSG_STORE_SanitizeCallsign(void)
{
    gMessengerConfig.callsign[MSG_CALLSIGN_EDIT_LEN] = 0;
    for (uint8_t i = 0; i < MSG_CALLSIGN_EDIT_LEN; i++) {
        char c = gMessengerConfig.callsign[i];
        if (c == 0) break;
        if (c >= 'a' && c <= 'z') gMessengerConfig.callsign[i] = (char)(c - ('a' - 'A'));
    }
}

void MSG_STORE_SaveConfig(void)
{
    MSG_ConfigPrefix_t prefix;
    const uint8_t tail[3] = {
        gMessengerConfig.call_tone,
        gMessengerConfig.call_vol,
        gMessengerConfig.rng_rsp,
    };

    MSG_STORE_SanitizeCallsign();
    memset(&prefix, 0, sizeof(prefix));
    prefix.magic = MSG_CFG_MAGIC;
    prefix.version = MSG_CFG_VERSION;
    prefix.msg_rx = gMessengerConfig.msg_rx;
    prefix.callsign_tx = 1u;
    prefix.msg_ack = gMessengerConfig.msg_ack;
    prefix.msg_hop = 0u;
    prefix.msg_beep = gMessengerConfig.msg_beep;
    prefix.msg_led = gMessengerConfig.msg_led;
    prefix.msg_debug = 0u;
    prefix.next_msg_id = gMessengerConfig.next_msg_id;
    memcpy(prefix.callsign, gMessengerConfig.callsign, sizeof(prefix.callsign));

    flash_write_struct(MSG_CFG_FLASH_ADDR, &prefix, sizeof(prefix));
    flash_write_struct(MSG_CFG_FLASH_ADDR + offsetof(MSG_ConfigFlash_t, call_tone), tail, sizeof(tail));
}

static void MSG_STORE_LoadRuntime(const MSG_ConfigFlash_t *stored)
{
    gMessengerConfig.msg_rx = stored->msg_rx;
    gMessengerConfig.msg_ack = stored->msg_ack;
    gMessengerConfig.msg_beep = stored->msg_beep;
    gMessengerConfig.msg_led = stored->msg_led;
    gMessengerConfig.next_msg_id = stored->next_msg_id ? stored->next_msg_id : 1u;
    memcpy(gMessengerConfig.callsign, stored->callsign, sizeof(gMessengerConfig.callsign));
    gMessengerConfig.call_tone = stored->call_tone;
    gMessengerConfig.call_vol = stored->call_vol;
    gMessengerConfig.rng_rsp = stored->rng_rsp;
}

static void MSG_STORE_WriteDefaultDrafts(void)
{
    static const char *const defaults[MSG_DRAFT_CAPACITY] = {
        "OK", "NEED HELP", "WHERE ARE YOU?", "ON THE WAY",
        "ARRIVED SAFE", "CALL ME", "NEGATIVE", "BATTERY LOW",
    };
    char slot[MSG_TEXT_LEN + 1];

    for (uint8_t i = 0u; i < MSG_DRAFT_CAPACITY; i++) {
        memset(slot, 0, sizeof(slot));
        strncpy(slot, defaults[i], MSG_TEXT_LEN);
        flash_write_struct(MSG_CFG_FLASH_ADDR + offsetof(MSG_ConfigFlash_t, drafts) +
                           (uint32_t)i * sizeof(slot), slot, sizeof(slot));
    }
}

// Temporary bad test3/test4 layout inserted call_tone/call_vol before next_msg_id.
typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t version;
    uint8_t msg_rx;
    uint8_t callsign_tx;
    uint8_t msg_ack;
    uint8_t msg_hop;
    uint8_t msg_beep;
    uint8_t msg_led;
    uint8_t msg_debug;
    uint8_t call_tone;
    uint8_t call_vol;
    uint16_t next_msg_id;
    char callsign[MSG_CALLSIGN_LEN + 1];
    char drafts[MSG_DRAFT_CAPACITY][MSG_TEXT_LEN + 1];
} MSG_Config_BadV5_t;

static bool MSG_STORE_LooksLikeBadV5(const MSG_ConfigFlash_t *cfg)
{
    return cfg->version == 5u &&
           ((uint8_t)cfg->callsign[0] < 0x20u || (uint8_t)cfg->callsign[1] < 0x20u);
}

void MSG_STORE_Init(void)
{
    MSG_ConfigFlash_t stored;
    flash_read_struct(MSG_CFG_FLASH_ADDR, &stored, sizeof(stored));
    if (stored.magic != MSG_CFG_MAGIC) {
        MSG_STORE_DefaultConfig();
        MSG_STORE_SaveConfig();
        MSG_STORE_WriteDefaultDrafts();
    } else if (stored.version == 4u) {
        // Clean v0.3.12 migration: common fields keep their original offsets;
        // CllTon/CllVol are appended at the end only.
        MSG_STORE_DefaultConfig();
        MSG_STORE_LoadRuntime(&stored);
        gMessengerConfig.call_tone = 0;
        gMessengerConfig.call_vol = 1;
        gMessengerConfig.rng_rsp = 1;
        MSG_STORE_SaveConfig();
    } else if (MSG_STORE_LooksLikeBadV5(&stored)) {
        // Recover from the bad intermediate layout as much as possible and then
        // rewrite the sector with the fixed v6 layout. Bytes already overwritten
        // by the bad test build cannot always be reconstructed, but this stops
        // further offset damage.
        const MSG_Config_BadV5_t *bad = (const MSG_Config_BadV5_t *)(const void *)&stored;
        MSG_STORE_DefaultConfig();
        gMessengerConfig.msg_rx = bad->msg_rx;
        gMessengerConfig.msg_ack = bad->msg_ack;
        gMessengerConfig.msg_beep = bad->msg_beep;
        gMessengerConfig.msg_led = bad->msg_led;
        gMessengerConfig.next_msg_id = bad->next_msg_id ? bad->next_msg_id : 1u;
        memcpy(gMessengerConfig.callsign, bad->callsign, sizeof(gMessengerConfig.callsign));
        gMessengerConfig.call_tone = (bad->call_tone <= 4u) ? bad->call_tone : 0u;
        gMessengerConfig.call_vol = (bad->call_vol == 0u) ? 0u : 1u;
        gMessengerConfig.rng_rsp = 1;
        memmove(stored.drafts, bad->drafts, sizeof(stored.drafts));
        flash_write_struct(MSG_CFG_FLASH_ADDR + offsetof(MSG_ConfigFlash_t, drafts),
                           stored.drafts, sizeof(stored.drafts));
        MSG_STORE_SanitizeCallsign();
        MSG_STORE_SaveConfig();
    } else if (stored.version == 6u) {
        /* v7 appends RngRsp at the end only; preserve all v6 offsets. */
        MSG_STORE_LoadRuntime(&stored);
        if (gMessengerConfig.call_tone > 4u) gMessengerConfig.call_tone = 0;
        if (gMessengerConfig.call_vol > 1u) gMessengerConfig.call_vol = 1;
        gMessengerConfig.rng_rsp = 1;
        MSG_STORE_SanitizeCallsign();
        MSG_STORE_SaveConfig();
    } else if (stored.version != MSG_CFG_VERSION) {
        MSG_STORE_DefaultConfig();
        MSG_STORE_SaveConfig();
        MSG_STORE_WriteDefaultDrafts();
    } else {
        MSG_STORE_LoadRuntime(&stored);
        if (gMessengerConfig.call_tone > 4u) gMessengerConfig.call_tone = 0;
        if (gMessengerConfig.call_vol > 1u) gMessengerConfig.call_vol = 1;
        if (gMessengerConfig.rng_rsp > 1u) gMessengerConfig.rng_rsp = 1;
        MSG_STORE_SanitizeCallsign();
    }
}

uint16_t MSG_STORE_NextMsgId(void)
{
    uint16_t id = gMessengerConfig.next_msg_id++;
    if (gMessengerConfig.next_msg_id == 0) gMessengerConfig.next_msg_id = 1;
    MSG_STORE_SaveConfig();
    return id;
}

bool MSG_STORE_IsDuplicateInbox(const char *from, uint16_t id)
{
    if (id == 0u) return false;
    const char *safe_from = (from && from[0]) ? from : "UVK1";
    for (uint8_t i = 0; i < MSG_INBOX_CAPACITY; i++) {
        if (!gMessengerInbox[i].used) continue;
        if (gMessengerInbox[i].id == id &&
            strncmp(gMessengerInbox[i].from, safe_from, MSG_CALLSIGN_LEN) == 0) {
            return true;
        }
    }
    return false;
}

void MSG_STORE_AddInboxMessage(const char *text, const char *from, const char *to, uint16_t id, uint8_t ttl_init, uint8_t ttl_remain, bool unread)
{
    /* Duplicate retry frames must not create a second inbox entry or trigger
     * unread/beep state. ACK resend is handled by the RF layer before this call. */
    if (MSG_STORE_IsDuplicateInbox(from, id)) return;

    for (int i = MSG_INBOX_CAPACITY - 1; i > 0; --i) gMessengerInbox[i] = gMessengerInbox[i - 1];
    memset(&gMessengerInbox[0], 0, sizeof(gMessengerInbox[0]));
    gMessengerInbox[0].used = true;
    gMessengerInbox[0].unread = unread;
    gMessengerInbox[0].id = id;
    strncpy(gMessengerInbox[0].from, from && from[0] ? from : "UVK1", MSG_CALLSIGN_LEN);
    strncpy(gMessengerInbox[0].text, text ? text : "TEST", MSG_TEXT_LEN);
    (void)to;
    (void)ttl_init;
    (void)ttl_remain;
    gUpdateStatus = true;
    gUpdateDisplay = true;
    if (unread && gMessengerConfig.msg_beep) {
        gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_FORCE;
    }
}

void MSG_STORE_AddOutboxMessage(const char *text, const char *from, const char *to, uint16_t id, uint8_t ttl_init, uint8_t ttl_remain)
{
    for (int i = MSG_OUTBOX_CAPACITY - 1; i > 0; --i) gMessengerOutbox[i] = gMessengerOutbox[i - 1];
    memset(&gMessengerOutbox[0], 0, sizeof(gMessengerOutbox[0]));
    gMessengerOutbox[0].used = true;
    gMessengerOutbox[0].id = id;
    strncpy(gMessengerOutbox[0].to, to && to[0] ? to : "ALL", MSG_CALLSIGN_LEN);
    strncpy(gMessengerOutbox[0].text, text ? text : "TEST", MSG_TEXT_LEN);
    gMessengerOutbox[0].status = MSG_STATUS_PENDING;
    (void)from;
    (void)ttl_init;
    (void)ttl_remain;
}

void MSG_STORE_SetOutboxStatusById(uint16_t id, uint8_t status)
{
    for (uint8_t i = 0; i < MSG_OUTBOX_CAPACITY; i++) {
        if (gMessengerOutbox[i].used && gMessengerOutbox[i].id == id) {
            gMessengerOutbox[i].status = status;
            gUpdateDisplay = true;
            return;
        }
    }
}

void MSG_STORE_AddOutboxAckSourceById(uint16_t id, const char *from)
{
    if (!from || !from[0]) return;
    for (uint8_t i = 0; i < MSG_OUTBOX_CAPACITY; i++) {
        MSG_OutboxMessage_t *m = &gMessengerOutbox[i];
        if (!m->used || m->id != id) continue;

        for (uint8_t j = 0; j < m->ack_count && j < MSG_ACK_SOURCE_MAX; j++) {
            if (strncmp(m->ack_from[j], from, MSG_ACK_ID_LEN) == 0) {
                return;
            }
        }

        if (m->ack_count < MSG_ACK_SOURCE_MAX) {
            memset(m->ack_from[m->ack_count], 0, sizeof(m->ack_from[m->ack_count]));
            strncpy(m->ack_from[m->ack_count], from, MSG_ACK_ID_LEN);
            m->ack_count++;
            gUpdateDisplay = true;
        }
        return;
    }
}

void MSG_STORE_AddInboxDemo(const char *text)
{
    MSG_STORE_AddInboxMessage(text, "DEMO", "ALL", MSG_STORE_NextMsgId(), 1u, 1u, true);
}

void MSG_STORE_AddOutboxDemo(const char *text)
{
    MSG_STORE_AddOutboxMessage(text, gMessengerConfig.callsign, "ALL", MSG_STORE_NextMsgId(), 1u, 1u);
}

bool MSG_STORE_InjectNativePacket(const char *text)
{
    uint8_t frame[MSG_PKT_WIRE_LEN];
    MSG_Packet_t pkt;
    uint16_t id = MSG_STORE_NextMsgId();
    if (!MSG_PACKET_BuildText(frame, sizeof(frame), id, "NODE2", text ? text : "NATIVE TEST", 1u)) return false;
    if (!MSG_PACKET_Parse(frame, sizeof(frame), &pkt)) return false;
    if (pkt.type != MSG_PKT_TYPE_TEXT) return false;
    MSG_STORE_AddInboxMessage(pkt.payload, pkt.from, pkt.to, pkt.id, pkt.ttl_init, pkt.ttl_remain, true);
    return true;
}

void MSG_STORE_DeleteInbox(uint8_t index)
{
    if (index >= MSG_INBOX_CAPACITY || !gMessengerInbox[index].used) return;
    for (uint8_t i = index; i + 1u < MSG_INBOX_CAPACITY; ++i) gMessengerInbox[i] = gMessengerInbox[i + 1u];
    memset(&gMessengerInbox[MSG_INBOX_CAPACITY - 1u], 0, sizeof(gMessengerInbox[0]));
}

void MSG_STORE_DeleteOutbox(uint8_t index)
{
    if (index >= MSG_OUTBOX_CAPACITY || !gMessengerOutbox[index].used) return;
    for (uint8_t i = index; i + 1u < MSG_OUTBOX_CAPACITY; ++i) gMessengerOutbox[i] = gMessengerOutbox[i + 1u];
    memset(&gMessengerOutbox[MSG_OUTBOX_CAPACITY - 1u], 0, sizeof(gMessengerOutbox[0]));
}
void MSG_STORE_MarkInboxRead(uint8_t index)
{
    if (index < MSG_INBOX_CAPACITY) {
        gMessengerInbox[index].unread = false;
        gUpdateStatus = true;
        gUpdateDisplay = true;
    }
}
uint8_t MSG_STORE_CountInbox(void)
{
    uint8_t count = 0u;
    for (uint8_t i = 0; i < MSG_INBOX_CAPACITY; ++i) if (gMessengerInbox[i].used) ++count;
    return count;
}

uint8_t MSG_STORE_CountOutbox(void)
{
    uint8_t count = 0u;
    for (uint8_t i = 0; i < MSG_OUTBOX_CAPACITY; ++i) if (gMessengerOutbox[i].used) ++count;
    return count;
}
uint8_t MSG_STORE_CountDrafts(void) { return MSG_DRAFT_CAPACITY; }

void MSG_STORE_GetDraft(uint8_t index, char *out)
{
    if (!out) return;
    memset(out, 0, MSG_TEXT_LEN + 1u);
    if (index >= MSG_DRAFT_CAPACITY) return;

    flash_read_struct(MSG_CFG_FLASH_ADDR + offsetof(MSG_ConfigFlash_t, drafts) +
                      (uint32_t)index * (MSG_TEXT_LEN + 1u), out, MSG_TEXT_LEN + 1u);
    if ((uint8_t)out[0] == 0xFFu) out[0] = 0;
    out[MSG_TEXT_LEN] = 0;
}

void MSG_STORE_SetDraft(uint8_t index, const char *text)
{
    char slot[MSG_TEXT_LEN + 1];
    if (index >= MSG_DRAFT_CAPACITY) return;
    memset(slot, 0, sizeof(slot));
    if (text && text[0]) {
        strncpy(slot, text, MSG_TEXT_LEN);
        slot[MSG_TEXT_LEN] = 0;
    }
    flash_write_struct(MSG_CFG_FLASH_ADDR + offsetof(MSG_ConfigFlash_t, drafts) +
                       (uint32_t)index * sizeof(slot), slot, sizeof(slot));
    gUpdateDisplay = true;
}


bool MSG_STORE_HasUnread(void)
{
    for (uint8_t i = 0; i < MSG_INBOX_CAPACITY; i++) if (gMessengerInbox[i].used && gMessengerInbox[i].unread) return true;
    return false;
}
