/**
 * @file ign_proto.c
 * @author Мизикин Владислав
 * @date 26.02.2026
 * @brief Реализация основного функционала протокола IGN: инициализация, парсинг фреймов, обработка команд
 */

/* Includes ------------------------------------------------------------------*/

#include "ign_proto_internal.h"
#include <string.h>

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Глобальный указатель на контекст протокола
 *
 * Используется для передачи контекста в callback‑функции приёма байтов.
 */
static ign_proto_t *g_ctx = NULL;

/**
 * @brief Внутренняя функция обработки входящих байтов
 * @param data Указатель на массив входящих байтов
 * @param size Количество байтов в массиве
 * @details
 * Функция вызывается через callback, возвращённый ign_proto_init().
 * Обрабатывает байты по одному, обновляет состояние парсера и при
 * завершении фрейма вызывает dispatch_frame().
 */
static void ign_proto_rx_bytes_impl(const uint8_t *data, uint32_t size);

/**
 * @brief Инициализирует контекст протокола IGN
 * @param p Указатель на структуру контекста протокола
 * @param cfg Указатель на конфигурацию протокола (должна содержать валидные функции)
 * @param payload_buf Указатель на буфер для полезных данных
 * @param payload_buf_sz Размер буфера полезных данных в байтах
 * @return Указатель на функцию приёма байтов (ign_proto_rx_cb_t) или NULL при ошибке
 * @details
 * Выполняет следующие действия:
 * 1. Проверяет валидность входных параметров.
 * 2. Обнуляет структуру контекста.
 * 3. Копирует конфигурацию и сохраняет указатель на буфер.
 * 4. Устанавливает начальное состояние парсера (ST_SOF0).
 * 5. Включает обработку CORE‑команд по умолчанию.
 * 6. Сохраняет глобальный указатель на контекст.
 */
ign_proto_rx_cb_t ign_proto_init(ign_proto_t *p,
								 const ign_proto_cfg_t *cfg,
								 uint8_t *payload_buf,
								 uint16_t payload_buf_sz)
{
	// Проверка входных параметров
	if (!p || !cfg || !cfg->tx_bytes || !cfg->crc_init || !cfg->crc_update || !cfg->crc_final || !cfg->dispatch ||
		!payload_buf || payload_buf_sz == 0u)
	{
		return NULL;
	}

	memset(p, 0, sizeof(*p));
	p->cfg = *cfg;
	p->payload_buf = payload_buf;
	p->payload_buf_sz = payload_buf_sz;
	p->st = ST_SOF0;
	p->enable_core_cmds = 1u;

	g_ctx = p;
	return ign_proto_rx_bytes_impl;
}

ign_proto_t *ign_proto_get_ctx(void)
{
	return g_ctx;
}

/**
 * @brief Включает/отключает обработку базовых (CORE) команд протокола
 * @param p Указатель на контекст протокола
 * @param enable Флаг включения (ненулевое значение — включить, 0 — отключить)
 * @details
 * Если обработка CORE‑команд отключена, все команды из диапазона 0x00–0x1F
 * будут передаваться в пользовательский диспетчер (cfg.dispatch).
 */
void ign_proto_enable_core(ign_proto_t *p, int enable)
{
	if (!p)
	{
		return;
	}
	p->enable_core_cmds = enable ? 1u : 0u;
}

/**
 * @brief Получает указатель на структуру метрик протокола
 * @param p Указатель на контекст протокола
 * @return Указатель на ign_proto_metrics_t или NULL, если p == NULL
 */
const ign_proto_metrics_t *ign_proto_metrics_get(const ign_proto_t *p)
{
	return p ? &p->metrics : NULL;
}

/**
 * @brief Увеличивает счётчик попыток установки соединения
 * @param p Указатель на контекст протокола
 */
void ign_proto_metrics_install_attempt(ign_proto_t *p)
{
	if (!p)
	{
		return;
	}
	p->metrics.install_attempts++;
}

/**
 * @brief Фиксирует результат установки соединения (успех/ошибка)
 * @param p Указатель на контекст протокола
 * @param success Флаг успеха (ненулевое значение — успех, 0 — ошибка)
 */
void ign_proto_metrics_install_result(ign_proto_t *p, int success)
{
	if (!p)
	{
		return;
	}
	if (success)
	{
		p->metrics.install_success++;
	}
	else
	{
		p->metrics.install_fail++;
	}
}

/**
 * @brief Сбрасывает состояние парсера к начальному
 * @param p Указатель на контекст протокола
 * @details
 * Устанавливает:
 * - состояние ST_SOF0;
 * - длину полезной нагрузки 0;
 * - позицию в буфере 0;
 * - код команды 0;
 * - позицию CRC 0.
 */
static void parser_reset(ign_proto_t *p)
{
	p->st = ST_SOF0;
	p->payload_len = 0u;
	p->payload_pos = 0u;
	p->cmd = 0u;
	p->crc_pos = 0u;
}

/**
 * @brief Обрабатывает завершённый фрейм (проверяет CRC, вызывает диспетчер команд)
 * @param p Указатель на контекст протокола
 * @details
 * Алгоритм:
 * 1. Восстанавливает принятый CRC32 из little‑endian.
 * 2. Сравнивает с рассчитанным CRC.
 * 3. При совпадении увеличивает счётчик корректных фреймов и вызывает диспетчер.
 * 4. При несовпадении отправляет NACK и увеличивает счётчик ошибок CRC.
 */
