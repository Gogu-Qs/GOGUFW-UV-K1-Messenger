#include <string.h>
#include "app/messenger.h"
#include "app/messenger_store.h"
#include "app/text_input.h"
#include "app/messenger_rf.h"
#include "app/messenger_packet.h"
#include "app/messenger_ui.h"
#include "app/generic.h"
#include "app/chFrScanner.h"
#include "app/scanner.h"
#ifdef ENABLE_FMRADIO_EMBEDDED
#include "app/fm.h"
#endif
#include "audio.h"
#include "functions.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "misc.h"

typedef enum {
    MSG_SCREEN_HOME = 0,
    MSG_SCREEN_INBOX,
    MSG_SCREEN_OUTBOX,
    MSG_SCREEN_DRAFTS,
    MSG_SCREEN_COMPOSE,
    MSG_SCREEN_READ,
    MSG_SCREEN_RANGE,
    MSG_SCREEN_RANGE_MENU,
    MSG_SCREEN_RANGE_AUTO,
} MSG_Screen_t;

MSG_Screen_t gMsgScreen = MSG_SCREEN_HOME;
uint8_t gMsgCursor;
uint8_t gMsgScroll;
uint8_t gMsgHomeCursor;
char gMsgComposeBuf[MSG_TEXT_LEN + 1];
TEXT_INPUT_Editor_t gMsgEditor;
uint8_t gMsgReadIndex;
uint8_t gMsgReadSource;

#define MSG_RANGE_WAIT_TICKS 1200u

MSG_RangeFound_t gMsgRangeFound[MSG_RANGE_MAX_FOUND];
uint8_t gMsgRangeCount;
uint8_t gMsgRangeScroll;
uint8_t gMsgRangeStatus; /* 0 idle, 1 wait, 2 ok */
static uint16_t s_msgRangeWaitTicks;
static uint8_t s_msgAgeSubTicks;
static uint16_t s_msgAutoCountdownTicks;
static uint8_t s_msgAutoState;
uint8_t gMsgTxLockNoticeTicks;

#define MSG_AUTO_INTERVAL_MASK 0x07u
#define MSG_AUTO_RESULT_VALID 0x08u
#define MSG_AUTO_WAIT         0x10u
#define MSG_AUTO_REPLY        0x20u
#define MSG_AUTO_SUCCESS      0x40u
#define MSG_AUTO_ALERT        0x80u
#define MSG_AUTO_INTERVAL()   (s_msgAutoState & MSG_AUTO_INTERVAL_MASK)
static const uint16_t s_msgAutoIntervals[] = { 3000u, 6000u, 30000u, 60000u };

static bool gMsgComposeIsDraftEdit;
static uint8_t gMsgComposeDraftIndex;

static void show_tx_blocked_notice(bool play_now)
{
    /* Keep the current Messenger screen underneath a short floating warning.
     * Four beeps are intentionally distinct from the two-beep RX alert.
     * Key-triggered sends use the normal end-of-key beep queue.  Auto Check
     * has no key dispatcher to consume that queue, so play it immediately
     * before its warning is rendered and leave no delayed beep behind. */
    gMsgTxLockNoticeTicks = 200u;
    if (play_now) {
        gBeepToPlay = BEEP_NONE;
        AUDIO_PlayBeep(BEEP_TX_BLOCKED_FORCE);
    } else {
        gBeepToPlay = BEEP_TX_BLOCKED_FORCE;
    }
    gUpdateDisplay = true;
}

static bool handle_send_failure(const char *text)
{
    if (MSG_RF_LastSendWasBlocked()) {
        show_tx_blocked_notice(false);
        return true;
    }
    MSG_STORE_AddOutboxDemo(text);
    return false;
}

