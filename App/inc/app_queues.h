/*
 * app_queues.h
 *
 *  Created on: Dec 9, 2025
 *      Author: andrey
 *
 *  Refactored: Mar 6, 2026
 *  Updated: Apr 7, 2026 (Compliance with DDS-240)
 */

#ifndef APP_QUEUES_H_
#define APP_QUEUES_H_

#include "cmsis_os.h" // Для osMessageQueueId_t

// Глобальные хэндлы для всех очередей FreeRTOS
extern osMessageQueueId_t can_rx_queueHandle;   // Для приема сырых CAN-фреймов (ISR -> CAN Handler)
extern osMessageQueueId_t can_tx_queueHandle;   // Для отправки CAN-сообщений (любая задача -> CAN Handler)
extern osMessageQueueId_t dispatcher_queueHandle; // Для передачи команд (CAN Handler -> Dispatcher)
extern osMessageQueueId_t fluidics_queueHandle;   // Dispatcher -> Pump Controller

// Хэндлы задач (для osThreadFlagsSet)
extern osThreadId_t task_can_handleHandle;
extern osThreadId_t task_dispatcherHandle;      // <--- ОБНОВЛЕНО


#endif /* APP_QUEUES_H_ */
