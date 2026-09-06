#ifndef BLUETOOTH_PROTOCOL_H
#define BLUETOOTH_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/bluetooth/uuid.h>

#define SCALE_SERVICE_UUID            0x181D
#define SCALE_WEIGHT_CHAR_UUID        0x2A9D
#define SCALE_COMMAND_CHAR_UUID       BT_UUID_128_ENCODE(0x553f4e49, 0xbf21, 0x4468, 0x9c6c, 0x0e4fb5b17697)

#define SCALE_WEIGHT_EVENT_ID         0x10
#define SCALE_START_REQUEST_LEN       2U
#define SCALE_HEARTBEAT_LEN           1U
#define SCALE_TARE_COMMAND_LEN        1U
#define SCALE_TARE_COMMAND_BYTE       0x00U

bool scale_protocol_is_stream_start_request(const uint8_t *request, uint16_t len);
bool scale_protocol_is_heartbeat_request(const uint8_t *request, uint16_t len);
bool scale_protocol_is_tare_command(const uint8_t *command, uint16_t len);

void scale_protocol_encode_weight_frame(uint8_t frame[9], int32_t weight_dg);

#endif // BLUETOOTH_PROTOCOL_H
