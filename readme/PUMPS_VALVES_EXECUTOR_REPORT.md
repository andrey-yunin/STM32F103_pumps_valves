# Отчёт: Исполнитель насосов и клапанов (Pump Board, CAN_ADDR=0x30)

## 1. Назначение

Исполнитель управляет 13 насосами и 3 клапанами биохимического анализатора ДДС-240
через GPIO выходы микроконтроллера STM32F103C8T6. Получает команды от дирижера
по CAN-шине и выполняет включение/выключение устройств.

## 2. Аппаратная платформа

| Параметр | Значение |
|:---------|:---------|
| MCU | STM32F103C8T6 (20 КБ RAM, 64 КБ Flash) |
| Тактирование | HSI 8 МГц -> PLL x16 -> SYSCLK 64 МГц |
| APB1 | 32 МГц (для CAN) |
| APB2 | 64 МГц |
| CAN | bxCAN, 500 кбит/с, 29-bit Extended ID |
| RTOS | FreeRTOS, CMSIS_V2 |

## 3. Распиновка GPIO

### 3.1. Насосы (13 шт.)

| Физ. индекс | Метка CubeMX | Пин | Порт |
|:------------|:-------------|:-----|:-----|
| 0 | PUMP_1 | PA0 | GPIOA |
| 1 | PUMP_2 | PA1 | GPIOA |
| 2 | PUMP_3 | PA2 | GPIOA |
| 3 | PUMP_4 | PA3 | GPIOA |
| 4 | PUMP_5 | PA4 | GPIOA |
| 5 | PUMP_6 | PA5 | GPIOA |
| 6 | PUMP_7 | PA6 | GPIOA |
| 7 | PUMP_8 | PA7 | GPIOA |
| 8 | PUMP_9 | PA8 | GPIOA |
| 9 | PUMP_10 | PA9 | GPIOA |
| 10 | PUMP_11 | PA10 | GPIOA |
| 11 | PUMP_12 | PA15 | GPIOA |
| 12 | PUMP_13 | PB0 | GPIOB |

### 3.2. Клапаны (3 шт.)

| Физ. индекс | Метка CubeMX | Пин | Порт |
|:------------|:-------------|:-----|:-----|
| 0 | VALVE_1 | PB1 | GPIOB |
| 1 | VALVE_2 | PB2 | GPIOB |
| 2 | VALVE_3 | PB3 | GPIOB |

### 3.3. CAN

| Сигнал | Пин |
|:-------|:----|
| CAN_RX | PB8 |
| CAN_TX | PB9 |

### 3.4. Прочее

| Сигнал | Пин | Назначение |
|:-------|:----|:-----------|
| GPIO | PC13 | Светодиод на плате (отладка) |

## 4. История разработки

### 4.1. Фаза 1: Создание проекта (декабрь 2025)

- Проект скопирован из `STM32F103_step_motors`
- Переименованы файлы проекта (.ioc, .project, .launch)
- `task_command_parser` переименован в `task_dispatcher`
- `g_performer_id` установлен в 1
- В CubeMX отключены TIM1, TIM2, RTC, USART1, USART2
- Настроены GPIO для 13 насосов и 3 клапанов
- Создан модуль `pumps_valves_gpio.c/.h`
- Удалён код шаговых двигателей из dispatcher и app_config
- Добавлены команды `CMD_SET_PUMP_STATE (0x10)`, `CMD_SET_VALVE_STATE (0x11)`
- Проект собирается без ошибок

### 4.2. Фаза 2: Аудит перед интеграцией с дирижером (06.03.2026)

Выявлены критические несовместимости с протоколом дирижера:

1. **CAN ID**: 11-bit StdId вместо 29-bit ExtID
2. **Payload**: однобайтовые команды вместо двухбайтовых cmd_code + device_id
3. **Ответы**: кустарный ACK вместо стандартных ACK/NACK/DONE
4. **CAN RX**: отсутствует обработчик прерывания FIFO0 (есть только FIFO1)
5. **Маппинг**: нет трансляции логических device_id дирижера
6. **Остаточный код**: motor_gpio.h, motion_planner.h, extern TIM2
7. **Баг индексации**: PumpID_t начинается с 1, массив с 0

### 4.3. Фаза 3: Рефакторинг (06.03.2026)

Подробный план — см. `refactoring_plan.md`.

Выполнено:
- **Этапы 0-4**: Полный переход на 29-bit ExtID CAN-протокол дирижера
- CAN-адрес: `CAN_ADDR_PUMP_BOARD = 0x30`
- 3-задачная архитектура: CAN Handler → Command Parser → Pump Controller
- Стандартные ответы ACK/NACK/DONE (`CAN_SendAck`, `CAN_SendNackPublic`, `CAN_SendDone`)
- Маппинг device_id через `device_mapping.c` (13 насосов + 3 клапана)
- Удаление кода шаговых двигателей (motor_gpio.h, motion_planner.h, TIM2)
- Удалён `command_protocol.h` из включений main.c (файл подлежит удалению)
- **Этап 6**: main.c обновлён — domain_queue, task_pump_controller, ParsedCanCommand_t
- **Этап 5 (10.03.2026)**: Исправление индексации в `pumps_valves_gpio.h/.c` (критический баг устранен).
- **Этап 7**: Удаление файла `command_protocol.h`.
- **Сборка**: Проект собирается без ошибок.

