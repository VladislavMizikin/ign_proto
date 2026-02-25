/**
 * @file ign_proto_errors.h
 * @author Мизикин Владислав
 * @date 26.02.2026
 * @brief Заголовочный файл с определениями кодов ошибок протокола IGN
 *
 * Содержит перечисления статусов выполнения операций и кодов ошибок для NACK‑ответов.
 */

/* Directive to prevent recursive inclusion ----------------------------------*/

#pragma once

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>

/* Exported types -----------------------------------------------------------*/

/**
 * @brief Статус выполнения операций протокола IGN
 *
 * Перечисление кодов возврата функций протокола. Положительные значения
 * (включая 0) означают успех, отрицательные — различные ошибки.
 */
typedef enum
{
	IGN_PROTO_OK = 0,           ///< Операция выполнена успешно
	IGN_PROTO_E_ARG = -1,    ///< Некорректные входные аргументы
	IGN_PROTO_E_CRC = -2,   ///< Ошибка контрольной суммы (CRC)
	IGN_PROTO_E_FRAME = -3,  ///< Ошибка формата фрейма (неверная структура)
	IGN_PROTO_E_TX = -4,    ///< Ошибка передачи данных
	IGN_PROTO_E_UNSUPPORTED = -5, ///< Команда не поддерживается
	IGN_PROTO_E_INTERNAL = -6  ///< Внутренняя ошибка протокола/устройства
} ign_proto_status_t;

/**
 * @brief Коды ошибок для NACK‑ответов (передаются в payload[0])
 *
 * Используются в отрицательных подтверждениях (NACK) для указания причины
 * отказа в выполнении команды. Значения подобраны так, чтобы:
 * - занимать 1 байт (удобно для передачи);
 * - иметь «читаемые» шестнадцатеричные значения;
 * - разделять категории ошибок (формат, CRC, поддержка и т. д.).
 */
typedef enum
{
	IGN_NACK_BAD_LEN = 0x01,     ///< Некорректная длина полезной нагрузки
	IGN_NACK_BAD_CRC = 0x02,   ///< Ошибка контрольной суммы (CRC не совпала)
	IGN_NACK_BAD_FRAME = 0x03, ///< Ошибка формата фрейма (нарушена структура)
	IGN_NACK_UNSUPPORTED = 0x07, ///< Команда не поддерживается устройством
	IGN_NACK_INTERNAL = 0x0F      ///< Внутренняя ошибка устройства (переполнение буфера, нехватка памяти и т. п.)
} ign_nack_code_t;

/**
 * @brief Вспомогательная функция для преобразования статуса в читаемую строку
 * @param status Код статуса из ign_proto_status_t
 * @return Указатель на строку с описанием ошибки или "Unknown error"
 * @details
 * Может использоваться для логирования и отладки. Пример:
 * ```c
 * printf("Protocol error: %s\n", ign_proto_status_to_str(err));
 * ```
 */
static inline const char* ign_proto_status_to_str(ign_proto_status_t status)
{
	switch (status) {
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

/**
 * @brief Вспомогательная функция для преобразования кода NACK в читаемую строку
 * @param code Код ошибки из ign_nack_code_t
 * @return Указатель на строку с описанием ошибки или "Unknown NACK code"
 * @details
 * Удобна для логирования принятых NACK‑ответов и анализа причин ошибок.
 */
static inline const char* ign_nack_code_to_str(ign_nack_code_t code)
{
	switch (code) {
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
		default:
			return "Unknown NACK code";
	}
}
