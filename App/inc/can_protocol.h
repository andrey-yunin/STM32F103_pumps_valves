/*
 * can_protocol.h
 *
 * Константы CAN-протокола для исполнителя насосов и клапанов (Pump Board).
 * Адаптация протокола дирижера (can_packer.h) для STM32F103 bxCAN.
 *
 *  Created on: Mar 6, 2026
 *      Author: andrey
 */

#ifndef CAN_PROTOCOL_H_
#define CAN_PROTOCOL_H_

#include <stdint.h>

// ============================================================
// Приоритеты (биты 28-26 CAN ID)
// ============================================================
#define CAN_PRIORITY_HIGH           0
#define CAN_PRIORITY_NORMAL         1

// ============================================================
// Типы сообщений (биты 25-24 CAN ID)
// ============================================================
#define CAN_MSG_TYPE_COMMAND        0   // Conductor -> Executor
#define CAN_MSG_TYPE_ACK            1   // Executor -> Conductor
#define CAN_MSG_TYPE_NACK           2   // Executor -> Conductor
#define CAN_MSG_TYPE_DATA_DONE_LOG  3   // Executor -> Conductor

// ============================================================
// Подтипы для MSG_TYPE_DATA_DONE_LOG (первый байт payload)
// ============================================================
#define CAN_SUB_TYPE_DONE           0x01
#define CAN_SUB_TYPE_DATA           0x02
#define CAN_SUB_TYPE_LOG            0x03

// ============================================================
// Адреса узлов (биты 23-16 dst, 15-8 src)
// ============================================================
#define CAN_ADDR_BROADCAST          0x00
#define CAN_ADDR_HOST               0x01
#define CAN_ADDR_CONDUCTOR          0x10
#define CAN_ADDR_MOTOR_BOARD        0x20
#define CAN_ADDR_PUMP_BOARD         0x30
#define CAN_ADDR_THERMO_BOARD       0x40

// Адрес этой платы
#define CAN_ADDR_THIS_BOARD         CAN_ADDR_PUMP_BOARD

// ============================================================
// Коды команд (байты 0-1 payload, Little-Endian)
// ============================================================

// Насосы (0x02xx)
#define CAN_CMD_PUMP_RUN_DURATION   0x0201  // Резерв
#define CAN_CMD_PUMP_START          0x0202  // Включить насос
#define CAN_CMD_PUMP_STOP           0x0203  // Выключить насос

// Клапаны (0x02xx, расширение)
#define CAN_CMD_VALVE_OPEN          0x0204  // Открыть клапан
#define CAN_CMD_VALVE_CLOSE         0x0205  // Закрыть клапан

// ============================================================
// Коды ошибок NACK
// ============================================================
#define CAN_ERR_NONE                0x0000
#define CAN_ERR_UNKNOWN_CMD         0x0001
#define CAN_ERR_INVALID_DEVICE_ID   0x0002
#define CAN_ERR_DEVICE_BUSY         0x0003

// ============================================================
// Макрос построения 29-bit CAN ID
// ============================================================
#define CAN_BUILD_ID(priority, msg_type, dst_addr, src_addr) \
	((uint32_t)(((priority) & 0x07) << 26) | \
			(((msg_type) & 0x03) << 24) | \
			(((dst_addr) & 0xFF) << 16) | \
			(((src_addr) & 0xFF) << 8))

// Макросы извлечения полей из CAN ID
#define CAN_GET_PRIORITY(id)    ((uint8_t)(((id) >> 26) & 0x07))
#define CAN_GET_MSG_TYPE(id)    ((uint8_t)(((id) >> 24) & 0x03))
#define CAN_GET_DST_ADDR(id)    ((uint8_t)(((id) >> 16) & 0xFF))
#define CAN_GET_SRC_ADDR(id)    ((uint8_t)(((id) >> 8)  & 0xFF))

// ============================================================
// Публичные функции отправки ответов дирижеру
// ============================================================
void CAN_SendAck(uint16_t cmd_code);
void CAN_SendNackPublic(uint16_t cmd_code, uint16_t error_code);
void CAN_SendDone(uint16_t cmd_code, uint8_t device_id);


#endif /* CAN_PROTOCOL_H_ */
