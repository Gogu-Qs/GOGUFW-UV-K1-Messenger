#include <string.h>
#include <stdio.h>
#include "app/messenger_store.h"
#include "app/messenger.h"
#include "app/messenger_rf.h"
#include "app/text_input.h"
#include "app/messenger_packet.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "ui/helper.h"
#include "ui/main.h"
#include "ui/ui.h"
#include "misc.h"

extern uint8_t gMsgHomeCursor;
extern uint8_t gMsgCursor;
extern uint8_t gMsgScroll;
extern uint8_t gMsgReadIndex;
extern uint8_t gMsgReadSource;
extern char gMsgComposeBuf[];
extern TEXT_INPUT_Editor_t gMsgEditor;
extern uint8_t gMsgScreen;

extern uint8_t gMsgRangeCount;
extern uint8_t gMsgRangeScroll;
extern uint8_t gMsgRangeStatus;
extern uint16_t gMsgRangeSession;
extern uint8_t gMsgTxLockNoticeTicks;

enum { MSG_SCREEN_HOME = 0, MSG_SCREEN_INBOX, MSG_SCREEN_OUTBOX, MSG_SCREEN_DRAFTS, MSG_SCREEN_COMPOSE, MSG_SCREEN_READ, MSG_SCREEN_RANGE };


static void format_age(uint16_t seconds, char *buf, uint8_t len)
{
    if (!buf || len == 0u) return;
    if (seconds < 60u) snprintf(buf, len, "NOW");
    else if (seconds < 3600u) snprintf(buf, len, "%um", (unsigned)(seconds / 60u));
    else snprintf(buf, len, "%uh", (unsigned)(seconds / 3600u));
}

static const char *packet_type_short(uint8_t type)
{
    switch (type) {
        case MSG_PKT_TYPE_TEXT: return "MSG";
        case MSG_PKT_TYPE_ACK:  return "ACK";
        case MSG_PKT_TYPE_PING: return "PNG";
        case MSG_PKT_TYPE_PONG: return "PON";
        default: return "---";
    }
}

static const char *tx_block_reason_text(void)
{
    switch (MSG_RF_LastSendBlockReason()) {
        case MSG_RF_BLOCK_NO_FSK:       return "NO FSK SELECTED";
        case MSG_RF_BLOCK_TX_FREQUENCY: return "TX FREQ BLOCKED";
        case MSG_RF_BLOCK_MODULATION:   return "FM MODE REQUIRED";
        case MSG_RF_BLOCK_CONFIG:       return "CONFIG IN PROGRESS";
        case MSG_RF_BLOCK_BATTERY:      return "BATTERY BLOCK";
        default:                        return "FSK NOT ALLOWED";
    }
}

static void draw_title(const char *s)
{
    UI_DisplayClear();
    UI_GOGU_DrawHeader(s, NULL);
}

static void msg_set_pixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= 128U || y >= 64U) return;
    uint8_t mask = (uint8_t)(1U << (y & 7U));
    if (on) gFrameBuffer[y >> 3][x] |= mask;
    else    gFrameBuffer[y >> 3][x] &= (uint8_t)~mask;
}

static void msg_fill_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on)
{
    if (x1 > 127U) x1 = 127U;
    if (y1 > 63U) y1 = 63U;
    for (uint8_t y = y0; y <= y1; y++) {
        for (uint8_t x = x0; x <= x1; x++) msg_set_pixel(x, y, on);
    }
}

static uint8_t range_rssi_bars(int8_t rssi)
{
    /* Same visual scale as the UV-K5 GOGUFW Messenger UI. */
    if (rssi > -75)  return 5u;
    if (rssi > -88)  return 4u;
    if (rssi > -100) return 3u;
    if (rssi > -112) return 2u;
    if (rssi > -124) return 1u;
    return 0u;
}

static void draw_rssi_bars(uint8_t x, uint8_t y, int8_t rssi)
{
    uint8_t bars = range_rssi_bars(rssi);
    if (bars > 5u) bars = 5u;

    /* UV-K5-style five-step bar: inactive bars remain visible as a small
     * baseline tick, active bars are filled vertical columns. */
    for (uint8_t i = 0u; i < 5u; i++) {
        const uint8_t h = (uint8_t)(2u + i);
        const uint8_t bx = (uint8_t)(x + i * 4u);
        const uint8_t base = (uint8_t)(y + 6u);
        const uint8_t y0 = (uint8_t)(base - h);
        if (i < bars) msg_fill_rect(bx, y0, (uint8_t)(bx + 2u), base, true);
        else UI_DrawLineBuffer(gFrameBuffer, bx, base, (uint8_t)(bx + 2u), base, 1);
    }
}

