/*
 * app_flash.h
 *
 * Модуль управления энергонезависимой конфигурацией.
 * Соответствует стандарту DDS-240 для единой экосистемы
 *
 *  Created on: Apr 7, 2026
 *      Author: andrey
 */

#ifndef APP_FLASH_H_
#define APP_FLASH_H_

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

// Адрес последней страницы Flash (Page 63 для 64KB STM32F103)
#define APP_CONFIG_FLASH_ADDR    0x0800FC00
#define APP_CONFIG_MAGIC         0x55AAEEFF // Ключ валидности данных

/**
 * @brief Структура конфигурации устройства, хранимая во Flash.
 * Размер кратен 4 байтам (32 байта) для выравнивания во Flash.
 */
 typedef struct {
	 uint32_t magic;             // Метка инициализации памяти (4 байта)
     uint8_t  performer_id;      // Настраиваемый CAN NodeID платы (1 байт)
     uint8_t  device_map[16];    // Таблица маппинга: [Idx] -> Logical ID (16 байт)
     uint8_t  reserved_bytes[7]; // Выравнивание (7 байт)
     uint16_t reserved;          // Резерв (2 байта)
     uint16_t checksum;          // Контрольная сумма структуры CRC16 (2 байта)
} AppConfig_t;


/**
 * @brief Чтение 96-битного уникального идентификатора чипа (MCU UID).
 * @param out_uid Указатель на массив (минимум 12 байт).
 */
void AppConfig_GetMCU_UID(uint8_t* out_uid);

/**
 * @brief Инициализирует конфигурацию (загрузка из Flash или Default) и создает Mutex.
 */
void AppConfig_Init(void);

/**
 * @brief Сброс настроек к заводским (очистка Flash).
 */
void AppConfig_FactoryReset(void);

/**
 * @brief Безопасное чтение логического ID для физического индекса устройства.
 * @param physical_idx Индекс в массиве GPIO (0..15).
 */
uint8_t AppConfig_GetFluidicLogicalID(uint8_t physical_idx);

/**
 * @brief Безопасная запись логического ID в таблицу маппинга (в RAM).
 */
void AppConfig_SetFluidicLogicalID(uint8_t physical_idx, uint8_t logical_id);


/**
 * @brief Чтение текущего CAN ID платы.
 */
uint32_t AppConfig_GetPerformerID(void);

/**
 * @brief Безопасная запись CAN ID платы (в RAM).
 */
void AppConfig_SetPerformerID(uint32_t id);

/**
 * @brief Сохранение всех изменений из RAM во Flash.
 * @return true если запись успешна.
 */
 bool AppConfig_Commit(void);


#endif /* APP_FLASH_H_ */
