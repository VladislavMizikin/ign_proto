#include "ign_proto.h"
#include <stdio.h>

/* Dummy streaming CRC for tests: s = (s*33) ^ byte */
static uint32_t g_crc = 0;
static void dummy_crc_init(void) { g_crc = 0; }
static void dummy_crc_update(const void *data, uint32_t len)
{
	const unsigned char *p = (const unsigned char *)data;
	for (uint32_t i = 0; i < len; i++) {
		g_crc = (g_crc * 33u) ^ p[i];
	}
}
static uint32_t dummy_crc_final(void) { return g_crc; }

/**
 * @brief Dummy‑функция передачи байтов для тестирования
 * @param data Указатель на буфер с данными для передачи
 * @param len Количество байтов для передачи
 * @return Всегда 0 (успех)
 * @details
 * Заглушка, имитирующая успешную передачу данных. В реальной реализации
 * здесь был бы вызов драйвера UART/SPI и т. д.
 */
static int dummy_tx(const uint8_t *data, uint32_t len)
{
	(void)data;
	(void)len;
	return 0;
}

/**
 * @brief Обработчик команд для тестовой сборки
 * @param cmd Код команды
 * @param request_id Идентификатор транзакции
 * @param payload Указатель на полезные данные
 * @param payload_len Длина полезных данных в байтах
 * @return IGN_PROTO_E_UNSUPPORTED (все команды не поддерживаются)
 * @details
 * В тестах используется заглушка, которая не обрабатывает никакие команды.
 * Позволяет проверить инициализацию и базовую работу парсера.
 */
ign_proto_status_t ign_proto_cmd_dispatch(uint8_t cmd, uint16_t request_id, const uint8_t *payload, uint16_t payload_len)
{
	(void)cmd;
	(void)payload;
	(void)request_id;
	(void)payload_len;
	return IGN_PROTO_E_UNSUPPORTED;
}

int main(void)
{
	printf("=== IGN Protocol Test Suite ===\n");

	// Контекст протокола
	ign_proto_t p;
	// Буфер для полезных данных (256 байт)
	uint8_t payload_buf[256];

	// Конфигурация протокола с заглушками
	ign_proto_cfg_t cfg = {
			.tx_bytes = dummy_tx,           // Функция передачи байтов
			.crc_init = dummy_crc_init,
			.crc_update = dummy_crc_update,
			.crc_final = dummy_crc_final,
			.dispatch = ign_proto_cmd_dispatch  // Диспетчер команд
	};

	// Инициализация протокола
	printf("[1] Initializing protocol...\n");
	ign_proto_rx_cb_t cb = ign_proto_init(&p, &cfg, payload_buf, sizeof(payload_buf));
	if (!cb) {
		printf("ERROR: init failed\n");
		return 1;
	}
	printf("SUCCESS: protocol initialized\n");

	// Проверка метрик после инициализации
	const ign_proto_metrics_t *metrics = ign_proto_metrics_get(&p);
	if (metrics) {
		printf("[2] Metrics after init:\n");
		printf("    rx_bytes: %lu\n", (unsigned long)metrics->rx_bytes);
		printf("    rx_frames_ok: %lu\n", (unsigned long)metrics->rx_frames_ok);
		printf("    install_attempts: %lu\n", (unsigned long)metrics->install_attempts);
	}

	// Тестовый сценарий: отправка ACK
	printf("[3] Testing ACK transmission...\n");
	ign_proto_status_t status = ign_proto_send_ack(&p, IGN_CMD_HELLO, 1u);
	if (status == IGN_PROTO_OK) {
		printf("SUCCESS: ACK sent for CMD_HELLO\n");
	} else {
		printf("ERROR: failed to send ACK (status: %s)\n",
			   ign_proto_status_to_str(status));
		return 2;
	}

	// Тестовый сценарий: отправка NACK
	printf("[4] Testing NACK transmission...\n");
	status = ign_proto_send_nack(&p, IGN_CMD_BOOT_MIN, IGN_NACK_BAD_LEN, 2u);
	if (status == IGN_PROTO_OK) {
		printf("SUCCESS: NACK sent for CMD_BOOT_MIN (code: BAD_LEN)\n");
	} else {
		printf("ERROR: failed to send NACK (status: %s)\n",
			   ign_proto_status_to_str(status));
		return 3;
	}

	// Тестовый сценарий: имитация приёма байтов
	printf("[5] Testing byte reception...\n");
	// Последовательность байтов: SOF0, SOF1, LEN=0, REQ=1, CMD=0x01 (HELLO), CRC×4
	uint8_t test_data[] = {0xA5, 0x5A, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
	cb(test_data, sizeof(test_data));

	// Проверка обновления метрик после приёма данных
	metrics = ign_proto_metrics_get(&p);
	if (metrics) {
		printf("[6] Metrics after test reception:\n");
		printf("    rx_bytes: %lu\n", (unsigned long)metrics->rx_bytes);
		printf("    rx_frames_ok: %lu\n", (unsigned long)metrics->rx_frames_ok);
		printf("    rx_frames_crc_err: %lu\n", (unsigned long)metrics->rx_frames_crc_err);
	}

	printf("\n=== Test Suite Completed Successfully ===\n");
	return 0;
}
