# CAN Verification Report: Fluidics Executor

**Date:** 2026-04-17, updated 2026-04-20  
**Board:** STM32F103 Pump/Valve Executor  
**NodeID:** `0x30`  
**Adapter:** CANable via Linux SocketCAN (`gs_usb`)  
**Status:** PASS at 1 Mbit/s

---

## 1. Verified Bus Profile

| Parameter | Value |
|:----------|:------|
| CAN type | Classical CAN |
| Bitrate | 1 Mbit/s |
| SocketCAN sample point | 0.750 |
| STM32 APB1 CAN clock | 32 MHz |
| STM32 prescaler | 2 |
| STM32 BS1 / BS2 | 11 TQ / 4 TQ |
| ID format | 29-bit Extended |
| Command DLC | 8 |
| Response DLC | 8 |
| Conductor address | `0x10` |
| Fluidics executor address | `0x30` |
| CAN pins | `PA11=CAN_RX`, `PA12=CAN_TX` |
| Tested topology | CANable -> passive CAN switch -> Fluidics executor |
| Switch capacity | Up to 16 CAN devices |

SocketCAN setup:

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000 sample-point 0.750 restart-ms 100 loopback off
sudo ip link set can0 up
```

Project helper:

```bash
./App_users/can_fluidics_test.sh setup
```

---

## 2. Verified Transactions

### 2.1. Service Discovery

Command:

```bash
./App_users/can_fluidics_test.sh info
```

Raw command frame:

```text
TX 00301000 [8] 01 F0 00 00 00 00 00 00
```

Expected response pattern:

```text
RX 05103000 [8] 01 F0 00 00 00 00 00 00
RX 07103000 [8] 02 80 ...
RX 07103000 [8] 02 80 ...
RX 07103000 [8] 02 80 ...
RX 07103000 [8] 01 01 F0 00 00 00 00 00
```

Observed result: `ACK`, three `DATA` frames, and `DONE` received.

### 2.2. Pump Channel 0 Start/Stop

Command:

```bash
./App_users/can_fluidics_test.sh pump-cycle 0 1
```

Raw command frames:

```text
TX 00301000 [8] 02 02 00 00 00 00 00 00
TX 00301000 [8] 03 02 00 00 00 00 00 00
```

Expected response pattern:

```text
RX 05103000 [8] 02 02 00 00 00 00 00 00
RX 07103000 [8] 01 02 02 00 00 00 00 00
RX 05103000 [8] 03 02 00 00 00 00 00 00
RX 07103000 [8] 01 03 02 00 00 00 00 00
```

Observed result: `ACK` and `DONE` received for both `PUMP_START` and `PUMP_STOP`.

### 2.3. Passive CAN Switch and Physical Load Test

Test date: 2026-04-20.

Topology:

```text
CANable -> passive CAN switch -> Fluidics executor -> physical loads
```

The previous no-response condition was traced to a damaged cable connector. After replacing/fixing the connector, the same 1 Mbit/s CAN profile worked through the passive switch.

Command:

```bash
./App_users/can_fluidics_test.sh load-cycle 0 1 3 1 0.5
```

Test sequence per cycle:

```text
load 0 START -> load 0 STOP
load 1 START -> load 1 STOP
load 0 START + load 1 START -> load 0 STOP + load 1 STOP
```

Observed result:

- 3 complete cycles executed.
- Channel 0 physical load switched ON/OFF.
- Channel 1 physical load switched ON/OFF.
- Channels 0 and 1 switched ON together and then stopped.
- Physical loads were coolers; rotation confirmed actual output switching.
- Every `PUMP_START` and `PUMP_STOP` command returned `ACK` and `DONE`.

Post-test SocketCAN state:

```text
can state ERROR-ACTIVE
bitrate 1000000 sample-point 0.750
re-started 0
bus-errors 0
arbit-lost 0
bus-off 0
RX errors 0
TX errors 0
```

After additional traffic, RX/TX packet counters increased while `error-warn`, `error-pass`, `bus-errors`, and `bus-off` did not increase. The remaining non-zero warning/passive counters are attributed to earlier tests with the damaged connector.

### 2.4. NACK Matrix

Test date: 2026-04-20.

| Scenario | TX frame | Expected result | Observed |
|:---------|:---------|:----------------|:---------|
| Pump command to valve index 13 | `00301000#02020D0000000000` | `ACK` + `NACK 0x0002` | PASS |
| Valve command to pump index 0 | `00301000#0402000000000000` | `ACK` + `NACK 0x0002` | PASS |
| Pump command to out-of-range index 16 | `00301000#0202100000000000` | `ACK` + `NACK 0x0002` | PASS |
| Unknown command `0x9999` | `00301000#9999000000000000` | `ACK` + `NACK 0x0001` | PASS |
| REBOOT with invalid key | `00301000#02F0000000000000` | `ACK` + `NACK 0x0004` | PASS |
| FACTORY_RESET with invalid key | `00301000#06F0000000000000` | `ACK` + `NACK 0x0004` | PASS |
| Invalid SET_NODE_ID `0x00` | `00301000#05F0000000000000` | `ACK` + `NACK 0x0001` | PASS |
| Invalid SET_NODE_ID `0x01` | `00301000#05F0010000000000` | `ACK` + `NACK 0x0001` | PASS |
| Invalid SET_NODE_ID `0x10` | `00301000#05F0100000000000` | `ACK` + `NACK 0x0001` | PASS |
| Invalid SET_NODE_ID `0x80` | `00301000#05F0800000000000` | `ACK` + `NACK 0x0001` | PASS |

