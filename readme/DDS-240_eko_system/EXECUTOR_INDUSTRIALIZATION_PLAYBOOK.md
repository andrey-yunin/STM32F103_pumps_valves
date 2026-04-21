# DDS-240 Executor Industrialization Playbook

**Статус:** обязательный рабочий гид для доведения STM32 Executor-плат до промышленного стандарта DDS-240.

Этот документ написан так, чтобы инженер или AI-ассистент мог открыть только папку `DDS-240_eko_system`, прочитать нормативы и выполнить работы без истории предыдущего чата.

Проверенный эталон: плата Fluidics STM32F103, 13 насосов + 3 клапана, CAN 1 Mbit/s, FreeRTOS, IWDG, safe-state, `GET_STATUS`, fault-injection и production smoke-test.

---

## 1. Какие документы читать перед работой

Обязательный порядок чтения:

1. `dds240_global_config.h` - канонические константы экосистемы.
2. `DDS-240_ECOSYSTEM_STANDARD.md` - нормативы архитектуры, CAN, safety, watchdog, сервисов и приемки.
3. `CONDUCTOR_INTEGRATION_GUIDE.md` - проверенный отчет Fluidics и требования к Дирижеру.
4. `EXECUTOR_TESTING_GUIDE.md` - сценарии ручной имитации исполнителей и проверка Дирижера.
5. Локальный проект конкретного исполнителя - только после понимания пунктов выше.

`dds240_global_config.h` является reference config. Его не обязательно напрямую подключать в существующие прошивки, но локальные константы не должны ему противоречить.

---

## 2. Цель доведения исполнителя

Исполнитель считается приведенным к промышленному стандарту DDS-240, если выполнено все ниже:

*   CAN transport соответствует `Conductor <-> Executor`: 1 Mbit/s, 29-bit Extended ID, strict `DLC=8`.
*   Все команды имеют цикл `ACK -> DATA... -> DONE` или `ACK -> NACK`; транспортно невалидные кадры игнорируются без `ACK/NACK`.
*   `DONE` означает завершение атомарного действия, а не окончание длительного шага рецепта.
*   Дирижер контролирует recipe timer, operation timeout и resource lock.
*   Все RTOS-объекты проверяются после создания.
*   Safe-state hook существует, не зависит от FreeRTOS/CAN/dynamic memory и вызывается из startup/error/fault paths.
*   IWDG обслуживается только supervisor-задачей после heartbeat критических задач.
*   Реализованы `0xF001..0xF007`, включая `GET_STATUS` с общими метриками.
*   bxCAN TX ordering защищен: `TransmitFifoPriority = ENABLE`.
*   При `ACK` без `DONE` Дирижер выполняет recovery/discovery и не считает команду успешной.
*   Все тестовые fault-injection флаги возвращены в `0` перед production-сборкой.
*   Результаты тестов внесены в отчет экосистемы.

---

## 3. Рабочий порядок для любого исполнителя

### 3.1. Первичный аудит

Собрать факты по проекту:

*   MCU, CAN peripheral, HAL/Cube version.
*   CAN bitrate, timing, sample point.
*   Текущий NodeID и DeviceType.
*   Список доменных команд и каналов.
*   Есть ли FreeRTOS tasks: transport, dispatcher, domain.
*   Есть ли очереди, таймеры, mutex/semaphore.
*   Есть ли safe-state hook.
*   Есть ли IWDG.
*   Есть ли сервисы `GET_DEVICE_INFO`, `GET_UID`, `SET_NODE_ID`, `COMMIT`, `REBOOT`, `FACTORY_RESET`, `GET_STATUS`.
*   Есть ли CAN diagnostics counters.
*   Есть ли linker protection для Flash config page.

Не начинать рефакторинг до понимания, какие изменения уже есть в рабочем дереве. Нельзя откатывать чужие изменения без явного запроса.

### 3.2. Сверка с global config

Сверить локальный протокол с `dds240_global_config.h`:

