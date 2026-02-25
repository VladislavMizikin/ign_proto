/**
 * @file ign_proto_framing.c
 * @author Мизикин Владислав
 * @date 26.02.2026
 * @brief Реализация функций формирования и отправки фреймов протокола IGN
 */

/* Includes ------------------------------------------------------------------*/

#include "ign_proto_internal.h"
#include <string.h>

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Отправляет фрейм по протоколу IGN
 *
 * @param p Указатель на контекст протокола IGN (не должен быть NULL)
 * @param cmd Код команды для отправки (1 байт)
 * @param payload Указатель на буфер с полезными данными (может быть NULL, если payload_len == 0)
 * @param payload_len Длина полезных данных в байтах (0–512 байт)
 * @return Статус выполнения операции:
 *  - IGN_PROTO_OK: успех
 *  - IGN_PROTO_E_ARG: некорректные входные параметры
 *  - IGN_PROTO_E_INTERNAL: внутренняя ошибка (превышен лимит длины данных)
 *  - IGN_PROTO_E_TX: ошибка передачи данных
 *
 * @details
 * Формирует и отправляет фрейм следующего формата:
 * [SOF0][SOF1][LEN_L][LEN_H][CMD][PAYLOAD...][CRC32_LE]
 * - SOF: 2‑байтовая сигнатура начала кадра (0xAA, 0x55)
 * - LEN: 2‑байтное поле длины полезных данных (little‑endian)
 * - CMD: 1‑байт код команды
 * - PAYLOAD: опциональные данные (0–512 байт)
 * - CRC32: 4‑байт контрольная сумма (little‑endian), вычисляется от [LEN][CMD][PAYLOAD]
 */
ign_proto_status_t ign_proto_tx_frame(ign_proto_t *p, uint8_t cmd,
									  const uint8_t *payload, uint16_t payload_len)
{
	// Проверка входных параметров
	if (!p || !p->cfg.tx_bytes || !p->cfg.crc_init || !p->cfg.crc_update || !p->cfg.crc_final) {
		return IGN_PROTO_E_ARG;
	}
	if (payload_len && !payload) {
		return IGN_PROTO_E_ARG;
	}

	// Формирование заголовка (5 байт): SOF + LEN + CMD
	uint8_t hdr[5];
	hdr[0] = IGN_SOF0;  // 0xA5 — первый байт сигнатуры начала кадра
	hdr[1] = IGN_SOF1;  // 0x5A — второй байт сигнатуры начала кадра
	hdr[2] = (uint8_t)(payload_len & 0xFFu);        // LEN_L — младший байт длины
	hdr[3] = (uint8_t)((payload_len >> 8) & 0xFFu); // LEN_H — старший байт длины
	hdr[4] = cmd;                                   // CMD — код команды

	// Streaming CRC32 over [LEN][CMD][PAYLOAD]
	p->cfg.crc_init();
	p->cfg.crc_update(&hdr[2], 3u); // LEN_L LEN_H CMD
	if (payload_len) {
		p->cfg.crc_update(payload, payload_len);
	}
	uint32_t crc = p->cfg.crc_final();

	// Преобразование CRC32 в little‑endian (4 байта)
	uint8_t crc_le[4] = {
			(uint8_t)(crc & 0xFFu),
			(uint8_t)((crc >> 8) & 0xFFu),
			(uint8_t)((crc >> 16) & 0xFFu),
			(uint8_t)((crc >> 24) & 0xFFu)
	};

	// Последовательная отправка частей фрейма с проверкой ошибок
	if (p->cfg.tx_bytes(hdr, (uint32_t)sizeof(hdr)) != 0) {
		return IGN_PROTO_E_TX;
	}
	if (payload_len && p->cfg.tx_bytes(payload, payload_len) != 0) {
		return IGN_PROTO_E_TX;
	}
	if (p->cfg.tx_bytes(crc_le, 4u) != 0) {
		return IGN_PROTO_E_TX;
	}

	return IGN_PROTO_OK;
}

/**
 * @brief Отправляет подтверждение (ACK) для указанной команды
 * @param p Указатель на контекст протокола IGN
 * @param orig_cmd Исходный код команды, для которой отправляется подтверждение
 * @return Статус выполнения (IGN_PROTO_OK при успехе)
 * @details
 * Отправляет фрейм с командой IGN_CMD_ACK и полезной нагрузкой из 1 байта:
 * [orig_cmd]
 */
ign_proto_status_t ign_proto_send_ack(ign_proto_t *p, uint8_t orig_cmd)
{
	uint8_t payload[1] = { orig_cmd };
	return ign_proto_tx_frame(p, IGN_CMD_ACK, payload, 1u);
}

/**
 * @brief Отправляет отрицательное подтверждение (NACK) с кодом ошибки
 * @param p Указатель на контекст протокола IGN
 * @param orig_cmd Исходный код команды, вызвавшей ошибку
 * @param nack_code Код ошибки (определяется протоколом)
 * @return Статус выполнения (IGN_PROTO_OK при успехе)
 * @details
 * Отправляет фрейм с командой IGN_CMD_NACK и полезной нагрузкой из 2 байт:
 * [nack_code][orig_cmd]
 */
ign_proto_status_t ign_proto_send_nack(ign_proto_t *p, uint8_t orig_cmd, uint8_t nack_code)
{
	uint8_t payload[2] = { nack_code, orig_cmd };
	return ign_proto_tx_frame(p, IGN_CMD_NACK, payload, 2u);
}
