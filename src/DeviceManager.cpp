/**
 * @file DeviceManager.cpp
 * @author Lollokara
 * @brief Implementation for the DeviceManager class.
 *
 * This file contains the logic for managing multiple EcoFlow devices, including
 * BLE scanning, connection management, and persistence of device details.
 */

#include "DeviceManager.h"
#include "Credentials.h"
#include <NimBLEDevice.h>

/**
 * @brief Extracts the device serial number from the BLE manufacturer data.
 * @param manufacturerData The raw manufacturer data from the advertisement packet.
 * @return The extracted serial number as a string, or an empty string if not found.
 */
static std::string extractSerial(const std::string& manufacturerData) {
    // The serial number is typically embedded in the manufacturer data payload.
    // For EcoFlow devices, it is 16 bytes long and starts at an offset.
    if (manufacturerData.length() < 19) return "";

    char serialBuf[17];
    memcpy(serialBuf, manufacturerData.data() + 3, 16);
    serialBuf[16] = '\0';
    return std::string(serialBuf);
}

//--------------------------------------------------------------------------
//--- Singleton and Constructor
//--------------------------------------------------------------------------

DeviceManager& DeviceManager::getInstance() {
    static DeviceManager instance;
    return instance;
}

DeviceManager::DeviceManager() {
    // Initialize device slots for Delta 3 and Wave 2
    slotD3.instance = &d3;
    slotD3.name = "D3";
    slotD3.type = DeviceType::DELTA_3;
    slotD3.isConnected = false;

    slotW2.instance = &w2;
    slotW2.name = "W2";
    slotW2.type = DeviceType::WAVE_2;
    slotW2.isConnected = false;

    _scanMutex = xSemaphoreCreateMutex();
}

//--------------------------------------------------------------------------
//--- Public Methods
//--------------------------------------------------------------------------

void DeviceManager::initialize() {
    NimBLEDevice::init(""); // Initialize BLE centrally
    prefs.begin("ecoflow", false);
    loadDevices();

    // Initialize instances for any saved devices
    if (!slotD3.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring D3: %s", slotD3.serialNumber.c_str());
        slotD3.instance->begin(ECOFLOW_USER_ID, slotD3.serialNumber, slotD3.macAddress, 3);
    }
    if (!slotW2.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring W2: %s", slotW2.serialNumber.c_str());
        slotW2.instance->begin(ECOFLOW_USER_ID, slotW2.serialNumber, slotW2.macAddress, 2);
    }
}

void DeviceManager::update() {
    // The main loop is split into three parts:
    // 1. Handle any pending connection found during a scan.
    _handlePendingConnection();

    // 2. Call the update loop for each device instance.
    slotD3.instance->update();
    slotW2.instance->update();
    slotD3.isConnected = slotD3.instance->isConnected();
    slotW2.isConnected = slotW2.instance->isConnected();

    // 3. Manage the scanning state (auto-reconnect or timeout).
    _manageScanning();
}

void DeviceManager::scanAndConnect(DeviceType type) {
    if (_isScanning) return;
    startScan(type);
}

void DeviceManager::disconnect(DeviceType type) {
    DeviceSlot* slot = getSlot(type);
    if (slot) {
        slot->instance->disconnectAndForget();
        if (type == DeviceType::DELTA_3) {
            prefs.remove("d3_mac");
            prefs.remove("d3_sn");
        } else {
            prefs.remove("w2_mac");
            prefs.remove("w2_sn");
        }
        slot->macAddress = "";
        slot->serialNumber = "";
    }
}

EcoflowESP32* DeviceManager::getDevice(DeviceType type) {
    return (type == DeviceType::DELTA_3) ? &d3 : &w2;
}

DeviceSlot* DeviceManager::getSlot(DeviceType type) {
    return (type == DeviceType::DELTA_3) ? &slotD3 : &slotW2;
}

bool DeviceManager::isScanning() {
    return _isScanning;
}

bool DeviceManager::isAnyConnecting() {
    return d3.isConnecting() || w2.isConnecting();
}


//--------------------------------------------------------------------------
//--- Private Helper Methods
//--------------------------------------------------------------------------

/**
 * @brief Handles the connection logic for a device that was found during a scan.
 * This is called from the main update loop to avoid blocking the BLE callback.
 */