static void msg_draw_small_at_y(const char *s, uint8_t x, uint8_t y, bool inverted)
{
    UI_GOGU_PrintSmallAtY(s, x, y, false);
    if (inverted)
        UI_GOGU_InvertBand(y > 0u ? (uint8_t)(y - 1u) : y, 9u);
}

static void print_line_y(const char *s, uint8_t y, bool sel)
{
    char safe[19];
    strncpy(safe, s, sizeof(safe) - 1U);
    safe[sizeof(safe) - 1U] = 0;
    msg_draw_small_at_y(safe, 1, y, sel);
}

static void print_wrapped_small_y(const char *s, uint8_t y, uint8_t max_lines)
{
    char linebuf[18];
    uint8_t line = 0;
    while (*s && line < max_lines) {
        uint8_t n = 0;
        while (s[n] && n < 17U) {
            linebuf[n] = s[n];
            n++;
        }
        linebuf[n] = 0;
        msg_draw_small_at_y(linebuf, 0, (uint8_t)(y + (line * 8U)), false);
        s += n;
        line++;
    }
}


static void msg_draw_hline(uint8_t x0, uint8_t x1, uint8_t y, bool on)
{
    if (x1 > 127U) x1 = 127U;
    for (uint8_t x = x0; x <= x1; x++) msg_set_pixel(x, y, on);
}

static void msg_draw_vline(uint8_t x, uint8_t y0, uint8_t y1, bool on)
{
    if (y1 > 63U) y1 = 63U;
    for (uint8_t y = y0; y <= y1; y++) msg_set_pixel(x, y, on);
}

