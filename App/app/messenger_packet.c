#include <string.h>
#include "app/messenger_packet.h"

static void put_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t get_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint16_t MSG_PACKET_Crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x8000u) crc = (uint16_t)((crc << 1) ^ 0x1021u);
            else crc <<= 1;
        }
    }
    return crc;
}

static void copy_field(uint8_t *dst, uint8_t dst_len, const char *src)
{
    memset(dst, 0, dst_len);
    if (!src) return;
    for (uint8_t i = 0; i < dst_len && src[i]; i++) dst[i] = (uint8_t)src[i];
}

uint8_t MSG_PACKET_BuildText(uint8_t *out, uint8_t out_len, uint16_t id, const char *from, const char *text, uint8_t ttl_init)
{
    if (!out || out_len < MSG_PKT_WIRE_LEN) return 0;
    memset(out, 0, MSG_PKT_WIRE_LEN);
    out[0] = MSG_PKT_MAGIC0;
    out[1] = MSG_PKT_MAGIC1;
    out[2] = MSG_PKT_MAGIC2;
    out[3] = MSG_PKT_MAGIC3;
    out[4] = MSG_PKT_VERSION;
    out[5] = MSG_PKT_TYPE_TEXT;
    out[6] = 0; // flags, reserved for future unicast/encryption/priority
    put_u16_le(&out[7], id);
    out[9] = ttl_init;
    out[10] = ttl_init; // ttl_remain starts equal to ttl_init at source
    copy_field(&out[11], MSG_CALLSIGN_LEN, from && from[0] ? from : "UVK1");
    copy_field(&out[19], MSG_CALLSIGN_LEN, MSG_PKT_TO_ALL);

    uint8_t text_len = 0;
    if (text) {
        while (text[text_len] && text_len < MSG_PKT_MAX_PAYLOAD) text_len++;
        memcpy(&out[28], text, text_len);
    }
    out[27] = text_len;

    uint16_t crc = MSG_PACKET_Crc16(out, MSG_PKT_WIRE_LEN - 2u);
    put_u16_le(&out[MSG_PKT_WIRE_LEN - 2u], crc);
    return MSG_PKT_WIRE_LEN;
}


uint8_t MSG_PACKET_BuildAck(uint8_t *out, uint8_t out_len, uint16_t id, const char *from, const char *to)
{
    if (!out || out_len < MSG_PKT_WIRE_LEN) return 0;
    memset(out, 0, MSG_PKT_WIRE_LEN);
    out[0] = MSG_PKT_MAGIC0;
    out[1] = MSG_PKT_MAGIC1;
    out[2] = MSG_PKT_MAGIC2;
    out[3] = MSG_PKT_MAGIC3;
    out[4] = MSG_PKT_VERSION;
    out[5] = MSG_PKT_TYPE_ACK;
    out[6] = 0;
    put_u16_le(&out[7], id);
    out[9] = 1;
    out[10] = 1;
    copy_field(&out[11], MSG_CALLSIGN_LEN, from && from[0] ? from : "UVK1");
    copy_field(&out[19], MSG_CALLSIGN_LEN, to && to[0] ? to : MSG_PKT_TO_ALL);
    /* RF21: mirror the ACKed MsgID in the ACK payload as well.
     * The packet header id remains canonical; payload[0..1] gives a robust
     * fallback for ACK matching without creating a new message id.
     */
    out[27] = 2;
    out[28] = (uint8_t)(id & 0xFFu);
    out[29] = (uint8_t)(id >> 8);
    uint16_t crc = MSG_PACKET_Crc16(out, MSG_PKT_WIRE_LEN - 2u);
    put_u16_le(&out[MSG_PKT_WIRE_LEN - 2u], crc);
    return MSG_PKT_WIRE_LEN;
}

static uint8_t build_control(uint8_t *out, uint8_t out_len, uint8_t type, uint16_t id, const char *from, const char *to)
{
    if (!out || out_len < MSG_PKT_WIRE_LEN) return 0;
    memset(out, 0, MSG_PKT_WIRE_LEN);
    out[0] = MSG_PKT_MAGIC0;
    out[1] = MSG_PKT_MAGIC1;
    out[2] = MSG_PKT_MAGIC2;
    out[3] = MSG_PKT_MAGIC3;
    out[4] = MSG_PKT_VERSION;
    out[5] = type;
    out[6] = 0;
    put_u16_le(&out[7], id);
    out[9] = 1;
    out[10] = 1;
    copy_field(&out[11], MSG_CALLSIGN_LEN, from && from[0] ? from : "UVK1");
    copy_field(&out[19], MSG_CALLSIGN_LEN, to && to[0] ? to : MSG_PKT_TO_ALL);
    out[27] = 0;
    uint16_t crc = MSG_PACKET_Crc16(out, MSG_PKT_WIRE_LEN - 2u);
    put_u16_le(&out[MSG_PKT_WIRE_LEN - 2u], crc);
    return MSG_PKT_WIRE_LEN;
}

