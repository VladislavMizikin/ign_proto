/**
 * @file ign_proto_cmdspace.h
 * @author Мизикин Владислав
 * @date 16.06.2026
 * @brief Пространство команд протокола IGN.
 *
 * Содержит диапазоны кодов команд и определения конкретных команд,
 * сгруппированных по функциональному назначению.
 */

/* Directive to prevent recursive inclusion ----------------------------------*/

#pragma once

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/

/**
 * @defgroup IGN_CMD_CORE Core commands (0x00–0x1F)
 * @brief Базовые команды протокола
 *
 * Предоставляют фундаментальную функциональность:
 * инициализацию соединения, синхронизацию и диагностику.
 * @{
 */
#define IGN_CMD_CORE_MIN     0x00u  ///< Нижняя граница диапазона базовых команд
#define IGN_CMD_CORE_MAX     0x1Fu  ///< Верхняя граница диапазона базовых команд
/** @} */

/**
 * @defgroup IGN_CMD_BOOT Bootloader commands (0x20–0x4F)
 * @brief Команды загрузчика (bootloader)
 *
 * Используются для обновления прошивки и низкоуровневого управления устройством.
 * @{
 */
#define IGN_CMD_BOOT_MIN     0x20u  ///< Нижняя граница диапазона команд загрузчика
#define IGN_CMD_BOOT_MAX     0x4Fu  ///< Верхняя граница диапазона команд загрузчика
/** @} */

/**
 * @defgroup IGN_CMD_APP Application commands (0x50–0x7D)
 * @brief Команды уровня приложения
 *
 * Определяются конкретной реализацией и используются для бизнес-логики устройства.
 * @{
 */
#define IGN_CMD_APP_MIN      0x50u  ///< Нижняя граница диапазона команд приложения
#define IGN_CMD_APP_MAX      0x7Du  ///< Верхняя граница диапазона команд приложения
/** @} */

/**
 * @defgroup IGN_CMD_APP_CONFIG Configuration session commands
 * @brief Команды конфигурационной сессии (stage-based)
 *
 * Используются для управления конфигурацией через staging-механизм:
 * изменения сначала записываются во временную область, а затем применяются.
 * @{
 */
#define IGN_CMD_CFG_BEGIN       0x50u  ///< Открыть сессию редактирования конфигурации
#define IGN_CMD_CFG_STATUS      0x51u  ///< Получить состояние текущей конфигурационной сессии
#define IGN_CMD_CFG_READ        0x52u  ///< Прочитать объект конфигурации по идентификатору
#define IGN_CMD_CFG_WRITE       0x53u  ///< Записать объект конфигурации в staging-область
#define IGN_CMD_CFG_COMMIT      0x54u  ///< Применить изменения (staged → active, только RAM)
#define IGN_CMD_CFG_CANCEL      0x55u  ///< Отменить изменения и закрыть сессию
#define IGN_CMD_CFG_LIST        0x56u  ///< Получить список объектов конфигурации и их метаданные
#define IGN_CMD_CFG_RESET_DEFAULTS 0x6Du  ///< Сбросить persistent-конфигурацию к значениям по умолчанию
/** @} */

/**
 * @defgroup IGN_CMD_APP_VALUES Runtime telemetry commands
 * @brief Команды текущих runtime-значений и истории событий APP.
 * @{
 */
#define IGN_CMD_APP_VALUES_LIST      0x57u  ///< Получить страницу каталога runtime-значений
#define IGN_CMD_APP_VALUES_SNAPSHOT  0x58u  ///< Получить страницу latest-state значений
#define IGN_CMD_APP_VALUES_GET_DELTA 0x59u  ///< Получить накопленные изменения runtime-значений и событий
/** @} */


/**
 * @defgroup IGN_CMD_APP_ADAPT Команды адаптаций
 * @brief Команды сервисного контура адаптаций.
 *
 * Используются отдельно от CONFIG_*: адаптации имеют собственную сессию,
 * staged-значения, проверку, применение, сохранение и операции сброса.
 * @{
 */