Post-test state: `ERROR-ACTIVE`, `bus-errors=0`, `bus-off=0`, RX/TX errors `0`. `error-warn` and `error-pass` did not increase.

### 2.5. Addressing and Broadcast

Test date: 2026-04-20.

| Scenario | TX frame | Expected result | Observed |
|:---------|:---------|:----------------|:---------|
| Foreign destination `0x20` | `00201000#01F0000000000000` | No response from Fluidics | PASS |
| Broadcast destination `0x00` | `00001000#01F0000000000000` | `ACK` + 3 `DATA` + `DONE` from `0x30` | PASS |

Post-test state: `ERROR-ACTIVE`, `bus-errors=0`, `bus-off=0`, RX/TX errors `0`.

### 2.6. Read-only Service Layer

Test date: 2026-04-20.

`GET_UID (0xF004)`:

```text
TX 00301000 [8] 04 F0 00 00 00 00 00 00
RX 05103000 [8] 04 F0 00 00 00 00 00 00
RX 07103000 [8] 02 80 48 FF 6A 06 49 85
RX 07103000 [8] 02 80 70 54 25 58 15 67
RX 07103000 [8] 01 04 F0 00 00 00 00 00
```

Observed result: `ACK`, two `DATA` frames, and `DONE`.

UID:

```text
48 FF 6A 06 49 85 70 54 25 58 15 67
```

The UID matches the UID fragments returned by `GET_DEVICE_INFO`.

### 2.7. SET_NODE_ID in RAM

Test date: 2026-04-20.

The test changed the executor NodeID in RAM only, without `COMMIT`, then returned it to the default `0x30`.

| Step | TX frame | Observed result |
|:-----|:---------|:----------------|
| Set `0x30 -> 0x31` | `00301000#05F0310000000000` | `ACK` from `0x30`, `DONE` from `0x31` |
| Query old address | `00301000#01F0000000000000` | No response |
| Query new address | `00311000#01F0000000000000` | `ACK` + 3 `DATA` + `DONE` from `0x31` |
| Set `0x31 -> 0x30` | `00311000#05F0300000000000` | `ACK` from `0x31`, `DONE` from `0x30` |
| Query restored address | `00301000#01F0000000000000` | `ACK` + 3 `DATA` + `DONE` from `0x30` |

Important behavior: for `SET_NODE_ID`, the current firmware sends `ACK` from the old NodeID and `DONE` from the new NodeID because `AppConfig_SetPerformerID()` is applied before `CAN_SendDone()`. The Conductor must account for this transition behavior, or the firmware can later be changed to complete the whole transaction from the old address.

Post-test state: `ERROR-ACTIVE`, `bus-errors=0`, `bus-off=0`, RX/TX errors `0`. `error-warn` and `error-pass` did not increase.

### 2.8. Persistence, Reset, and Transport Negative Tests

Test date: 2026-04-20.

