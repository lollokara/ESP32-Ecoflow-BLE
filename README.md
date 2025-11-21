# ESP32 Ecoflow BLE

This project provides a C++ library for ESP32 devices to communicate with EcoFlow power stations over Bluetooth Low Energy (BLE). It allows you to monitor key metrics like battery percentage and power input/output, as well as control the device's AC, DC, and USB ports—all locally, without relying on the cloud.

This library is a port of the excellent Python implementation, bringing its core functionalities to the PlatformIO ecosystem for embedded devices.

## Features

-   **Local Control:** Communicate directly with your EcoFlow device over BLE.
-   **Real-time Monitoring:** Get live data for battery percentage, input/output power, and more.
-   **Remote Control:** Toggle the AC, DC, and USB ports on and off.
-   **Secure:** Implements the EcoFlow authentication handshake and AES-encrypted communication.
-   **REALLY SECURE** SECP160A EC was custom implemented since EF used a not really standard curve

## Supported Devices

Currently, the library has been tested and is known to work with:

-   **EcoFlow Delta 3**
-   **EcoFlow Wave 2 (Partial Support)**

Support for more devices is planned for the future. Contributions are welcome!

## Installation

This library is designed for the PlatformIO ecosystem.

1.  **Clone the Repository:**
    ```
    git clone [repository-url]
    ```

2.  **Add to Your Project:**
    Place the `EcoflowESP32` directory inside the `lib/` folder of your PlatformIO project.

3.  **Credentials:**
    You will need to provide your EcoFlow credentials in a `Credentials.h` file inside the `src/` directory.

    Populate `EcoflowESP32/src/Credentials.h` with the following content:
    ```cpp
    #define ECOFLOW_USER_ID "YOUR_ECOFLOW_USER_ID"
    ```

## How to Use

Here is a basic example of how to use the library to connect to a Delta 3 device and print its battery level. For more advanced examples, including how to manage multiple devices, please see the `examples/` directory.

```cpp
#include <Arduino.h>
#include "DeviceManager.h"
#include "Credentials.h" // Make sure to fill this file with your data

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 example...");

  // Initialize the DeviceManager.
  DeviceManager::getInstance().initialize();

  // Start scanning for a Delta 3 device.
  // After this the device will be paired
  DeviceManager::getInstance().scanAndConnect(DeviceType::DELTA_3);
}

void loop() {
  // The DeviceManager handles all BLE updates.
  DeviceManager::getInstance().update();

  // Get the device instance from the manager.
  EcoflowESP32* delta3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);

  // Check if the device is fully connected and authenticated.
  if (delta3 && delta3->isAuthenticated()) {
    Serial.print("Battery Level: ");
    Serial.print(delta3->getBatteryLevel());
    Serial.println("%");
  } else {
    Serial.println("Device not authenticated, waiting...");
  }

  delay(5000);
}
```

For more detailed examples, please see the `examples/` directory.

## Contributing

Contributions are welcome! If you would like to help improve this library, please feel free to submit a pull request or open an issue. Whether it's adding support for a new device, fixing a bug, or improving documentation, all contributions are appreciated.
