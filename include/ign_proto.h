/**
 * @file ign_proto.h
 * @author Мизикин Владислав
 * @date 26.02.2026
 * @brief Заголовочный файл протокола IGN: определения типов, структур и прототипов функций
 */

/* Directive to prevent recursive inclusion ----------------------------------*/

#pragma once

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <stddef.h>
#include "ign_proto_errors.h"
#include "ign_proto_cmdspace.h"

/* Exported types ----------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Структура метрик протокола IGN
 *
 * Содержит статистические данные о работе протокола.
 */
typedef struct
{
	uint32_t host_unix_sec;      ///< Unix‑время хоста (секунды)
	int16_t host_tz_min;       ///< Часовой пояс хоста (минуты относительно UTC)

	uint32_t install_attempts;  ///< Количество попыток установки соединения
	uint32_t install_success;  ///< Количество успешных установок соединения
	uint32_t install_fail;     ///< Количество неудачных установок соединения

	uint32_t rx_bytes;         ///< Общее количество принятых байтов
	uint32_t rx_frames_ok;     ///< Количество корректно принятых фреймов
	uint32_t rx_frames_crc_err; ///< Количество фреймов с ошибкой CRC
} ign_proto_metrics_t;

/**
 * @brief Тип функции для передачи байтов
 * @param data Указатель на буфер с данными для передачи
 * @param len Количество байтов для передачи
 * @return 0 при успехе, ненулевое значение при ошибке
 */
typedef int (*ign_proto_tx_bytes_fn)(const uint8_t *data, uint32_t len);

/**
 * @brief Тип функции инициализации/сброса CRC
 */
typedef void (*ign_proto_crc_init_fn)(void);

/**
 * @brief Тип функции обновления CRC с новыми данными
 * @param data Указатель на данные для расчёта CRC
 * @param len Длина данных в байтах
 */
typedef void (*ign_proto_crc_update_fn)(const void *data, uint32_t len);

/**
 * @brief Тип функции завершения расчёта CRC (получение итогового значения)
 * @return Итоговое значение CRC32
 */
typedef uint32_t (*ign_proto_crc_final_fn)(void);

/**
 * @brief Тип функции диспетчеризации команд
 * @param cmd Код команды
 * @param payload Указатель на полезные данные (может быть NULL, если payload_len == 0)
 * @param payload_len Длина полезных данных в байтах
 * @return Статус выполнения (IGN_PROTO_OK при успехе)
 */
typedef ign_proto_status_t (*ign_proto_cmd_dispatch_fn)(
		uint8_t cmd, const uint8_t *payload, uint16_t payload_len);

/**
 * @brief Конфигурация протокола IGN
 */
typedef struct
{
	ign_proto_tx_bytes_fn tx_bytes;   ///< Функция передачи байтов
	ign_proto_crc_init_fn crc_init;   ///< Функция инициализации/сброса расчёта CRC
	ign_proto_crc_update_fn crc_update; ///< Функция обновления CRC с новыми данными
	ign_proto_crc_final_fn crc_final;  ///< Функция завершения расчёта CRC и получения итогового значения
	ign_proto_cmd_dispatch_fn dispatch; ///< Функция диспетчеризации команд
} ign_proto_cfg_t;

/**
 * @brief Тип callback‑функции для приёма байтов
 * @param data Указатель на массив входящих байтов
 * @param size Количество байтов в массиве
 */
typedef void (*ign_proto_rx_cb_t)(const uint8_t *data, uint32_t size);

/**
 * @brief Основной контекст протокола IGN
 */
typedef struct ign_proto
{
	ign_proto_cfg_t cfg;           ///< Конфигурация протокола

	uint8_t *payload_buf;          ///< Буфер для полезных данных
	uint16_t payload_buf_sz;       ///< Размер буфера полезных данных

	/* parsing state */
	uint8_t st;                  ///< Текущее состояние парсера (ST_SOF0, ST_SOF1 и т.д.)
	uint16_t payload_len;         ///< Ожидаемая длина полезной нагрузки
	uint16_t payload_pos;        ///< Текущая позиция в буфере полезной нагрузки
	uint8_t cmd;               ///< Код принятой команды

	uint8_t crc_bytes[4];       ///< Принятые байты CRC32
	uint8_t crc_pos;           ///< Текущая позиция в массиве CRC

	ign_proto_metrics_t metrics;  ///< Метрики протокола

	uint8_t enable_core_cmds;  ///< Флаг включения обработки базовых команд (1 — включено, 0 — выключено)
} ign_proto_t;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief Инициализирует контекст протокола IGN
 * @param p Указатель на структуру контекста протокола
 * @param cfg Указатель на конфигурацию протокола
 * @param payload_buf Указатель на буфер для полезных данных
 * @param payload_buf_sz Размер буфера полезных данных в байтах
 * @return Указатель на функцию приёма байтов (ign_proto_rx_cb_t) или NULL при ошибке
 * @details
 * Инициализирует структуру протокола, устанавливает обработчик приёма байтов.
 * После вызова этой функции можно передавать входящие данные через возвращённый
 * callback.
 */
ign_proto_rx_cb_t ign_proto_init(ign_proto_t *p,
								 const ign_proto_cfg_t *cfg,
								 uint8_t *payload_buf,
								 uint16_t payload_buf_sz);

/**
 * @brief Получает метрики протокола
 * @param p Указатель на контекст протокола
 * @return Указатель на структуру метрик или NULL, если p == NULL
 */
const ign_proto_metrics_t *ign_proto_metrics_get(const ign_proto_t *p);

/**
 * @brief Увеличивает счётчик попыток установки соединения
 * @param p Указатель на контекст протокола
 */
void ign_proto_metrics_install_attempt(ign_proto_t *p);

/**
 * @brief Фиксирует результат установки соединения (успех/ошибка)
 * @param p Указатель на контекст протокола
 * @param success Флаг успеха (ненулевое значение — успех, 0 — ошибка)
 */
void ign_proto_metrics_install_result(ign_proto_t *p, int success);

/**
 * @brief Отправляет подтверждение (ACK) для указанной команды
 * @param p Указатель на контекст протокола
 * @param orig_cmd Исходный код команды, для которой отправляется подтверждение
 * @return Статус выполнения (IGN_PROTO_OK при успехе)
 */
ign_proto_status_t ign_proto_send_ack(ign_proto_t *p, uint8_t orig_cmd);

/**
 * @brief Отправляет отрицательное подтверждение (NACK) с кодом ошибки
 * @param p Указатель на контекст протокола
 * @param orig_cmd Исходный код команды, вызвавшей ошибку
 * @param nack_code Код ошибки (определяется протоколом)
 * @return Статус выполнения (IGN_PROTO_OK при успехе)
 */
ign_proto_status_t ign_proto_send_nack(ign_proto_t *p, uint8_t orig_cmd, uint8_t nack_code);

/**
 * @brief Включает/отключает обработку базовых команд протокола
 * @param p Указатель на контекст протокола
 * @param enable Флаг включения (ненулевое значение — включить, 0 — отключить)
 */
void ign_proto_enable_core(ign_proto_t *p, int enable);

#ifdef __cplusplus
}
#endif
