#include "bluetooth_protocol.h"

/* Varia packets use a simple XOR over bytes 1..N-1 for integrity. */
static uint8_t scale_protocol_checksum(const uint8_t *data, size_t first_idx, size_t last_idx)
{
	uint8_t sum = 0U;

	for (size_t idx = first_idx; idx <= last_idx; ++idx) {
		sum ^= data[idx];
	}

	return sum;
}

void scale_protocol_encode_weight_frame(uint8_t frame[SCALE_WEIGHT_FRAME_LEN], int32_t weight_cg)
{
	uint32_t abs_weight_cg;
	uint8_t sign_and_high_nibble;

	if (frame == NULL) {
		return;
	}

	abs_weight_cg = (uint32_t)(weight_cg < 0 ? -weight_cg : weight_cg);
	sign_and_high_nibble = (weight_cg < 0 ? 0x10U : 0x00U) | ((abs_weight_cg >> 16) & 0x0FU);

	/* Varia weight frame: [FA 01] 03 [sign|high4] [mid8] [low8] [xor]. */
	frame[0] = SCALE_SYSTEM_MESSAGE_ID;
	frame[1] = SCALE_WEIGHT_MESSAGE_ID;
	frame[2] = 0x03U;
	frame[3] = sign_and_high_nibble;
	frame[4] = (uint8_t)((abs_weight_cg >> 8) & 0xFFU);
	frame[5] = (uint8_t)(abs_weight_cg & 0xFFU);
	frame[6] = scale_protocol_checksum(frame, 1U, 5U);
}

void scale_protocol_encode_tare_command(uint8_t packet[SCALE_TARE_PACKET_LEN])
{
	if (packet == NULL) {
		return;
	}

	/* Varia tare command payload on FFF2. */
	packet[0] = SCALE_SYSTEM_MESSAGE_ID;
	packet[1] = SCALE_TARE_MESSAGE_ID;
	packet[2] = 0x01U;
	packet[3] = 0x01U;
	packet[4] = scale_protocol_checksum(packet, 1U, 3U);
}
