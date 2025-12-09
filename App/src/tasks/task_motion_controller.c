/*
 * task_motion_controller.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */

#include "task_motion_controller.h"
#include "main.h"
#include "cmsis_os.h"
#include "motion_planner.h" // Для MotorMotionState_t

// В будущем здесь будут объявления очередей и глобальных переменных
// extern osMessageQueueId_t motion_queueHandle;
// extern MotorMotionState_t motor_states[8]; // Массив состояний для 8 моторов

void app_start_task_motion_controller(void *argument)
{
	// Инициализация состояний всех 8 моторов
	// for(int i=0; i<8; i++) {
	//     MotionPlanner_InitMotorState(&motor_states[i], 0);
	// }

	// Бесконечный цикл задачи
	for(;;)
		{
		 // 1. Ждать задание на движение в очереди motion_queue
		 // 2. Получив задание, извлечь из него motor_id, steps_to_go, direction и т.д.
		 // 3. Установить пины DIR и EN для нужного мотора
		 // 4. Сконфигурировать и запустить таймер TIM2 в режиме прерываний
		 //    (период таймера будет определять скорость)
		osDelay(1);
		}

}
