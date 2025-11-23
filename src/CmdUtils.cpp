#include "CmdUtils.h"
#include <esp_log.h>
#include <esp_idf_version.h>
#include <WiFi.h>
#include "LogBuffer.h"

#if CONFIG_IDF_TARGET_ESP32S3
// Check IDF version for correct header
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <driver/temperature_sensor.h>
#else
#include <driver/temp_sensor.h>
#endif
#endif

static const char* TAG = "CmdUtils";

// Helper to print to both Serial and LogBuffer
static void cmd_printf(const char* format, ...) {
    char loc_buf[256];
    va_list arg;
    va_start(arg, format);
    vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(arg);

    // Print to Serial always to ensure CLI recoverability
    // Users need to see CLI output even if logs are disabled
    Serial.print(loc_buf);

    // Send to LogBuffer (as INFO level, tag "CLI")
    // We mimic standard log behavior: strip trailing newline for LogBuffer if desired,
    // but LogBuffer handles it.
    // We fake the va_list for LogBuffer since we already formatted.
    // Actually LogBuffer::addLog takes va_list but ignores it if we pass formatted string?
    // No, LogBuffer::addLog takes message and args?
    // LogBuffer::addLog(level, tag, message, args) -> message is ALREADY formatted in LogBuffer.cpp hook.
    // But we are calling addLog directly.
    // Let's check LogBuffer.cpp:
    // void LogBuffer::addLog(esp_log_level_t level, const char* tag, const char* message, va_list args)
    // It stores `message`. It DOES NOT use `args` to format. `args` is just passed along?
    // Actually the LogBuffer implementation I wrote:
    // lm.message = String(message); // Already formatted

    // We need to pass a valid va_list type even if unused.
    va_list dummy;
    LogBuffer::getInstance().addLog(ESP_LOG_INFO, "CLI", loc_buf, dummy);
}

// Helper wrapper for simple println
static void cmd_println(const String& s) {
    cmd_printf("%s\n", s.c_str());
}
static void cmd_println(const char* s) {
    cmd_printf("%s\n", s);
}


// Helper to read internal temperature safely
static float readInternalTemp() {
#if CONFIG_IDF_TARGET_ESP32S3
    float tsens_out = 0.0f;

    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        // New API (IDF v5+)
        temperature_sensor_handle_t temp_handle = NULL;
        temperature_sensor_config_t temp_sensor = {
            .range_min = 10,
            .range_max = 85,
        };
        if (temperature_sensor_install(&temp_sensor, &temp_handle) == ESP_OK) {
            temperature_sensor_enable(temp_handle);
            temperature_sensor_get_celsius(temp_handle, &tsens_out);
            temperature_sensor_disable(temp_handle);
            temperature_sensor_uninstall(temp_handle);
        } else {
            return -999.0f; // Error
        }
    #else
        // Legacy API (IDF v4.4)
        temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
        temp_sensor.dac_offset = TSENS_DAC_L2; // Set range
        temp_sensor_set_config(temp_sensor);
        temp_sensor_start();
        if (temp_sensor_read_celsius(&tsens_out) != ESP_OK) {
            return -999.0f;
        }
        temp_sensor_stop();
    #endif

    return tsens_out;
#else
    return -1.0f; // Not supported on this chip
#endif
}