static void handle_range_ping_failure(bool automatic)
{
    gMsgRangeStatus = 0u;
    s_msgRangeWaitTicks = 0u;

    if (automatic) {
        /* A blocked/failed automatic PING must not leave Auto Check armed.
         * Clear the interval and every result/wait flag so the status icon
         * disappears and no later retry is scheduled silently. */
        s_msgAutoState = 0u;
        s_msgAutoCountdownTicks = 0u;
        gUpdateStatus = true;
    } else {
        s_msgAutoState &= (uint8_t)~MSG_AUTO_WAIT;
    }

    /* Match manual SEND PING: explain policy failures instead of returning
     * silently to HEARD.  The initial Auto Check attempt is made while that
     * screen is still active, so the same floating reason popup is shown. */
    if (MSG_RF_LastSendWasBlocked()) {
        if (automatic) {
            /* A scheduled check can fail while another screen is open.  Bring
             * HEARD forward so the existing modal warning is actually visible
             * instead of expiring invisibly in the background. */
            gMsgScreen = MSG_SCREEN_RANGE;
            gRequestDisplayScreen = DISPLAY_MESSENGER;
        }
        show_tx_blocked_notice(automatic);
    }
}

void MSG_Init(void)
{
    MSG_STORE_Init();
}

void MSG_Open(void)
{
    MSG_Init();
    gMsgScreen = MSG_SCREEN_HOME;
    gMsgCursor = 0;
    gMsgScroll = 0;
    gMsgHomeCursor = 0;
    gRequestDisplayScreen = DISPLAY_MESSENGER;
}

void MSG_Tick(void)
{
    if (gMsgTxLockNoticeTicks > 0u && --gMsgTxLockNoticeTicks == 0u)
        gUpdateDisplay = true;
    if (gMsgScreen == MSG_SCREEN_COMPOSE && TEXT_INPUT_Tick(&gMsgEditor))
        gUpdateDisplay = true;
    if (gMsgRangeStatus == 1u) {
        if (s_msgRangeWaitTicks > 0u) --s_msgRangeWaitTicks;
        if (s_msgRangeWaitTicks == 0u) {
            gMsgRangeStatus = 2u;
            MSG_RF_CancelRangeWait();
            MSG_RF_HardRestoreVoicePath();
            if (s_msgAutoState & MSG_AUTO_WAIT) {
                if (s_msgAutoState & MSG_AUTO_REPLY)
                    s_msgAutoState |= MSG_AUTO_RESULT_VALID | MSG_AUTO_SUCCESS;
                else {
                    s_msgAutoState = (uint8_t)((s_msgAutoState & ~MSG_AUTO_SUCCESS) |
                                               MSG_AUTO_RESULT_VALID | MSG_AUTO_ALERT);
                    s_msgAgeSubTicks = 0u;
                }
            }
            s_msgAutoState &= (uint8_t)~(MSG_AUTO_WAIT | MSG_AUTO_REPLY);
            gUpdateStatus = true;
            gUpdateDisplay = true;
        }
    }

    /* Unlike key beeps, an automatic failure alert must not wait for a key
     * event.  Keep one pending alert while voice RX/TX owns the audio path,
     * then play it directly from the global 10 ms service as soon as RF is
     * idle.  This works independently of the currently displayed screen. */
    if ((s_msgAutoState & MSG_AUTO_ALERT) &&
        !MSG_RF_TransactionActive() &&
        (gCurrentFunction == FUNCTION_FOREGROUND || gCurrentFunction == FUNCTION_POWER_SAVE)) {
        s_msgAutoState &= (uint8_t)~MSG_AUTO_ALERT;
        AUDIO_PlayBeep(BEEP_AUTO_CHECK_FAIL_FORCE);
    }

    if (MSG_AUTO_INTERVAL()) {
        if (s_msgAutoCountdownTicks > 0u) --s_msgAutoCountdownTicks;
        if (s_msgAutoCountdownTicks == 0u && gMsgRangeStatus != 1u &&
            !MSG_RF_TransactionActive() &&
            (gCurrentFunction == FUNCTION_FOREGROUND || gCurrentFunction == FUNCTION_POWER_SAVE) &&
            !gPttIsPressed && gScanStateDir == SCAN_OFF && !SCANNER_IsScanning()
#ifdef ENABLE_FMRADIO_EMBEDDED
            && gFM_ScanState == FM_SCAN_OFF
#endif
            ) {
            gMsgRangeStatus = 1u;
            s_msgRangeWaitTicks = MSG_RANGE_WAIT_TICKS;
            s_msgAutoState = (uint8_t)((s_msgAutoState | MSG_AUTO_WAIT) &
                                       ~MSG_AUTO_REPLY);
            gUpdateStatus = true;
            s_msgAutoCountdownTicks = s_msgAutoIntervals[MSG_AUTO_INTERVAL() - 1u];
            if (!MSG_RF_SendRangePing()) {
                handle_range_ping_failure(true);
            }
            gUpdateDisplay = true;
        }
    }

    if (++s_msgAgeSubTicks == 100u || s_msgAgeSubTicks >= 200u) {
        if (s_msgAgeSubTicks >= 200u) s_msgAgeSubTicks = 0u;
        if (MSG_AUTO_INTERVAL()) {
            gUpdateStatus = true;
        }
        for (uint8_t i = 0; i < MSG_RANGE_MAX_FOUND; i++) {
            if (gMsgRangeFound[i].used && gMsgRangeFound[i].age_seconds < 0xFFFFu) ++gMsgRangeFound[i].age_seconds;
        }
        for (uint8_t i = 0; i < MSG_INBOX_CAPACITY; i++) {
            if (gMessengerInbox[i].used && gMessengerInbox[i].age_seconds < 0xFFFFu) ++gMessengerInbox[i].age_seconds;
        }
        for (uint8_t i = 0; i < MSG_OUTBOX_CAPACITY; i++) {
            if (gMessengerOutbox[i].used && gMessengerOutbox[i].age_seconds < 0xFFFFu) ++gMessengerOutbox[i].age_seconds;
        }
        if (gMsgScreen == MSG_SCREEN_RANGE || gMsgScreen == MSG_SCREEN_INBOX ||
            gMsgScreen == MSG_SCREEN_OUTBOX || gMsgScreen == MSG_SCREEN_READ) gUpdateDisplay = true;
    }
}

