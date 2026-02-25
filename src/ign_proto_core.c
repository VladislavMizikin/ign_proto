/**
 * @file ign_proto_core.c
 * @author Мизикин Владислав
 * @date 26.02.2026
 * @brief Реализация CORE‑команд протокола IGN
 */

#include "ign_proto_internal.h"
#include <string.h>


/**
 * @brief Обрабатывает команду HELLO (0x01)
 *
 * Отправляет ответ с версией протокола: 'B' (0x42), 'L' (0x4C), версия 1 (0x01).
 * @param p Указатель на контекст протокола IGN
 * @return Статус выполнения (IGN_PROTO_OK при успехе или код ошибки)
 */
static ign_proto_status_t handle_hello(ign_proto_t *p)
{
	/* Response payload: 'B' 'L' version(1) */
	const uint8_t resp[] = { 0x42u, 0x4Cu, 0x01u }; /* "BL", v1 */
	return ign_proto_tx_frame(p, IGN_CMD_HELLO, resp, (uint16_t)sizeof(resp));
}

/**
 * @brief Обрабатывает команду HOST_TIME (0x02)
 *
 * Обновляет время хоста в метриках протокола на основе принятых данных.
 * @param p Указатель на контекст протокола IGN
 * @param payload Указатель на полезные данные (unix_sec + tz_min, 6 байт, LE)
 * @param len Длина полезных данных в байтах
 * @return IGN_PROTO_OK при успехе, IGN_NACK_BAD_LEN при неверной длине
 */
static ign_proto_status_t handle_host_time(ign_proto_t *p, const uint8_t *payload, uint16_t len)
{
	if (len != 6u) {
		return ign_proto_send_nack(p, IGN_CMD_HOST_TIME, IGN_NACK_BAD_LEN);
	}

	// Разбор unix_sec (4 байта, little‑endian)
	uint32_t sec = (uint32_t)payload[0]
				   | ((uint32_t)payload[1] << 8)
				   | ((uint32_t)payload[2] << 16)
				   | ((uint32_t)payload[3] << 24);
	// Разбор tz_min (2 байта, little‑endian, знаковое)
	int16_t tz = (int16_t)((uint16_t)payload[4] | ((uint16_t)payload[5] << 8));

	p->metrics.host_unix_sec = sec;
	p->metrics.host_tz_min = tz;

	/* ACK with payload [orig_cmd] */
	return ign_proto_send_ack(p, IGN_CMD_HOST_TIME);
}

/**
 * @brief Записывает 32‑битное значение в буфер в формате little‑endian
 *
 * Заполняет 4 последовательных байта в буфере, начиная с dst[0].
 * @param dst Указатель на начало буфера для записи
 * @param v 32‑битное значение для записи
 */
static void wr_u32_le(uint8_t *dst, uint32_t v)
{
	dst[0] = (uint8_t)(v & 0xFFu);
	dst[1] = (uint8_t)((v >> 8) & 0xFFu);
	dst[2] = (uint8_t)((v >> 16) & 0xFFu);
	dst[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/**
 * @brief Обрабатывает команду GET_METRICS (0x03)
 *
 * Формирует и отправляет 28‑байтовый ответ с текущими метриками протокола.
 * @param p Указатель на контекст протокола IGN
 * @param len Длина принятых полезных данных (должна быть 0)
 * @return IGN_PROTO_OK при успехе, IGN_NACK_BAD_LEN при ненулевой длине
 * @details
 * Формат ответа (28 байт):
 *  0..3   u32 host_unix_sec
 *  4..5   i16 host_tz_min
 *  6..7   u16 reserved (заполняется нулями)
 *  8..11  u32 install_attempts
 * 12..15  u32 install_success
 * 16..19  u32 install_fail
 * 20..23  u32 rx_bytes
 * 24..27  u32 rx_frames_ok
 */
static ign_proto_status_t handle_get_metrics(ign_proto_t *p, uint16_t len)
{
	if (len != 0u) {
		return ign_proto_send_nack(p, IGN_CMD_GET_METRICS, IGN_NACK_BAD_LEN);
	}

	uint8_t out[28];
	memset(out, 0, sizeof(out));

	wr_u32_le(&out[0],  p->metrics.host_unix_sec);

	uint16_t tz = (uint16_t)p->metrics.host_tz_min;
	out[4] = (uint8_t)(tz & 0xFFu);
	out[5] = (uint8_t)((tz >> 8) & 0xFFu);

	wr_u32_le(&out[8],  p->metrics.install_attempts);
	wr_u32_le(&out[12], p->metrics.install_success);
	wr_u32_le(&out[16], p->metrics.install_fail);
	wr_u32_le(&out[20], p->metrics.rx_bytes);
	wr_u32_le(&out[24], p->metrics.rx_frames_ok);

	return ign_proto_tx_frame(p, IGN_CMD_GET_METRICS, out, (uint16_t)sizeof(out));
}

/**
 * @brief Обрабатывает CORE‑команды протокола IGN (0x00–0x1F)
 *
 * Диспетчеризует входящие команды в соответствии с их кодом.
 * @param p Указатель на контекст протокола IGN (не должен быть NULL)
 * @param cmd Код команды для обработки
 * @param payload Указатель на полезные данные команды (может быть NULL, если len=0)
 * @param payload_len Длина полезных данных в байтах
 * @return Статус выполнения:
 *  - IGN_PROTO_OK — команда обработана успешно
 *  - IGN_PROTO_E_ARG — p == NULL
 *  - IGN_PROTO_E_UNSUPPORTED — команда не поддерживается
 *  - Другие коды ошибок — в зависимости от обработчика команды
 */
ign_proto_status_t ign_proto_handle_core_cmd(ign_proto_t *p, uint8_t cmd,
                                             const uint8_t *payload, uint16_t payload_len)
{
    if (!p) {
        return IGN_PROTO_E_ARG;
    }

    switch (cmd) {
        case IGN_CMD_HELLO:
            return handle_hello(p);

        case IGN_CMD_HOST_TIME:
            return handle_host_time(p, payload, payload_len);

        case IGN_CMD_GET_METRICS:
            return handle_get_metrics(p, payload_len);

        default:
            return IGN_PROTO_E_UNSUPPORTED;
    }
}
