/*
 * pumps_valves_gpio.h
 *
 *Модуль низкоуровневого управления GPIO для насосов и клапанов.
 * Использует 0-ориентированные физические индексы (physical_id).
 *
 *  Created on: Dec 12, 2025
 *      Author: andrey
 *  Refactored: Mar 10, 2026
 *		Author: andrey
 *
 *
 */

#ifndef PUMPS_VALVES_GPIO_H_
#define PUMPS_VALVES_GPIO_H_

#include <stdint.h>
#include <stdbool.h>

/* Модуль управления GPIO для насосов и клапанов. */
/* Использует физические индексы (0-based) для доступа к массивам портов и пинов. */

/**
 * Устанавливает состояние для насоса.
 * @param pump_idx Индекс в массиве (0..NUM_PUMPS-1).
 * @param is_on Включение (true) или выключение (false).
 */
void PumpsValves_SetPumpState(uint8_t pump_idx, bool is_on);

/**
 * Устанавливает состояние для клапана.
 * @param valve_idx Индекс в массиве (0..NUM_VALVES-1).
 * @param is_open Открытие (true) или закрытие (false).
 */
void PumpsValves_SetValveState(uint8_t valve_idx, bool is_open);

/**
 * Переводит все насосы и клапаны в безопасное состояние OFF.
 */
void PumpsValves_AllOff(void);




#endif /* PUMPS_VALVES_GPIO_H_ */