void MSG_RangeOpen(void)
{
    MSG_Init();
    gMsgScreen = MSG_SCREEN_RANGE;
    gMsgRangeScroll = 0;
    if (!(s_msgAutoState & MSG_AUTO_WAIT)) {
        gMsgRangeStatus = 0;
        s_msgRangeWaitTicks = 0;
    }
    gRequestDisplayScreen = DISPLAY_MESSENGER;
}

bool MSG_RangeIsOpen(void)
{
    return gMsgScreen == MSG_SCREEN_RANGE;
}

bool MSG_IsHomeOpen(void)
{
    return gMsgScreen != MSG_SCREEN_RANGE;
}

bool MSG_AutoCheckEnabled(void) { return MSG_AUTO_INTERVAL() != 0u; }
bool MSG_AutoCheckBlink(void) { return s_msgAgeSubTicks < 100u; }
bool MSG_AutoCheckResultValid(void) { return (s_msgAutoState & MSG_AUTO_RESULT_VALID) != 0u; }
bool MSG_AutoCheckSuccess(void) { return (s_msgAutoState & MSG_AUTO_SUCCESS) != 0u; }
bool MSG_RangeReplySilent(void) { return (s_msgAutoState & MSG_AUTO_WAIT) != 0u; }

#ifdef ENABLE_FEAT_F4HWN_ACTION_PICKER
bool MSG_ActionPickerAllowed(void)
{
    /* F remains Delete in Inbox/Sent/Read and part of text editing in Compose.
     * Only screens where short F was unused may arm the action picker. */
    if (gMsgTxLockNoticeTicks > 0u)
        return false;

    return gMsgScreen == MSG_SCREEN_HOME ||
           (gMsgScreen == MSG_SCREEN_RANGE && gMsgRangeStatus != 1u);
}
#endif

