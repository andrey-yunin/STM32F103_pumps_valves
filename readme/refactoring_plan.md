# План рефакторинга исполнителя насосов и клапанов

## 1. Общая цель

Привести исполнитель насосов и клапанов (`CAN_ADDR = 0x30`) в соответствие
с типовой архитектурой исполнителя ДДС-240 (описана в `executor_architecture_guide.md`).
Текущий код унаследован от проекта шаговых двигателей и использует устаревший
11-bit CAN-протокол, несовместимый с дирижером.

---

## 2. Исходное состояние (до рефакторинга)

| Параметр | Текущее значение | Требуемое значение |
|:---------|:-----------------|:-------------------|
| CAN ID | 11-bit StdId (`0x100 \| perf_id<<4 \| dev_id`) | 29-bit ExtID (`CAN_BUILD_ID(...)`) |
| CAN-адрес | `g_performer_id = 1` | `CAN_ADDR_PUMP_BOARD = 0x30` |
| Формат payload | `[cmd_1byte][payload_4bytes]` | `[cmd_code_2bytes_LE][device_id][params...]` |
| Коды команд | `CMD_SET_PUMP_STATE=0x10`, `CMD_SET_VALVE_STATE=0x11` | `CAN_CMD_PUMP_START=0x0202`, `CAN_CMD_PUMP_STOP=0x0203` |
| Ответы | Кустарный ACK через StdId 0x200 | ACK/NACK/DONE по протоколу дирижера |
| CAN RX interrupt | Только FIFO1 (CAN1_RX1_IRQHandler) | FIFO0 (USB_LP_CAN1_RX0_IRQHandler) |
| Задачи FreeRTOS | 2 (can_handler + dispatcher) | 3 (can_handler + command_parser + domain_controller) |
| Очереди | 3 (rx, tx, dispatcher) | 5 (rx, tx, parser, domain, can_tx) |
| Маппинг device_id | Нет | `DeviceMapping_ToPhysicalId()` |
| Остаточный код | motor_gpio.h, motion_planner.h, TIM2 extern | Удалён |

---

## 3. План работ по этапам

### Этап 0: Конфигурация оборудования (STM32CubeMX)

- [x] **0.1** Включён `USB_LP_CAN1_RX0` в NVIC Settings
- [x] **0.2** `USB_LP_CAN1_RX0_IRQHandler` появился в `stm32f1xx_it.c`
- [x] **0.3** Код перегенерирован
- [x] **0.4** Callback: добавлен `osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_RX)`

---

### Этап 1: Очистка остаточного кода шаговых двигателей

- [x] **1.0** Переименование task_dispatcher -> task_command_parser
    - Переименованы файлы: `task_dispatcher.c/.h` -> `task_command_parser.c/.h`
    - Переименована функция: `app_start_task_dispatcher` -> `app_start_task_command_parser`
    - Переименована очередь: `dispatcher_queueHandle` -> `parser_queueHandle`
    - Переименована константа: `DISPATCHER_QUEUE_LEN` -> `PARSER_QUEUE_LEN`
    - Обновлены: `main.c`, `app_queues.h`, `app_config.h`, `task_can_handler.c`

- [x] **1.1** Файл: `App/inc/motor_gpio.h`
    - Действие: УДАЛЁН

- [x] **1.2** Файл: `App/inc/motion_planner.h`
    - Действие: УДАЛЁН

- [x] **1.3** Файл: `Core/Src/stm32f1xx_it.c`
    - Удалены: `#include "motor_gpio.h"`, `#include "app_globals.h"`, `extern TIM_HandleTypeDef htim2;`

- [x] **1.4** Файл: `App/inc/app_globals.h`
    - Удалены: `extern volatile int8_t g_active_motor_id;`, `#include "app_config.h"`

---

### Этап 2: Создание модулей протокола

