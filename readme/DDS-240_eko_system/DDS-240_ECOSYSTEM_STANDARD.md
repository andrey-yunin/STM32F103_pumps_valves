# DDS-240 ECOSYSTEM STANDARD (Advanced Level)
**Версия:** 1.2 (Апрель 2026)  
**Статус:** Обязателен для всех Executor-устройств на базе STM32.

## 1. Архитектурный фундамент
Все устройства должны строиться на базе FreeRTOS (CMSIS_V2) с четким разделением ответственности между задачами.

### 1.1. Обязательные задачи (Tasks)
1.  **Transport Task (`task_can_handler`)**: Приоритет `High`. Только прием/передача сырых фреймов.
2.  **Dispatcher Task (`task_dispatcher`)**: Приоритет `Normal`. Прикладная логика, транзакции, сервисный слой.
3.  **Application Task(s)**: Приоритет `Realtime` (движение) или `BelowNormal` (мониторинг). Прямое взаимодействие с железом.

### 1.2. Межзадачное взаимодействие (IPC)
*   **Очереди (`osMessageQueue`)**: Категорически запрещено использование глобальных переменных для передачи команд. Только очереди с фиксированным размером структур.
*   **Флаги (`osThreadFlags`)**: Использование для Event-driven уведомлений (например, `FLAG_CAN_RX`).
*   **Инкапсуляция**: Глобальные данные (Shadow RAM Config) должны быть скрыты внутри модулей (`static`) и доступны только через Thread-safe API (Mutex-protected).

---

## 2. Стандарт CAN-связи (Транспорт)

### 2.1. Физический уровень
*   **Bitrate**: 1 Mbit/s.
*   **Executor DLC Policy**: Strict DLC=8 for `Conductor -> Executor` and `Executor -> Conductor` CAN frames. Unused bytes are padded with `0x00`.
*   **Host Boundary**: This executor policy does not change `Host -> Conductor` / `User_Commands`, where command-specific dynamic DLC remains allowed.
*   **ID Format**: 29-bit Extended ID.
*   **Addressing**: Динамический `NodeID` (0x02..0x7F). Адрес `0x10` зарезервирован за Дирижером.
*   **Reference Timing**: STM32F103 executor profile verified at APB1=32 MHz, Prescaler=2, BS1=11TQ, BS2=4TQ, SJW=1TQ, sample point 75%.

### 2.2. Фильтрация и надежность
*   **Broadcast**: Каждое устройство ОБЯЗАНО принимать сообщения с `DstAddr = 0x00`.
*   **Mailbox Guard**: Перед вызовом `HAL_CAN_AddTxMessage` обязательна проверка наличия свободных ящиков (`HAL_CAN_GetTxMailboxesFreeLevel`) с таймаутом ожидания до 10мс.
*   **Transport Envelope Drop**: Кадры с невалидной транспортной оболочкой игнорируются без `ACK/NACK`. К таким кадрам относятся: не Extended ID, чужой `DstAddr`, неверный `Message Type`, `DLC != 8` для executor-уровня.
*   **Conductor ACK Timeout**: Дирижер обязан трактовать отсутствие `ACK` в заданное окно как transport/protocol timeout и применять retry/fault policy.

### 2.3. Жизненный цикл транзакции
Любая команда от Дирижера должна пройти цикл:
1.  **ACK (0x01)**: Немедленное подтверждение приема (отправляет Диспетчер).
2.  **DATA (0x03, Sub:0x02)**: (Опционально) Передача промежуточных результатов.
3.  **DONE (0x03, Sub:0x01)**: Финальное подтверждение успешного выполнения.
4.  **NACK (0x02)**: Ошибка выполнения с кодом из единого реестра.

`NACK` отправляется только для транспортно валидных `COMMAND`-кадров (`Extended ID`, наш/broadcast `DstAddr`, `Message Type=COMMAND`, `DLC=8`), которые не прошли прикладную валидацию.

### 2.3.1. Обязанности Дирижера при промышленной эксплуатации
Дирижер является владельцем сценария и обязан отличать транспортные ошибки от прикладных отказов:

*   **ACK timeout**: отсутствие `ACK` не является `NACK`; это transport/protocol timeout с отдельной retry/fault policy.
*   **Operation timeout**: для каждой исполнительной команды Дирижер задает верхний предел ожидания `DONE`. Safety timeout исполнителя не заменяет сценарный timeout Дирижера.
*   **Service recovery**: после `COMMIT`, `REBOOT`, `FACTORY_RESET` и смены `NodeID` Дирижер выполняет повторный discovery и сверку `NodeID`, `device_type`, `UID`.
*   **Timeout encoding**: длительности на executor-уровне передаются как `uint32_t` Little-Endian в payload bytes 3-6 при `DLC=8`.
*   **Host boundary**: динамический DLC допустим только на уровне `Host -> Conductor`; на уровне `Conductor <-> Executor` Дирижер обязан формировать strict `DLC=8`.

### 2.4. Приемочный CAN-набор для каждого Executor
Каждая плата-исполнитель считается совместимой с DDS-240 только после прохождения общего набора CAN-тестов:

*   **Discovery**: `0xF001 GET_DEVICE_INFO` возвращает `ACK -> DATA... -> DONE`.
*   **Unknown Command**: неизвестный `cmd_code` возвращает `NACK`.
*   **Invalid Channel**: индекс канала вне диапазона возвращает `NACK`.
*   **Invalid Resource Type**: команда не своего типа ресурса возвращает `NACK` (например, команда насоса на индекс клапана).
*   **Invalid Magic Key**: сервисная команда с неверным ключом возвращает `NACK`.
*   **Broadcast**: команда с `DstAddr = 0x00` принимается, если команда разрешена для broadcast.
*   **Foreign Destination**: команда с чужим `DstAddr` игнорируется без ACK/NACK.
*   **Strict Frame Format**: все executor-кадры используют 29-bit Extended ID и `DLC=8`; кадры с `DLC != 8` игнорируются без ACK/NACK и обрабатываются Дирижером по ACK timeout.

### 2.5. Safety / Failsafe Policy
Каждый Executor обязан иметь явно описанное безопасное состояние:

*   При старте и после reset все исполнительные выходы переводятся в safe state.
*   При потере связи исполнитель не должен оставаться в опасном активном состоянии без ограничения по времени.
*   Длительные действия должны иметь timeout или иной аппаратно-программный предел.
*   Если safety timer или иной механизм ограничения не может быть запущен, исполнитель обязан оставить/вернуть канал в safe state и завершить команду ошибкой.
*   Автоотключение по safety timeout должно быть задокументировано: отправляется диагностическое событие или состояние фиксируется для последующего запроса.
*   Safe state определяется доменом платы: Fluidics - насосы/клапаны OFF, Motion - остановка движения, Thermo - отсутствие опасного управляющего воздействия.

### 2.5.1. Mandatory Safe-State Hook
Каждая STM32 Executor-плата обязана иметь доменный safe-state hook: одну функцию или минимальный набор функций, которые переводят все исполнительные выходы платы в безопасное состояние.

Требования к safe-state hook:

*   Вызывается после инициализации GPIO, до приема исполнительных команд.
*   Вызывается из `Error_Handler()`.
*   Вызывается из fault handlers до входа в бесконечный аварийный цикл: `NMI_Handler`, `HardFault_Handler`, `MemManage_Handler`, `BusFault_Handler`, `UsageFault_Handler`.
*   Не использует RTOS-очереди, mutex, динамическую память, CAN-отправку или длительные задержки.
*   Не должен зависеть от работоспособности планировщика FreeRTOS.
*   Допускает прямую работу с GPIO/HAL или регистрами, необходимую для безопасного выключения выходов.

Доменная трактовка:

*   **Fluidics**: все насосы и клапаны переводятся в `OFF`.
*   **Motion**: прекращаются STEP-импульсы, драйверы переводятся в disable/safe stop, если это поддержано железом.
*   **Thermo**: нагреватели, ШИМ и силовые выходы переводятся в `OFF`; датчики могут оставаться пассивными.

Если safe-state hook был вызван из fault handler, Executor не обязан отправлять `NACK/DONE`: связь может быть недоступна, а приоритетом является физическая безопасность. Дирижер обязан обнаружить такую ситуацию по timeout связи и выполнить recovery/discovery после восстановления узла.

### 2.6. CAN Fault Handling
Каждый Executor должен иметь единый минимум диагностики CAN:

