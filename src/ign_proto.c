/**
 * @file ign_proto.c
 * @author Мизикин Владислав
 * @date 26.02.2026
 * @brief Реализация протокола IGN.
 *
 * Содержит реализацию инициализации, приёма и парсинга входящих данных,
 * обработки фреймов и диспетчеризации команд.
 */

/* Includes ------------------------------------------------------------------*/

#include "ign_proto_internal.h"
#include <string.h>

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Глобальный указатель на контекст протокола
 *
 * Используется для доступа к текущему контексту из callback-функции.
 *
 * @warning Не потокобезопасно. Поддерживается только один экземпляр протокола.
 */
static ign_proto_t *g_ctx = NULL;

/**
 * @brief Внутренняя функция приёма байтов
 */
static void ign_proto_rx_bytes_impl(const uint8_t *data, uint32_t size);

/**
 * @brief Инициализация контекста протокола IGN
 */
ign_proto_rx_cb_t ign_proto_init(ign_proto_t *p,
								 const ign_proto_cfg_t *cfg,
								 uint8_t *payload_buf,
								 uint16_t payload_buf_sz)
{
	// Проверяем все критичные зависимости сразу
	if (!p || !cfg || !cfg->tx_bytes || !cfg->crc_init || !cfg->crc_update || !cfg->crc_final || !cfg->dispatch ||
		!payload_buf || payload_buf_sz == 0u)
	{
		return NULL;
	}

	// Полная очистка контекста — гарантируем отсутствие мусора
	memset(p, 0, sizeof(*p));

	// Копируем конфигурацию (функциональные указатели)
	p->cfg = *cfg;

	// Назначаем буфер для payload
	p->payload_buf = payload_buf;
	p->payload_buf_sz = payload_buf_sz;

	// Парсер стартует с ожидания сигнатуры начала фрейма
	p->st = ST_SOF0;

	// По умолчанию разрешаем обработку CORE-команд
	p->enable_core_cmds = 1u;

	// Сохраняем глобальный контекст (используется в callback)
	g_ctx = p;

	// Возвращаем функцию, в которую пользователь будет "скармливать" байты
	return ign_proto_rx_bytes_impl;
}

/**
 * @brief Получение текущего контекста
 */
ign_proto_t *ign_proto_get_ctx(void)
{
	return g_ctx;
}

/**
 * @brief Включение/отключение CORE-команд
 */
void ign_proto_enable_core(ign_proto_t *p, int enable)
{
	if (!p)
	{
		return;
	}

	// Приводим к строгому 0/1
	p->enable_core_cmds = enable ? 1u : 0u;
}

/**
 * @brief Получение метрик
 */
const ign_proto_metrics_t *ign_proto_metrics_get(const ign_proto_t *p)
{
	return p ? &p->metrics : NULL;
}

void ign_proto_metrics_install_attempt(ign_proto_t *p)
{
	if (!p)
	{
		return;
	}

	p->metrics.install_attempts++;
}

