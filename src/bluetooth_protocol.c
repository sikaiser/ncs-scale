#include "bluetooth_protocol.h"

bool scale_protocol_is_stream_start_request(const uint8_t *request, uint16_t len)
{
	return (request != NULL) &&
	       (len == SCALE_START_REQUEST_LEN) &&
	       (request[0] == 0x02U) &&
	       (request[1] == 0x00U);
}

bool scale_protocol_is_heartbeat_request(const uint8_t *request, uint16_t len)
{
	return (request != NULL) &&
	       (len == SCALE_HEARTBEAT_LEN) &&
	       (request[0] == 0x00U);
}

bool scale_protocol_is_tare_command(const uint8_t *command, uint16_t len)
{
	return (command != NULL) &&
	       (len == SCALE_TARE_COMMAND_LEN) &&
	       (command[0] == SCALE_TARE_COMMAND_BYTE);
}

void scale_protocol_encode_weight_frame(uint8_t frame[9], int32_t weight_dg)
{
	uint32_t encoded_weight_dg;

	if (frame == NULL) {
		return;
	}

	encoded_weight_dg = weight_dg < 0 ? 0U : (uint32_t)weight_dg;

	/* Frame format: [event][dripper_le32][scale_le32]. */
	frame[0] = SCALE_WEIGHT_EVENT_ID;
	frame[1] = 0x00U;
	frame[2] = 0x00U;
	frame[3] = 0x00U;
	frame[4] = 0x00U;
	frame[5] = (uint8_t)(encoded_weight_dg & 0xffU);
	frame[6] = (uint8_t)((encoded_weight_dg >> 8) & 0xffU);
	frame[7] = (uint8_t)((encoded_weight_dg >> 16) & 0xffU);
	frame[8] = (uint8_t)((encoded_weight_dg >> 24) & 0xffU);
}
