/**
 * @file ign_proto_errors.h
 * @brief Коды статусов и NACK-ошибок протокола IGN
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Статус выполнения операций протокола IGN
 */
typedef enum
{
	IGN_PROTO_OK = 0,
	IGN_PROTO_E_ARG = -1,
	IGN_PROTO_E_CRC = -2,
	IGN_PROTO_E_FRAME = -3,
	IGN_PROTO_E_TX = -4,
	IGN_PROTO_E_UNSUPPORTED = -5,
	IGN_PROTO_E_INTERNAL = -6
} ign_proto_status_t;

/**
 * @brief Коды ошибок для NACK
 */
typedef enum
{
	IGN_NACK_BAD_LEN = 0x01,
	IGN_NACK_BAD_CRC = 0x02,
	IGN_NACK_BAD_FRAME = 0x03,
	IGN_NACK_UNSUPPORTED = 0x07,
	IGN_NACK_INTERNAL = 0x0F,

	/* Stage 3 */
	IGN_NACK_UNKNOWN_OBJECT = 0x10,
	IGN_NACK_BAD_TYPE = 0x11,
	IGN_NACK_OUT_OF_RANGE = 0x12,
	IGN_NACK_INVALID_SESSION = 0x13,
	IGN_NACK_READ_ONLY = 0x14,
	IGN_NACK_NOT_SUPPORTED = 0x15
} ign_nack_code_t;

static inline const char *ign_proto_status_to_str(ign_proto_status_t status)
{
	switch (status)
	{
		case IGN_PROTO_OK:
			return "OK";
		case IGN_PROTO_E_ARG:
			return "Invalid argument";
		case IGN_PROTO_E_CRC:
			return "CRC error";
		case IGN_PROTO_E_FRAME:
			return "Frame format error";
		case IGN_PROTO_E_TX:
			return "Transmission error";
		case IGN_PROTO_E_UNSUPPORTED:
			return "Unsupported command";
		case IGN_PROTO_E_INTERNAL:
			return "Internal error";
		default:
			return "Unknown error";
	}
}

static inline const char *ign_nack_code_to_str(ign_nack_code_t code)
{
	switch (code)
	{
		case IGN_NACK_BAD_LEN:
			return "Bad payload length";
		case IGN_NACK_BAD_CRC:
			return "Bad CRC";
		case IGN_NACK_BAD_FRAME:
			return "Bad frame format";
		case IGN_NACK_UNSUPPORTED:
			return "Command not supported";
		case IGN_NACK_INTERNAL:
			return "Internal device error";
		case IGN_NACK_UNKNOWN_OBJECT:
			return "Unknown object";
		case IGN_NACK_BAD_TYPE:
			return "Bad type";
		case IGN_NACK_OUT_OF_RANGE:
			return "Out of range";
		case IGN_NACK_INVALID_SESSION:
			return "Invalid session";
		case IGN_NACK_READ_ONLY:
			return "Read only";
		case IGN_NACK_NOT_SUPPORTED:
			return "Not supported";
		default:
			return "Unknown NACK code";
	}
}

#ifdef __cplusplus
}
#endif