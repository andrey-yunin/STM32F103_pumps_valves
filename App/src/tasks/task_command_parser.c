/*
 * task_command_parser.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */

#include "task_command_parser.h"
#include "main.h"         // Для HAL-функций и типов
#include "cmsis_os.h"     // Для osDelay, osMessageQueueId_t и т.д.
#include "command_protocol.h" // Для CAN_Command_t и CommandID_t

// В будущем здесь будут объявления очередей
// extern osMessageQueueId_t parser_queueHandle;
// extern osMessageQueueId_t motion_queueHandle;
// extern osMessageQueueId_t tmc_manager_queueHandle;
// extern osMessageQueueId_t can_tx_queueHandle; // Для отправки ответов


void app_start_task_command_parser(void *argument)
{
	// Структура для хранения принятой команды
    // CAN_Command_t received_command;

    // Бесконечный цикл задачи
  for(;;)
  {
	  // 1. Ждать сообщение в очереди parser_queue (от Task_CAN_Handler)
      // 2. Забрать сообщение (например, структуру CAN_Command_t)
      // 3. Распарсить команду (CommandID_t)
      // 4. В зависимости от типа команды:
      //    - Если это команда движения, положить её в motion_queue для Task_Motion_Controller
      //    - Если это команда для TMC-драйвера, положить её в tmc_manager_queue для Task_TMC_Manager
      //    - Если это запрос статуса, получить статус и положить ответ в can_tx_queue

    osDelay(1);
  }

}