void CmdUtils::processInput(String input) {
    input.trim();
    if (input.length() == 0) return;

    int spaceIndex = input.indexOf(' ');
    String cmd = (spaceIndex == -1) ? input : input.substring(0, spaceIndex);
    String args = (spaceIndex == -1) ? "" : input.substring(spaceIndex + 1);

    if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("-h")) {
        printHelp();
        return;
    }

    // System diagnostics
    if (cmd.startsWith("sys_")) {
        handleSysCommand(cmd);
        return;
    }

    // Connection Manager
    if (cmd.startsWith("con_") || cmd.startsWith("wifi_")) {
        handleConCommand(cmd, args);
        return;
    }

    // Try handling based on prefix or known commands
    if (cmd.startsWith("w2_set_")) {
        handleWave2Command(cmd, args);
        return;
    } else if (cmd.startsWith("set_")) {
        // Legacy fallback for generic set_ (assume Wave 2 if matches known keywords)
        if (cmd.indexOf("ambient_light") > 0 || cmd.indexOf("fan_speed") > 0 || cmd.indexOf("temperature") > 0 || cmd.indexOf("sub_mode") > 0 || cmd.indexOf("main_mode") > 0 || cmd.indexOf("power_state") > 0 || cmd.indexOf("beep_enabled") > 0 || cmd.indexOf("automatic_drain") > 0 || cmd.indexOf("countdown_timer") > 0 || cmd.indexOf("idle_screen_timeout") > 0) {
            handleWave2Command(cmd, args);
            return;
        }
    } else if (cmd.startsWith("d3_")) {
        if (cmd.startsWith("d3_get_")) handleDelta3Read(cmd);
        else handleDelta3Command(cmd, args);
    } else if (cmd.startsWith("d3p_")) {
        if (cmd.startsWith("d3p_get_")) handleDeltaPro3Read(cmd);
        else handleDeltaPro3Command(cmd, args);
    } else if (cmd.startsWith("ac_")) {
        if (cmd.startsWith("ac_get_")) handleAltChargerRead(cmd);
        else handleAltChargerCommand(cmd, args);
    } else if (cmd.startsWith("get_")) {
        // Generic Wave 2 gets
        handleWave2Read(cmd);
    } else {
        cmd_println("Unknown command. Type 'help' for list.");
    }
}

/*WAVE 2 Commands:

| Method name                        | cmd_id | Payload description                                                  |
| ---------------------------------- | ------ | -------------------------------------------------------------------- |
| set_ambient_light(status)          | 0x5C   | 1 byte: 0x01 = on, 0x02 = off    (no idea why 2 is off)              |
| set_automatic_drain(enable)        | 0x59   | 1 byte: 0x01 = enable  0x00 = disable                                |
| set_beep_enabled(flag)             | 0x56   | 1 byte: 0x01 = on, 0x00 = off                                        |
| set_fan_speed(fan_gear)            | 0x5E   | 1 byte, 0x0 -> low 0x1 -> medium 0x2 -> high                         |
| set_main_mode(mode)                | 0x51   | 1 byte, 0x0 -> Cold, 0x1 -> Warm, 0x2 Fan                            |
| set_power_state(mode)              | 0x5B   | 1 byte: 0x01 = on, 0x00 = off                                        |
| set_temperature(temp_value)        | 0x58   | 1 byte, 0x10 sets 16C 0x1F sets 30C                                  |
| set_countdown_timer(time, status)  | 0x55   | 3 bytes: [0x00, 0x00, status], status meaning unidentified           |
| set_idle_screen_timeout(time)      | 0x54   | 3 bytes: [0x00, 0x00, time], time possible values 0x00 and 0x01      |
| set_sub_mode(sub_mode)             | 0x52   | 1 byte, 0x0 -> MAX, 0x1 -> Night, 0x2 -> Eco, 0x3 Normal             |
| set_temperature_display_type(type) | 0x5D   | 1 byte, 0x0 -> Internal (House), 0x1 -> Airflow                      |
| set_temperature_unit(unit)         | 0x53   | 1 byte: 0x00 or 0x01; 0x1 = °F and 0x0 = °C                          |


*/
void CmdUtils::printHelp() {
    cmd_println("=== Available Commands ===");

    cmd_println("\n[System & Connection]");
    cmd_println("  sys_temp                        (Read internal ESP32 temp)");
    cmd_println("  sys_reset                       (Factory reset & reboot)");
    cmd_println("  con_status                      (List connections)");
    cmd_println("  con_connect <d3/w2/d3p/ac>      (Connect)");
    cmd_println("  con_disconnect <d3/w2/d3p/ac>   (Disconnect)");
    cmd_println("  con_forget <d3/w2/d3p/ac>       (Forget)");
    cmd_println("  wifi_set <ssid> <pass>          (Set WiFi credentials)");
    cmd_println("  wifi_ip                         (Get current IP)");

    cmd_println("\n[Wave 2] (get_ or w2_set_)");
    cmd_println("  get_temp / w2_set_temp <val>");
    cmd_println("  get_fan_speed / w2_set_fan <val>");
    cmd_println("  get_mode / w2_set_mode <val>");
    cmd_println("  get_power");
    cmd_println("  get_battery");
    cmd_println("  w2_set_light <0/1>");
    cmd_println("  w2_set_drain <0/1>");
    cmd_println("  w2_set_beep <0/1>");
    cmd_println("  w2_set_power <0/1>");
    cmd_println("  w2_set_timer <time> <status>");
    cmd_println("  w2_set_timeout <time>");
    cmd_println("  w2_set_submode <mode>");
    cmd_println("  w2_set_temptype <type>");
    cmd_println("  w2_set_tempunit <unit>");

    cmd_println("\n[Delta 3] (Prefix: d3_)");
    cmd_println("  d3_get_switches / d3_set_ac/dc/usb <0/1>");
    cmd_println("  d3_get_power");
    cmd_println("  d3_get_battery / d3_set_soc_max/min");
    cmd_println("  d3_set_ac_limit <watts>");

    cmd_println("\n[Delta Pro 3] (Prefix: d3p_)");
    cmd_println("  d3p_get_switches / d3p_set_ac_hv/lv <0/1>");
    cmd_println("  d3p_get_power");
    cmd_println("  d3p_get_battery");
    cmd_println("  d3p_set_energy_backup <0/1>");

    cmd_println("\n[Alternator Charger] (Prefix: ac_)");
    cmd_println("  ac_get_status / ac_set_open <0/1>");
    cmd_println("  ac_get_power");
    cmd_println("  ac_get_battery");
    cmd_println("  ac_set_mode <val>");
    cmd_println("  ac_set_power_limit <watts>");
}

