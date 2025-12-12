/*
 * pumps_valves_gpio.h
 *
 *  Created on: Dec 12, 2025
 *      Author: andrey
 */

#ifndef PUMPS_VALVES_GPIO_H_
#define PUMPS_VALVES_GPIO_H_

#include <stdint.h>
#include <stdbool.h>

// --- Определения ID для насосов и клапанов ---
// Эти ID будут использоваться в командах CAN и в функциях управления.
// Вы можете расширить эти списки по мере необходимости.
typedef enum {
	PUMP_1  = 0x1,
	PUMP_2  = 0x2,
	PUMP_3  = 0x3,
	PUMP_4  = 0x4,
	PUMP_5  = 0x5,
	PUMP_6  = 0x6,
	PUMP_7  = 0x7,
	PUMP_8  = 0x8,
	PUMP_9  = 0x9,
	PUMP_10 = 0x10,
	PUMP_11 = 0x11,
	PUMP_12 = 0x12,
	PUMP_13 = 0x13
} PumpID_t;

typedef enum {
	VALVE_1 = 0x1,
	VALVE_2 = 0x2,
	VALVE_3 = 0x3
} ValveID_t;

// --- Прототипы функций управления ---
// Эти функции будут изменять состояние соответствующих GPIO.

/**
 * @brief Устанавливает состояние для указанного насоса.
 * @param pump_id ID насоса (из PumpID_t).
 * @param is_on true для включения, false для выключения.
 */
void PumpsValves_SetPumpState(PumpID_t pump_id, bool is_on);

/**
  * @brief Устанавливает состояние для указанного клапана.
  * @param valve_id ID клапана (из ValveID_t).
  * @param is_open true для открытия, false для закрытия.
  */
void PumpsValves_SetValveState(ValveID_t valve_id, bool is_open);







#endif /* PUMPS_VALVES_GPIO_H_ */
