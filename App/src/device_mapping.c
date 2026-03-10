/*
 * device_mapping.c
 *
 *  Created on: Mar 6, 2026
 *      Author: andrey
 */


#include "device_mapping.h"

DeviceMappingResult_t DeviceMapping_Resolve(uint8_t device_id)
{
	DeviceMappingResult_t result;

	switch (device_id)
		{
			// --- Насосы: device_id -> физический индекс в массиве pump_ports[] ---
			case DEV_WASH_PUMP_FILL:
				result.physical_id = 0;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_WASH_PUMP_DRAIN:
				result.physical_id = 1;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_3:
				result.physical_id = 2;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_4:
				result.physical_id = 3;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_5:
				result.physical_id = 4;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_6:
				result.physical_id = 5;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_7:
				result.physical_id = 6;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_8:
				result.physical_id = 7;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_9:
				result.physical_id = 8;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_10:
				result.physical_id = 9;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_11:
				result.physical_id = 10;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_12:
				result.physical_id = 11;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

			case DEV_PUMP_13:
				result.physical_id = 12;
				result.device_type = DEVICE_TYPE_PUMP;
				break;

          // --- Клапаны: device_id -> физический индекс в массиве valve_ports[] ---

			case DEV_VALVE_1:
				result.physical_id = 0;
				result.device_type = DEVICE_TYPE_VALVE;
				break;

			case DEV_VALVE_2:
				result.physical_id = 1;
				result.device_type = DEVICE_TYPE_VALVE;
				break;

			case DEV_VALVE_3:
				result.physical_id = 2;
				result.device_type = DEVICE_TYPE_VALVE;
				break;

			default:
				result.physical_id = DEVICE_ID_INVALID;
				result.device_type = DEVICE_TYPE_UNKNOWN;
				break;
	}
	return result;
}