void ign_proto_metrics_install_result(ign_proto_t *p, int success)
{
	if (!p)
	{
		return;
	}

	// Разносим результат по разным счётчикам
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
 * @brief Сброс парсера
 */
static void parser_reset(ign_proto_t *p)
{
	// Возврат в начальное состояние автомата
	p->st = ST_SOF0;

	// Сбрасываем накопленные данные текущего фрейма
	p->payload_len = 0u;
	p->payload_pos = 0u;
	p->cmd = 0u;
	p->request_id = 0u;

	// Подготовка к приёму CRC заново
	p->crc_pos = 0u;
}

/**
 * @brief Обработка готового фрейма
 */
static void dispatch_frame(ign_proto_t *p)
{
	// Собираем CRC из 4 байтов (little-endian)
	uint32_t crc_rx = (uint32_t) p->crc_bytes[0]
					  | ((uint32_t) p->crc_bytes[1] << 8)
					  | ((uint32_t) p->crc_bytes[2] << 16)
					  | ((uint32_t) p->crc_bytes[3] << 24);

	// Получаем рассчитанный CRC (накопленный во время парсинга)
	uint32_t crc_calc = p->cfg.crc_final();

	// Проверка целостности фрейма
	if (crc_calc != crc_rx)
	{
		p->metrics.rx_frames_crc_err++;

#ifdef IGN_PROTO_HOST_PASSIVE_RX
		// В пассивном режиме просто игнорируем ошибку
		return;
#else
		// В активном режиме уведомляем отправителя
		ign_proto_send_nack(p, p->cmd, IGN_NACK_BAD_CRC, p->request_id);
		return;
#endif
	}

	// Фрейм валидный
	p->metrics.rx_frames_ok++;

	ign_proto_status_t st = IGN_PROTO_E_UNSUPPORTED;

	// Сначала даём шанс встроенным (CORE) командам
	if (p->enable_core_cmds)
	{
		st = ign_proto_handle_core_cmd(p, p->cmd, p->request_id, p->payload_buf, p->payload_len);
	}

	// Если не обработано — передаём пользователю
	if (st == IGN_PROTO_E_UNSUPPORTED)
	{
		st = p->cfg.dispatch(p->cmd, p->request_id, p->payload_buf, p->payload_len);
	}

	// Если никто не обработал — сигнализируем ошибку
	if (st == IGN_PROTO_E_UNSUPPORTED)
	{
#ifndef IGN_PROTO_HOST_PASSIVE_RX
		ign_proto_send_nack(p, p->cmd, IGN_NACK_UNSUPPORTED, p->request_id);
#endif
	}
}

/**
 * @brief Парсинг входящего потока байтов
 */
static void ign_proto_rx_bytes_impl(const uint8_t *data, uint32_t size)
{
	ign_proto_t *p = g_ctx;

	// Базовая защита от некорректных вызовов
	if (!p || !data || size == 0u)
	{
		return;
	}

	// Учитываем общий трафик
	p->metrics.rx_bytes += size;

	for (uint32_t i = 0; i < size; i++)
	{
		uint8_t b = data[i];

		// Конечный автомат парсинга
		switch ((ign_proto_state_t) p->st)
		{
			case ST_SOF0:
				// Ждём первый байт сигнатуры
				if (b == IGN_SOF0)
				{
					p->st = ST_SOF1;
				}
				break;

			case ST_SOF1:
				// Ждём второй байт сигнатуры
				if (b == IGN_SOF1)
				{
					p->st = ST_LEN0;

					// Начинаем новый расчёт CRC (с поля длины)
					p->cfg.crc_init();
				}
				else
				{
					// Ложное срабатывание — начинаем заново
					p->st = ST_SOF0;
				}
				break;

			case ST_LEN0:
				// Младший байт длины
				p->payload_len = (uint16_t) b;
				p->cfg.crc_update(&b, 1u);
				p->st = ST_LEN1;
				break;

			case ST_LEN1:
				// Старший байт длины
				p->payload_len |= ((uint16_t) b << 8);
				p->cfg.crc_update(&b, 1u);

				// Готовимся принимать payload
				p->payload_pos = 0u;
				p->st = ST_REQ0;
				break;

			case ST_REQ0:
				p->request_id = (uint16_t) b;
				p->cfg.crc_update(&b, 1u);
				p->st = ST_REQ1;
				break;

			case ST_REQ1:
				p->request_id |= ((uint16_t) b << 8);
				p->cfg.crc_update(&b, 1u);
				p->st = ST_CMD;
				break;

			case ST_CMD:
				p->cmd = b;
				p->cfg.crc_update(&b, 1u);

				// Проверка на переполнение буфера
				if (p->payload_len > p->payload_buf_sz)
				{
#ifndef IGN_PROTO_HOST_PASSIVE_RX
					ign_proto_send_nack(p, p->cmd, IGN_NACK_BAD_LEN, p->request_id);
#endif
					parser_reset(p);
					break;
				}

				// Если payload пустой — сразу идём к CRC
				p->st = (p->payload_len == 0u) ? ST_CRC : ST_PAYLOAD;
				break;

			case ST_PAYLOAD:
				// Пишем данные в буфер
				p->payload_buf[p->payload_pos++] = b;
				p->cfg.crc_update(&b, 1u);

				// Если всё получили — переходим к CRC
				if (p->payload_pos >= p->payload_len)
				{
					p->crc_pos = 0u;
					p->st = ST_CRC;
				}
				break;

			case ST_CRC:
				// Принимаем CRC по байтам
				if (p->crc_pos < 4u)
				{
					p->crc_bytes[p->crc_pos++] = b;
				}

				// CRC полностью получен — завершаем фрейм
				if (p->crc_pos >= 4u)
				{
					dispatch_frame(p);

					// Готовимся к следующему фрейму
					parser_reset(p);
				}
				break;

			default:
				// Любое неизвестное состояние — полный сброс автомата
				parser_reset(p);
				break;
		}
	}
}