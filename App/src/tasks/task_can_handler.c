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
#include "app_flash.h"
#include "main.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include "app_watchdog.h"
#include "can_protocol.h"
#include <string.h>


// --- Внешние хэндлы HAL ---
extern CAN_HandleTypeDef hcan;

/*
 * CAN diagnostics are intentionally kept in the transport layer.
 * They count what happens before a frame reaches Dispatcher:
 * queue overflows, transport-level drops, HAL TX errors, and CAN fault states.
 *
 * The counters are exposed later through service command 0xF007 GET_STATUS.
 */
static volatile CanDiagnostics_t g_can_diag;

/*
 * Centralized TX enqueue helper.
 *
 * All ACK/NACK/DONE/DATA frames pass through can_tx_queueHandle.
 * If the queue is full, we cannot report that over CAN reliably, so we only
 * increment the diagnostic counter. The Conductor will observe the missing
 * response by timeout and can later query GET_STATUS.
 */
static void CAN_QueueTxFrame(CanTxFrame_t *tx)
{
	if (osMessageQueuePut(can_tx_queueHandle, tx, 0, 0) == osOK) {
		osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_TX);
	}
	else {
		g_can_diag.tx_queue_overflow++;
	}
}

/*
 * Snapshot getter for Dispatcher / GET_STATUS.
 *
 * The struct is copied with interrupts disabled because some counters can be
 * updated from CAN callbacks while the transport task is running.
 */
void CAN_Diagnostics_GetSnapshot(CanDiagnostics_t *out)
{
	if (out == NULL) {
		return;
	}

	__disable_irq();
	memcpy(out, (const void *)&g_can_diag, sizeof(CanDiagnostics_t));
	__enable_irq();
}

/*
 * Called from CAN RX ISR path when the ISR cannot enqueue a received frame.
 */
void CAN_Diagnostics_RecordRxQueueOverflow(void)
{
	g_can_diag.rx_queue_overflow++;
}

/*
 * Called by application/domain tasks when their command queue is full.
 * For Fluidics this means Dispatcher could not enqueue into fluidics_queueHandle.
 */
void CAN_Diagnostics_RecordAppQueueOverflow(void)
{
	g_can_diag.app_queue_overflow++;
}

/*
 * Called from HAL_CAN_ErrorCallback().
 *
 * hal_error is the HAL accumulated error mask.
 * esr is a raw snapshot of the bxCAN ESR register:
 * - EWGF: error warning
 * - EPVF: error passive
 * - BOFF: bus-off
 */
void CAN_Diagnostics_RecordCanError(uint32_t hal_error, uint32_t esr)
{
	g_can_diag.can_error_callback_count++;
	g_can_diag.last_hal_error = hal_error;
	g_can_diag.last_esr = esr;

	if ((esr & CAN_ESR_EWGF) != 0U) {
		g_can_diag.error_warning_count++;
	}

	if ((esr & CAN_ESR_EPVF) != 0U) {
		g_can_diag.error_passive_count++;
	}

	if ((esr & CAN_ESR_BOFF) != 0U) {
		g_can_diag.bus_off_count++;
	}
}


// ============================================================
// Публичные функции отправки ответов дирижеру
// ============================================================

void CAN_SendAck(uint16_t cmd_code)
{
	CanTxFrame_t tx;
	memset(tx.data, 0, 8); // Важно!

    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL, CAN_MSG_TYPE_ACK,
                                      CAN_ADDR_CONDUCTOR, AppConfig_GetPerformerID());
    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 8; // Строго 8
    tx.data[0] = (uint8_t)(cmd_code & 0xFF);
    tx.data[1] = (uint8_t)((cmd_code >> 8) & 0xFF);

    CAN_QueueTxFrame(&tx);
    }