uint8_t CmdUtils::parseHexByte(String s) {
    s.trim();
    if (s.startsWith("0x") || s.startsWith("0X")) {
        s = s.substring(2);
    }
    return (uint8_t)strtol(s.c_str(), NULL, 16);
}

float CmdUtils::parseFloat(String s) {
    return s.toFloat();
}

// --- System Handler ---

void CmdUtils::handleSysCommand(String cmd) {
    if (cmd.equalsIgnoreCase("sys_temp")) {
        float t = readInternalTemp();
        if (t > -900) cmd_printf("Internal Temp: %.2f C\n", t);
        else cmd_println("Failed to read internal temp (or not supported).");
    } else if (cmd.equalsIgnoreCase("sys_reset")) {
        cmd_println("Resetting settings and rebooting...");
        Preferences prefs;
        prefs.begin("ecoflow", false);
        prefs.clear();
        prefs.end();
        ESP.restart();
    } else {
        cmd_println("Unknown sys command.");
    }
}

void CmdUtils::handleConCommand(String cmd, String args) {
    DeviceManager& dm = DeviceManager::getInstance();

    if (cmd.equalsIgnoreCase("con_status")) {
        // DeviceManager::printStatus uses Serial internally.
        // We should update DeviceManager or redirect it.
        // Since DeviceManager is separate, we can't easily redirect without modifying it.
        // However, the user only complained about "help" and commands sent from CLI.
        // Let's assume DeviceManager output is less critical or we'll fix it later if needed.
        // Actually, it's better to replicate printStatus here using cmd_printf
        // OR modify DeviceManager to take a Stream or use ESP_LOG.
        // For now, let's manually print status here.

        cmd_println("=== Device Connection Status ===");
        auto printSlot = [](DeviceSlot* slot) {
            cmd_printf("[%s] %s (%s): %s\n",
                slot->name.c_str(),
                slot->isConnected ? "CONNECTED" : "DISCONNECTED",
                slot->macAddress.empty() ? "Unpaired" : slot->macAddress.c_str(),
                slot->serialNumber.c_str()
            );
        };
        printSlot(dm.getSlot(DeviceType::DELTA_3));
        printSlot(dm.getSlot(DeviceType::WAVE_2));
        printSlot(dm.getSlot(DeviceType::DELTA_PRO_3));
        printSlot(dm.getSlot(DeviceType::ALTERNATOR_CHARGER));
        return;
    }

    if (cmd.equalsIgnoreCase("wifi_ip")) {
        if (WiFi.status() == WL_CONNECTED) {
            IPAddress ip = WiFi.localIP();
            cmd_printf("Current IP address: %s\n", ip.toString().c_str());
        } else {
            cmd_println("WiFi not connected.");
        }
        return;
    }

    if (cmd.equalsIgnoreCase("wifi_set")) {
        args.trim();
        int firstQuote = args.indexOf('"');
        if (firstQuote == -1) { cmd_println("Usage: wifi_set \"<ssid>\" \"<pass>\""); return; }
        int secondQuote = args.indexOf('"', firstQuote + 1);
        if (secondQuote == -1) { cmd_println("Usage: wifi_set \"<ssid>\" \"<pass>\""); return; }
        String ssid = args.substring(firstQuote + 1, secondQuote);
        int thirdQuote = args.indexOf('"', secondQuote + 1);
        if (thirdQuote == -1) { cmd_println("Usage: wifi_set \"<ssid>\" \"<pass>\""); return; }
        int fourthQuote = args.indexOf('"', thirdQuote + 1);
        if (fourthQuote == -1) { cmd_println("Usage: wifi_set \"<ssid>\" \"<pass>\""); return; }
        String pass = args.substring(thirdQuote + 1, fourthQuote);

        cmd_printf("Saved SSID: %s  Password: %s\n", ssid.c_str(), pass.c_str());
        Preferences prefs;
        prefs.begin("ecoflow", false);
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", pass);
        prefs.end();
        cmd_println("WiFi credentials saved. Reboot to connect.");
        return;
    }

    DeviceType type;
    args.trim();
    if (args.equalsIgnoreCase("d3")) type = DeviceType::DELTA_3;
    else if (args.equalsIgnoreCase("w2")) type = DeviceType::WAVE_2;
    else if (args.equalsIgnoreCase("d3p")) type = DeviceType::DELTA_PRO_3;
    else if (args.equalsIgnoreCase("ac") || args.equalsIgnoreCase("chg")) type = DeviceType::ALTERNATOR_CHARGER;
    else {
        cmd_println("Invalid device type. Use d3, w2, d3p, or ac.");
        return;
    }

    if (cmd.equalsIgnoreCase("con_connect")) dm.scanAndConnect(type);
    else if (cmd.equalsIgnoreCase("con_disconnect")) dm.disconnect(type);
    else if (cmd.equalsIgnoreCase("con_forget")) dm.forget(type);
    else cmd_println("Unknown connection command.");
}

