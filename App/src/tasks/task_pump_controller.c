/*
 * task_pump_controller.c
 *
 * Доменный контроллер насосов и клапанов.
 * Приём PumpValveCommand_t из domain_queue,
 * исполнение GPIO, отправка ACK/DONE дирижеру.
 *
 *  Created on: Mar 6, 2026
 *      Author: andrey
 */

#include "task_pump_controller.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include "can_protocol.h"
#include "device_mapping.h"
#include "pumps_valves_gpio.h"

void app_start_task_pump_controller(void *argument)
{
	PumpValveCommand_t cmd;
	for (;;)
		{
		// Ждём доменную команду от Command Parser
		if (osMessageQueueGet(fluidics_queueHandle, &cmd, NULL, osWaitForever) != osOK) {
			continue;
			}

		// Подтверждение приёма команды
		CAN_SendAck(cmd.cmd_code);

		// Исполнение: управление GPIO
		if (cmd.device_type == DEVICE_TYPE_PUMP) {
			PumpsValves_SetPumpState(cmd.physical_id, cmd.state);
			}
		else if (cmd.device_type == DEVICE_TYPE_VALVE) {
			PumpsValves_SetValveState(cmd.physical_id, cmd.state);
			}

		// Уведомление дирижера о завершении
		CAN_SendDone(cmd.cmd_code, cmd.device_id);
		}
}



