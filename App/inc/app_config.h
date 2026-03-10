/*
 * app_config.h
 *
 *  Created on: Dec 9, 2025
 *      Author: andrey
 *
 *  Refactored: Mar 6, 2026
 */

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include "main.h"
#include <stdbool.h>

// =============================================================================
//                             ОБЩИЕ НАСТРОЙКИ ПРИЛОЖЕНИЯ
// =============================================================================

#define CAN_DATA_MAX_LEN            8   // Максимальная длина поля данных CAN-фрейма

// Количество насосов в системе
#define NUM_PUMPS                   13

// Количество клапанов в системе
#define NUM_VALVES                  3

// =============================================================================
//                             ФЛАГИ СОБЫТИЙ (osThreadFlags)
// =============================================================================

#define FLAG_CAN_RX                 0x01
#define FLAG_CAN_TX                 0x02

// =============================================================================
//                             НАСТРОЙКИ ОЧЕРЕДЕЙ FREERTOS
// =============================================================================

#define CAN_RX_QUEUE_LEN            10
#define CAN_TX_QUEUE_LEN            10
#define PARSER_QUEUE_LEN            10
#define DOMAIN_QUEUE_LEN            5

// =============================================================================
//                             СТРУКТУРЫ ЭЛЕМЕНТОВ ОЧЕРЕДЕЙ
// =============================================================================

// Структура для хранения полного Rx CAN-фрейма (header + data)
typedef struct {
	CAN_RxHeaderTypeDef header;
    uint8_t data[CAN_DATA_MAX_LEN];
    } CanRxFrame_t;

// Структура для хранения полного Tx CAN-фрейма (header + data)
typedef struct {
	CAN_TxHeaderTypeDef header;
	uint8_t data[CAN_DATA_MAX_LEN];
	} CanTxFrame_t;

// Промежуточная структура: CAN Handler -> Command Parser
typedef struct {
	uint16_t cmd_code;      // CAN-код команды (байты 0-1 payload, LE)
	uint8_t  device_id;     // Логический ID устройства (байт 2 payload)
    uint8_t  data[5];       // Сырые данные параметров (байты 3-7 payload)
    uint8_t  data_len;      // Количество байт в data (DLC - 3)
    } ParsedCanCommand_t;

// Доменная структура: Command Parser -> Pump Controller
typedef struct {
	uint8_t  physical_id;   // Физический индекс (0-based) в массиве GPIO
	uint8_t  device_type;   // DEVICE_TYPE_PUMP или DEVICE_TYPE_VALVE
	uint16_t cmd_code;      // Для ACK/NACK/DONE
	uint8_t  device_id;     // Логический ID (для DONE)
	bool     state;         // true=ON/OPEN, false=OFF/CLOSE
	} PumpValveCommand_t;

#endif // APP_CONFIG_H