void MSG_HeardUpdate(const char *callsign, int8_t rssi_dbm, uint8_t packet_type)
{
    if (!callsign || !callsign[0]) return;
    if (strncmp(callsign, gMessengerConfig.callsign, MSG_CALLSIGN_LEN) == 0) return;

    uint8_t pos = 0xFFu;
    for (uint8_t i = 0; i < gMsgRangeCount; i++) {
        if (strncmp(gMsgRangeFound[i].callsign, callsign, MSG_CALLSIGN_EDIT_LEN) == 0) { pos = i; break; }
    }
    if (pos == 0xFFu) {
        pos = (gMsgRangeCount < MSG_RANGE_MAX_FOUND) ? gMsgRangeCount++ : (MSG_RANGE_MAX_FOUND - 1u);
    }

    MSG_RangeFound_t rec = gMsgRangeFound[pos];
    memset(&rec, 0, sizeof(rec));
    rec.used = true;
    strncpy(rec.callsign, callsign, MSG_CALLSIGN_EDIT_LEN);
    rec.callsign[MSG_CALLSIGN_EDIT_LEN] = 0;
    rec.rssi = rssi_dbm;
    rec.packet_type = packet_type;
    rec.age_seconds = 0u;

    for (uint8_t i = pos; i > 0u; i--) gMsgRangeFound[i] = gMsgRangeFound[i - 1u];
    gMsgRangeFound[0] = rec;
    gMsgRangeScroll = 0u;
    gUpdateDisplay = true;
}

void MSG_RangeOnPong(const char *callsign, int8_t rssi_dbm, uint16_t battery_cv)
{
    if (s_msgAutoState & MSG_AUTO_WAIT) {
        s_msgAutoState |= MSG_AUTO_REPLY | MSG_AUTO_RESULT_VALID | MSG_AUTO_SUCCESS;
        gUpdateStatus = true;
    }
    MSG_HeardUpdate(callsign, rssi_dbm, MSG_PKT_TYPE_PONG);
    if (!callsign || !callsign[0]) return;
    for (uint8_t i = 0; i < gMsgRangeCount; i++) {
        if (strncmp(gMsgRangeFound[i].callsign, callsign, MSG_CALLSIGN_EDIT_LEN) == 0) {
            gMsgRangeFound[i].battery_cv = battery_cv;
            gMsgRangeFound[i].rssi = rssi_dbm;
            break;
        }
    }
    /* Keep collecting until the original Range Check window expires. The
     * record update above is enough for the result to appear immediately. */
    gUpdateDisplay = true;
}

bool MSG_HasUnread(void) { return MSG_STORE_HasUnread(); }

static uint8_t current_count(void)
{
    if (gMsgScreen == MSG_SCREEN_INBOX) return MSG_STORE_CountInbox();
    if (gMsgScreen == MSG_SCREEN_OUTBOX) return MSG_STORE_CountOutbox();
    if (gMsgScreen == MSG_SCREEN_DRAFTS) return MSG_STORE_CountDrafts();
    return 0;
}

static void list_move(int8_t dir)
{
    uint8_t count = current_count();
    if (count == 0) return;
    int16_t next = (int16_t)gMsgCursor + dir;
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;
    gMsgCursor = (uint8_t)next;
    if (gMsgCursor < gMsgScroll) gMsgScroll = gMsgCursor;
    if (gMsgCursor >= gMsgScroll + UI_GOGU_CONTENT_ROWS)
        gMsgScroll = (uint8_t)(gMsgCursor - (UI_GOGU_CONTENT_ROWS - 1u));
}

static void list_normalize_after_delete(void)
{
    const uint8_t count = current_count();
    if (count == 0u) {
        gMsgCursor = 0u;
        gMsgScroll = 0u;
        return;
    }
    if (gMsgCursor >= count)
        gMsgCursor = (uint8_t)(count - 1u);
    if (gMsgCursor < gMsgScroll)
        gMsgScroll = gMsgCursor;
    if (gMsgCursor >= (uint8_t)(gMsgScroll + UI_GOGU_CONTENT_ROWS))
        gMsgScroll = (uint8_t)(gMsgCursor - (UI_GOGU_CONTENT_ROWS - 1u));
}

static void go_home(void)
{
    gMsgScreen = MSG_SCREEN_HOME;
    gMsgCursor = 0;
    gMsgScroll = 0;
}