| Область | Должно совпадать |
|:--------|:-----------------|
| CAN | `DDS240_CAN_BITRATE`, strict `DLC=8`, Extended ID |
| ID layout | priority/type/dst/src bit layout |
| NodeID | Conductor `0x10`, default Motion `0x20`, Fluidics `0x30`, Thermo `0x40` |
| DeviceType | Motion `0x01`, Thermo `0x02`, Fluidics `0x03` |
| Service | `0xF001..0xF007` |
| Magic Keys | reboot `0x55AA`, factory reset `0xDEAD` |
| NACK | common registry `0x0000..0x0006` |
| GET_STATUS | common metric IDs `0x0001..0x0012` |
| Safety | `ACK without DONE` is fault, IWDG supervisor, safe-state |

Доменные команды могут оставаться локальными, если они не противоречат общему контракту.

### 3.3. CAN transport

Обязательные настройки:

*   CAN bitrate: `1 Mbit/s`.
*   Extended ID only for executor traffic.
*   Conductor -> Executor strict `DLC=8`.
*   Executor -> Conductor strict `DLC=8`.
*   Broadcast `DstAddr=0x00` принимается, если команда разрешена.
*   Чужой `DstAddr` игнорируется без ответа.
*   Standard ID игнорируется без ответа.
*   Wrong message type игнорируется без ответа.
*   Wrong DLC игнорируется без ответа.
*   `NACK` отправляется только после валидной transport envelope.

Для STM32 bxCAN:

```c
hcan.Init.TransmitFifoPriority = ENABLE;
```

Это обязательно. Без этого `DONE` может обогнать последние DATA-кадры в многокадровом ответе.

Перед `HAL_CAN_AddTxMessage()` нужен mailbox guard:

```c
HAL_CAN_GetTxMailboxesFreeLevel(&hcan)
```

Если mailbox не освободился в заданное окно, увеличивается `tx_mailbox_timeout`, команда Дирижера завершается timeout/recovery policy.

### 3.4. RTOS resource integrity

Все RTOS-объекты проверяются:

*   `osThreadNew()`
*   `osMessageQueueNew()`
*   `osTimerNew()`
*   `osMutexNew()`
*   `osSemaphoreNew()`

Если критический объект не создан:

1. Перевести доменные выходы в safe state.
2. Войти в `Error_Handler()`.
3. Не принимать команды в частично рабочем состоянии.

Проверенный ориентир Fluidics STM32F103:

```c
configTOTAL_HEAP_SIZE = 8192
```

Это не универсальное значение для всех плат. Motion/Thermo рассчитываются отдельно, но правило проверки объектов обязательно.

### 3.5. Архитектура задач

Минимальный паттерн:

| Задача | Назначение |
|:-------|:-----------|
| `task_can_handler` | CAN RX/TX, фильтрация, очереди, диагностика транспорта |
| `task_dispatcher` | ACK, сервисы, валидация команд, маршрутизация в домен |
| domain task | управление железом и доменными safety timers |
| `task_watchdog` | supervisor heartbeat, единственный refresh IWDG |

Задачи, ожидающие очередь или флаги, не должны висеть в `osWaitForever`, если они являются IWDG-клиентами. Они должны периодически выходить по timeout и публиковать heartbeat.

Пример нормы:

```c
#define APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS 500U
```

### 3.6. ACK, DONE и NACK

Правила:

*   `ACK` отправляет Dispatcher после принятия валидной команды.
*   `DONE` отправляется после завершения атомарного действия.
*   `NACK` отправляется при прикладной ошибке валидной команды.
*   После `NACK` не отправлять `DONE`.
*   После `DONE` не отправлять DATA той же транзакции.
*   Transport drops не дают `NACK`.

Для state-changing команд:

*   `DONE` означает: ресурс переведен в требуемое состояние, локальная защита активирована.
*   `DONE` не означает окончание рецепта.
*   Recipe timer принадлежит Дирижеру.
*   Дирижер держит resource lock и отправляет `STOP/CLOSE` сам.

Если `ACK` есть, а `DONE` нет:

*   Дирижер считает команду невыполненной или неизвестно завершенной.
*   Возможные причины: зависание доменной задачи, RTOS resource fault, IWDG recovery, fault handler.
*   Дирижер выполняет recovery/discovery и сверяет `NodeID`, `device_type`, `UID`.

