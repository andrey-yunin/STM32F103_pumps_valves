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

Выполненные корректировки:

1. **Переименование task_dispatcher -> task_command_parser** — приведение
   к единой архитектуре с проектом шаговых двигателей. Затронуты файлы:
   `task_command_parser.c/.h`, `main.c`, `app_queues.h`, `app_config.h`,
   `task_can_handler.c`. CubeMX сгенерировал укороченное имя задачи
   `task_command_pa` (ограничение 15 символов) — функционально не влияет.

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

Не выполнено:
- **Этап 5**: Исправление индексации в `pumps_valves_gpio.h/.c` (критический баг)
- **Этап 7**: Удаление файла `command_protocol.h`
- **Этап 8**: Тестирование
| Component | Status |
|:----------|:-------|
| GPIO насосов/клапанов | Настроен в CubeMX, модуль создан |
| pumps_valves_gpio индексация | ВЫПОЛНЕНО (устранено смещение и hex-ошибки) |
| Переименование task -> command_parser | ВЫПОЛНЕНО |
| CAN RX0 interrupt | ВКЛЮЧЕН, callback с FLAG_CAN_RX |
| CAN-протокол дирижера (29-bit ExtID) | ВЫПОЛНЕНО |
| ACK/NACK/DONE | ВЫПОЛНЕНО |
| can_protocol.h | СОЗДАН |
| device_mapping | СОЗДАН |
| task_command_parser | ВЫПОЛНЕНО |
| task_pump_controller | ВЫПОЛНЕНО |
| main.c | ВЫПОЛНЕНО |
| Очистка от шаговых | ВЫПОЛНЕНО |
| command_protocol.h | УДАЛЁН |
| Сборка | ВЫПОЛНЕНО (сборка без ошибок) |

---
...
---

*Документ обновлён: 10.03.2026*

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
| task_command_pa | 128×4 (512 Б) | Normal | Парсинг cmd_code, маппинг device_id, валидация |
| task_pump_contr | 128×4 (512 Б) | Normal | Исполнение GPIO, ACK/DONE |

## 8. Очереди FreeRTOS

| Очередь | Размер | Элемент | Направление |
|:--------|:-------|:--------|:------------|
| can_rx_queue | 10 | CanRxFrame_t | ISR → CAN Handler |
| can_tx_queue | 10 | CanTxFrame_t | Любая задача → CAN Handler |
| parser_queue | 10 | ParsedCanCommand_t | CAN Handler → Command Parser |
| domain_queue | 5 | PumpValveCommand_t | Command Parser → Pump Controller |

---

*Документ обновлён: 06.03.2026*