static void open_list(MSG_Screen_t screen)
{
    gMsgScreen = screen;
    gMsgCursor = 0;
    gMsgScroll = 0;
}

static void open_sent_after_send(void)
{
    gMsgScreen = MSG_SCREEN_OUTBOX;
    gMsgCursor = 0;
    gMsgScroll = 0;
}

static void return_to_read_source_list(void)
{
    const MSG_Screen_t src = (gMsgReadSource == MSG_SCREEN_OUTBOX) ? MSG_SCREEN_OUTBOX : MSG_SCREEN_INBOX;
    const uint8_t count = (src == MSG_SCREEN_OUTBOX) ? MSG_STORE_CountOutbox() : MSG_STORE_CountInbox();
    gMsgScreen = src;
    gMsgCursor = gMsgReadIndex;
    if (count == 0u) {
        gMsgCursor = 0u;
        gMsgScroll = 0u;
        return;
    }
    if (gMsgCursor >= count) gMsgCursor = (uint8_t)(count - 1u);
    if (gMsgCursor < gMsgScroll) gMsgScroll = gMsgCursor;
    if (gMsgCursor >= (uint8_t)(gMsgScroll + UI_GOGU_CONTENT_ROWS))
        gMsgScroll = (uint8_t)(gMsgCursor - (UI_GOGU_CONTENT_ROWS - 1u));
}

static void open_compose(const char *seed)
{
    gMsgComposeIsDraftEdit = false;
    gMsgComposeDraftIndex = 0;
    memset(gMsgComposeBuf, 0, sizeof(gMsgComposeBuf));
    if (seed) strncpy(gMsgComposeBuf, seed, MSG_TEXT_LEN);
    gMsgComposeBuf[MSG_TEXT_LEN] = 0;
    TEXT_INPUT_Start(&gMsgEditor, gMsgComposeBuf, MSG_TEXT_LEN);
    gMsgScreen = MSG_SCREEN_COMPOSE;
}

static void open_draft_edit(uint8_t index)
{
    if (index >= MSG_DRAFT_CAPACITY) index = 0;
    gMsgComposeIsDraftEdit = true;
    gMsgComposeDraftIndex = index;
    MSG_STORE_GetDraft(index, gMsgComposeBuf);
    TEXT_INPUT_Start(&gMsgEditor, gMsgComposeBuf, MSG_TEXT_LEN);
    gMsgScreen = MSG_SCREEN_COMPOSE;
}

static uint8_t read_count(void)
{
    return (gMsgReadSource == MSG_SCREEN_OUTBOX) ? MSG_STORE_CountOutbox() : MSG_STORE_CountInbox();
}

static const char *read_message_text(void)
{
    if (gMsgReadSource == MSG_SCREEN_OUTBOX) {
        if (gMsgReadIndex >= MSG_STORE_CountOutbox()) return 0;
        return gMessengerOutbox[gMsgReadIndex].text;
    }
    if (gMsgReadIndex >= MSG_STORE_CountInbox()) return 0;
    return gMessengerInbox[gMsgReadIndex].text;
}

static void read_move(int8_t dir)
{
    uint8_t count = read_count();
    if (count == 0) return;
    int16_t next = (int16_t)gMsgReadIndex + dir;
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;
    gMsgReadIndex = (uint8_t)next;
    if (gMsgReadSource == MSG_SCREEN_INBOX) MSG_STORE_MarkInboxRead(gMsgReadIndex);
}

static uint8_t range_current_count(void)
{
    return gMsgRangeCount;
}

static void range_move(int8_t dir)
{
    const uint8_t count = range_current_count();
    if (count == 0u) {
        gMsgRangeScroll = 0u;
        return;
    }
    int16_t next = (int16_t)gMsgRangeScroll + dir;
    if (next < 0)
        next = (int16_t)count - 1;
    if (next >= count)
        next = 0;
    gMsgRangeScroll = (uint8_t)next;
}