| Scenario | TX frame | Observed result |
|:---------|:---------|:----------------|
| Valid REBOOT | `00301000#02F000AA55000000` | `ACK` + `DONE`, executor rebooted, `0x30` answered `GET_DEVICE_INFO` |
| COMMIT current config | `00301000#03F0000000000000` | `ACK` + `DONE`, Flash write path completed |
| SET_NODE_ID persistence `0x30 -> 0x31` | `SET_NODE_ID`, `COMMIT`, `REBOOT`, `GET_INFO` on `0x31` | PASS, NodeID `0x31` survived reboot |
| Restore persistence `0x31 -> 0x30` | `SET_NODE_ID`, `COMMIT`, `REBOOT`, `GET_INFO` on `0x30` | PASS, default NodeID `0x30` survived reboot |
| Short DLC | `00301000#01F0` | Silent drop, no `ACK/NACK` |
| Standard 11-bit ID | `123#01F0000000000000` | Silent drop, no `ACK/NACK` |
| Non-COMMAND message type | `01301000#01F0000000000000` | Silent drop, no `ACK/NACK` |
| Valid FACTORY_RESET | `00301000#06F000ADDE000000` | `ACK` + `DONE`, executor reinitialized, default `0x30` answered `GET_DEVICE_INFO` |

Post-test state after these checks: `ERROR-ACTIVE`, `bus-errors=0`, `bus-off=0`, RX/TX errors `0`. `error-warn=89` and `error-pass=23` did not increase.

### 2.9. Non-zero Timeout and RUN_DURATION Verification

Test date: 2026-04-20.

After fixing the `timeout_ms` Little-Endian parser in `task_dispatcher.c`, non-zero timeout behavior was verified on a physical load.

`PUMP_START (0x0202)` with `2000 ms` timeout:

```text
TX 00301000 [8] 02 02 00 D0 07 00 00 00
RX 05103000 [8] 02 02 00 00 00 00 00 00
RX 07103000 [8] 01 02 02 00 00 00 00 00
```

`RUN_DURATION (0x0201)` with `2000 ms` timeout:

```text
TX 00301000 [8] 01 02 00 D0 07 00 00 00
RX 05103000 [8] 01 02 00 00 00 00 00 00
RX 07103000 [8] 01 01 02 00 00 00 00 00
```

Common frame meaning:

- device index `0`
- timeout `0x000007D0 = 2000 ms`

Observed result: `ACK` and `DONE` were received, the physical load started, then switched off automatically after approximately 2 seconds. No additional `DONE` is expected on safety auto-off in the current Fluidics logic.

### 2.10. Safety Timer Start Guard Regression

Test date: 2026-04-20.

The Fluidics executor was updated so a load is not energized until the channel safety timer has been successfully started. If the timer is missing or `osTimerStart()` fails, the channel is forced to `OFF`, the executor sends `NACK 0x0003`, and no `DONE` is emitted.

Regression after the change:

```text
TX 00301000 [8] 01 F0 00 00 00 00 00 00
RX 05103000 [8] 01 F0 00 00 00 00 00 00
RX 07103000 [8] 02 80 03 01 01 10 48 FF
RX 07103000 [8] 02 80 6A 06 49 85 70 54
RX 07103000 [8] 02 80 25 58 15 67 00 00
RX 07103000 [8] 01 01 F0 00 00 00 00 00

TX 00301000 [8] 02 02 00 00 00 00 00 00
RX 05103000 [8] 02 02 00 00 00 00 00 00
RX 07103000 [8] 01 02 02 00 00 00 00 00

TX 00301000 [8] 03 02 00 00 00 00 00 00
RX 05103000 [8] 03 02 00 00 00 00 00 00
RX 07103000 [8] 01 03 02 00 00 00 00 00

TX 00301000 [8] 01 02 00 D0 07 00 00 00
RX 05103000 [8] 01 02 00 00 00 00 00 00
RX 07103000 [8] 01 01 02 00 00 00 00 00
```

Result: `GET_DEVICE_INFO`, `PUMP_START/STOP`, and `RUN_DURATION 2000 ms` remained operational after the safety guard change. Direct fault injection for the timer failure path is still pending.

### 2.11. Safe-State Hook Regression

Test date: 2026-04-20.

The Fluidics executor now has a domain safe-state hook for all pump/valve outputs:

- `PumpsValves_AllOff()` switches all pump and valve GPIO outputs to `OFF`.
- The hook is called after GPIO/CAN init and before executor command handling starts.
- `Error_Handler()` calls the hook before disabling interrupts.
- `NMI`, `HardFault`, `MemManage`, `BusFault`, and `UsageFault` handlers call the hook before entering the fault loop.

Regression after the change:

```text
TX 00301000 [8] 02 02 00 00 00 00 00 00
RX 05103000 [8] 02 02 00 00 00 00 00 00
RX 07103000 [8] 01 02 02 00 00 00 00 00

TX 00301000 [8] 03 02 00 00 00 00 00 00
RX 05103000 [8] 03 02 00 00 00 00 00 00
RX 07103000 [8] 01 03 02 00 00 00 00 00

TX 00301000 [8] 01 02 00 D0 07 00 00 00
RX 05103000 [8] 01 02 00 00 00 00 00 00
RX 07103000 [8] 01 01 02 00 00 00 00 00
```

