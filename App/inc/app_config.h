/*
 * app_config.h
 *
 *  Created on: Dec 9, 2025
 *      Author: andrey
 *
 *  Refactored: Mar 6, 2026
 *  Updated: Apr 7, 2026 (Compliance with DDS-240 Ecosystem Standard)
 */

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <main.h>
#include <stdbool.h>

// =============================================================================
//                             ОБЩИЕ НАСТРОЙКИ ПРИЛОЖЕНИЯ
// =============================================================================

// Количество исполнительных устройств на плате
#define NUM_PUMPS                   13
#define NUM_VALVES                  3
#define TOTAL_DEVICES               (NUM_PUMPS + NUM_VALVES) // Всего 16

#define DEVICE_ID_INVALID           0xFF // Значение для неинициализированного устройства

// Версия прошивки
#define FW_REV_MAJOR                0x01
#define FW_REV_MINOR                0x01
#define FW_REV_PATCH                0x00

#define CAN_DATA_MAX_LEN            8    // Максимальная длина поля данных CAN-фрейма

// =============================================================================
//                             НАСТРОЙКИ ОЧЕРЕДЕЙ FREERTOS
// =============================================================================

#define CAN_RX_QUEUE_LEN            16
#define CAN_TX_QUEUE_LEN            32
#define DISPATCHER_QUEUE_LEN        10
#define FLUIDICS_QUEUE_LEN          8

// Флаги для Task_CAN_Handler (osThreadFlags)
#define FLAG_CAN_RX                 0x01
#define FLAG_CAN_TX                 0x02


// Таймауты безопасности по умолчанию (Safety Timeouts)
#define DEFAULT_PUMP_TIMEOUT_MS     60000   // 60 секунд
#define DEFAULT_VALVE_TIMEOUT_MS    300000  // 5 минут (300 секунд)



// =============================================================================
//                             СТРУКТУРЫ ДАННЫХ
// =============================================================================

/**
 * @brief Промежуточная структура: CAN Handler -> Dispatcher
 * Содержит распакованные поля CAN-фрейма по стандарту DDS-240
 */
typedef struct {
    uint16_t cmd_code;      // Код команды (байты 0-1 payload, Little-Endian)
    uint8_t  device_id;     // ID целевого устройства/канала (байт 2 payload)
    uint8_t  data[5];       // Параметры команды (байты 3-7 payload)
    uint8_t  data_len;      // Длина полезных данных в массиве data
} ParsedCanCommand_t;

/**
 * @brief Доменная структура задания для Task_Pump_Controller
 */
typedef struct {
	uint8_t  physical_id;   // Физический индекс (0-based) в массиве GPIO
	uint8_t  device_type;   // 0=PUMP, 1=VALVE
	uint16_t cmd_code;      // Для ACK/NACK/DONE
	uint8_t  device_id;     // Логический ID (для DONE)
	bool     state;         // true=ON/OPEN, false=OFF/CLOSE
	uint32_t timeout_ms;    // Таймаут безопасности (0 = бесконечно или по умолчанию)
} PumpValveCommand_t;

/**
 * @brief Обертки для HAL CAN фреймов
 */
typedef struct {
    CAN_RxHeaderTypeDef header;
    uint8_t data[CAN_DATA_MAX_LEN];
} CanRxFrame_t;

typedef struct {
    CAN_TxHeaderTypeDef header;
    uint8_t data[CAN_DATA_MAX_LEN];
} CanTxFrame_t;

#endif // APP_CONFIG_H