static void range_send_ping(bool automatic)
{
    if (gMsgRangeStatus == 1u) return;
    gMsgRangeStatus = 1u;
    s_msgRangeWaitTicks = MSG_RANGE_WAIT_TICKS;
    if (automatic) s_msgAutoState = (uint8_t)((s_msgAutoState | MSG_AUTO_WAIT) &
                                               ~MSG_AUTO_REPLY);
    else s_msgAutoState &= (uint8_t)~(MSG_AUTO_WAIT | MSG_AUTO_REPLY);
    gMsgRangeScroll = 0u;
    if (gScreenToDisplay == DISPLAY_MESSENGER) UI_DisplayMessenger();
    if (!MSG_RF_SendRangePing())
        handle_range_ping_failure(automatic);
}

static void auto_check_set(uint8_t interval)
{
    if (interval > 4u) interval = 0u;
    if (interval)
        s_msgAutoState = (uint8_t)((s_msgAutoState &
            (MSG_AUTO_RESULT_VALID | MSG_AUTO_WAIT | MSG_AUTO_REPLY |
             MSG_AUTO_SUCCESS | MSG_AUTO_ALERT)) | interval);
    else
        s_msgAutoState &= MSG_AUTO_WAIT;
    s_msgAutoCountdownTicks = interval ? 1u : 0u;
    if (!interval && (s_msgAutoState & MSG_AUTO_WAIT)) {
        MSG_RF_CancelRangeWait();
        MSG_RF_HardRestoreVoicePath();
        gMsgRangeStatus = 0u;
        s_msgRangeWaitTicks = 0u;
        s_msgAutoState = 0u;
    }
    gUpdateStatus = true;
}