### 3.7. Safety timer / local protection

Для команд, включающих физическую нагрузку:

1. Сначала подготовить/запустить локальную защиту.
2. Только после успешного запуска protection timer включить выход.
3. Если timer не создан или `osTimerStart()` вернул ошибку, выход оставить/вернуть в safe state.
4. Отправить `NACK`, например `DEVICE_BUSY`, без `DONE`.

Fluidics verified behavior:

*   `PUMP_START timeout=0`: default 60 s.
*   `VALVE_OPEN timeout=0`: default 300 s.
*   `RUN_DURATION timeout=0`: invalid.
*   timeout передается как `uint32_t LE` в payload bytes 3..6.

### 3.8. Safe-state hook

Каждая плата должна иметь доменный safe-state hook:

```c
void Domain_AllOff(void);
```

Требования:

*   Выключает все опасные выходы.
*   Работает без FreeRTOS.
*   Не отправляет CAN.
*   Не использует очереди/mutex/dynamic memory.
*   Может вызываться до старта scheduler.
*   Может вызываться из `Error_Handler()` и fault handlers.

Пример fault path:

```c
static void EnterSafeFaultState(void)
{
    __disable_irq();
    Domain_AllOff();
    __DSB();
    __ISB();
}
```

Вызывать в начале:

*   `NMI_Handler`
*   `HardFault_Handler`
*   `MemManage_Handler`
*   `BusFault_Handler`
*   `UsageFault_Handler`

### 3.9. IWDG supervisor

Правило: доменные задачи не вызывают `HAL_IWDG_Refresh()` напрямую.

Паттерн:

*   `AppWatchdog_Heartbeat(client)` вызывается критическими задачами.
*   `task_watchdog` раз в `1000 ms` проверяет, что все клиенты продвинулись.
*   Только если все клиенты живы, supervisor вызывает `HAL_IWDG_Refresh()`.
*   Если клиент завис, supervisor вызывает safe-state hook и прекращает refresh.
*   Аппаратный IWDG сбрасывает МК.

Проверенный профиль Fluidics STM32F103:

*   IWDG prescaler: `256`
*   reload: `624`
*   фактический timeout зависит от LSI, ориентир около 4 s

Минимальные клиенты:

*   CAN/transport
*   Dispatcher
*   domain task

### 3.10. Service layer

Обязательные сервисы:

| Command | Назначение |
|:--------|:-----------|
| `0xF001` | `GET_DEVICE_INFO` |
| `0xF002` | `REBOOT` |
| `0xF003` | `COMMIT` |
| `0xF004` | `GET_UID` |
| `0xF005` | `SET_NODE_ID` |
| `0xF006` | `FACTORY_RESET` |
| `0xF007` | `GET_STATUS` |

Опасные команды защищены Magic Key:

*   `REBOOT`: `0x55AA`
*   `FACTORY_RESET`: `0xDEAD`

После `COMMIT`, `REBOOT`, `FACTORY_RESET`, `SET_NODE_ID` Дирижер обязан выполнить rediscovery.

Особый случай `SET_NODE_ID`: если реализация отправляет `ACK` со старого NodeID, а `DONE` уже с нового NodeID, это должно быть явно задокументировано. Дирижер должен принять оба адреса в рамках этой транзакции.

### 3.11. GET_STATUS

`GET_STATUS (0xF007)` обязателен.

Порядок:

```text
ACK -> DATA metric 0x0001 -> ... -> DATA metric 0x0012 -> DONE
```

Формат DATA:

```text
byte 0: 0x02 DATA
byte 1: 0x80
byte 2..3: metric_id uint16 LE
byte 4..7: value uint32 LE
```

Общие метрики:

