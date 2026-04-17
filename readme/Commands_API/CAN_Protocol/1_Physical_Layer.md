# CAN Protocol: 1. Physical & Data Link Layer

---

## 1.1. Physical Layer

| Parameter         | Value              | Notes                               |
|-------------------|--------------------|-------------------------------------|
| **Bit Rate**      | 1 Mbit/s           | Target DDS-240 ecosystem bus speed. |
| **CAN Standard**  | Classical CAN      | Compatible with STM32F103.          |
| **Termination**   | 120 Ohm            | Standard termination is required.   |
| **Connector**     | DB9 Male           | On the device side.                 |

### Verified Executor Bit Timing

The STM32F103 executor profile verified on the Fluidics board uses:

| Clock / Segment | Value |
|-----------------|-------|
| APB1 CAN clock  | 32 MHz |
| Prescaler       | 2 |
| BS1             | 11 TQ |
| BS2             | 4 TQ |
| SJW             | 1 TQ |
| Sample point    | 75% |

This profile was validated with CANable/SocketCAN on 2026-04-17 and must be kept aligned across Fluidics, Motion, and Thermo executor boards unless the Conductor profile is changed.

### Pinout (DB9 Connector)

| Pin | Signal  | Description         |
|-----|---------|---------------------|
| 2   | CAN-L   | CAN Low Line        |
| 3   | GND     | Ground              |
| 7   | CAN-H   | CAN High Line       |

## 1.2. Data Link Layer

| Parameter          | Value              | Notes                               |
|--------------------|--------------------|-------------------------------------|
| **CAN ID**         | 29-bit (Extended)  | See `2_Frame_Format.md` for details. |
| **DLC**            | 8 bytes            | Directive 2.0 requires strict DLC=8 for executor command/response frames. |
