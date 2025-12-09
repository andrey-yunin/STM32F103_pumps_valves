/*
 * task_command_parser.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */

#include "task_command_parser.h"
#include "main.h"             // Для HAL-функций
#include "cmsis_os.h"         // Для osDelay, osMessageQueueXxx
#include "command_protocol.h" // Для CAN_Command_t, CommandID_t
#include "app_queues.h"       // Для хэндлов очередей
#include "app_config.h"       // Для MotionCommand_t

// --- Внешние хэндлы HAL ---
// extern CAN_HandleTypeDef hcan; // Не нужен напрямую в этой задаче, так как Task_CAN_Handler уже обработал

// --- Глобальная переменная для хранения ID исполнителя ---
// Пока просто заглушка, в будущем будет читаться из Flash
uint8_t g_performer_id = 0xFF; // 0xFF означает, что ID еще не установлен



void app_start_task_command_parser(void *argument)
{
	CAN_Command_t received_command; // Буфер для принятой команды
	MotionCommand_t motion_cmd;     // Буфер для команды движения

	// --- Логика определения ID исполнителя (пока заглушка) ---
	// В будущем здесь будет чтение ID из Flash
	g_performer_id = 0; // Для тестирования, примем, что наш ID = 0


	// Бесконечный цикл задачи
	for(;;)
		{
		// 1. Ждем команду из очереди parser_queue
	    if (osMessageQueueGet(parser_queueHandle, &received_command, NULL, osWaitForever) == osOK)
	    	{
	    	// --- 2. Фильтрация по Performer ID ---
	        // Если команда адресована конкретному исполнителю, а не всем
	        // (Motor_ID 0xFF означает широковещательную команду)
	        if (received_command.motor_id != 0xFF && (received_command.motor_id >> 3) != g_performer_id) {
	        	// Это не наша команда, игнорируем
	        	continue;
	        	}
	        // --- 3. Обработка команды ---
	        switch (received_command.command_id)
	        {
	             case CMD_MOVE_ABSOLUTE:
	             case CMD_MOVE_RELATIVE:
	             case CMD_SET_SPEED:
	             case CMD_SET_ACCELERATION:
	             case CMD_STOP:
	            	 // Команды движения - отправляем в очередь Task_Motion_Controller
	            	 motion_cmd.motor_id = received_command.motor_id;
	                 motion_cmd.steps = received_command.payload; // Или что-то другое
	                 // ... заполнить остальные поля motion_cmd ...
	                 osMessageQueuePut(motion_queueHandle, &motion_cmd, 0, 0);
	                 break;

	             case CMD_GET_STATUS:
	             case CMD_SET_CURRENT:
	             case CMD_ENABLE_MOTOR:
	            	 // Команды для TMC-драйверов - отправляем в очередь Task_TMC_Manager
	            	 // Пока передадим CAN_Command_t напрямую
	            	 osMessageQueuePut(tmc_manager_queueHandle, &received_command, 0, 0);
	                 break;

	             case CMD_PERFORMER_ID_SET:
	            	 // Команда установки ID исполнителя (для провизионинга)
	            	 // (received_command.payload будет содержать новый ID)
	            	 // В будущем здесь будет логика записи в Flash и перезагрузки
	            	 g_performer_id = (uint8_t)received_command.payload;
	            	 // ... отправить подтверждение через can_tx_queue ...
	                 break;

	              default:
	                 // Неизвестная команда
	                 // ... Отправить ошибку через can_tx_queue ...
	              break;
	         }

	     }
	    osDelay(1); // Небольшая задержка, чтобы не нагружать процессор без надобности
	   }
}