static void msg_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on)
{
    int dx = (x1 > x0) ? (int)(x1 - x0) : (int)(x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -((y1 > y0) ? (int)(y1 - y0) : (int)(y0 - y1));
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int x = x0, y = y0;
    while (1) {
        msg_set_pixel((uint8_t)x, (uint8_t)y, on);
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

static void draw_icon_envelope(uint8_t x, uint8_t y)
{
    /* HOME icon: clean 30x18 envelope, centered in the right-side icon area. */
    msg_draw_hline(x, (uint8_t)(x + 29U), y, true);
    msg_draw_hline(x, (uint8_t)(x + 29U), (uint8_t)(y + 17U), true);
    msg_draw_vline(x, y, (uint8_t)(y + 17U), true);
    msg_draw_vline((uint8_t)(x + 29U), y, (uint8_t)(y + 17U), true);
    msg_draw_line((uint8_t)(x + 1U), (uint8_t)(y + 2U), (uint8_t)(x + 14U), (uint8_t)(y + 10U), true);
    msg_draw_line((uint8_t)(x + 28U), (uint8_t)(y + 2U), (uint8_t)(x + 15U), (uint8_t)(y + 10U), true);
    msg_draw_line((uint8_t)(x + 1U), (uint8_t)(y + 16U), (uint8_t)(x + 11U), (uint8_t)(y + 9U), true);
    msg_draw_line((uint8_t)(x + 28U), (uint8_t)(y + 16U), (uint8_t)(x + 18U), (uint8_t)(y + 9U), true);
}

static void draw_icon_pencil(uint8_t x, uint8_t y)
{
    /* HOME icon: clear diagonal pencil, 30x28.  Drawn as a thick slanted
     * body with a visible point and a short baseline, matching the UI mockup. */
    for (uint8_t i = 0; i < 20U; i++) {
        uint8_t px = (uint8_t)(x + 4U + i);
        uint8_t py = (uint8_t)(y + 23U - i);
        msg_set_pixel(px, py, true);
        msg_set_pixel((uint8_t)(px + 1U), py, true);
        msg_set_pixel(px, (uint8_t)(py - 1U), true);
        msg_set_pixel((uint8_t)(px + 1U), (uint8_t)(py - 1U), true);
    }
    /* pencil tip */
    msg_draw_line((uint8_t)(x + 2U), (uint8_t)(y + 25U), (uint8_t)(x + 6U), (uint8_t)(y + 23U), true);
    msg_draw_line((uint8_t)(x + 2U), (uint8_t)(y + 25U), (uint8_t)(x + 4U), (uint8_t)(y + 21U), true);
    msg_set_pixel((uint8_t)(x + 1U), (uint8_t)(y + 26U), true);
    /* eraser/cap */
    msg_draw_line((uint8_t)(x + 22U), (uint8_t)(y + 3U), (uint8_t)(x + 26U), y, true);
    msg_draw_line((uint8_t)(x + 24U), (uint8_t)(y + 6U), (uint8_t)(x + 29U), (uint8_t)(y + 2U), true);
    msg_draw_line((uint8_t)(x + 26U), y, (uint8_t)(x + 29U), (uint8_t)(y + 2U), true);
    msg_draw_line((uint8_t)(x + 22U), (uint8_t)(y + 3U), (uint8_t)(x + 24U), (uint8_t)(y + 6U), true);
    /* writing line */
    msg_draw_hline((uint8_t)(x + 10U), (uint8_t)(x + 29U), (uint8_t)(y + 27U), true);
}

static void draw_icon_up_arrow(uint8_t x, uint8_t y)
{
    /* HOME icon: bold upload/up arrow, 31x27. */
    const uint8_t cx = (uint8_t)(x + 15U);
    for (uint8_t r = 0; r < 9U; r++) {
        msg_draw_hline((uint8_t)(cx - r), (uint8_t)(cx + r), (uint8_t)(y + r), true);
    }
    msg_fill_rect((uint8_t)(x + 12U), (uint8_t)(y + 9U), (uint8_t)(x + 18U), (uint8_t)(y + 22U), true);
    msg_draw_hline((uint8_t)(x + 4U), (uint8_t)(x + 26U), (uint8_t)(y + 26U), true);
}

static void draw_icon_floppy(uint8_t x, uint8_t y)
{
    /* HOME icon: visually square floppy disk.
     * The LCD pixels look slightly taller than wide, so the bitmap is
     * intentionally wider than high (34x24) to read as a square on-device.
     * Outer shape: square body with only the top-right corner cut off. */
    msg_draw_hline(x, (uint8_t)(x + 28U), y, true);                 /* top edge stops at the cut */
    msg_draw_line((uint8_t)(x + 29U), y, (uint8_t)(x + 33U), (uint8_t)(y + 4U), true);
    msg_draw_vline((uint8_t)(x + 33U), (uint8_t)(y + 4U), (uint8_t)(y + 23U), true);
    msg_draw_hline(x, (uint8_t)(x + 33U), (uint8_t)(y + 23U), true);
    msg_draw_vline(x, y, (uint8_t)(y + 23U), true);

    /* Top shutter/label area. */
    msg_draw_hline((uint8_t)(x + 4U), (uint8_t)(x + 23U), (uint8_t)(y + 3U), true);
    msg_draw_hline((uint8_t)(x + 4U), (uint8_t)(x + 23U), (uint8_t)(y + 9U), true);
    msg_draw_vline((uint8_t)(x + 4U), (uint8_t)(y + 3U), (uint8_t)(y + 9U), true);
    msg_draw_vline((uint8_t)(x + 23U), (uint8_t)(y + 3U), (uint8_t)(y + 9U), true);
    msg_fill_rect((uint8_t)(x + 17U), (uint8_t)(y + 4U), (uint8_t)(x + 20U), (uint8_t)(y + 7U), true);

    /* Bottom label window, kept wide to avoid a tall/narrow look. */
    msg_draw_hline((uint8_t)(x + 6U), (uint8_t)(x + 27U), (uint8_t)(y + 14U), true);
    msg_draw_hline((uint8_t)(x + 6U), (uint8_t)(x + 27U), (uint8_t)(y + 22U), true);
    msg_draw_vline((uint8_t)(x + 6U), (uint8_t)(y + 14U), (uint8_t)(y + 22U), true);
    msg_draw_vline((uint8_t)(x + 27U), (uint8_t)(y + 14U), (uint8_t)(y + 22U), true);
    msg_draw_hline((uint8_t)(x + 9U), (uint8_t)(x + 24U), (uint8_t)(y + 17U), true);
    msg_draw_hline((uint8_t)(x + 9U), (uint8_t)(x + 24U), (uint8_t)(y + 19U), true);
}

static void draw_home_icon(uint8_t idx)
{
    /* Icons share one center point in the right side, midway between the
     * INBOX and DRAFTS rows.  They are not tied to the bottom separator. */
    switch (idx) {
        case 0: draw_icon_envelope(88U, 17U); break;  /* 30x18, center y~26 */
        case 1: draw_icon_pencil(89U, 13U); break;    /* 30x28, center y~27 */
        case 2: draw_icon_up_arrow(88U, 14U); break;  /* 31x27, center y~27 */
        default: draw_icon_floppy(86U, 15U); break;   /* 34x24, visual center y~27 */
    }
}

static void draw_home(void)
{
    static const char *items[] = { "INBOX", "COMPOSE", "SENT", "DRAFTS" };
    draw_title("MESSENGER");
    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);
    for (uint8_t i = 0; i < 4; i++) {
        print_line_y(items[i], UI_GOGU_CONTENT_ROW_Y(i), false);
        if (gMsgHomeCursor == i)
            UI_GOGU_InvertArea(0u, 63u, (uint8_t)(UI_GOGU_CONTENT_ROW_Y(i) - 1u), 9u);
    }
    draw_home_icon(gMsgHomeCursor);

    UI_GOGU_DrawFooter("SELECT", NULL, "EXIT");
}

static void current_list_info(uint8_t *count, const char **title)
{
    if (gMsgScreen == MSG_SCREEN_INBOX) { *count = MSG_STORE_CountInbox(); *title = "INBOX"; return; }
    if (gMsgScreen == MSG_SCREEN_OUTBOX) { *count = MSG_STORE_CountOutbox(); *title = "SENT"; return; }
    *count = MSG_DRAFT_CAPACITY;
    *title = "DRAFTS";
}

static void draw_list(void)
{
    uint8_t count; const char *title;
    current_list_info(&count, &title);
    char buf[24];
    snprintf(buf, sizeof(buf), "%u/%u", count ? (gMsgCursor + 1) : 0, count);
    UI_DisplayClear();
    UI_GOGU_DrawHeader(title, buf);
    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);
    if (!count) {
        if (gMsgScreen == MSG_SCREEN_DRAFTS)
            msg_draw_small_at_y("EMPTY", 47u, 25u, false);
        else
            msg_draw_small_at_y("NO MESSAGE", 29u, 25u, false);
        UI_GOGU_DrawFooter(gMsgScreen == MSG_SCREEN_DRAFTS ? "EDIT" : "READ",
                           gMsgScreen == MSG_SCREEN_DRAFTS ? NULL : "F:DEL", "EXIT");
        return;
    }
    for (uint8_t row = 0; row < UI_GOGU_CONTENT_ROWS; row++) {
        uint8_t idx = gMsgScroll + row;
        if (idx >= count) break;
        if (gMsgScreen == MSG_SCREEN_DRAFTS) {
            char draft[MSG_TEXT_LEN + 1];
            MSG_STORE_GetDraft(idx, draft);
            snprintf(buf, sizeof(buf), "%u %.18s", idx + 1, draft);
        }
        else if (gMsgScreen == MSG_SCREEN_OUTBOX) {
            char st = '?';
            char age[5];
            format_age(gMessengerOutbox[idx].age_seconds, age, sizeof(age));
            if (gMessengerOutbox[idx].status == MSG_STATUS_ACKED) st = '+';
            else if (gMessengerOutbox[idx].status == MSG_STATUS_FAILED) st = 'x';
            /* Fill the complete 18-character row so the age column reaches
             * the LCD's right edge instead of ending two character cells
             * early. */
            snprintf(buf, sizeof(buf), "%c%-13.13s%4s", st, gMessengerOutbox[idx].text, age);
        } else {
            char age[5];
            format_age(gMessengerInbox[idx].age_seconds, age, sizeof(age));
            /* Keep an explicit unread marker before the sender.  The complete
             * row remains 18 cells: marker, six-character callsign,
             * five-character preview and age. */
            snprintf(buf, sizeof(buf), "%c%-6.6s %-5.5s %4s",
                     gMessengerInbox[idx].unread ? '*' : ' ',
                     gMessengerInbox[idx].from,
                     gMessengerInbox[idx].text,
                     age);
        }
        /* Pixel-positioned renderer safely supports the complete 18-cell
         * row and keeps its selection capsule inside x=0..127. */
        print_line_y(buf, UI_GOGU_CONTENT_ROW_Y(row), idx == gMsgCursor);
    }
    UI_GOGU_DrawFooter(gMsgScreen == MSG_SCREEN_DRAFTS ? "EDIT" : "READ",
                       gMsgScreen == MSG_SCREEN_DRAFTS ? NULL : "F:DEL", "EXIT");
}

static void draw_read(void)
{
    const bool sent = gMsgReadSource == MSG_SCREEN_OUTBOX;
    const MSG_OutboxMessage_t *outbox = sent ? &gMessengerOutbox[gMsgReadIndex] : 0;
    const MSG_InboxMessage_t *inbox = sent ? 0 : &gMessengerInbox[gMsgReadIndex];
    const char *text = sent ? outbox->text : inbox->text;
    char buf[32];
    {
        uint8_t total = sent ? MSG_STORE_CountOutbox() : MSG_STORE_CountInbox();
        snprintf(buf, sizeof(buf), "%u/%u", total ? (uint8_t)(gMsgReadIndex + 1U) : 0U, total);
        UI_DisplayClear();
        UI_GOGU_DrawHeader(sent ? "SENT" : "READ", buf);
    }

    char age[5];
    format_age(sent ? outbox->age_seconds : inbox->age_seconds, age, sizeof(age));
    if (sent) {
        char st = '?';
        if (outbox->status == MSG_STATUS_ACKED) st = '+';
        else if (outbox->status == MSG_STATUS_FAILED) st = 'x';

        const char stbuf[2] = { st, 0 };
        UI_PrintStringSmallNormal(stbuf, 0u, 0u, 0u);
        snprintf(buf, sizeof(buf), "TO:%s", outbox->to);
    } else {
        snprintf(buf, sizeof(buf), "FROM:%s", inbox->from);
    }

    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);
    GUI_DisplaySmallest(buf, 0u, 12u, false, true);
    {
        const uint8_t age_width = (uint8_t)(strlen(age) * 4u);
        GUI_DisplaySmallest(age, age_width >= 128u ? 0u : (uint8_t)(128u - age_width),
                            12u, false, true);
    }
    if (sent && outbox->ack_count > 0u) {
        print_wrapped_small_y(text, 20, 2);
        char ackbuf[32];
        uint8_t pos = 0u;
        pos += (uint8_t)snprintf(ackbuf + pos, sizeof(ackbuf) - pos, "ACK:");
        for (uint8_t i = 0u; i < outbox->ack_count && i < MSG_ACK_SOURCE_MAX && pos < sizeof(ackbuf); i++) {
            pos += (uint8_t)snprintf(ackbuf + pos, sizeof(ackbuf) - pos, "%s%.*s", i ? " " : "", MSG_ACK_ID_LEN, outbox->ack_from[i]);
        }
        GUI_DisplaySmallest(ackbuf, 0, 40, false, true);
    } else {
        print_wrapped_small_y(text, 20, 3);
    }
    UI_GOGU_DrawFooter(sent ? "RESEND" : "REPLY", "F:DEL", "EXIT");
}

