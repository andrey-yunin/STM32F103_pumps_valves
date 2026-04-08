/*
 * task_pump_controller.c
 *
 *
 * Доменный контроллер насосов и клапанов с поддержкой Safety Timeout.
 * Реализовано на базе программных таймеров FreeRTOS.
 *
 *  Created on: Mar 6, 2026
 *  Updated: Apr 8, 2026 (Safety Timeout Implementation)
 *
 *      Author: andrey
 */

#include "task_pump_controller.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include "can_protocol.h"
#include "device_mapping.h"
#include "pumps_valves_gpio.h"


// --- Локальные переменные ---
static osTimerId_t fluidic_timers[TOTAL_DEVICES];

/**
 * @brief Структура для передачи контекста в коллбэк таймера
 */
typedef struct {
	uint8_t physical_id;
	uint8_t device_type;
} TimerContext_t;

static TimerContext_t timer_contexts[TOTAL_DEVICES];


/**
 * @brief Коллбэк программного таймера (вызывается при истечении времени)
 * @param argument Указатель на TimerContext_t
 */
static void AutoOff_Callback(void *argument)
{
	TimerContext_t *ctx = (TimerContext_t *)argument;

	if (ctx->device_type == DEVICE_TYPE_PUMP) {
		PumpsValves_SetPumpState(ctx->physical_id, false);
		}
	else {
		PumpsValves_SetValveState(ctx->physical_id, false);
		}

	// Примечание: DONE не отправляется, так как это защитное отключение
	}

/**
 * @brief Инициализация пула таймеров
 */
static void Init_FluidicTimers(void)
{
	for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
		timer_contexts[i].physical_id = (i < NUM_PUMPS) ? i : (i - NUM_PUMPS);
		timer_contexts[i].device_type = (i < NUM_PUMPS) ? DEVICE_TYPE_PUMP : DEVICE_TYPE_VALVE;

		fluidic_timers[i] = osTimerNew(AutoOff_Callback, osTimerOnce, &timer_contexts[i], NULL);
		}
}

void app_start_task_pump_controller(void *argument)
{
	PumpValveCommand_t cmd;

	// 1. Инициализация таймеров перед входом в цикл
	Init_FluidicTimers();

	for (;;)
		{
		// 2. Ждём доменную команду от Dispatcher
		if (osMessageQueueGet(fluidics_queueHandle, &cmd, NULL, osWaitForever) != osOK) {
			continue;
			}

		// Вычисляем индекс в глобальном массиве таймеров (0..15)
		uint8_t timer_idx = (cmd.device_type == DEVICE_TYPE_PUMP) ? cmd.physical_id : (cmd.physical_id + NUM_PUMPS);

		// 3. Исполнение: управление GPIO и таймерами
		if (cmd.state) {
			// --- ВКЛЮЧЕНИЕ ---
		    if (cmd.device_type == DEVICE_TYPE_PUMP) {
		    	PumpsValves_SetPumpState(cmd.physical_id, true);
		    	}
		    else
		    {
		    	PumpsValves_SetValveState(cmd.physical_id, true);
		    	}

		    // Расчет таймаута
		    uint32_t timeout = cmd.timeout_ms;
		    if (timeout == 0)
		    {
		    	timeout = (cmd.device_type == DEVICE_TYPE_PUMP) ? DEFAULT_PUMP_TIMEOUT_MS : DEFAULT_VALVE_TIMEOUT_MS;
		    	}

		    // Запуск/Перезапуск таймера
		    if (fluidic_timers[timer_idx] != NULL) {
		    	osTimerStart(fluidic_timers[timer_idx], timeout);
		    	}
		    }

		else
		{
			// --- ВЫКЛЮЧЕНИЕ ---
			if (cmd.device_type == DEVICE_TYPE_PUMP) {
				PumpsValves_SetPumpState(cmd.physical_id, false);
				}
			else
			{
				PumpsValves_SetValveState(cmd.physical_id, false);
				}
			// Остановка таймера
			if (fluidic_timers[timer_idx] != NULL)
			{
				osTimerStop(fluidic_timers[timer_idx]);
				}
			}

		// 4. Уведомление дирижера о штатном завершении
		CAN_SendDone(cmd.cmd_code, cmd.device_id);
		}
}



