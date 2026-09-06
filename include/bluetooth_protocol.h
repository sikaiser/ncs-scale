#ifndef BLUETOOTH_PROTOCOL_H
#define BLUETOOTH_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/bluetooth/uuid.h>

/* Varia AKU service and characteristic UUIDs. */
#define SCALE_SERVICE_UUID            0xFFF0
#define SCALE_WEIGHT_CHAR_UUID        0xFFF1
#define SCALE_COMMAND_CHAR_UUID       0xFFF2

/* Packet identifiers and fixed sizes used by the Varia-compatible protocol. */
#define SCALE_SYSTEM_MESSAGE_ID       0xFAU
#define SCALE_WEIGHT_MESSAGE_ID       0x01U
#define SCALE_TARE_MESSAGE_ID         0x82U
#define SCALE_WEIGHT_FRAME_LEN        7U
#define SCALE_TARE_PACKET_LEN         5U

void scale_protocol_encode_weight_frame(uint8_t frame[SCALE_WEIGHT_FRAME_LEN], int32_t weight_cg);
void scale_protocol_encode_tare_command(uint8_t packet[SCALE_TARE_PACKET_LEN]);

#endif // BLUETOOTH_PROTOCOL_H
