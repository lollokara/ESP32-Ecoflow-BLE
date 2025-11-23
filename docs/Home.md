# ⧉ ECOFLOW-ESP32 // WIKI_INDEX

> **ACCESS LEVEL:** PUBLIC
> **STATUS:** ONLINE
> **SYSTEM:** ECOFLOW CONTROL PROTOCOL

Welcome to the technical core of the EcoflowESP32 project. This documentation provides a deep dive into the architecture, protocol reverse-engineering, and hardware implementation.

---

## ≡ NAVIGATION

| MODULE | DESCRIPTION |
| :--- | :--- |
| **[>> SYSTEM ARCHITECTURE](Architecture.md)** | High-level system design, class hierarchies, and state machines. |
| **[>> PROTOCOL REFERENCE](Protocol.md)** | Byte-level analysis of the V2/V3 protocols, packet structures, and encryption. |
| **[>> HARDWARE SCHEMATICS](Hardware_Reference.md)** | Wiring diagrams, pinouts, and component specifications. |
| **[>> CODEBASE MAP](Code_Reference.md)** | API documentation, class breakdowns, and extension guides. |

---

## ≡ PROJECT OVERVIEW

The **EcoflowESP32** library is a reverse-engineered implementation of the EcoFlow BLE protocol for ESP32 microcontrollers. It enables local, cloud-free monitoring and control of EcoFlow power stations.

### KEY CAPABILITIES
*   **Protocol V2 & V3 Support:** Native handling of both binary (Wave 2) and Protobuf (Delta 3/Pro 3) payloads.
*   **Cryptographic Handshake:** Full implementation of the ECDH + AES-128-CBC authentication flow.
*   **Multi-Device Management:** Singleton-based `DeviceManager` capable of handling multiple concurrent device slots.
*   **Reactive UI:** Integrated driver for APA102/DotStar LED matrices and button input handling.

---

> *ERROR: CLOUD CONNECTION NOT FOUND.*
> *INITIATING LOCAL CONTROL OVERRIDE...*