- [x] **2.1** Файл: `App/inc/can_protocol.h` (СОЗДАН)
    - Константы: приоритеты, типы сообщений, адреса узлов
    - `CAN_ADDR_THIS_BOARD = CAN_ADDR_PUMP_BOARD (0x30)`
    - Коды команд: `CAN_CMD_PUMP_START (0x0202)`, `CAN_CMD_PUMP_STOP (0x0203)`
    - Коды клапанов: `CAN_CMD_VALVE_OPEN (0x0204)`, `CAN_CMD_VALVE_CLOSE (0x0205)`
    - Макросы: `CAN_BUILD_ID`, `CAN_GET_*`
    - Коды ошибок: `CAN_ERR_UNKNOWN_CMD`, `CAN_ERR_INVALID_DEVICE_ID`, `CAN_ERR_DEVICE_BUSY`
    - Прототипы: `CAN_SendAck()`, `CAN_SendNackPublic()`, `CAN_SendDone()`

- [x] **2.2** Файлы: `App/inc/device_mapping.h`, `App/src/device_mapping.c` (СОЗДАНЫ)
    - Маппинг 13 насосов (device_id 10-22) и 3 клапанов (device_id 23-25)
    - `DeviceMapping_Resolve()` возвращает `DeviceMappingResult_t {physical_id, device_type}`
    - `DEVICE_ID_INVALID = 0xFF`
    - Типы: `DEVICE_TYPE_PUMP`, `DEVICE_TYPE_VALVE`, `DEVICE_TYPE_UNKNOWN`

---

### Этап 3: Обновление инфраструктурных модулей

- [x] **3.1** Файл: `App/inc/app_config.h`
    - Добавлены: `ParsedCanCommand_t`, `PumpValveCommand_t`, `FLAG_CAN_RX/TX`, `DOMAIN_QUEUE_LEN`

- [x] **3.2** Файл: `App/inc/app_queues.h`
    - Добавлены: `domain_queueHandle`, `task_can_handleHandle`

- [x] **3.3** Файл: `App/inc/app_globals.h`
    - Очищен на Этапе 1, содержит только `g_performer_id`

- [x] **3.4** Файл: `App/src/app_globals.c`
    - Корректен, содержит только `g_performer_id`

---

### Этап 4: Переработка задач

- [x] **4.1** Файл: `App/src/tasks/task_can_handler.c`
    - Полная переработка: 29-bit ExtID, event-driven (osThreadFlags)
    - CAN-фильтр по DstAddr=0x30, валидация ExtID/MsgType/DLC
    - Публичные функции: `CAN_SendAck()`, `CAN_SendNackPublic()`, `CAN_SendDone()`
    - `command_protocol.h` заменён на `can_protocol.h`

- [x] **4.2** Файл: `App/src/tasks/task_command_parser.c`
    - Полная переработка: `ParsedCanCommand_t`, маппинг через `DeviceMapping_Resolve()`
    - NACK при невалидном device_id или несовпадении типа устройства/команды
    - Парсинг: PUMP_START/STOP, VALVE_OPEN/CLOSE
    - Формирование `PumpValveCommand_t` -> `domain_queue`

- [x] **4.3** Файл: `App/src/tasks/task_pump_controller.c` (СОЗДАН)
    - Доменный контроллер насосов и клапанов
    - Приём `PumpValveCommand_t` из `domain_queue`
    - ACK при приёме, исполнение GPIO, DONE после выполнения
    - Header: `App/inc/tasks/task_pump_controller.h`

---

### Этап 5: Исправление модуля GPIO

- [x] **5.1** Файл: `App/inc/pumps_valves_gpio.h`
    - Удалить enum-ы `PumpID_t` и `ValveID_t` (не нужны — device_mapping даёт 0-based physical_id)
    - Параметры функций: `uint8_t pump_idx` / `uint8_t valve_idx` (0-based)
    - **Баги в текущем коде:**
      - `PUMP_1=0x1` — enum с 1, массивы с 0 → обращение к соседнему насосу
      - `PUMP_10=0x10` (=16 десятичное!) → выход за границы массива
      - Проверка `pump_id < NUM_PUMPS` при PUMP_13=0x13=19 → всегда false для насосов 10-13

