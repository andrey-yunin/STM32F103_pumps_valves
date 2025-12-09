/*
 * task_tmc2209_manager.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */

#include "task_tmc2209_manager.h"
#include "main.h"
#include "cmsis_os.h"
#include "tmc2209_driver.h" // Для TMC2209_Handle_t

// В будущем здесь будут объявления очередей и глобальных переменных
// extern osMessageQueueId_t tmc_manager_queueHandle;
// extern TMC2209_Handle_t tmc_drivers[8]; // Массив хэндлов для 8 драйверов

void app_start_task_tmc2209_manager(void *argument)
{
	// Инициализация драйверов TMC2209 при старте системы
	// В будущем, возможно, будет цикл по motor_id
	// for(int i=0; i<8; i++) {
	//   TMC2209_Init(&tmc_drivers[i], &huart1, i % 4); // Пример
	//   TMC2209_SetMotorCurrent(&tmc_drivers[i], 80, 50); // Пример: 80% run, 50% hold
	//   TMC2209_SetMicrosteps(&tmc_drivers[i], 16);
	// }

	// Бесконечный цикл задачи

	for(;;)
		{
		osDelay(1);
		}
}
