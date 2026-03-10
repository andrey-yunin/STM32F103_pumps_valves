/*
 * task_command_parser.c
 *
 * Прикладной уровень: парсинг команд, маппинг device_id,
 * формирование доменных команд для Pump Controller.
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 *
 *  Refactored: Mar 6, 2026
 */

#include "task_command_parser.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include "can_protocol.h"
#include "device_mapping.h"
#include <string.h>

void app_start_task_command_parser(void *argument)
{
	ParsedCanCommand_t cmd;
	PumpValveCommand_t domain_cmd;

	for (;;)
		{
		// Ждём команду от CAN Handler
		if (osMessageQueueGet(parser_queueHandle, &cmd, NULL, osWaitForever) != osOK) {
			continue;
			}

		 // Очистка доменной структуры перед заполнением
		memset(&domain_cmd, 0, sizeof(domain_cmd));

		// Маппинг логического device_id в физический индекс
		DeviceMappingResult_t mapping = DeviceMapping_Resolve(cmd.device_id);

		if (mapping.physical_id == DEVICE_ID_INVALID) {
			CAN_SendNackPublic(cmd.cmd_code, CAN_ERR_INVALID_DEVICE_ID);
			continue;
			}

		// Заполняем общие поля доменной команды
		domain_cmd.physical_id = mapping.physical_id;
		domain_cmd.device_type = mapping.device_type;
		domain_cmd.cmd_code = cmd.cmd_code;
		domain_cmd.device_id = cmd.device_id;

		// Парсинг параметров по коду команды
		switch (cmd.cmd_code)
			{
				case CAN_CMD_PUMP_START:
					if (mapping.device_type != DEVICE_TYPE_PUMP) {
						CAN_SendNackPublic(cmd.cmd_code, CAN_ERR_INVALID_DEVICE_ID);
						continue;
						}
					domain_cmd.state = true;
					break;

				case CAN_CMD_PUMP_STOP:
					if (mapping.device_type != DEVICE_TYPE_PUMP) {
						CAN_SendNackPublic(cmd.cmd_code, CAN_ERR_INVALID_DEVICE_ID);
						continue;
						}
					domain_cmd.state = false;
					break;

				case CAN_CMD_VALVE_OPEN:
					if (mapping.device_type != DEVICE_TYPE_VALVE) {
						CAN_SendNackPublic(cmd.cmd_code, CAN_ERR_INVALID_DEVICE_ID);
						continue;
						}
					domain_cmd.state = true;
					break;

				case CAN_CMD_VALVE_CLOSE:
					if (mapping.device_type != DEVICE_TYPE_VALVE) {
						CAN_SendNackPublic(cmd.cmd_code, CAN_ERR_INVALID_DEVICE_ID);
						continue;
						}
					domain_cmd.state = false;
					break;

				default:
					CAN_SendNackPublic(cmd.cmd_code, CAN_ERR_UNKNOWN_CMD);
					continue;
					}

		// Отправляем в доменную очередь для Pump Controller
		osMessageQueuePut(domain_queueHandle, &domain_cmd, 0, 0);
		}
}

