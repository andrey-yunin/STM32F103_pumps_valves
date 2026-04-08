/*
 * device_mapping.c
 *
 *  Created on: Mar 6, 2026
 *      Author: andrey
 */


#include "device_mapping.h"
#include "app_flash.h"
#include "app_config.h"


DeviceMappingResult_t DeviceMapping_Resolve(uint8_t device_id)
{
	DeviceMappingResult_t result = { .physical_id = DEVICE_ID_INVALID, .device_type = DEVICE_TYPE_UNKNOWN };

	// Поиск по всей таблице маппинга (16 каналов)
	for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
		if (AppConfig_GetFluidicLogicalID(i) == device_id) {
			result.physical_id = i;
			// Первые NUM_PUMPS индексов - это насосы, остальные - клапаны
			if (i < NUM_PUMPS) {
				result.physical_id = i;
				result.device_type = DEVICE_TYPE_PUMP;
				}
			else
			{
				result.physical_id = i - NUM_PUMPS;
				result.device_type = DEVICE_TYPE_VALVE;
				}
			break;
			}
		}
	return result;
}




