#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "app/messenger.h"
#include "app/messenger_packet.h"

int main(void)
{
    assert(MSG_PACKET_SelfTest());
    assert(MSG_PACKET_Crc16((const uint8_t *)"123456789", 9) == 0x29b1);
    assert(sizeof(MSG_RangeFound_t) == 16 && MSG_RANGE_MAX_FOUND == 6);
    uint8_t wire[MSG_PKT_WIRE_LEN];
    MSG_Packet_t packet;
    const char *text = "123456789012345678901234567890123456";
    assert(MSG_PACKET_BuildText(wire, sizeof(wire), 65535, "ABCDEFGH", text, 5) == 94);
    assert(MSG_PACKET_Parse(wire, sizeof(wire), &packet));
    assert(packet.id == 65535 && !strcmp(packet.from, "ABCDEFGH") && !strcmp(packet.payload, text));
    wire[40] ^= 1;
    assert(!MSG_PACKET_Parse(wire, sizeof(wire), &packet));
    assert(MSG_PACKET_BuildPing(wire, sizeof(wire), 1, "UVK1") == 94);
    assert(MSG_PACKET_Parse(wire, sizeof(wire), &packet) && packet.type == MSG_PKT_TYPE_PING);
    assert(MSG_PACKET_BuildPong(wire, sizeof(wire), 1, "UVK1", "NODE2", 750) == 94);
    assert(MSG_PACKET_Parse(wire, sizeof(wire), &packet) && packet.type == MSG_PKT_TYPE_PONG);
    assert(MSG_PACKET_BuildWake(wire, sizeof(wire), 1, "UVK1") == 94);
    assert(MSG_PACKET_Parse(wire, sizeof(wire), &packet) && packet.type == MSG_PKT_TYPE_WAKE);
    puts("Messenger: self-test, CRC vector, full text/callsign, corrupt packet, ping/pong/wake and HEARD layout PASS");
}
