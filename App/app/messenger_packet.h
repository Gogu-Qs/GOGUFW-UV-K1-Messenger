#ifndef APP_MESSENGER_PACKET_H
#define APP_MESSENGER_PACKET_H

#include <stdbool.h>
#include <stdint.h>
#include "app/messenger_store.h"

#define MSG_PKT_MAGIC0          'G'
#define MSG_PKT_MAGIC1          'G'
#define MSG_PKT_MAGIC2          'M'
#define MSG_PKT_MAGIC3          '2'
#define MSG_PKT_VERSION         2u
#define MSG_PKT_TYPE_TEXT       1u
#define MSG_PKT_TYPE_ACK        2u
#define MSG_PKT_TYPE_PING       3u
#define MSG_PKT_TYPE_PONG       4u
#define MSG_PKT_TO_ALL          "ALL"
#define MSG_PKT_WIRE_LEN        94u
#define MSG_PKT_MAX_PAYLOAD     MSG_TEXT_LEN

typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint16_t id;
    uint8_t  ttl_init;
    uint8_t  ttl_remain;
    char     from[MSG_CALLSIGN_LEN + 1];
    char     to[MSG_CALLSIGN_LEN + 1];
    uint8_t  payload_len;
    char     payload[MSG_TEXT_LEN + 1];
} MSG_Packet_t;

uint16_t MSG_PACKET_Crc16(const uint8_t *data, uint16_t len);
uint8_t MSG_PACKET_BuildText(uint8_t *out, uint8_t out_len, uint16_t id, const char *from, const char *text, uint8_t ttl_init);
uint8_t MSG_PACKET_BuildAck(uint8_t *out, uint8_t out_len, uint16_t id, const char *from, const char *to);
uint8_t MSG_PACKET_BuildPing(uint8_t *out, uint8_t out_len, uint16_t id, const char *from);
uint8_t MSG_PACKET_BuildPong(uint8_t *out, uint8_t out_len, uint16_t id, const char *from, const char *to, uint16_t battery_cv);
bool MSG_PACKET_Parse(const uint8_t *data, uint8_t len, MSG_Packet_t *pkt);
bool MSG_PACKET_SelfTest(void);

#endif