void MSG_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    if (gMsgTxLockNoticeTicks > 0u) {
        if (!bKeyPressed && !bKeyHeld) {
            gMsgTxLockNoticeTicks = 0u;
            gUpdateDisplay = true;
        }
        return;
    }

    if (bKeyHeld) {
        if (bKeyPressed && gMsgScreen == MSG_SCREEN_COMPOSE && Key >= KEY_0 && Key <= KEY_9) {
            TEXT_INPUT_HandleLongKey(&gMsgEditor, Key);
            gUpdateDisplay = true;
        }
        return;
    }
    if (bKeyPressed) return;

    switch (gMsgScreen) {
        case MSG_SCREEN_HOME:
            if (Key == KEY_UP || Key == KEY_DOWN) {
                gMsgHomeCursor = (uint8_t)((gMsgHomeCursor + (Key == KEY_UP ? 3 : 1)) % 4);
            } else if (Key == KEY_MENU) {
                switch (gMsgHomeCursor) {
                    case 0: open_list(MSG_SCREEN_INBOX); break;
                    case 1: open_compose(""); break;
                    case 2: open_list(MSG_SCREEN_OUTBOX); break;
                    case 3: open_list(MSG_SCREEN_DRAFTS); break;
                }
            } else if (Key == KEY_EXIT) {
                GENERIC_ExitApplication();
            }
            break;

        case MSG_SCREEN_INBOX:
        case MSG_SCREEN_OUTBOX:
        case MSG_SCREEN_DRAFTS:
            if (Key == KEY_UP || Key == KEY_DOWN) list_move(Key == KEY_UP ? -1 : 1);
            else if (Key == KEY_MENU) {
                if (gMsgScreen == MSG_SCREEN_DRAFTS) open_draft_edit(gMsgCursor);
                else if (current_count() > 0) {
                    bool was_inbox = (gMsgScreen == MSG_SCREEN_INBOX);
                    gMsgReadSource = (uint8_t)gMsgScreen;
                    gMsgReadIndex = gMsgCursor;
                    gMsgScreen = MSG_SCREEN_READ;
                    if (was_inbox) MSG_STORE_MarkInboxRead(gMsgReadIndex);
                }
            } else if (Key == KEY_EXIT) go_home();
            else if (Key == KEY_F) {
                if (gMsgScreen == MSG_SCREEN_INBOX) MSG_STORE_DeleteInbox(gMsgCursor);
                else if (gMsgScreen == MSG_SCREEN_OUTBOX) MSG_STORE_DeleteOutbox(gMsgCursor);
                list_normalize_after_delete();
            }
            break;

        case MSG_SCREEN_READ:
            if (Key == KEY_UP) {
                read_move(-1);
            } else if (Key == KEY_DOWN) {
                read_move(1);
            } else if (Key == KEY_MENU) {
                if (gMsgReadSource == MSG_SCREEN_OUTBOX) {
                    const char *text = read_message_text();
                    if (text) {
                        if (!MSG_RF_SendText(text) && handle_send_failure(text)) break;
                    }
                    open_sent_after_send();
                } else {
                    open_compose("RE: ");
                }
            } else if (Key == KEY_F) {
                if (gMsgReadSource == MSG_SCREEN_OUTBOX) {
                    MSG_STORE_DeleteOutbox(gMsgReadIndex);
                } else {
                    MSG_STORE_DeleteInbox(gMsgReadIndex);
                }
                return_to_read_source_list();
            } else if (Key == KEY_EXIT) {
                return_to_read_source_list();
            }
            break;

        case MSG_SCREEN_COMPOSE:
            if (Key == KEY_MENU) {
                TEXT_INPUT_Commit(&gMsgEditor);
                if (gMsgComposeIsDraftEdit) {
                    /* Drafts are quick-message templates: MENU saves the edited
                     * draft persistently and immediately sends the same text.
                     * This avoids needing an extra key combination on UV-K1.
                     */
                    MSG_STORE_SetDraft(gMsgComposeDraftIndex, gMsgComposeBuf);
                    const char *send_text = gMsgComposeBuf[0] ? gMsgComposeBuf : "EMPTY";
                    if (!MSG_RF_SendText(send_text) && handle_send_failure(send_text)) break;
                    open_sent_after_send();
                } else {
                    const char *send_text = gMsgComposeBuf[0] ? gMsgComposeBuf : "EMPTY";
                    if (!MSG_RF_SendText(send_text) && handle_send_failure(send_text)) break;
                    open_sent_after_send();
                }
            }
            else if (Key == KEY_EXIT) { TEXT_INPUT_Commit(&gMsgEditor); go_home(); }
            else TEXT_INPUT_HandleKey(&gMsgEditor, Key);
            break;

        case MSG_SCREEN_RANGE:
            if (Key == KEY_EXIT) {
                if (!(s_msgAutoState & MSG_AUTO_WAIT)) {
                    MSG_RF_CancelRangeWait();
                    MSG_RF_HardRestoreVoicePath();
                    gMsgRangeStatus = 0u;
                    s_msgRangeWaitTicks = 0u;
                }
                GENERIC_ExitApplication();
            } else if (Key == KEY_MENU) {
                gMsgCursor = 0u;
                gMsgScreen = MSG_SCREEN_RANGE_MENU;
            } else if (Key == KEY_UP) {
                range_move(-1);
            } else if (Key == KEY_DOWN) {
                range_move(1);
            }
            break;

        case MSG_SCREEN_RANGE_MENU:
            if (Key == KEY_UP || Key == KEY_DOWN)
                gMsgCursor ^= 1u;
            else if (Key == KEY_EXIT)
                gMsgScreen = MSG_SCREEN_RANGE;
            else if (Key == KEY_MENU) {
                if (gMsgCursor == 0u) {
                    gMsgScreen = MSG_SCREEN_RANGE;
                    range_send_ping(false);
                } else {
                    gMsgCursor = MSG_AUTO_INTERVAL();
                    gMsgScreen = MSG_SCREEN_RANGE_AUTO;
                }
            }
            break;

        case MSG_SCREEN_RANGE_AUTO:
            if (Key == KEY_UP)
                gMsgCursor = gMsgCursor ? (uint8_t)(gMsgCursor - 1u) : 4u;
            else if (Key == KEY_DOWN)
                gMsgCursor = (uint8_t)((gMsgCursor + 1u) % 5u);
            else if (Key == KEY_EXIT)
                gMsgScreen = MSG_SCREEN_RANGE_MENU;
            else if (Key == KEY_MENU) {
                auto_check_set(gMsgCursor);
                gMsgScreen = MSG_SCREEN_RANGE;
            }
            break;

    }
    gUpdateDisplay = true;
}