#define IGN_CMD_ADAPT_OPEN             0x5Au  ///< Открыть сервисную сессию адаптаций
#define IGN_CMD_ADAPT_STATUS           0x5Bu  ///< Получить состояние сервиса адаптаций
#define IGN_CMD_ADAPT_LIST             0x5Cu  ///< Получить страницу каталога адаптационных объектов
#define IGN_CMD_ADAPT_READ             0x5Du  ///< Прочитать значение адаптационного объекта
#define IGN_CMD_ADAPT_VALIDATE         0x5Eu  ///< Проверить staged-значение адаптации
#define IGN_CMD_ADAPT_APPLY            0x5Fu  ///< Применить staged-значение адаптации в runtime-состояние
#define IGN_CMD_ADAPT_COMMIT           0x60u  ///< Сохранить применённые адаптации в энергонезависимое состояние
#define IGN_CMD_ADAPT_RESET_SELECTED   0x61u  ///< Сбросить выбранный адаптационный объект
#define IGN_CMD_ADAPT_RESET_GROUP      0x62u  ///< Сбросить группу адаптационных объектов
#define IGN_CMD_ADAPT_CLOSE            0x63u  ///< Закрыть сервисную сессию адаптаций
/** @} */

/**
 * @defgroup IGN_CMD_APP_DIAG Команды диагностики
 * @brief Команды сервисного контура диагностики.
 *
 * Используются для чтения состояния, ошибок, событий и запуска безопасных
 * диагностических тестов без превращения диагностики в редактор параметров.
 * @{
 */
#define IGN_CMD_DIAG_STATUS            0x64u  ///< Получить краткую сводку диагностики
#define IGN_CMD_DIAG_LIST              0x65u  ///< Получить список диагностических параметров
#define IGN_CMD_DIAG_READ              0x66u  ///< Прочитать диагностические параметры
#define IGN_CMD_DIAG_FAULTS            0x67u  ///< Получить активные и сохранённые ошибки
#define IGN_CMD_DIAG_EVENTS            0x68u  ///< Получить историю диагностических событий
#define IGN_CMD_DIAG_CLEAR_EVENTS      0x69u  ///< Очистить историю диагностических событий
#define IGN_CMD_DIAG_TEST_LIST         0x6Au  ///< Получить список безопасных диагностических тестов
#define IGN_CMD_DIAG_TEST_START        0x6Bu  ///< Запустить безопасный диагностический тест
#define IGN_CMD_DIAG_TEST_STATUS       0x6Cu  ///< Получить результат последнего диагностического теста
/** @} */

/**
 * @defgroup IGN_CMD_RESP Response commands (0x7E–0x7F)
 * @brief Служебные команды ответа
 *
 * Используются для подтверждения или отклонения выполнения команд.
 * @{
 */
#define IGN_CMD_ACK          0x7Eu  ///< Подтверждение успешного выполнения (ACK)
#define IGN_CMD_NACK         0x7Fu  ///< Отрицательное подтверждение (ошибка выполнения)
/** @} */

/**
 * @defgroup IGN_CMD_CORE_COMMANDS Конкретные базовые команды
 * @brief Определения конкретных команд из группы CORE
 * @{
 */

/**
 * @brief Команда обнаружения устройства (HELLO)
 *
 * Используется для проверки доступности устройства и определения
 * его режима работы (bootloader или application).
 *
 * @details
 * При успешной обработке устройство отвечает кадром с тем же кодом
 * команды IGN_CMD_HELLO и payload следующего формата:
 * [kind0][kind1][proto_ver]
 *
 * Где:
 * - kind0, kind1 — ASCII-идентификатор типа устройства
 * - proto_ver — версия протокола
 *
 * Примеры payload:
 * - {'B','L',1} — bootloader, версия протокола 1
 * - {'A','P',1} — приложение, версия протокола 1
 */
#define IGN_CMD_HELLO        0x01u

/**
 * @brief Команда установки времени хоста
 *
 * Используется для синхронизации времени устройства с хостом.
 *
 * @details
 * Payload содержит:
 * - Unix-время (секунды)
 * - смещение часового пояса (в минутах относительно UTC)
 *
 * При успешной обработке устройство отвечает ACK с payload,
 * содержащим код исходной команды (IGN_CMD_HOST_TIME).
 */
#define IGN_CMD_HOST_TIME    0x02u

/**
 * @brief Команда запроса метрик протокола
 *
 * Используется для получения статистики работы протокола.
 *
 * @details
 * При успешной обработке устройство отвечает кадром с тем же кодом
 * команды IGN_CMD_GET_METRICS, а payload содержит структуру
 * ign_proto_metrics_t.
 */
#define IGN_CMD_GET_METRICS  0x03u

/** @} */