static void draw_compose(void)
{
    UI_GOGU_DrawTextEditor("COMPOSE", gMsgComposeBuf, MSG_TEXT_LEN, "SEND",
                           (gMsgEditor.mode == 2U) ? "2" : (gMsgEditor.upper ? "B" : "b"),
                           true);
}

static uint8_t range_window_start(uint8_t count, uint8_t cursor)
{
    if (count <= UI_GOGU_CONTENT_ROWS || cursor < 2u)
        return 0u;
    if (cursor + 1u >= count)
        return (uint8_t)(count - UI_GOGU_CONTENT_ROWS);
    return (uint8_t)(cursor - 1u);
}

static void draw_range(void)
{
    char buf[28];
    const bool active = gMsgRangeStatus == 1u || gMsgRangeStatus == 2u;
    uint8_t order[MSG_RANGE_MAX_FOUND];
    uint8_t count = 0u;

    if (active) {
        bool used[MSG_RANGE_MAX_FOUND];
        memset(used, 0, sizeof(used));
        for (;;) {
            int8_t best_rssi = -128;
            uint8_t best = 0xFFu;
            for (uint8_t i = 0u; i < gMsgRangeCount && i < MSG_RANGE_MAX_FOUND; i++) {
                if (used[i] || !gMsgRangeFound[i].used ||
                    gMsgRangeFound[i].range_session != gMsgRangeSession)
                    continue;
                if (best == 0xFFu || gMsgRangeFound[i].rssi > best_rssi) {
                    best = i;
                    best_rssi = gMsgRangeFound[i].rssi;
                }
            }
            if (best == 0xFFu)
                break;
            used[best] = true;
            order[count++] = best;
        }
    } else {
        count = gMsgRangeCount;
        for (uint8_t i = 0u; i < count; i++)
            order[i] = i;
    }

    if (count == 0u)
        gMsgRangeScroll = 0u;
    else if (gMsgRangeScroll >= count)
        gMsgRangeScroll = (uint8_t)(count - 1u);

    snprintf(buf, sizeof(buf), "%u/%u", count ? (uint8_t)(gMsgRangeScroll + 1u) : 0u, count);
    UI_DisplayClear();
    UI_GOGU_DrawHeader(active ? "RANGE CHECK" : "HEARD", buf);
    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);

    if (count == 0u) {
        if (gMsgRangeStatus == 1u) {
            msg_draw_small_at_y("WAIT", 50u, 25u, false);
        } else {
            msg_draw_small_at_y(active ? "NOT FOUND" : "NO HEARD", active ? 33u : 36u, 27u, false);
        }
    } else {
        const uint8_t first = range_window_start(count, gMsgRangeScroll);
        for (uint8_t row = 0u; row < UI_GOGU_CONTENT_ROWS; row++) {
            const uint8_t position = (uint8_t)(first + row);
            if (position >= count)
                break;
            const uint8_t idx = order[position];
            const uint8_t y = UI_GOGU_CONTENT_ROW_Y(row);

            snprintf(buf, sizeof(buf), "%-6s", gMsgRangeFound[idx].callsign);
            msg_draw_small_at_y(buf, 0u, y, false);
            if (active) {
                snprintf(buf, sizeof(buf), "%4d", (int)gMsgRangeFound[idx].rssi);
                msg_draw_small_at_y(buf, 43u, y, false);
                draw_rssi_bars(74u, y, gMsgRangeFound[idx].rssi);
                snprintf(buf, sizeof(buf), "%u.%uV",
                         (unsigned)(gMsgRangeFound[idx].battery_cv / 100u),
                         (unsigned)((gMsgRangeFound[idx].battery_cv / 10u) % 10u));
                msg_draw_small_at_y(buf, 100u, y, false);
            } else {
                char age[5];
                format_age(gMsgRangeFound[idx].age_seconds, age, sizeof(age));
                draw_rssi_bars(48u, y, gMsgRangeFound[idx].rssi);
                msg_draw_small_at_y(packet_type_short(gMsgRangeFound[idx].packet_type), 76u, y, false);
                const uint8_t age_x = (uint8_t)(127u - ((uint8_t)strlen(age) * 7u));
                msg_draw_small_at_y(age, age_x, y, false);
            }

            if (position == gMsgRangeScroll)
                UI_GOGU_InvertBand((uint8_t)(y - 1u), 9u);
        }
    }

    UI_GOGU_DrawFooter(gMsgRangeStatus == 1u ? "WAIT" : "PING", NULL, "EXIT");
}