static void dispatch_frame(ign_proto_t *p)
{
	// Восстановление принятого CRC32 из little‑endian
	uint32_t crc_rx = (uint32_t) p->crc_bytes[0]
					  | ((uint32_t) p->crc_bytes[1] << 8)
					  | ((uint32_t) p->crc_bytes[2] << 16)
					  | ((uint32_t) p->crc_bytes[3] << 24);

	// CRC накапливается в потоковом режиме во время парсинга (LEN + CMD + PAYLOAD)
	uint32_t crc_calc = p->cfg.crc_final();
	if (crc_calc != crc_rx)
	{
		p->metrics.rx_frames_crc_err++;
		ign_proto_send_nack(p, p->cmd, IGN_NACK_BAD_CRC);
		return;
	}

	p->metrics.rx_frames_ok++;

	ign_proto_status_t st = IGN_PROTO_E_UNSUPPORTED;

	if (p->enable_core_cmds)
	{
		st = ign_proto_handle_core_cmd(p, p->cmd, p->payload_buf, p->payload_len);
	}
	if (st == IGN_PROTO_E_UNSUPPORTED)
	{
		st = p->cfg.dispatch(p->cmd, p->payload_buf, p->payload_len);
	}
	if (st == IGN_PROTO_E_UNSUPPORTED)
	{
		ign_proto_send_nack(p, p->cmd, IGN_NACK_UNSUPPORTED);
	}
}

/**
 * @brief Основная функция парсинга входящих байтов
 * @param data Указатель на массив входящих байтов
 * @param size Количество байтов в массиве
 * @details
 * Обрабатывает каждый байт в соответствии с текущим состоянием парсера (p->st):
 * - ST_SOF0: ожидание SOF0 (0xA5)
 * - ST_SOF1: ожидание SOF1 (0x5A), переход к ST_LEN0
 * - ST_LEN0/ST_LEN1: разбор длины полезной нагрузки (little‑endian)
 * - ST_CMD: сохранение кода команды, проверка размера буфера
 * - ST_PAYLOAD: запись данных в буфер полезной нагрузки, обновление CRC
 * - ST_CRC: приём байтов CRC32, завершение фрейма и его обработка
 * При ошибке или неизвестном состоянии парсер сбрасывается.
 */
static void ign_proto_rx_bytes_impl(const uint8_t *data, uint32_t size)
{
	ign_proto_t *p = g_ctx;
	if (!p || !data || size == 0u)
	{
		return;
	}

	p->metrics.rx_bytes += size;

	for (uint32_t i = 0; i < size; i++)
	{
		uint8_t b = data[i];

		switch ((ign_proto_state_t) p->st)
		{
			case ST_SOF0:
				if (b == IGN_SOF0)
				{
					p->st = ST_SOF1;
				}
				break;

			case ST_SOF1:
				if (b == IGN_SOF1)
				{
					p->st = ST_LEN0;
					// Начало накопления CRC с байта длины (CRC покрывает LEN+CMD+PAYLOAD)
					p->cfg.crc_init();
				}
				else
				{
					// Неверная сигнатура — сброс парсера
					p->st = ST_SOF0;
				}
				break;

			case ST_LEN0:
				p->payload_len = (uint16_t) b;
				p->cfg.crc_update(&b, 1u);
				p->st = ST_LEN1;
				break;

			case ST_LEN1:
				p->payload_len |= ((uint16_t) b << 8);
				p->cfg.crc_update(&b, 1u);
				p->payload_pos = 0u;
				p->st = ST_CMD;
				break;

			case ST_CMD:
				p->cmd = b;
				p->cfg.crc_update(&b, 1u);
				// Проверка, помещается ли полезная нагрузка в буфер
				if (p->payload_len > p->payload_buf_sz)
				{
					ign_proto_send_nack(p, p->cmd, IGN_NACK_BAD_LEN);
					parser_reset(p);
					break;
				}
				// Переход к следующему состоянию: если длина 0, сразу к CRC, иначе к PAYLOAD
				p->st = (p->payload_len == 0u) ? ST_CRC : ST_PAYLOAD;
				break;

			case ST_PAYLOAD:
				// Буфер уже проверен в ST_CMD, тут просто пишем данные
				p->payload_buf[p->payload_pos++] = b;
				p->cfg.crc_update(&b, 1u);

				if (p->payload_pos >= p->payload_len)
				{
					p->crc_pos = 0u;
					p->st = ST_CRC;
				}
				break;

			case ST_CRC:
				if (p->crc_pos < 4u)
				{
					p->crc_bytes[p->crc_pos++] = b;
				}

				if (p->crc_pos >= 4u)
				{
					// Завершение фрейма: проверка CRC и диспетчеризация
					dispatch_frame(p);
					// После обработки — сброс парсера для приёма следующего фрейма
					parser_reset(p);
				}
				break;

			default:
				// Неизвестное состояние — сброс парсера к начальному
				parser_reset(p);
				break;
		}
	}
}