uint8_t MSG_PACKET_BuildPing(uint8_t *out, uint8_t out_len, uint16_t id, const char *from)
{
    return build_control(out, out_len, MSG_PKT_TYPE_PING, id, from, MSG_PKT_TO_ALL);
}

uint8_t MSG_PACKET_BuildPong(uint8_t *out, uint8_t out_len, uint16_t id, const char *from, const char *to, uint16_t battery_cv)
{
    uint8_t n = build_control(out, out_len, MSG_PKT_TYPE_PONG, id, from, to && to[0] ? to : MSG_PKT_TO_ALL);
    if (n == MSG_PKT_WIRE_LEN) {
        /* Range Check 0.5.13: carry remote battery voltage in centivolts.
         * Old packets have payload_len=0, so receivers can show --.- safely.
         */
        out[27] = 2;
        out[28] = (uint8_t)(battery_cv & 0xFFu);
        out[29] = (uint8_t)(battery_cv >> 8);
        uint16_t crc = MSG_PACKET_Crc16(out, MSG_PKT_WIRE_LEN - 2u);
        put_u16_le(&out[MSG_PKT_WIRE_LEN - 2u], crc);
    }
    return n;
}

static void copy_cstr(char *dst, uint8_t dst_len, const uint8_t *src, uint8_t src_len)
{
    uint8_t n = src_len;
    if (n > dst_len) n = dst_len;
    memcpy(dst, src, n);
    dst[n] = 0;
    while (n > 0 && dst[n - 1] == 0) n--;
    dst[n] = 0;
}

bool MSG_PACKET_Parse(const uint8_t *data, uint8_t len, MSG_Packet_t *pkt)
{
    if (!data || !pkt || len < MSG_PKT_WIRE_LEN) return false;

    // Native GGFW frames are fixed length for Stage 6/7.  Later RF capture may
    // pass a larger buffer, so scan for the magic instead of assuming offset 0.
    for (uint8_t off = 0; off + MSG_PKT_WIRE_LEN <= len; off++) {
        const uint8_t *p = &data[off];
        if (p[0] != MSG_PKT_MAGIC0 || p[1] != MSG_PKT_MAGIC1 || p[2] != MSG_PKT_MAGIC2 || p[3] != MSG_PKT_MAGIC3) continue;
        if (p[4] != MSG_PKT_VERSION) continue;
        uint16_t got = get_u16_le(&p[MSG_PKT_WIRE_LEN - 2u]);
        uint16_t want = MSG_PACKET_Crc16(p, MSG_PKT_WIRE_LEN - 2u);
        if (got != want) continue;
        if (p[27] > MSG_PKT_MAX_PAYLOAD) continue;

        memset(pkt, 0, sizeof(*pkt));
        pkt->type = p[5];
        pkt->flags = p[6];
        pkt->id = get_u16_le(&p[7]);
        pkt->ttl_init = p[9];
        pkt->ttl_remain = p[10];
        copy_cstr(pkt->from, MSG_CALLSIGN_LEN, &p[11], MSG_CALLSIGN_LEN);
        copy_cstr(pkt->to, MSG_CALLSIGN_LEN, &p[19], MSG_CALLSIGN_LEN);
        pkt->payload_len = p[27];
        copy_cstr(pkt->payload, MSG_TEXT_LEN, &p[28], pkt->payload_len);
        return true;
    }
    return false;
}

bool MSG_PACKET_SelfTest(void)
{
    uint8_t buf[MSG_PKT_WIRE_LEN];
    MSG_Packet_t pkt;
    uint8_t n = MSG_PACKET_BuildText(buf, sizeof(buf), 0x1234u, "UVK1", "HELLO", 5);
    if (n != MSG_PKT_WIRE_LEN) return false;
    if (!MSG_PACKET_Parse(buf, n, &pkt)) return false;
    if (pkt.id != 0x1234u) return false;
    if (pkt.ttl_init != 5 || pkt.ttl_remain != 5) return false;
    if (strcmp(pkt.from, "UVK1") != 0) return false;
    if (strcmp(pkt.to, "ALL") != 0) return false;
    if (strcmp(pkt.payload, "HELLO") != 0) return false;
    n = MSG_PACKET_BuildAck(buf, sizeof(buf), 0x1234u, "UVK1", "NODE2");
    if (n != MSG_PKT_WIRE_LEN) return false;
    if (!MSG_PACKET_Parse(buf, n, &pkt)) return false;
    if (pkt.type != MSG_PKT_TYPE_ACK || pkt.id != 0x1234u) return false;
    return true;
}