void UI_DisplayMessenger(void)
{
#ifdef ENABLE_FEAT_F4HWN_ACTION_PICKER
    if (UI_DisplayActionPicker())
        return;
#endif

    switch (gMsgScreen) {
        case MSG_SCREEN_HOME: draw_home(); break;
        case MSG_SCREEN_INBOX:
        case MSG_SCREEN_OUTBOX:
        case MSG_SCREEN_DRAFTS: draw_list(); break;
        case MSG_SCREEN_READ: draw_read(); break;
        case MSG_SCREEN_COMPOSE: draw_compose(); break;
        case MSG_SCREEN_RANGE: draw_range(); break;
        default: draw_home(); break;
    }

    if (gMsgTxLockNoticeTicks > 0u) {
        /* Floating modal: retain the current screen and cover only its centre. */
        msg_fill_rect(19u, 19u, 108u, 43u, false);
        msg_draw_hline(19u, 108u, 19u, true);
        msg_draw_hline(19u, 108u, 43u, true);
        msg_draw_vline(19u, 19u, 43u, true);
        msg_draw_vline(108u, 19u, 43u, true);
        msg_draw_small_at_y("TX BLOCKED", 29u, 23u, false);
        {
            const char *reason = tx_block_reason_text();
            const uint8_t width = (uint8_t)(strlen(reason) * 4u);
            GUI_DisplaySmallest(reason, width >= 128u ? 0u : (uint8_t)((128u - width) / 2u),
                                34u, false, true);
        }
    }
    ST7565_BlitFullScreen();
}
