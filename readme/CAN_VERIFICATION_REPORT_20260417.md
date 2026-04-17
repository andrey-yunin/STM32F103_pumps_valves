# CAN Verification Report: Fluidics Executor

**Date:** 2026-04-17  
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

The helper script `App_users/can_fluidics_test.sh` is a local Fluidics test tool and can be used as a template for Motion/Thermo executor test scripts.

---

## 4. Follow-up Verification

Remaining tests:

- all pump channels `0..12`
- all valve channels `13..15`
- invalid channel/type NACK tests
- default safety timeout behavior
- `RUN_DURATION (0x0201)` timeout parsing after code audit
