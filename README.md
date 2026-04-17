# 🧬 DDS-240 Fluidic Executor (Pump & Valve Controller)

![Platform: STM32F103](https://img.shields.io/badge/Platform-STM32F103C8T6-blue)
![RTOS: FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.3-green)
![Standard: DDS--240](https://img.shields.io/badge/Standard-DDS--240%20Compliant-orange)
![Directive: 2.0](https://img.shields.io/badge/Directive-2.0%20(Strict%20DLC=8)-red)

Firmware for the Fluidic Execution Unit of the **DDS-240** Biochemical Analyzer. This module manages 13 pumps and 3 valves, providing high-precision timing and a multi-level safety system.

---

## 🚀 Key Features

- **Directive 2.0 Ready**: Strict transport unification (DLC=8) and 0-based resource indexing.
- **Safety First Architecture**: Built-in hardware and software Safety Timeouts to prevent overflows and hardware damage.
- **Advanced Service Layer**: Remote diagnostics, MCU UID retrieval, software reboot, and factory reset via CAN bus.
- **Flexible Mapping**: Software-defined mapping of logical channels to physical GPIO pins via non-volatile Flash configuration.
- **Deterministic Bus Handling**: Robust bxCAN (1 Mbps) implementation with Mailbox Guard protection.

---

## 🛠 Hardware Specifications

| Parameter | Value |
|:---------|:---------|
| **MCU**  | STM32F103C8T6 (64KB Flash, 20KB RAM) |
| **Clock**| 64 MHz (PLL from HSI) |
| **CAN**  | bxCAN, 1 Mbps, 29-bit Extended ID |
| **GPIO** | 16 High-Speed Outputs (Pumps 0-12, Valves 13-15) |

---

## 📡 Control Protocol (CAN ID Map)

The system operates on a **Resource-Based Model**. Each board has a default `NodeID = 0x30` (configurable).

| Component | Local Indices (ch_idx) | Supported Commands |
|:----------|:---------------------------|:--------|
| **Pumps** | `0 - 12`                   | `RUN_DURATION (0x0201)`, `START (0x0202)`, `STOP (0x0203)` |
| **Valves**| `13 - 15`                  | `OPEN (0x0204)`, `CLOSE (0x0205)` |

> **Important:** All commands require **DLC=8**. Parameters (e.g., Timeout in ms) must be passed in bytes 3-6 (Little-Endian).

---

## 📂 Documentation Structure

Detailed documentation is available in the `readme/` directory:

- 📘 [Conductor Integration Guide](readme/CONDUCTOR_INTEGRATION_GUIDE.md) — Command API and transaction diagrams.
- 📜 [Executor Report](readme/PUMPS_VALVES_EXECUTOR_REPORT.md) — GPIO pinout, memory map, and version history.
- ✅ [CAN Verification Report](readme/CAN_VERIFICATION_REPORT_20260417.md) — CANable validation at 1 Mbps.
- ⚡ [Refactoring Plan](readme/refactoring_plan.md) — Current status and development history.
- 🏗 [Ecosystem Pattern](readme/pattern/) — Reference implementation and ecosystem standards.

---

## 🔨 Build & Flash

Developed using **STM32CubeIDE 1.19.0**.

1. Clone the repository.
2. Import the project into STM32CubeIDE.
3. Configure CAN parameters (if needed) in `App/inc/can_protocol.h`.
4. Build the project and flash via ST-Link.

---
© 2026 DDS-240 Ecosystem | Senior Embedded Engineering Team
