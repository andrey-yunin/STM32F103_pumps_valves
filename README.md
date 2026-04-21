# DDS-240 Fluidics Executor (Pump & Valve Controller)

![Platform: STM32F103](https://img.shields.io/badge/Platform-STM32F103C8T6-blue)
![RTOS: FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.3-green)
![Standard: DDS--240](https://img.shields.io/badge/Standard-DDS--240%20Compliant-orange)
![Directive: 2.0](https://img.shields.io/badge/Directive-2.0%20(Strict%20DLC=8)-red)

Firmware for the Fluidics Executor of the **DDS-240** ecosystem. The board controls 13 pumps and 3 valves over CAN and implements the industrial executor pattern validated on STM32F103: strict CAN transport, service diagnostics, RTOS resource checks, safe-state handling and IWDG supervision.

---

## Current Status

Industrial safety/CAN/RTOS validation is closed for the reference Fluidics channel 0.

Verified:

- CANable / SocketCAN at `1 Mbit/s`.
- CAN-switch operation with passive termination.
- Strict `DLC=8`, 29-bit Extended ID, broadcast and foreign destination behavior.
- `GET_DEVICE_INFO`, `GET_UID`, `GET_STATUS`.
- `SET_NODE_ID`, `COMMIT`, `REBOOT`, `FACTORY_RESET`.
- `PUMP_START`, `PUMP_STOP`, `RUN_DURATION`, non-zero timeout parsing.
- Safety timer guard and safety timer fault-injection.
- RTOS resource checks and FreeRTOS heap baseline `8192`.
- bxCAN TX ordering with `TransmitFifoPriority = ENABLE`.
- IWDG supervisor normal idle and fault-injection recovery.
- Fault handler safe-state and final production smoke-test after all test flags returned to `0`.

Remaining physical acceptance items:

- Full physical pass for pumps `2..12`.
- Valve channels `13..15`.
- Default safety timeout for valves.

---

## Hardware

| Parameter | Value |
|:---------|:---------|
| **MCU**  | STM32F103C8T6 (64KB Flash, 20KB RAM) |
| **Clock**| 64 MHz (PLL from HSI) |
| **CAN**  | bxCAN, 1 Mbit/s, 29-bit Extended ID |
| **GPIO** | 16 High-Speed Outputs (Pumps 0-12, Valves 13-15) |

---

## CAN Protocol

Default Fluidics `NodeID = 0x30` and Conductor `NodeID = 0x10`.

| Component | Local Indices (ch_idx) | Supported Commands |
|:----------|:---------------------------|:--------|
| **Pumps** | `0 - 12`                   | `RUN_DURATION (0x0201)`, `START (0x0202)`, `STOP (0x0203)` |
| **Valves**| `13 - 15`                  | `OPEN (0x0204)`, `CLOSE (0x0205)` |

Rules:

- `Conductor <-> Executor` always uses strict `DLC=8`.
- `Host -> Conductor` may still use command-specific dynamic DLC.
- Command payload is little-endian.
- Fluidics timeout is `uint32_t LE` in payload bytes `3..6`.
- `DONE` confirms an atomic action, not the end of a recipe interval.

---

## Documentation

Primary ecosystem documentation:

- [DDS-240 Ecosystem Standard](readme/DDS-240_eko_system/DDS-240_ECOSYSTEM_STANDARD.md)
- [Conductor Integration Guide](readme/DDS-240_eko_system/CONDUCTOR_INTEGRATION_GUIDE.md)
- [Executor Industrialization Playbook](readme/DDS-240_eko_system/EXECUTOR_INDUSTRIALIZATION_PLAYBOOK.md)
- [Global Ecosystem Config](readme/DDS-240_eko_system/dds240_global_config.h)
- [Executor Testing Guide](readme/DDS-240_eko_system/EXECUTOR_TESTING_GUIDE.md)

Local Linux/CAN test helper:

- [can_fluidics_test.sh](App_users/can_fluidics_test.sh)

---

## Build And Test

Developed using **STM32CubeIDE 1.19.0**.

1. Clone the repository.
2. Import the project into STM32CubeIDE.
3. Build the project.
4. Flash via ST-Link.
5. Configure SocketCAN and run the production smoke-test:

```bash
./App_users/can_fluidics_test.sh setup
./App_users/can_fluidics_test.sh info
./App_users/can_fluidics_test.sh pump-cycle 0 1
sleep 10
./App_users/can_fluidics_test.sh info
```

Expected result: `ACK/DONE` for domain commands and `ACK -> DATA -> DONE` for `GET_DEVICE_INFO`.

---
© 2026 DDS-240 Ecosystem | Senior Embedded Engineering Team