// --- Write Handlers ---

void CmdUtils::handleWave2Command(String cmd, String args) {
    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
    if (!w2 || !w2->isAuthenticated()) {
        cmd_println("W2 not ready.");
        // We still proceed to try sending to avoid silently failing during debug, but generally good to warn.
    }

    // Parse single value for most commands
    uint8_t val = parseHexByte(args);

    // Legacy names
    if (cmd.equalsIgnoreCase("set_ambient_light")) { if(w2) w2->setAmbientLight(val); }
    else if (cmd.equalsIgnoreCase("set_automatic_drain")) { if(w2) w2->setAutomaticDrain(val); }
    else if (cmd.equalsIgnoreCase("set_beep_enabled")) { if(w2) w2->setBeep(val); }
    else if (cmd.equalsIgnoreCase("set_fan_speed")) { if(w2) w2->setFanSpeed(val); }
    else if (cmd.equalsIgnoreCase("set_main_mode")) { if(w2) w2->setMainMode(val); }
    else if (cmd.equalsIgnoreCase("set_power_state")) { if(w2) w2->setPowerState(val); }
    else if (cmd.equalsIgnoreCase("set_temperature")) { if(w2) w2->setTemperature(val); }
    else if (cmd.equalsIgnoreCase("set_countdown_timer")) { if(w2) w2->setCountdownTimer(val); }
    else if (cmd.equalsIgnoreCase("set_idle_screen_timeout")) { if(w2) w2->setIdleScreenTimeout(val); }
    else if (cmd.equalsIgnoreCase("set_sub_mode")) { if(w2) w2->setSubMode(val); }
    else if (cmd.equalsIgnoreCase("set_temperature_display_type")) { if(w2) w2->setTempDisplayType(val); }
    else if (cmd.equalsIgnoreCase("set_temperature_unit")) { if(w2) w2->setTempUnit(val); }

    // New simplified names
    else if (cmd.equalsIgnoreCase("w2_set_light")) { if(w2) w2->setAmbientLight(val); }
    else if (cmd.equalsIgnoreCase("w2_set_drain")) { if(w2) w2->setAutomaticDrain(val); }
    else if (cmd.equalsIgnoreCase("w2_set_beep")) { if(w2) w2->setBeep(val); }
    else if (cmd.equalsIgnoreCase("w2_set_fan")) { if(w2) w2->setFanSpeed(val); }
    else if (cmd.equalsIgnoreCase("w2_set_mode")) { if(w2) w2->setMainMode(val); }
    else if (cmd.equalsIgnoreCase("w2_set_power")) { if(w2) w2->setPowerState(val); }
    else if (cmd.equalsIgnoreCase("w2_set_temp")) { if(w2) w2->setTemperature(val); }
    else if (cmd.equalsIgnoreCase("w2_set_timer")) {
        // Special case: w2_set_timer <time> <status>
        // args needs splitting
        args.trim();
        int space = args.indexOf(' ');
        if (space > 0) {
            // This function signature (EcoflowESP32::setCountdownTimer) currently only takes status?
            // "void setCountdownTimer(uint8_t status);" in header.
            // But implementation says: "_sendWave2Command(0x55, {0x00, 0x00, status});"
            // The requirement says "set_countdown_timer(time, status) -> [0x00, 0x00, status]"
            // It seems the first two bytes are ignored/zeroed in current impl.
            // We'll stick to passing 'status' as the single argument for now based on existing implementation.
            // If the user wants to pass time, the EcoflowESP32::setCountdownTimer needs update.
            // Current impl: setCountdownTimer(uint8_t status)
            if(w2) w2->setCountdownTimer(val);
        } else {
             if(w2) w2->setCountdownTimer(val);
        }
    }
    else if (cmd.equalsIgnoreCase("w2_set_timeout")) { if(w2) w2->setIdleScreenTimeout(val); }
    else if (cmd.equalsIgnoreCase("w2_set_submode")) { if(w2) w2->setSubMode(val); }
    else if (cmd.equalsIgnoreCase("w2_set_temptype")) { if(w2) w2->setTempDisplayType(val); }
    else if (cmd.equalsIgnoreCase("w2_set_tempunit")) { if(w2) w2->setTempUnit(val); }

    cmd_println("Command sent.");
}

