/*
 * task_dispatcher.h
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 *
 *  Refactored on Mar 6- 2026
 *  Refactored on Apr 7, 2026 (Compliance with DDS-240)
 */

#ifndef TASK_DISPATCHER_H_
#define TASK_DISPATCHER_H_

#include <stdint.h>

/**
 * Точка входа для задачи диспетчера (Application Logic).
 * Вызывается из обертки CubeMX в main.c.
 */
void app_start_task_dispatcher(void *argument);

#endif /* TASK_DISPATCHER_H_ */