*   Обработка `bus-off` и восстановление после перезапуска CAN-контроллера.
*   Счетчики CAN-ошибок: bus errors, error warning, error passive, bus-off, restart count.
*   Диагностическая service-команда или DATA/LOG-ответ для чтения состояния транспорта.
*   Зафиксированное поведение при переполнении RX/TX очередей.

### 2.7. Watchdog
Для промышленной готовности STM32 Executor-платы должны использовать аппаратный IWDG:

*   IWDG обслуживается только при штатной работе основных задач.
*   После watchdog reset GPIO переводятся в safe state до приема новых команд.
*   Причина последнего reset должна быть доступна через сервисный статус или диагностический ответ.

---

## 3. Сервисный слой и Идентификация (0xFxxx)
Каждое устройство обязано поддерживать диапазон команд `0xF001 - 0xF00F` для обслуживания.

### 3.1. Обязательные сервисные команды
*   **`0xF001` (Get Info)**: Возврат типа устройства, версии прошивки и UID чипа.
*   **`0xF002` (Reboot)**: Перезагрузка по Magic Key `0x55AA`.
*   **`0xF003` (Commit)**: Сохранение текущих настроек из RAM во Flash.
*   **`0xF005` (Set NodeID)**: Изменение сетевого адреса устройства.
*   **`0xF006` (Factory Reset)**: Очистка Flash-конфигурации по Magic Key `0xDEAD`.
*   **`0xF007` (Get Status)**: Возврат диагностического статуса исполнителя, включая CAN fault counters и счетчики переполнения очередей.

### 3.2. Единый формат `GET_DEVICE_INFO`
Ответ `0xF001` должен начинаться с ACK, затем передавать один или несколько DATA-кадров и завершаться DONE.

Первый DATA-кадр обязан иметь следующий формат:

| Byte | Поле | Описание |
|:-----|:-----|:---------|
| 0 | `0x02` | Sub-Type DATA |
| 1 | `0x80` | EOT=1 для одиночного фрагмента или sequence info для многофрагментного ответа |
| 2 | `device_type` | Тип устройства из раздела 3.4 |
| 3 | `fw_major` | Major версия прошивки |
| 4 | `fw_minor` | Minor версия прошивки |
| 5 | `channel_count` | Количество каналов/ресурсов платы |
| 6-7 | `uid0..uid1` | Начало MCU UID или `0x00` |

Если UID не помещается в первый DATA-кадр, исполнитель передает дополнительные DATA-кадры перед DONE.

### 3.2.1. Единый формат `GET_STATUS (0xF007)`
Ответ `0xF007` должен начинаться с ACK, затем передавать набор DATA-кадров и завершаться DONE.

Каждый DATA-кадр статуса содержит одну метрику:

| Byte | Поле | Описание |
|:-----|:-----|:---------|
| 0 | `0x02` | Sub-Type DATA |
| 1 | `0x80` | EOT=1 для текущей метрики |
| 2-3 | `metric_id` | Идентификатор метрики, `uint16_t LE` |
| 4-7 | `value` | Значение метрики, `uint32_t LE` |

Обязательные CAN diagnostic metrics для STM32 Executor:

| Metric ID | Метрика |
|:----------|:--------|
| `0x0001` | RX frames accepted by transport task |
| `0x0002` | TX frames submitted to HAL |
| `0x0003` | RX queue overflow |
| `0x0004` | TX queue overflow |
| `0x0005` | Dispatcher queue overflow |
| `0x0006` | Dropped frame: not Extended ID |
| `0x0007` | Dropped frame: foreign destination |
| `0x0008` | Dropped frame: non-COMMAND message type |
| `0x0009` | Dropped frame: invalid executor DLC |
| `0x000A` | TX mailbox guard timeout |
| `0x000B` | `HAL_CAN_AddTxMessage` error |
| `0x000C` | CAN error callback count |
| `0x000D` | CAN error-warning count |
| `0x000E` | CAN error-passive count |
| `0x000F` | CAN bus-off count |
| `0x0010` | Last HAL CAN error code |
| `0x0011` | Last CAN ESR register snapshot |
| `0x0012` | Application/domain queue overflow |

