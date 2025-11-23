#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "EcoflowESP32.h"
#include "types.h"
#include <vector>
#include <deque>
#include <Preferences.h>

/**
 * @struct DeviceSlot
 * @brief Holds the state and information for a single managed device.
 */
struct DeviceSlot {
    EcoflowESP32* instance;
    std::string macAddress;
    std::string serialNumber;
    std::string name; // "D3" or "W2"
    DeviceType type;
    bool isConnected;
};

/**
 * @class DeviceManager
 * @brief A singleton class to manage BLE connections to multiple EcoFlow devices.
 *
 * The DeviceManager handles the entire lifecycle of device connections, including:
 * - Scanning for specific devices.
 * - Connecting using saved credentials.
 * - Reconnecting if a connection is lost.
 * - Providing access to the underlying EcoflowESP32 instances.
 */
class DeviceManager {
public:
    /**
     * @brief Gets the singleton instance of the DeviceManager.
     * @return Reference to the DeviceManager instance.
     */
    static DeviceManager& getInstance();

    /**
     * @brief Initializes the manager, loads device configurations, and sets up BLE scanning.
     */
    void initialize();

    /**
     * @brief Main loop function for the manager, handles connection state and scanning.
     */
    void update();

    /**
     * @brief Starts a BLE scan to find and connect to a specific device type.
     * @param type The type of device to scan for.
     */
    void scanAndConnect(DeviceType type);

    /**
     * @brief Disconnects from a device and clears its saved configuration.
     * @param type The type of device to disconnect from.
     */
    void disconnect(DeviceType type);

    /**
     * @brief Retrieves the EcoflowESP32 instance for a given device type.
     * @param type The device type.
     * @return A pointer to the EcoflowESP32 instance, or nullptr if not found.
     */
    EcoflowESP32* getDevice(DeviceType type);

    /**
     * @brief Retrieves the DeviceSlot for a given device type.
     * @param type The device type.
     * @return A pointer to the DeviceSlot, or nullptr if not found.
     */
    DeviceSlot* getSlot(DeviceType type);

    /**
     * @brief Checks if the manager is currently scanning for devices.
     * @return True if scanning, false otherwise.
     */
    bool isScanning();

    /**
      * @brief Checks if any device is currently in the process of connecting.
      * @return True if any device is connecting, false otherwise.
      */
    bool isAnyConnecting();

    // --- Management Commands ---
    void printStatus();
    void forget(DeviceType type);
    String getDeviceStatusJson();

    // --- Telemetry History ---
    std::vector<int> getWave2TempHistory();
    std::vector<int> getSolarHistory(DeviceType type);

private:
    DeviceManager();

    // Device instances
    EcoflowESP32 d3;
    EcoflowESP32 w2;
    EcoflowESP32 d3p;
    EcoflowESP32 ac;

    DeviceSlot slotD3;
    DeviceSlot slotW2;
    DeviceSlot slotD3P;
    DeviceSlot slotAC;

    Preferences prefs;

    // Telemetry History
    std::deque<int8_t> _wave2History;
    std::deque<int16_t> _d3SolarHistory;
    std::deque<int16_t> _d3pSolarHistory;
    uint32_t _lastHistorySample = 0;

    // BLE Scanning members
    NimBLEScan* pScan = nullptr;
    bool _isScanning = false;
    uint32_t _scanStartTime = 0;
    DeviceType _targetScanType;

    /**
     * @class ManagerScanCallbacks
     * @brief Internal class to handle callbacks for BLE scan results.
     */
    class ManagerScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
        DeviceManager* _parent;
    public:
        ManagerScanCallbacks(DeviceManager* parent) : _parent(parent) {}
        void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
    };

    void startScan(DeviceType type);
    void stopScan();
    void onDeviceFound(NimBLEAdvertisedDevice* device);
    bool isTargetDevice(const std::string& sn, DeviceType type);
    void saveDevice(DeviceType type, const std::string& mac, const std::string& sn);
    void loadDevices();
    void _handlePendingConnection();
    void _manageScanning();
    void _updateHistory();

    // Connection queue members (not currently used but kept for potential future use)
    bool _hasPendingConnection = false;
    std::string _pendingSN;
    NimBLEAdvertisedDevice* _pendingDevice = nullptr;
    SemaphoreHandle_t _scanMutex;
};

#endif