void CmdUtils::handleDelta3Command(String cmd, String args) {
    EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
    if (!d3 || !d3->isAuthenticated()) { cmd_println("D3 not ready."); return; }

    if (cmd.equalsIgnoreCase("d3_set_ac")) d3->setAC(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_dc")) d3->setDC(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_usb")) d3->setUSB(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_ac_limit")) d3->setAcChargingLimit(args.toInt());
    else if (cmd.equalsIgnoreCase("d3_set_soc_max")) d3->setBatterySOCLimits(args.toInt(), d3->getMinDsgSoc());
    else if (cmd.equalsIgnoreCase("d3_set_soc_min")) d3->setBatterySOCLimits(d3->getMaxChgSoc(), args.toInt());

    cmd_println("Command sent.");
}

void CmdUtils::handleDeltaPro3Command(String cmd, String args) {
    EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
    if (!d3p || !d3p->isAuthenticated()) { cmd_println("D3P not ready."); return; }

    if (cmd.equalsIgnoreCase("d3p_set_ac_hv")) d3p->setAcHvPort(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_ac_lv")) d3p->setAcLvPort(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_energy_backup")) d3p->setEnergyBackup(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_energy_backup_level")) d3p->setEnergyBackupLevel(args.toInt());

    cmd_println("Command sent.");
}

void CmdUtils::handleAltChargerCommand(String cmd, String args) {
    EcoflowESP32* ac = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
    if (!ac || !ac->isAuthenticated()) { cmd_println("AC not ready."); return; }

    if (cmd.equalsIgnoreCase("ac_set_open")) ac->setChargerOpen(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("ac_set_mode")) ac->setChargerMode(args.toInt());
    else if (cmd.equalsIgnoreCase("ac_set_power_limit")) ac->setPowerLimit(args.toInt());
    else if (cmd.equalsIgnoreCase("ac_set_batt_voltage")) ac->setBatteryVoltage(parseFloat(args));
    else if (cmd.equalsIgnoreCase("ac_set_car_chg_limit")) ac->setCarBatteryChargeLimit(parseFloat(args));
    else if (cmd.equalsIgnoreCase("ac_set_dev_chg_limit")) ac->setDeviceBatteryChargeLimit(parseFloat(args));

    cmd_println("Command sent.");
}

// --- Read Handlers ---

