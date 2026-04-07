/*
 * task_can_handler.c
 *
 * Транспортный уровень CAN для Pump Board (CAN_ADDR=0x30).
 * Event-driven через osThreadFlags.
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 *
 *  Refactored: Mar 6, 2026
 */

#include "task_can_handler.h"
#include "main.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include "can_protocol.h"

// --- Внешние хэндлы HAL ---
extern CAN_HandleTypeDef hcan;

// ============================================================
// Публичные функции отправки ответов дирижеру
// ============================================================

void CAN_SendAck(uint16_t cmd_code)
{
	CanTxFrame_t tx;
    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL, CAN_MSG_TYPE_ACK,
                                      CAN_ADDR_CONDUCTOR, CAN_ADDR_THIS_BOARD);
    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 2;
    tx.data[0] = (uint8_t)(cmd_code & 0xFF);
    tx.data[1] = (uint8_t)((cmd_code >> 8) & 0xFF);

    osMessageQueuePut(can_tx_queueHandle, &tx, 0, 0);
    osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_TX);
    }

void CAN_SendNackPublic(uint16_t cmd_code, uint16_t error_code)
{
	CanTxFrame_t tx;
	tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL, CAN_MSG_TYPE_NACK,
                                      CAN_ADDR_CONDUCTOR, CAN_ADDR_THIS_BOARD);
    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 4;
    tx.data[0] = (uint8_t)(cmd_code & 0xFF);
    tx.data[1] = (uint8_t)((cmd_code >> 8) & 0xFF);
    tx.data[2] = (uint8_t)(error_code & 0xFF);
    tx.data[3] = (uint8_t)((error_code >> 8) & 0xFF);

    osMessageQueuePut(can_tx_queueHandle, &tx, 0, 0);
    osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_TX);
}

void CAN_SendDone(uint16_t cmd_code, uint8_t device_id)
{
	CanTxFrame_t tx;
    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL, CAN_MSG_TYPE_DATA_DONE_LOG,
    		CAN_ADDR_CONDUCTOR, CAN_ADDR_THIS_BOARD);
    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 4;
    tx.data[0] = CAN_SUB_TYPE_DONE;
    tx.data[1] = (uint8_t)(cmd_code & 0xFF);
    tx.data[2] = (uint8_t)((cmd_code >> 8) & 0xFF);
    tx.data[3] = device_id;

    osMessageQueuePut(can_tx_queueHandle, &tx, 0, 0);
    osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_TX);
}

// ============================================================
// Основная задача
// ============================================================

void app_start_task_can_handler(void *argument)
{
	CanRxFrame_t rx_frame;
    CanTxFrame_t tx_frame;
    uint32_t txMailbox;

    // --- Настройка CAN-фильтра для 29-bit ExtID ---
    // Фильтруем по DstAddr = CAN_ADDR_THIS_BOARD (0x30) в битах 23-16
    // В 32-bit фильтре bxCAN биты сдвинуты на 3 (выравнивание по регистру)
    // ExtID[28:0] → FilterId[31:3], IDE=1 → бит 2
    CAN_FilterTypeDef sFilterConfig;
    uint32_t filter_id   = ((uint32_t)CAN_ADDR_THIS_BOARD << 16) << 3 | (1 << 2); // IDE=1
    uint32_t filter_mask = ((uint32_t)0xFF << 16) << 3 | (1 << 2); // Маска на DstAddr + IDE

    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = (uint16_t)(filter_id >> 16);
    sFilterConfig.FilterIdLow = (uint16_t)(filter_id & 0xFFFF);
    sFilterConfig.FilterMaskIdHigh = (uint16_t)(filter_mask >> 16);
    sFilterConfig.FilterMaskIdLow = (uint16_t)(filter_mask & 0xFFFF);
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK) {
    	Error_Handler();
    	}

    // --- Запуск CAN ---
    if (HAL_CAN_Start(&hcan) != HAL_OK) {
    	Error_Handler();
    	}

    // --- Активация прерываний CAN RX FIFO0 ---
    if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
    	Error_Handler();
    	}

    // --- Event-driven основной цикл ---
    for (;;)
    	{
    	uint32_t flags = osThreadFlagsWait(FLAG_CAN_RX | FLAG_CAN_TX,
    			osFlagsWaitAny, osWaitForever);

    	// --- Обработка входящих CAN-фреймов ---
    	if (flags & FLAG_CAN_RX)
    		{
    		while (osMessageQueueGet(can_rx_queueHandle, &rx_frame, NULL, 0) == osOK)
    			{
    			// Проверка: Extended ID
    			if (rx_frame.header.IDE != CAN_ID_EXT) {
    				continue;
    				}

    			// Извлечение полей из 29-bit CAN ID
    			uint32_t ext_id = rx_frame.header.ExtId;
                uint8_t msg_type = CAN_GET_MSG_TYPE(ext_id);
                uint8_t dst_addr = CAN_GET_DST_ADDR(ext_id);

                // Проверка: адресовано нам или broadcast
                if (dst_addr != CAN_ADDR_THIS_BOARD && dst_addr != CAN_ADDR_BROADCAST) {
                	continue;
                	}

                // Проверка: только команды
                if (msg_type != CAN_MSG_TYPE_COMMAND) {
                	continue;
                	}

                // Проверка: минимальный DLC (cmd_code 2 байта + device_id 1 байт)
                if (rx_frame.header.DLC < 3) {
                	continue;
                	}

                // Упаковка в ParsedCanCommand_t
                ParsedCanCommand_t parsed;
                parsed.cmd_code = (uint16_t)(rx_frame.data[0] |
                		((uint16_t)rx_frame.data[1] << 8));
                parsed.device_id = rx_frame.data[2];
                parsed.data_len = rx_frame.header.DLC - 3;

                for (uint8_t i = 0; i < parsed.data_len && i < 5; i++) {
                	parsed.data[i] = rx_frame.data[3 + i];
                	}
                osMessageQueuePut(dispatcher_queueHandle, &parsed, 0, 0);
                }
    		}

    	// --- Отправка исходящих CAN-фреймов ---
    	if (flags & FLAG_CAN_TX)
    		{
    		while (osMessageQueueGet(can_tx_queueHandle, &tx_frame, NULL, 0) == osOK)
    			{
    			HAL_CAN_AddTxMessage(&hcan, &tx_frame.header, tx_frame.data, &txMailbox);
    			}
    		}
    	}
}
