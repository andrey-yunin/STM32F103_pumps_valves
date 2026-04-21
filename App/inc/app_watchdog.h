/*
 * app_watchdog.h
 *
 * Heartbeat API for the IWDG supervisor.
 *
 * Each critical task periodically reports progress through
 * AppWatchdog_Heartbeat(). The supervisor refreshes IWDG only when all
 * registered clients have progressed since the previous check.
 */

#ifndef APP_WATCHDOG_H_
#define APP_WATCHDOG_H_

#include <stdint.h>

#define APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS    500U

typedef enum {
	APP_WDG_CLIENT_CAN = 0,
	APP_WDG_CLIENT_DISPATCHER,
	APP_WDG_CLIENT_FLUIDICS,
	APP_WDG_CLIENT_COUNT
} AppWatchdogClient_t;

void AppWatchdog_Heartbeat(AppWatchdogClient_t client);

#endif /* APP_WATCHDOG_H_ */
