/*
 * task_command_parser.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */

#include "task_dispatcher.h"
#include "main.h"             // Для HAL-функций
#include "cmsis_os.h"         // Для osDelay, osMessageQueueXxx
#include "command_protocol.h" // Для CAN_Command_t, CommandID_t
#include "app_queues.h"       // Для хэндлов очередей
#include "app_config.h"       // Для MotionCommand_t
#include "app_globals.h"
#include "pumps_valves_gpio.h"



void app_start_task_dispatcher(void *argument)
{
	CAN_Command_t received_command; // Буфер для принятой команды от CAN_Handler
	CanTxFrame_t tx_response_frame; // Буфер для отправки ответов

    // --- Логика определения ID исполнителя (пока заглушка) ---
	// В будущем здесь будет чтение ID из Flash.
	// Пока для тестирования, примем, что наш ID = 0, если он еще не установлен
	if (g_performer_id == 0xFF) {
		g_performer_id = 1;
		}


	// Бесконечный цикл задачи
	for(;;)
		{
		 // 1. Ждем команду из очереди parser_queue (от Task_CAN_Handler)
		 if (osMessageQueueGet(dispatcher_queueHandle, &received_command, NULL, osWaitForever) == osOK){

		        // --- 3. Диспетчеризация команды ---
		 	    switch (received_command.command_id){
		 	        // --- Команды для Насосов и клапанов ---
		 	         case CMD_GET_STATUS:
		 	         case CMD_SET_CURRENT:
		 	         case CMD_ENABLE_MOTOR:

		 	        	 //Здесь будет логика для насосов и клапанов

		 	         break;


		 	        // >>> НАЧАЛО ДОБАВЛЕННЫХ КОМАНД ДЛЯ НАСОСОВ И КЛАПАНОВ <<<
		 	         case CMD_SET_PUMP_STATE:
		 	        	 {
		 	        		 // Payload: Bit 0 = State (1=ON, 0=OFF), Bits 7-1 = Pump ID
		 	        		 // Мы используем received_command.motor_id как ID насоса.
		 	        		 uint8_t pump_id = received_command.motor_id;
		 	        		 // Payload содержит состояние: любое ненулевое значение = ON (включить)
		 	        		 bool state = (received_command.payload != 0);
		 	        		 PumpsValves_SetPumpState((PumpID_t)pump_id, state);
		 	        		 // --- Отправляем ACK (подтверждение) ---
		 	        		 // Формируем StdId ответа: 0x200 (префикс ответа) | ID исполнителя | ID насоса
		 	        		 tx_response_frame.header.StdId = 0x200 | (g_performer_id << 4) | pump_id;
		 	        		 tx_response_frame.header.IDE = CAN_ID_STD;
		 	        		 tx_response_frame.header.RTR = CAN_RTR_DATA;
		 	        		 tx_response_frame.header.DLC = 2; // Команда + Статус
		 	        		 tx_response_frame.data[0] = CMD_SET_PUMP_STATE; // Подтверждаем, что это ответ на CMD_SET_PUMP_STATE
		 	        		 tx_response_frame.data[1] = (state ? 1 : 0); // Подтверждаем установленное состояние
		 	        		 osMessageQueuePut(can_tx_queueHandle, &tx_response_frame, 0, 0);
		 	        		 }
		 	        	 break;

		 	        	 case CMD_SET_VALVE_STATE:
		 	        		 {
		 	        			 // Payload: Bit 0 = State (1=OPEN, 0=CLOSE), Bits 7-1 = Valve ID
		 	        			 // Мы используем received_command.motor_id как ID клапана.
		 	        			 uint8_t valve_id = received_command.motor_id;
		 	        			 // Payload содержит состояние: любое ненулевое значение = OPEN (открыть)
		 	        			 bool state = (received_command.payload != 0);
		 	        			 PumpsValves_SetValveState((ValveID_t)valve_id, state);
		 	        			 // --- Отправляем ACK (подтверждение) ---
		 	        			 // Формируем StdId ответа: 0x200 (префикс ответа) | ID исполнителя | ID клапана
		 	        			 tx_response_frame.header.StdId = 0x200 | (g_performer_id << 4) | valve_id;
		 	        			 tx_response_frame.header.IDE = CAN_ID_STD;
		 	        			 tx_response_frame.header.RTR = CAN_RTR_DATA;
		 	        			 tx_response_frame.header.DLC = 2; // Команда + Статус
		 	        			 tx_response_frame.data[0] = CMD_SET_VALVE_STATE; // Подтверждаем, что это ответ на CMD_SET_VALVE_STATE
		 	        			 tx_response_frame.data[1] = (state ? 1 : 0); // Подтверждаем установленное состояние
		 	        			 osMessageQueuePut(can_tx_queueHandle, &tx_response_frame, 0, 0);
		 	        			 }
		 	        		 break;
		 	        		 // <<< КОНЕЦ ДОБАВЛЕННЫХ КОМАНД >>>

		 	         // --- Специальные команды ---
		 	         case CMD_PERFORMER_ID_SET:
		 	         // Команда установки ID исполнителя (для провизионинга)
		 	         // payload содержит новый ID
		 	         g_performer_id = (uint8_t)received_command.payload;
		 	         // TODO: Реализовать запись в Flash здесь
		 	         // TODO: Отправить подтверждение через can_tx_queue

		 	         // Пример ответа:
		 	         tx_response_frame.header.StdId = 0x200 | (g_performer_id << 3) | 0xFF; // Ответ от исполнителя
		 	         tx_response_frame.header.IDE = CAN_ID_STD;
		 	         tx_response_frame.header.RTR = CAN_RTR_DATA;
		 	         tx_response_frame.header.DLC = 2;
		 	         tx_response_frame.data[0] = CMD_PERFORMER_ID_SET;
		 	         tx_response_frame.data[1] = g_performer_id; // Подтверждаем установленный ID
		 	         osMessageQueuePut(can_tx_queueHandle, &tx_response_frame, 0, 0);
		 	         break;

		 	         default:
		 	        	 // Неизвестная команда
		 	             // TODO: Отправить ошибку через can_tx_queue
		 	         break;
		 	            }
		      }
	    }
}

