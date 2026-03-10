/**
 * @file ign_proto_internal.h
 * @author Мизикин Владислав
 * @date 26.02.2026
 * @brief Внутренние определения и прототипы функций протокола IGN
 *
 * Заголовочный файл содержит внутренние константы, типы и прототипы функций,
 * используемые внутри реализации протокола. Не предназначен для включения
 * в пользовательском коде.
 */

/* Directive to prevent recursive inclusion ----------------------------------*/

#pragma once

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include "ign_proto.h"

/* Exported defines ----------------------------------------------------------*/

/**
 * @defgroup IGN_SOF_SIGNATURES Сигнатуры начала фрейма (SOF)
 * @brief Байты начала фрейма протокола IGN (little‑endian: 0xA5 0x5A)
 * @{
 */
#define IGN_SOF0 0xA5u  ///< Первый байт сигнатуры SOF
#define IGN_SOF1 0x5Au  ///< Второй байт сигнатуры SOF
/** @} */

/**
 * @defgroup IGN_PROTO_CONSTANTS Прочие константы протокола
 * @brief Константы, используемые внутри реализации протокола
 * @{
 */
#define IGN_CRC_SIZE 4u  ///< Размер CRC32 в байтах
#define IGN_MAX_PAYLOAD_LEN 0xFFFFu  ///< Максимальная длина полезной нагрузки (65 535 байт)
/** @} */

/* Exported types -----------------------------------------------------------*/

/**
 * @brief Состояния парсера протокола IGN
 *
 * Определяет текущее состояние конечного автомата парсера при приёме фрейма.
 * Каждое состояние соответствует обработке определённого поля фрейма.
 */
typedef enum
{
	ST_SOF0 = 0,    ///< Ожидание первого байта сигнатуры SOF (0xA5)
	ST_SOF1,     ///< Ожидание второго байта сигнатуры SOF (0x5A)
	ST_LEN0,    ///< Приём младшего байта длины полезной нагрузки
	ST_LEN1,    ///< Приём старшего байта длины полезной нагрузки (little‑endian)
	ST_CMD,     ///< Приём кода команды
	ST_PAYLOAD,  ///< Приём полезной нагрузки (данных)
	ST_CRC       ///< Приём байтов CRC32 (4 байта)
} ign_proto_state_t;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief Отправляет фрейм протокола IGN
 * @param p Указатель на контекст протокола
 * @param cmd Код команды для отправки
 * @param payload Указатель на полезные данные (может быть NULL, если payload_len == 0)
 * @param payload_len Длина полезных данных в байтах
 * @return Статус выполнения (IGN_PROTO_OK при успехе)
 * @details
 * Формирует полный фрейм по протоколу:
 * - SOF (2 байта: 0xA5, 0x5A);
 * - длина полезной нагрузки (2 байта, little‑endian);
 * - код команды (1 байт);
 * - полезная нагрузка (0–65 535 байт);
 * - CRC32 (4 байта).
 * Затем передаёт фрейм через функцию tx_bytes из конфигурации.
 */
ign_proto_status_t ign_proto_tx_frame(ign_proto_t *p, uint8_t cmd,
									  const uint8_t *payload, uint16_t payload_len);

/**
 * @brief Обрабатывает базовые команды протокола (диапазон 0x00–0x1F)
 * @param p Указатель на контекст протокола
 * @param cmd Код принятой команды
 * @param payload Указатель на полезные данные
 * @param payload_len Длина полезных данных в байтах
 * @return Статус выполнения (IGN_PROTO_OK при успехе или код ошибки)
 * @details
 * Выполняет действия для встроенных команд протокола:
 * - IGN_CMD_HELLO: отправляет ACK;
 * - IGN_CMD_HOST_TIME: обновляет время хоста в метриках;
 * - IGN_CMD_GET_METRICS: отправляет метрики протокола.
 * Если обработка базовых команд отключена (enable_core_cmds == 0),
 * возвращает IGN_PROTO_E_UNSUPPORTED.
 */
ign_proto_status_t ign_proto_handle_core_cmd(ign_proto_t *p, uint8_t cmd,
											 const uint8_t *payload, uint16_t payload_len);

/**
 * @brief Возвращает текущий глобальный контекст протокола (g_ctx).
 * @return Указатель на активный ign_proto_t или NULL.
 * @note Используется для интеграции boot/app диспетчеров, где сигнатура
 *       callback не содержит указатель на контекст.
 */
ign_proto_t *ign_proto_get_ctx(void);

/* Internal helpers are defined as `static` inside implementation units. */
