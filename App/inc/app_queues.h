/*
 * app_queues.h
 *
 *  Created on: Dec 9, 2025
 *      Author: andrey
 *
 *  Refactored: Mar 6, 2026
 * 		Author: andrey
 */

#ifndef APP_QUEUES_H_
#define APP_QUEUES_H_

#include "cmsis_os.h" // Для osMessageQueueId_t

// Глобальные хэндлы для всех очередей FreeRTOS
extern osMessageQueueId_t can_rx_queueHandle;   // Для приема сырых CAN-фреймов (ISR -> CAN Handler)
extern osMessageQueueId_t can_tx_queueHandle;   // Для отправки CAN-сообщений (любая задача -> CAN Handler)
extern osMessageQueueId_t parser_queueHandle;   // Для передачи команд (CAN Handler -> Command Parser)
extern osMessageQueueId_t domain_queueHandle;   // Command Parser -> Pump Controller

// Хэндл задачи CAN Handler (для osThreadFlagsSet)
extern osThreadId_t task_can_handleHandle;


#endif /* APP_QUEUES_H_ */