| ID | Метрика |
|:---|:--------|
| `0x0001` | RX total |
| `0x0002` | TX total |
| `0x0003` | RX queue overflow |
| `0x0004` | TX queue overflow |
| `0x0005` | Dispatcher queue overflow |
| `0x0006` | Dropped not Extended ID |
| `0x0007` | Dropped wrong destination |
| `0x0008` | Dropped wrong message type |
| `0x0009` | Dropped wrong DLC |
| `0x000A` | TX mailbox timeout |
| `0x000B` | TX HAL error |
| `0x000C` | CAN error callback count |
| `0x000D` | CAN error warning |
| `0x000E` | CAN error passive |
| `0x000F` | CAN bus-off |
| `0x0010` | Last HAL error |
| `0x0011` | Last ESR |
| `0x0012` | Application/domain queue overflow |

Доменные метрики начинаются с `0x1000`.

Дирижер хранит baseline и считает дельты. Первый `GET_STATUS` после discovery/recovery является baseline, а не fault.

### 3.12. Flash config page

Если исполнитель хранит NodeID/config во Flash:

*   Адрес config page должен быть единственным источником в коде.
*   Linker script должен исключать config page из application FLASH area.
*   Документация должна содержать адрес page.
*   `COMMIT` пишет RAM config во Flash.
*   `FACTORY_RESET` чистит config и возвращает default NodeID.

Fluidics reference:

```text
Config page: 0x0800FC00..0x0800FFFF
FLASH LENGTH = 63K
```

---

## 4. Обязательная приемка