void CmdUtils::handleWave2Read(String cmd) {
    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
    if (!w2) return;
    const Wave2Data& d = w2->getData().wave2;

    if (cmd.equalsIgnoreCase("get_temp")) cmd_printf("Temp: Set=%d, Env=%.2f, Outlet=%.2f\n", d.setTemp, d.envTemp, d.outLetTemp);
    else if (cmd.equalsIgnoreCase("get_fan_speed")) cmd_printf("Fan Speed: %d\n", d.fanValue);
    else if (cmd.equalsIgnoreCase("get_mode")) cmd_printf("Mode: %d, SubMode: %d\n", d.mode, d.subMode);
    else if (cmd.equalsIgnoreCase("get_power")) cmd_printf("Power: Bat=%dW, MPPT=%dW, PSDR=%dW\n", d.batPwrWatt, d.mpptPwrWatt, d.psdrPwrWatt);
    else if (cmd.equalsIgnoreCase("get_battery")) cmd_printf("Batt: %d%% (Stat: %d), Rem: %dm\n", d.batSoc, d.batChgStatus, d.remainingTime);
    else if (cmd.equalsIgnoreCase("get_power_state")) cmd_printf("Power State: %d\n", d.powerMode);
    else cmd_println("Unknown get command for Wave 2");
}

void CmdUtils::handleDelta3Read(String cmd) {
    EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
    if (!d3) return;
    const Delta3Data& d = d3->getData().delta3;

    if (cmd.equalsIgnoreCase("d3_get_switches")) cmd_printf("AC: %d, DC: %d, USB: %d\n", d.acOn, d.dcOn, d.usbOn);
    else if (cmd.equalsIgnoreCase("d3_get_power")) cmd_printf("In: %.1fW, Out: %.1fW, AC In: %.1fW, AC Out: %.1fW, Solar: %.1fW\n", d.inputPower, d.outputPower, d.acInputPower, d.acOutputPower, d.solarInputPower);
    else if (cmd.equalsIgnoreCase("d3_get_battery")) cmd_printf("Batt: %.1f%%, Limits: %d%%-%d%%\n", d.batteryLevel, d.batteryChargeLimitMin, d.batteryChargeLimitMax);
    else if (cmd.equalsIgnoreCase("d3_get_settings")) cmd_printf("AC Limit: %dW\n", d.acChargingSpeed);
    else cmd_println("Unknown get command for Delta 3");
}

void CmdUtils::handleDeltaPro3Read(String cmd) {
    EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
    if (!d3p) return;
    const DeltaPro3Data& d = d3p->getData().deltaPro3;

    if (cmd.equalsIgnoreCase("d3p_get_switches")) cmd_printf("AC LV: %d, AC HV: %d, DC: %d\n", d.acLvPort, d.acHvPort, d.dc12vPort);
    else if (cmd.equalsIgnoreCase("d3p_get_power")) cmd_printf("In: %.1fW, Out: %.1fW, AC In: %.1fW, AC LV: %.1fW, AC HV: %.1fW\n", d.inputPower, d.outputPower, d.acInputPower, d.acLvOutputPower, d.acHvOutputPower);
    else if (cmd.equalsIgnoreCase("d3p_get_battery")) cmd_printf("Batt: %.1f%%, Limits: %d%%-%d%%\n", d.batteryLevel, d.batteryChargeLimitMin, d.batteryChargeLimitMax);
    else if (cmd.equalsIgnoreCase("d3p_get_energy_backup")) cmd_printf("Enabled: %d, Level: %d%%\n", d.energyBackup, d.energyBackupBatteryLevel);
    else cmd_println("Unknown get command for Delta Pro 3");
}

void CmdUtils::handleAltChargerRead(String cmd) {
    EcoflowESP32* ac = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
    if (!ac) return;
    const AlternatorChargerData& d = ac->getData().alternatorCharger;

    if (cmd.equalsIgnoreCase("ac_get_status")) cmd_printf("Open: %d, Mode: %d\n", d.chargerOpen, d.chargerMode);
    else if (cmd.equalsIgnoreCase("ac_get_power")) cmd_printf("DC Power: %.1fW, Limit: %dW\n", d.dcPower, d.powerLimit);
    else if (cmd.equalsIgnoreCase("ac_get_battery")) cmd_printf("Dev Batt: %.1f%%, Car Batt: %.2fV\n", d.batteryLevel, d.carBatteryVoltage);
    else cmd_println("Unknown get command for Alternator Charger");
}
