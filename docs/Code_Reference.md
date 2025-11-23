# ⧉ CODE REFERENCE // API_DOCS

> **ACCESS LEVEL:** DEVELOPER
> **LANGUAGE:** C++17

This section documents the core classes and public APIs available in the EcoflowESP32 library.

---

## ≡ EcoflowESP32

The primary class representing a physical device connection.

### INITIALIZATION
```cpp
bool begin(string userId, string sn, string mac, uint8_t protoVer = 3);
```
Initializes the internal state, sets the target device credentials, and prepares the crypto engine.
*   **protoVer:** `2` for Wave 2, `3` for Delta 3/Pro 3.

### DATA ACCESS
```cpp
const EcoflowData& getData() const;
```
Returns a read-only reference to the latest data structure.

```cpp
// Helper Getters
int getBatteryLevel();      // Returns 0-100%
int getInputPower();        // Total Input (Watts)
int getOutputPower();       // Total Output (Watts)
int getCellTemperature();   // Battery Temp (°C)
```

### CONTROL METHODS
All control methods return `true` if the command was successfully queued.

```cpp
bool setAC(bool on);        // Toggle AC Inverter
bool setDC(bool on);        // Toggle 12V DC Output
bool setUSB(bool on);       // Toggle USB Ports
bool setAcChargingLimit(int watts); // Set AC Charge Speed
```

---

## ≡ DeviceManager

A singleton that manages the lifecycle of multiple devices.

### SCANNING & CONNECTION
```cpp
void scanAndConnect(DeviceType type);
```
Starts a BLE scan for the specified device type (`DELTA_3`, `WAVE_2`, etc.). It filters results by Manufacturer ID and connects automatically to the first match.

### DEVICE RETRIEVAL
```cpp
EcoflowESP32* getDevice(DeviceType type);
```
Returns a pointer to the active device instance. Always check `if (dev && dev->isAuthenticated())` before using control commands.

---

## ≡ EcoflowData

The central data structure holds the telemetry for all supported devices.

```cpp
struct EcoflowData {
    bool isConnected;
    Delta3Data delta3;          // Active if type == DELTA_3
    Wave2Data wave2;            // Active if type == WAVE_2
    DeltaPro3Data deltaPro3;    // Active if type == DELTA_PRO_3
    // ...
};
```

**Common Fields:**
To simplify UI logic, many getters in `EcoflowESP32` automatically route to the correct substruct field based on the active device type.

---

## ≡ ADDING NEW DEVICES

To add support for a new EcoFlow device:

1.  **Define Data Struct:** Add a new substruct to `EcoflowData.h` (e.g., `River2Data`).
2.  **Update Parser:** Modify `EcoflowDataParser.cpp` to handle the new device's Protobuf definition or binary layout.
3.  **Register Type:** Add a new enum to `DeviceType` in `types.h`.
4.  **Instantiate:** Add a new slot in `DeviceManager.h`.

---

> *SOURCE CODE INDEXED.*
> *API INTERFACE LOADED.*