Дирижер обязан читать `0xF007` при вводе узла в работу, после recovery/discovery и при подозрении на деградацию шины. Формат метрик общий для Fluidics, Motion и Thermo; доменные платы могут добавлять собственные metric_id выше `0x1000`.

### 3.3. Адреса узлов (Node Address)
*   `0x10`: Conductor (Дирижер)
*   `0x20`: Motion Controller (Моторы)
*   `0x30`: Pump/Valve Controller (Помпы/Клапаны)
*   `0x40`: Thermo Controller (Термодатчики)

### 3.4. Типы устройств в `GET_DEVICE_INFO`
Эти значения передаются в payload ответа `0xF001` и не являются CAN NodeID.
*   `0x01`: Motion Controller.
*   `0x02`: Thermo Controller.
*   `0x03`: Fluidic Controller.

---

## 4. Энергонезависимая память (Flash)

### 4.1. Размещение
Использование **последней страницы** Flash-памяти МК (например, Page 63 для STM32F103C8).

Последняя страница Flash, используемая под конфигурацию, должна быть исключена из области приложения в linker script или зарезервирована отдельной секцией. `COMMIT` не должен иметь возможности стереть исполняемый код.

Для STM32F103C8 с 64K Flash и конфигурацией на `0x0800FC00` область приложения в linker script должна заканчиваться до последней страницы, например `FLASH LENGTH = 63K`. Аналогичное правило применяется ко всем Executor-платам: адрес конфигурации, erase page/sector и linker memory map должны быть согласованы в документации и в проекте.

### 4.2. Целостность данных
*   **Magic Key**: Проверка `0x55AAEEFF` в начале структуры.
*   **CRC16**: Обязательный расчет контрольной суммы всей структуры (Poly 0xA001). При несовпадении CRC устройство загружает «Заводские настройки» в RAM, но не перезаписывает Flash автоматически.
*   **Атомарность**: Стирание страницы перед записью новых данных.

---

## 5. Стандарты кодирования (Coding Style)

1.  **Запрет "Магических чисел"**: Все смещения, биты, команды и ошибки должны быть определены в `can_protocol.h` или `app_config.h`.
2.  **Префиксы функций**:
    *   `CAN_xxx`: Функции сетевого уровня.
    *   `AppConfig_xxx`: Функции работы с конфигурацией.
    *   `MotionDriver_xxx`: Функции работы с железом.
3.  **Безопасность**: Каждое обращение к периферии через HAL должно проверять возвращаемый статус (`HAL_OK`).

---

## 6. Регрессионная сверка протокола
Перед выпуском прошивки или обновлением документации необходимо сверять:

*   `can_protocol_*.h` каждой платы с `CONDUCTOR_INTEGRATION_GUIDE.md`.
*   NodeID, `device_type`, command codes, NACK-коды и Magic Keys.
*   Форматы ACK/NACK/DONE/DATA с фактическими ответами прошивки.
*   SocketCAN-примеры с реальными кадрами на шине.
*   Границу уровней: `Host -> Conductor` может использовать динамический DLC, `Conductor <-> Executor` обязан использовать strict `DLC=8`.

---

## Чек-лист соответствия (Compliance Checklist)
- [ ] Используется 29-битный ID?
- [ ] Принимается ли Broadcast (ID 0x00)?
- [ ] Отправляется ли ACK немедленно?
- [ ] Возвращается ли DONE после штатного завершения?
- [ ] Возвращается ли NACK на ошибочные команды?
- [ ] Все ли executor-кадры имеют DLC=8?
- [ ] Защищен ли доступ к конфигу через Mutex?
- [ ] Есть ли CRC16 во Flash?
- [ ] Поддерживается ли команда 0xF001 (Info)?
- [ ] Реализован ли единый формат `GET_DEVICE_INFO`?
- [ ] Определено ли safe state?
- [ ] Реализован ли доменный safe-state hook?
- [ ] Вызывается ли safe-state hook из `Error_Handler()` и fault handlers?
- [ ] Не зависит ли safe-state hook от FreeRTOS/CAN/динамической памяти?
- [ ] Реализован ли watchdog?
- [ ] Есть ли диагностика CAN fault state?
- [ ] Исключена ли Flash config page из области приложения в linker script?
- [ ] Совпадает ли адрес config page в коде, linker script и документации?