void DeviceManager::_handlePendingConnection() {
    if (!_hasPendingConnection) return;

    if (xSemaphoreTake(_scanMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        if (_pendingDevice) {
            // Stop the scan before attempting to connect
            if (pScan && pScan->isScanning()) {
                pScan->stop();
                _isScanning = false;
            }

            // Defer connection if another device is already in the process of connecting
            if (isAnyConnecting()) {
                ESP_LOGW("DeviceManager", "Another device is connecting, deferring pending connection");
                xSemaphoreGive(_scanMutex);
                return;
            }

            ESP_LOGI("DeviceManager", "Executing pending connection...");
            DeviceSlot* slot = getSlot(_targetScanType);
            saveDevice(_targetScanType, _pendingDevice->getAddress().toString(), slot->serialNumber);

            uint8_t version = (_targetScanType == DeviceType::WAVE_2) ? 2 : 3;
            slot->instance->begin(ECOFLOW_USER_ID, slot->serialNumber, slot->macAddress, version);
            slot->instance->connectTo(_pendingDevice);

            // Clean up
            delete _pendingDevice;
            _pendingDevice = nullptr;
            _hasPendingConnection = false;
        }
        xSemaphoreGive(_scanMutex);
    }
}

/**
 * @brief Manages the BLE scanning state, initiating scans for disconnected devices
 * and handling scan timeouts.
 */
void DeviceManager::_manageScanning() {
    if (_isScanning) {
        // Stop scanning if a device starts connecting or if the scan times out
        if (isAnyConnecting()) {
            stopScan();
            ESP_LOGI("DeviceManager", "Stopping scan due to active connection attempt");
        } else if (millis() - _scanStartTime > 10000) { // 10-second scan timeout
            stopScan();
            ESP_LOGI("DeviceManager", "Scan timeout");
        }
    } else if (!isAnyConnecting()) {
        // If not scanning and no device is connecting, check if we need to start a scan
        bool d3NeedsConnect = !slotD3.isConnected && !slotD3.macAddress.empty() && !d3.isConnecting();
        bool w2NeedsConnect = !slotW2.isConnected && !slotW2.macAddress.empty() && !w2.isConnecting();

        if (d3NeedsConnect || w2NeedsConnect) {
            startScan(d3NeedsConnect ? DeviceType::DELTA_3 : DeviceType::WAVE_2);
        }
    }
}

void DeviceManager::loadDevices() {
    String mac = prefs.getString("d3_mac", "");
    String sn = prefs.getString("d3_sn", "");
    if (!mac.isEmpty() && !sn.isEmpty()) {
        slotD3.macAddress = mac.c_str();
        slotD3.serialNumber = sn.c_str();
    }

    mac = prefs.getString("w2_mac", "");
    sn = prefs.getString("w2_sn", "");
    if (!mac.isEmpty() && !sn.isEmpty()) {
        slotW2.macAddress = mac.c_str();
        slotW2.serialNumber = sn.c_str();
    }
}

void DeviceManager::saveDevice(DeviceType type, const std::string& mac, const std::string& sn) {
    if (type == DeviceType::DELTA_3) {
        prefs.putString("d3_mac", mac.c_str());
        prefs.putString("d3_sn", sn.c_str());
        slotD3.macAddress = mac;
        slotD3.serialNumber = sn;
    } else {
        prefs.putString("w2_mac", mac.c_str());
        prefs.putString("w2_sn", sn.c_str());
        slotW2.macAddress = mac;
        slotW2.serialNumber = sn;
    }
}


//--------------------------------------------------------------------------
//--- BLE Scanning Logic
//--------------------------------------------------------------------------

void DeviceManager::startScan(DeviceType type) {
    ESP_LOGI("DeviceManager", "Starting scan for type %d", (int)type);
    _targetScanType = type;
    _isScanning = true;
    _scanStartTime = millis();
    _hasPendingConnection = false;

    if (!pScan) {
        pScan = NimBLEDevice::getScan();
        pScan->setAdvertisedDeviceCallbacks(new ManagerScanCallbacks(this), true);
        pScan->setActiveScan(true);
        pScan->setInterval(100);
        pScan->setWindow(99);
    }
    pScan->start(0, nullptr, false); // Non-blocking scan
}

void DeviceManager::stopScan() {
    if (xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
        if (pScan && pScan->isScanning()) {
            pScan->stop();
        }
        _isScanning = false;
        xSemaphoreGive(_scanMutex);
    }
}

void DeviceManager::ManagerScanCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    if (_parent->isScanning()) {
        _parent->onDeviceFound(advertisedDevice);
    }
}

/**
 * @brief Callback function that is executed when a BLE device is found during a scan.
 * It checks if the found device matches a configured device and flags it for connection.
 */
void DeviceManager::onDeviceFound(NimBLEAdvertisedDevice* device) {
    if (_hasPendingConnection) return; // Already found a device, ignore others

    if (xSemaphoreTake(_scanMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) {
        if (_hasPendingConnection) {
            xSemaphoreGive(_scanMutex);
            return;
        }

        if (!device->haveManufacturerData()) {
            xSemaphoreGive(_scanMutex);
            return;
        }

        std::string sn = extractSerial(device->getManufacturerData());
        if (sn.empty()) {
            xSemaphoreGive(_scanMutex);
            return;
        }

        DeviceSlot* targetSlot = nullptr;
        if (isTargetDevice(sn, DeviceType::DELTA_3) && !slotD3.isConnected && !d3.isConnecting()) {
            targetSlot = &slotD3;
        } else if (isTargetDevice(sn, DeviceType::WAVE_2) && !slotW2.isConnected && !w2.isConnecting()) {
            targetSlot = &slotW2;
        }

        if (targetSlot) {
            bool macMatch = !targetSlot->macAddress.empty() && (device->getAddress().toString() == targetSlot->macAddress);
            bool isNewDeviceScan = targetSlot->macAddress.empty() && _targetScanType == targetSlot->type;

            if (macMatch || isNewDeviceScan) {
                ESP_LOGI("DeviceManager", "Match found for %s (%s)! Pending connection...", targetSlot->name.c_str(), sn.c_str());
                if (_pendingDevice) delete _pendingDevice;
                _pendingDevice = new NimBLEAdvertisedDevice(*device);
                _hasPendingConnection = true;
                _targetScanType = targetSlot->type; // Lock in the target type
            }
        }
        xSemaphoreGive(_scanMutex);
    }
}

bool DeviceManager::isTargetDevice(const std::string& sn, DeviceType type) {
    if (type == DeviceType::DELTA_3) {
        return (sn.rfind("P2", 0) == 0); // Delta 3 devices
    } else {
        return (sn.rfind("KT", 0) == 0); // Wave 2 devices
    }
}