- [x] **5.2** Файл: `App/src/pumps_valves_gpio.c`
    - Параметры: `uint8_t pump_idx` / `uint8_t valve_idx` (0-based)
    - Проверка границ: `pump_idx < NUM_PUMPS` корректна при 0-based входе

---

### Этап 6: Обновление main.c и прерываний

- [x] **6.1** Файл: `Core/Src/main.c`
    - `parser_queue` → `sizeof(ParsedCanCommand_t)` (было `CAN_Command_t`)
    - Добавлено создание `domain_queueHandle`
    - Задача `task_pump_controller` создана через CubeMX (stack 128*4, osPriorityNormal)
    - Удалён `#include "command_protocol.h"`, добавлен `#include "task_pump_controller.h"`
    - Вызов `app_start_task_pump_controller(argument)` в wrapper-функции

- [x] **6.2** Файл: `Core/Src/stm32f1xx_it.c`
    - Callback `HAL_CAN_RxFifo0MsgPendingCallback`: `osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_RX)`
    - Выполнено на Этапе 0

---

### Этап 7: Удаление command_protocol.h

- [x] **7.1** Файл: `App/inc/command_protocol.h`
    - УДАЛИТЬ файл (заменён на `can_protocol.h`)
    - Убедиться, что ни один файл не ссылается на него

---

### Этап 8: Тестирование

- [x] **8.1** Подготовить тестовый CAN-кадр от дирижера:
    - `PUMP_START` для device_id=10 (DEV_WASH_PUMP_FILL):
    - ExtID = `CAN_BUILD_ID(0, 0, 0x30, 0x10)` = `0x00301000`
    - DLC = 3, Data = `02 02 0A` (cmd=0x0202, device=10)
- [x] **8.2** Ожидаемый результат:
    1. ACK от исполнителя (ExtID с MsgType=1)
    2. GPIO насоса 0 (physical) -> HIGH
    3. DONE от исполнителя (ExtID с MsgType=3, SubType=0x01, device_id=10)

---

## 4. Структуры данных

### 4.1. ParsedCanCommand_t (общая, из протокола дирижера)
```c
typedef struct {
    uint16_t cmd_code;      // CAN-код команды (байты 0-1 payload, LE)
    uint8_t  device_id;     // Логический ID устройства (байт 2 payload)
    uint8_t  data[5];       // Сырые данные параметров (байты 3-7 payload)
    uint8_t  data_len;      // Количество байт в data (DLC - 3)
} ParsedCanCommand_t;
```

### 4.2. PumpValveCommand_t (доменная структура)
```c
typedef struct {
    uint8_t  physical_id;   // Физический индекс (0-based) в массиве GPIO
    uint8_t  device_type;   // DEVICE_TYPE_PUMP или DEVICE_TYPE_VALVE
    uint16_t cmd_code;      // Для ACK/NACK/DONE
    uint8_t  device_id;     // Логический ID (для DONE)
    bool     state;         // true=ON/OPEN, false=OFF/CLOSE
} PumpValveCommand_t;
```

---

## 5. Маппинг устройств

| Логический device_id | Константа | Физический индекс | Тип |
|:---------------------|:----------|:-------------------|:----|
| 10 | DEV_WASH_PUMP_FILL | 0 | PUMP |
| 11 | DEV_WASH_PUMP_DRAIN | 1 | PUMP |
| ... | (уточнить остальные 11 насосов и 3 клапана) | ... | ... |

**Замечание:** Точный маппинг всех 13 насосов и 3 клапанов на логические device_id
требует уточнения. В `device_mapping.h` дирижера определены только `DEV_WASH_PUMP_FILL(10)`
и `DEV_WASH_PUMP_DRAIN(11)`. Остальные ID необходимо согласовать и добавить в реестр.

---

*Документ создан: 06.03.2026*
*Основан на архитектуре executor_architecture_guide.md и аудите текущего кода*