void CAN_SendNackPublic(uint16_t cmd_code, uint16_t error_code)
{
	CanTxFrame_t tx;
    memset(tx.data, 0, 8); // Унификация: зануляем всё

	tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL, CAN_MSG_TYPE_NACK,
                                      CAN_ADDR_CONDUCTOR, AppConfig_GetPerformerID());
    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 8;
    tx.data[0] = (uint8_t)(cmd_code & 0xFF);
    tx.data[1] = (uint8_t)((cmd_code >> 8) & 0xFF);
    tx.data[2] = (uint8_t)(error_code & 0xFF);
    tx.data[3] = (uint8_t)((error_code >> 8) & 0xFF);

    CAN_QueueTxFrame(&tx);
}

void CAN_SendDone(uint16_t cmd_code, uint8_t device_id)
{
	CanTxFrame_t tx;
	memset(tx.data, 0, 8); // Унификация: зануляем всё

    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL, CAN_MSG_TYPE_DATA_DONE_LOG,
			CAN_ADDR_CONDUCTOR, AppConfig_GetPerformerID());
    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 8;
    tx.data[0] = CAN_SUB_TYPE_DONE;
    tx.data[1] = (uint8_t)(cmd_code & 0xFF);
    tx.data[2] = (uint8_t)((cmd_code >> 8) & 0xFF);
    tx.data[3] = device_id;

    CAN_QueueTxFrame(&tx);
}


void CAN_SendData(uint16_t cmd_code, uint8_t *data, uint8_t len)
{
	CanTxFrame_t tx;
	memset(tx.data, 0, 8); // Унификация: зануляем всё
	tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL, CAN_MSG_TYPE_DATA_DONE_LOG,
			CAN_ADDR_CONDUCTOR, AppConfig_GetPerformerID());
	tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 8;

    tx.data[0] = CAN_SUB_TYPE_DATA;
    tx.data[1] = 0x80; // Sequence Info (EOT=1, Seq=0)

    for (uint8_t i = 0; i < len && i < 6; i++) {
		tx.data[2 + i] = data[i];
		}

    CAN_QueueTxFrame(&tx);
}


// ============================================================
// Основная задача
// ============================================================