### 4.1. CAN setup на Linux

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000 restart-ms 100 loopback off
sudo ip link set can0 up
ip -details -statistics link show can0
```

Ожидаемо:

*   bitrate `1000000`
*   state `ERROR-ACTIVE`
*   нет роста `bus-off`
*   нет новых RX/TX errors после тестов

### 4.2. Discovery

```bash
cansend can0 00301000#01F0000000000000
```

Для Fluidics default `0x30` ожидается:

```text
TX 00301000 [01 F0 ...]
RX 05103000 ACK
RX 07103000 DATA...
RX 07103000 DONE
```

Для других плат заменить `DstAddr`.

### 4.3. GET_STATUS

```bash
cansend can0 00301000#07F0000000000000
```

Ожидаемо:

```text
ACK -> DATA metrics -> DONE
```

Критично: `DONE` должен быть последним.

### 4.4. NACK matrix

Проверить:

*   unknown command -> `NACK UNKNOWN_CMD`
*   invalid channel -> `NACK INVALID_DEVICE_ID`
*   invalid resource type -> `NACK`
*   invalid Magic Key -> `NACK INVALID_KEY`
*   safety timer fail -> `ACK`, затем `NACK DEVICE_BUSY`, без `DONE`

### 4.5. Transport negative tests

Проверить, что нет ответа:

*   Standard ID
*   wrong `DstAddr`
*   non-COMMAND frame to Executor
*   `DLC != 8`

Дирижер должен обработать это как `ACK timeout`, а не как `NACK`.

### 4.6. Addressing

Проверить:

*   default NodeID отвечает;
*   чужой NodeID игнорируется;
*   broadcast `DstAddr=0x00` принимается для разрешенных команд;
*   `SET_NODE_ID` в RAM;
*   `SET_NODE_ID + COMMIT + REBOOT` сохраняет адрес;
*   `FACTORY_RESET` возвращает default.

### 4.7. Safety timer

Для каждого типа нагрузки:

*   команда включения с ненулевым timeout;
*   команда включения с default timeout;
*   команда выключения;
*   fault-injection отказа timer start;
*   production regression после выключения fault-injection.

### 4.8. IWDG normal runtime

Сценарий:

```bash
./App_users/can_fluidics_test.sh info
sleep 120
./App_users/can_fluidics_test.sh info
```

Ожидаемо:

*   обе команды дают `ACK -> DATA -> DONE`;
*   ложного watchdog reset в idle нет.

Для других плат использовать эквивалентный `GET_DEVICE_INFO`.

### 4.9. IWDG fault-injection

Сценарий:

1. Включить compile-time hook зависания одной критической задачи.
2. Собрать и прошить тестовую сборку.
3. Отправить команду, которая активирует ресурс.
4. Убедиться:
   *   `ACK` есть;
   *   `DONE` нет;
   *   выход уходит в safe state;
   *   после IWDG reset плата снова отвечает на `GET_DEVICE_INFO`.
5. Вернуть hook в `0`.
6. Собрать production.
7. Повторить smoke-test.

### 4.10. Fault handler safe-state

Сценарий:

1. Включить compile-time hook входа в `HardFault_Handler()` после включения тестовой нагрузки.
2. Собрать и прошить тестовую сборку.
3. Отправить команду включения.
4. Убедиться:
   *   `ACK` есть;
   *   `DONE` нет;
   *   нагрузка делает краткий `ON -> OFF`;
   *   после IWDG reset плата снова отвечает.
5. Повторить тест после явной перепрошивки, если есть сомнение, что была прошита нужная сборка.
6. Вернуть hook в `0`.
7. Собрать и прошить production.

### 4.11. Production smoke-test

После возврата всех test flags в `0`:

```bash
GET_DEVICE_INFO
одна доменная команда START/STOP или аналог
sleep 10
GET_DEVICE_INFO
```

Ожидаемо:

*   `ACK/DONE` штатные;
*   нет `DONE not found`;
*   плата не уходит в reset;
*   физическая нагрузка ведет себя штатно.

---

## 5. Test flags policy

Все fault-injection флаги должны быть явно видны в конфиге.

Перед production-сборкой обязательно:

```text
FORCE_TIMER_START_FAIL = 0
WATCHDOG_STALL_TEST = 0
FAULT_HANDLER_TEST = 0
```

Если названия отличаются, смысл должен быть тот же.

В отчете фиксировать:

*   когда флаг включали;
*   какой тест прошел;
*   когда флаг вернули в `0`;
*   что production-сборка после возврата прошла;
*   что production smoke-test прошел.

---

## 6. Что обязательно писать в отчет после работ

Для каждого исполнителя записать:

*   дата;
*   плата, MCU, NodeID, DeviceType;
*   CAN bitrate и timing;
*   список закрытых сервисов;
*   список закрытых доменных команд;
*   результаты NACK matrix;
*   результаты transport negative tests;
*   `GET_STATUS` metrics range;
*   состояние RTOS heap и проверок объектов;
*   safe-state hook path;
*   IWDG idle и fault-injection результаты;
*   fault handler safe-state результаты;
*   production smoke-test после возврата test flags;
*   что осталось не закрыто из-за отсутствия физики.

Формат записи должен позволять понять результат без чтения CAN-лога целиком, но ключевые кадры и выводы нужно сохранять.

---

## 7. Промышленные выводы из Fluidics reference

Эти выводы считаются переносимыми на другие STM32 Executor-платы:

*   Недостаточный RTOS heap может проявляться как `ACK` без `DONE`, потому что Dispatcher жив, а доменная задача/таймеры не стартовали.
*   `TransmitFifoPriority = DISABLE` на bxCAN может нарушить порядок `DATA...DONE`.
*   Неверный `DLC` не должен давать `NACK`: это transport drop и `ACK timeout` для Дирижера.
*   `DONE` подтверждает атомарное действие, а не длительность рецепта.
*   Safety timer должен стартовать до включения выхода.
*   Watchdog должен контролировать прогресс задач, а не просто обновляться из idle loop.
*   Fault handler обязан выключать физические выходы без FreeRTOS/CAN.
*   После любого `ACK` без `DONE` Дирижер должен выполнять recovery/discovery.
*   Финальный production smoke-test после возврата test flags обязателен.

---

## 8. Минимальный Definition of Done

Исполнитель можно считать промышленно доведенным только если:

*   локальный протокол не противоречит `dds240_global_config.h`;
*   сборка чистая;
*   все test flags выключены;
*   `GET_DEVICE_INFO` и `GET_STATUS` работают;
*   NACK matrix закрыта;
*   transport negative tests закрыты;
*   safe-state hook проверен;
*   IWDG normal idle проверен;
*   IWDG fault-injection проверен;
*   fault handler safe-state проверен;
*   production smoke-test после перепрошивки проверен;
*   отчет обновлен;
*   оставшиеся пункты явно перечислены как физически не проверенные, а не забытые.