### 4.4. Фаза 4: Синхронизация с экосистемой DDS-240 (07.04.2026)

Проведен аудит и приведение архитектуры к стандарту версии 1.1 (Апрель 2026).

**Выполненные изменения:**
- **Именование задач**: `task_command_parser` переименована обратно в `task_dispatcher` (согласно стандарту).
- **Унификация очередей**: `parser_queueHandle` -> `dispatcher_queueHandle`, `domain_queueHandle` -> `fluidics_queueHandle`.
- **Обновление конфигурации**: В `app_config.h` добавлены макросы версий прошивки и типа устройства `CAN_DEVICE_TYPE_PUMP (0x30)`.
- **Исправление типов**: Добавлен `stdbool.h` для корректной работы с типом `bool`.
- **Инфраструктура**: Файл `main.c` очищен от мусора и приведен к стандарту.
- **Flash и Идентификация**: Создан модуль `app_flash.h/c`, реализовано чтение UID и хранение маппинга во Flash (Page 63). Модуль интегрирован в `main.c`.

**Чек-лист соответствия (Compliance Checklist):**
- [x] **Архитектура**: Имена задач соответствуют стандарту (`task_dispatcher`).
- [x] **Именование IPC**: Очереди переименованы (`dispatcher_queueHandle`, `fluidics_queueHandle`).
- [x] **Идентификация**: Чтение 96-bit MCU UID реализовано в `app_flash.c`.
- [x] **Flash-конфиг**: Хранение NodeID на Page 63 с Magic Key и CRC16 реализовано.
- [ ] **Сервисный слой**: Поддержка команд `0xF0xx` (Info, Reboot, Set NodeID).
- [ ] **Транспорт**: Mailbox Guard и Broadcast (ID 0x00).
- [ ] **Fluidics**: Реализованы защитные таймауты (Safety Timeout).

**Текущий статус**: "Infrastructure Synchronized & Flash Ready" (Инфраструктура и Flash-слой готовы).

## 5. Таблица текущего состояния компонентов

| Component | Status |
|:----------|:-------|
| GPIO насосов/клапанов | ВЫПОЛНЕНО (модуль `pumps_valves_gpio`) |
| Архитектура задач (DDS-240) | ВЫПОЛНЕНО (`task_dispatcher`, `fluidics_queueHandle`) |
| CAN-протокол (29-bit ExtID) | ВЫПОЛНЕНО |
| ACK/NACK/DONE | ВЫПОЛНЕНО |
| can_protocol.h | СОЗДАН |
| device_mapping | СОЗДАН |
| Идентификация и Flash-конфиг | ВЫПОЛНЕНО (`app_flash`) |

---

*Заметки от 10.03.2026:*

1. **Маппинг device_id**: В реестре дирижера (`device_mapping.h`) определены
   только 2 насоса (`DEV_WASH_PUMP_FILL=10`, `DEV_WASH_PUMP_DRAIN=11`).
   Необходимо определить логические ID для всех 13 насосов и 3 клапанов.

2. **Таймер работы насоса**: Нужна ли автоматическая остановка насоса по таймеру
   (safety timeout)? Если да — потребуется FreeRTOS software timer.

3. **Команды клапанов**: В `can_packer.h` дирижера нет отдельных кодов команд
   для клапанов. Использовать `PUMP_START/STOP` для клапанов или добавить
   `CAN_CMD_VALVE_OPEN/CLOSE`?

---

## 7. Архитектура задач FreeRTOS

| Задача | Стек | Приоритет | Назначение |
|:-------|:-----|:----------|:-----------|
| task_can_handle | 128×4 (512 Б) | High | Транспорт: CAN RX/TX, фильтрация, event-driven |
| task_dispatcher | 128×4 (512 Б) | Normal | Прикладная логика, парсинг, сервисные команды |
| task_pump_contr | 128×4 (512 Б) | Normal | Исполнение GPIO, ACK/DONE |

## 8. Очереди FreeRTOS

| Очередь | Размер | Элемент | Направление |
|:--------|:-------|:--------|:------------|
| can_rx_queue | 16 | CanRxFrame_t | ISR → CAN Handler |
| can_tx_queue | 16 | CanTxFrame_t | Любая задача → CAN Handler |
| dispatcher_queue | 10 | ParsedCanCommand_t | CAN Handler → Dispatcher |
| fluidics_queue | 8 | PumpValveCommand_t | Dispatcher → Pump Controller |

---

*Документ обновлён: 07.04.2026*