void app_start_task_can_handler(void *argument)
{
	CanRxFrame_t rx_frame;
    CanTxFrame_t tx_frame;
    uint32_t txMailbox;


    // --- Настройка CAN-фильтра (Принимаем всё, фильтруем программно) ---

    CAN_FilterTypeDef sFilterConfig;

    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000 | (1 << 2); // IDE=1
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000 | (1 << 2);
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

    /*
     * Enable normal RX interrupt plus diagnostic CAN fault notifications.
     *
     * RX_FIFO0_MSG_PENDING is the normal receive path.
     * The rest are used only to count degraded CAN states for GET_STATUS.
     */

    if (HAL_CAN_ActivateNotification(&hcan,
			CAN_IT_RX_FIFO0_MSG_PENDING |
			CAN_IT_RX_FIFO0_FULL |
			CAN_IT_RX_FIFO0_OVERRUN |
			CAN_IT_ERROR_WARNING |
			CAN_IT_ERROR_PASSIVE |
			CAN_IT_BUSOFF |
			CAN_IT_LAST_ERROR_CODE |
			CAN_IT_ERROR) != HAL_OK) {
		Error_Handler();
		}

    // --- Event-driven основной цикл ---
    for (;;)
		{
		AppWatchdog_Heartbeat(APP_WDG_CLIENT_CAN);
		uint32_t flags = osThreadFlagsWait(FLAG_CAN_RX | FLAG_CAN_TX,
				osFlagsWaitAny, APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS);
		AppWatchdog_Heartbeat(APP_WDG_CLIENT_CAN);

		if ((flags & osFlagsError) != 0U) {
			continue;
			}

		// --- Обработка входящих CAN-фреймов ---
		if (flags & FLAG_CAN_RX)
			{
			while (osMessageQueueGet(can_rx_queueHandle, &rx_frame, NULL, 0) == osOK)
				{
				/*
				 * Executor transport accepts only 29-bit Extended ID frames.
				 * Standard 11-bit frames are intentionally dropped without NACK:
				 * the Conductor must detect this by ACK timeout.
				 */
				if (rx_frame.header.IDE != CAN_ID_EXT) {
					g_can_diag.dropped_not_ext++;
					continue;
					}


				uint32_t can_id = rx_frame.header.ExtId;
				uint8_t dst_addr = CAN_GET_DST_ADDR(can_id);

				/*
				 * Dynamic destination filtering:
				 * accept frames addressed to our current NodeID or broadcast.
				 * Foreign destination is a transport drop, not an application NACK.
				 */
				if (dst_addr != AppConfig_GetPerformerID() && dst_addr != CAN_ADDR_BROADCAST) {
					g_can_diag.dropped_wrong_dst++;
					continue;
					}


				/*
				 * Executor RX path accepts only COMMAND frames from the Conductor.
				 * ACK/NACK/DATA/DONE frames observed on the bus are ignored here.
				 */
				if (CAN_GET_MSG_TYPE(can_id) != CAN_MSG_TYPE_COMMAND) {
					g_can_diag.dropped_wrong_type++;
					continue;
					}



				/*
				 * DDS-240 Directive 2.0:
				 * Conductor <-> Executor uses strict DLC=8.
				 * Wrong DLC is silently dropped and counted for diagnostics.
				 */
				if (rx_frame.header.DLC != 8) {
					g_can_diag.dropped_wrong_dlc++;
					continue;
					}

				// Упаковка в ParsedCanCommand_t
				ParsedCanCommand_t parsed;
				memset(&parsed, 0, sizeof(parsed));
				parsed.cmd_code = (uint16_t)(rx_frame.data[0] |
						((uint16_t)rx_frame.data[1] << 8));
				parsed.device_id = rx_frame.data[2];
				parsed.data_len = rx_frame.header.DLC - 3;

				for (uint8_t i = 0; i < parsed.data_len && i < 5; i++) {
					parsed.data[i] = rx_frame.data[3 + i];
					}

				/*
				 * Try to forward the parsed command to Dispatcher.
				 *
				 * If this queue is full, the frame already passed the CAN transport checks,
				 * but application parsing will never see it. We cannot send NACK here because
				 * ACK/NACK policy belongs to Dispatcher, not to the transport task.
				 *
				 * The Conductor will observe ACK timeout and can then use GET_STATUS.
				 */
				if (osMessageQueuePut(dispatcher_queueHandle, &parsed, 0, 0) == osOK) {
					/*
					 * Count frames that were really accepted by the internal command path.
					 * Transport-level drops are counted separately above.
					 */
					g_can_diag.rx_total++;
					}
				else {
					g_can_diag.dispatcher_queue_overflow++;
					}
				}
			}

		// --- Отправка исходящих CAN-фреймов ---
		if (flags & FLAG_CAN_TX)
			{
			while (osMessageQueueGet(can_tx_queueHandle, &tx_frame, NULL, 0) == osOK)
				{
				// Mailbox Guard (Advanced)
				uint32_t start_tick = osKernelGetTickCount();
				while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0) {
					if ((osKernelGetTickCount() - start_tick) > 10) break;
					osDelay(1);
					}

				/*
				 * Mailbox Guard:
				 * wait a short bounded time for a free hardware TX mailbox.
				 * If none appears, drop the frame and count it. Blocking longer
				 * here would stall the transport task.
				 */
				if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0) {
					if (HAL_CAN_AddTxMessage(&hcan, &tx_frame.header, tx_frame.data, &txMailbox) == HAL_OK) {
						g_can_diag.tx_total++;
						}
					else {
						g_can_diag.tx_hal_error++;
						g_can_diag.last_hal_error = HAL_CAN_GetError(&hcan);
						g_can_diag.last_esr = hcan.Instance->ESR;
						}
					}
				else {
					g_can_diag.tx_mailbox_timeout++;
					}

				}
			}
		}
}
