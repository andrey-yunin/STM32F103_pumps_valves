/*
 * task_dispatcher.c
 *
 * Диспетчер команд (Application Logic).
 * Выполняет парсинг команд, сервисные функции и маппинг через Flash.
 * Соответствует стандарту DDS-240 для единой экосистемы.
 *
 *  Created on: Dec 8, 2025
 *  Refactored: Apr 7, 2026 (Compliance with DDS-240 Pattern)
 */

#include "task_dispatcher.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include "app_flash.h"
#include "can_protocol.h"
#include "device_mapping.h"
#include <string.h>
#include "main.h"

void app_start_task_dispatcher(void *argument)
{
	ParsedCanCommand_t parsed;
	PumpValveCommand_t fluid_cmd;

	for (;;)
	{
		// 1. Ожидаем команду от CAN Handler
		if (osMessageQueueGet(dispatcher_queueHandle, &parsed, NULL, osWaitForever) != osOK) {
			continue;
		}

		// 2. --- ЗОЛОТОЙ ЭТАЛОН: Немедленное подтверждение приема (ACK) ---
		CAN_SendAck(parsed.cmd_code);

		// 3. Распределение команд по группам
		switch (parsed.cmd_code)
		{
			// ============================================================
			// ГРУППА 0x02xx: УПРАВЛЕНИЕ ЖИДКОСТНОЙ СИСТЕМОЙ
			// ============================================================
		    case CAN_CMD_PUMP_RUN_DURATION:
		    case CAN_CMD_PUMP_START:
			case CAN_CMD_PUMP_STOP:
			case CAN_CMD_VALVE_OPEN:
			case CAN_CMD_VALVE_CLOSE:
			{

				// --- 1. Маппинг логического ID в физический через единый модуль ---
				DeviceMappingResult_t mapping = DeviceMapping_Resolve(parsed.device_id);

				if (mapping.physical_id == DEVICE_ID_INVALID) {
					CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_DEVICE_ID);
					continue;
					}

				// --- 2. Формируем доменную команду ---
				memset(&fluid_cmd, 0, sizeof(fluid_cmd));
				fluid_cmd.cmd_code = parsed.cmd_code;
				fluid_cmd.device_id = parsed.device_id;
				fluid_cmd.physical_id = mapping.physical_id;
				fluid_cmd.device_type = mapping.device_type;

				// --- 3. Парсинг таймаута (байты 3-6 payload -> data[0-3]) --
				if (parsed.data_len >= 4)
				{
					fluid_cmd.timeout_ms = (uint32_t)(parsed.data[0] |
							((uint32_t)parsed.data[2] << 8) |
							((uint32_t)parsed.data[3] << 16) |
							((uint32_t)parsed.data[4] << 24));
					}

				// --- 4. Специальная логика для RUN_DURATION (0x0201) ---
				if (parsed.cmd_code == CAN_CMD_PUMP_RUN_DURATION && fluid_cmd.timeout_ms == 0)
				{
					CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_KEY); // Ошибка: таймаут не указан
					continue;
					}

				// --- 5. Определяем целевое состояние ---
				if (parsed.cmd_code == CAN_CMD_PUMP_START ||
						parsed.cmd_code == CAN_CMD_VALVE_OPEN ||
						parsed.cmd_code == CAN_CMD_PUMP_RUN_DURATION)
					{
					fluid_cmd.state = true;
					}
				else
				{
					fluid_cmd.state = false;
					}

				// --- 6. Валидация типа устройства ---
				bool is_valve_cmd = (parsed.cmd_code == CAN_CMD_VALVE_OPEN || parsed.cmd_code == CAN_CMD_VALVE_CLOSE);
				bool is_pump_cmd = (parsed.cmd_code == CAN_CMD_PUMP_START || parsed.cmd_code == CAN_CMD_PUMP_STOP || parsed.cmd_code == CAN_CMD_PUMP_RUN_DURATION);
				if ((fluid_cmd.device_type == DEVICE_TYPE_PUMP && is_valve_cmd) ||
						(fluid_cmd.device_type == DEVICE_TYPE_VALVE && is_pump_cmd))
				{
					CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_DEVICE_ID);
					continue;
					}
				// --- 7. Отправляем исполнителю ---
				osMessageQueuePut(fluidics_queueHandle, &fluid_cmd, 0, 0);
				break;
			}

			// ============================================================
			// ГРУППА 0xF0xx: УНИВЕРСАЛЬНЫЕ СЕРВИСНЫЕ КОМАНДЫ
			// ============================================================
			case CAN_CMD_SRV_GET_DEVICE_INFO: {
				uint8_t uid[12];
				uint8_t data[6];
				AppConfig_GetMCU_UID(uid);
				
				// Пакет 1: Метаданные (Тип, Версия, Каналы) + начало UID
				data[0] = CAN_DEVICE_TYPE_FLUIDIC; 
				data[1] = FW_REV_MAJOR;
				data[2] = FW_REV_MINOR;
				data[3] = TOTAL_DEVICES;            
				data[4] = uid[0];
				data[5] = uid[1];
				CAN_SendData(parsed.cmd_code, data, 6);

				// Пакет 2: Середина UID
				memcpy(data, &uid[2], 6);
				CAN_SendData(parsed.cmd_code, data, 6);

				// Пакет 3: Конец UID
				memcpy(data, &uid[8], 4);
				data[4] = 0; data[5] = 0;
				CAN_SendData(parsed.cmd_code, data, 6);

				CAN_SendDone(parsed.cmd_code, 0);
				break;
			}

			case CAN_CMD_SRV_REBOOT: {
				uint16_t key = (uint16_t)(parsed.data[0] | (parsed.data[1] << 8));
				if (key == SRV_MAGIC_REBOOT) {
					CAN_SendDone(parsed.cmd_code, 0);
					osDelay(100); 
					NVIC_SystemReset();
				} else {
					CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_KEY);
				}
				break;
			}

			case CAN_CMD_SRV_FLASH_COMMIT: {
				if (AppConfig_Commit()) {
					CAN_SendDone(parsed.cmd_code, 0);
				} else {
					CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_FLASH_WRITE);
				}
				break;
			}

			case CAN_CMD_SRV_FACTORY_RESET: {
				uint16_t key = (uint16_t)(parsed.data[0] | (parsed.data[1] << 8));
				if (key == SRV_MAGIC_FACTORY_RESET) {
					AppConfig_FactoryReset();
					CAN_SendDone(parsed.cmd_code, 0);
					osDelay(100);
					NVIC_SystemReset();
				} else {
					CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_KEY);
				}
				break;
			}

			case CAN_CMD_SRV_SET_NODE_ID: {
				if (parsed.device_id >= 0x02 && parsed.device_id <= 0x7F && parsed.device_id != 0x10) {
					AppConfig_SetPerformerID(parsed.device_id);
					CAN_SendDone(parsed.cmd_code, parsed.device_id);
				} else {
					CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_UNKNOWN_CMD);
				}
				break;
			}

			case CAN_CMD_SRV_GET_UID: {
				uint8_t uid[12];
				uint8_t data[6];
				AppConfig_GetMCU_UID(uid);
				
				memcpy(data, &uid[0], 6);
				CAN_SendData(parsed.cmd_code, data, 6);

				memcpy(data, &uid[6], 6);
				CAN_SendData(parsed.cmd_code, data, 6);

				CAN_SendDone(parsed.cmd_code, 0);
				break;
			}

			default:
				CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_UNKNOWN_CMD);
				break;
		}
	}
}
