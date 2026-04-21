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
#include "app_watchdog.h"
#include "pumps_valves_gpio.h"

#if (APP_FAULT_HANDLER_TEST_TRIGGER_AFTER_ON != 0)
#include "stm32f1xx_it.h"
#endif


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

/**
 * @brief Тестовый хук для имитации отказа запуска safety timer.
 *
 * Нужен только для приемочного теста:
 * если protection timer не стартовал, выход не должен включаться.
 *
 * В production-сборке FLUIDICS_TEST_FORCE_TIMER_START_FAIL должен быть 0,
 * тогда функция всегда возвращает false и не влияет на поведение.
 */
static bool IsSafetyTimerFaultInjected(const PumpValveCommand_t *cmd)
{
#if (FLUIDICS_TEST_FORCE_TIMER_START_FAIL != 0)
	return (cmd->device_id == FLUIDICS_TEST_FAIL_DEVICE_ID);
#else
	(void)cmd;
	return false;
#endif
}

/**
 * @brief Тестовый хук для проверки IWDG supervisor.
 *
 * Хук нужен только для приемочного теста watchdog:
 * нагрузка уже включена, после чего Fluidics-задача перестает делать heartbeat.
 * Supervisor должен увидеть, что один из клиентов не продвигается, выключить
 * все выходы через PumpsValves_AllOff() и прекратить refresh аппаратного IWDG.
 *
 * В production-сборке APP_WATCHDOG_TEST_STALL_FLUIDICS_AFTER_ON должен быть 0,
 * тогда функция компилируется в пустую проверку и не влияет на выполнение.
 */
static void AppWatchdogTest_StallFluidicsAfterOn(const PumpValveCommand_t *cmd)
{
#if (APP_WATCHDOG_TEST_STALL_FLUIDICS_AFTER_ON != 0)
	if (cmd->state && cmd->device_id == APP_WATCHDOG_TEST_STALL_DEVICE_ID) {
		for (;;) {
			/*
			 * Намеренно не вызываем AppWatchdog_Heartbeat().
			 * osDelay() оставляет scheduler живым, поэтому supervisor-задача
			 * успевает выполнить safe-state перед аппаратным reset.
			 */
			osDelay(APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS);
			}
		}
#else
	(void)cmd;
#endif
}

/**
 * @brief Тестовый хук для проверки fault handler safe-state.
 *
 * Хук нужен только для приемочного теста:
 * нагрузка уже включена, после чего задача напрямую входит в HardFault_Handler().
 * Это проверяет тот же аварийный путь, который используется при реальном
 * HardFault: safe-state, отсутствие DONE и последующий IWDG reset.
 *
 * В production-сборке APP_FAULT_HANDLER_TEST_TRIGGER_AFTER_ON должен быть 0.
 */
static void AppFaultHandlerTest_TriggerAfterOn(const PumpValveCommand_t *cmd)
{
#if (APP_FAULT_HANDLER_TEST_TRIGGER_AFTER_ON != 0)
	if (cmd->state && cmd->device_id == APP_FAULT_HANDLER_TEST_DEVICE_ID) {
		HardFault_Handler();
		}
#else
	(void)cmd;
#endif
}

void app_start_task_pump_controller(void *argument)
{
	PumpValveCommand_t cmd;

	// 1. Инициализация таймеров перед входом в цикл
	Init_FluidicTimers();
	AppWatchdog_Heartbeat(APP_WDG_CLIENT_FLUIDICS);

	for (;;)
		{
		// 2. Ждём доменную команду от Dispatcher
		AppWatchdog_Heartbeat(APP_WDG_CLIENT_FLUIDICS);
		if (osMessageQueueGet(fluidics_queueHandle, &cmd, NULL,
				APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS) != osOK) {
			continue;
			}
		AppWatchdog_Heartbeat(APP_WDG_CLIENT_FLUIDICS);

		// Вычисляем индекс в глобальном массиве таймеров (0..15)
		uint8_t timer_idx = (cmd.device_type == DEVICE_TYPE_PUMP) ? cmd.physical_id : (cmd.physical_id + NUM_PUMPS);

		// 3. Исполнение: управление GPIO и таймерами

		if (cmd.state) {
			// --- ВКЛЮЧЕНИЕ ---
			uint32_t timeout = cmd.timeout_ms;
			if (timeout == 0){
				timeout = (cmd.device_type == DEVICE_TYPE_PUMP) ? DEFAULT_PUMP_TIMEOUT_MS : DEFAULT_VALVE_TIMEOUT_MS;
				}

			/*
			 * Safety rule:
			 * Нагрузку разрешено включать только после успешного старта
			 * защитного таймера. Если таймер не создан, не стартовал или
			 * тестовый fault-injection имитирует отказ, выход оставляем OFF.
			 */
			osStatus_t timer_status = osError;

			if (!IsSafetyTimerFaultInjected(&cmd) && fluidic_timers[timer_idx] != NULL) {
				timer_status = osTimerStart(fluidic_timers[timer_idx], timeout);
				}

			if (timer_status != osOK){
				if (cmd.device_type == DEVICE_TYPE_PUMP) {
					PumpsValves_SetPumpState(cmd.physical_id, false);
					}
				else {
					PumpsValves_SetValveState(cmd.physical_id, false);
					}

				CAN_SendNackPublic(cmd.cmd_code, CAN_ERR_DEVICE_BUSY);
				continue;
				}

			if (cmd.device_type == DEVICE_TYPE_PUMP) {
				PumpsValves_SetPumpState(cmd.physical_id, true);
				}
			else {
				PumpsValves_SetValveState(cmd.physical_id, true);
				}

			AppFaultHandlerTest_TriggerAfterOn(&cmd);
			AppWatchdogTest_StallFluidicsAfterOn(&cmd);
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