Result: normal CAN command handling remained operational after adding the safe-state hook. Direct fault-handler injection was not performed on the working board in this test pass.

### 2.12. Flash Config Page Reservation

Test date: 2026-04-20.

The executor stores persistent configuration on the last STM32F103C8 Flash page:

- config address: `0x0800FC00`
- reserved range: `0x0800FC00..0x0800FFFF`
- erase size: one Flash page

The application linker memory region was reduced from `64K` to `63K`, so normal firmware sections cannot be linked into the config page erased by `COMMIT` or `FACTORY_RESET`.

Build verification after the linker change:

```text
text 32308
data 96
bss 9848
```

Post-flash runtime regression:

```text
TX 00301000 [8] 01 F0 00 00 00 00 00 00
RX 05103000 [8] 01 F0 00 00 00 00 00 00
RX 07103000 [8] 02 80 03 01 01 10 48 FF
RX 07103000 [8] 02 80 6A 06 49 85 70 54
RX 07103000 [8] 02 80 25 58 15 67 00 00
RX 07103000 [8] 01 01 F0 00 00 00 00 00

TX 00301000 [8] 03 F0 00 00 00 00 00 00
RX 05103000 [8] 03 F0 00 00 00 00 00 00
RX 07103000 [8] 01 03 F0 00 00 00 00 00

TX 00301000 [8] 02 F0 00 AA 55 00 00 00
RX 05103000 [8] 02 F0 00 00 00 00 00 00
RX 07103000 [8] 01 02 F0 00 00 00 00 00

TX 00301000 [8] 01 F0 00 00 00 00 00 00
RX 05103000 [8] 01 F0 00 00 00 00 00 00
RX 07103000 [8] 02 80 03 01 01 10 48 FF
RX 07103000 [8] 02 80 6A 06 49 85 70 54
RX 07103000 [8] 02 80 25 58 15 67 00 00
RX 07103000 [8] 01 01 F0 00 00 00 00 00
```

Result: application build passed with large margin before the reserved page. `COMMIT` and `REBOOT` returned `ACK/DONE`; after reboot the executor answered `GET_DEVICE_INFO` on NodeID `0x30` with the same UID fragments.

---

## 3. Shared Ecosystem Notes

The verified executor transport profile must stay common for Fluidics, Motion, and Thermo executor boards:

- `1 Mbit/s`
- 29-bit Extended CAN ID
- strict `DLC=8`
- `DstAddr=0x00` broadcast acceptance
- command lifecycle: `ACK`, optional `DATA`, final `DONE`, or `NACK`
- conductor address `0x10`
- executor NodeIDs: Motion `0x20`, Fluidics `0x30`, Thermo `0x40`

Invalid transport envelopes are intentionally ignored without `ACK/NACK`. This includes wrong DLC, non-Extended-ID frames, foreign destination addresses, and non-`COMMAND` message types. The Conductor must detect these cases by ACK timeout. `NACK` is reserved for syntactically valid `COMMAND` frames with `DLC=8` that fail application-level validation.

The Fluidics `timeout_ms` parser is corrected. Non-zero timeout for `PUMP_START` and `RUN_DURATION (0x0201)` are verified on physical load. For timed actions, `DONE` confirms that the executor accepted the command and started the GPIO/timer action; the later safety auto-off does not emit an additional `DONE`. The Fluidics executor now guards load activation with a successful safety timer start; timer-start failure is an application `NACK`, not an ACK timeout. The domain safe-state hook is implemented and normal-mode regression passed. The Flash config page is reserved outside the application linker region and post-flash `COMMIT -> REBOOT -> GET_INFO` regression passed.

Industrial readiness blockers identified by the code audit:

- perform controlled fault-injection for timer-start and fault-handler paths
- add IWDG with health checks for CAN, Dispatcher, and Application tasks
- add CAN fault and queue overflow counters visible to service diagnostics

The helper script `App_users/can_fluidics_test.sh` is a local Fluidics test tool and can be used as a template for Motion/Thermo executor test scripts.

---

## 4. Follow-up Verification

Remaining tests:

- remaining pump channels `2..12`
- all valve channels `13..15`
- default safety timeout behavior
- industrial fault tests: Flash page reservation, safe-state on fault, IWDG reset, CAN error counters
