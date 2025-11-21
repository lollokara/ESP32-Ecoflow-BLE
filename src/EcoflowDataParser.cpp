#include "EcoflowDataParser.h"
#include "pb_utils.h"
#include "pd335_sys.pb.h"
#include <Arduino.h>
#include "esp_log.h"
#include <cmath>
#include <algorithm>

static const char* TAG = "EcoflowDataParser";

// --- Helper Functions ---

bool pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    // We don't need to actually decode strings for this application, just skip
    return pb_read(stream, NULL, stream->bytes_left);
}

bool pb_decode_to_vector(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<uint8_t> *vec = (std::vector<uint8_t> *)*arg;
    vec->resize(stream->bytes_left);
    return pb_read(stream, vec->data(), stream->bytes_left);
}

static uint16_t get_uint16_le(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static uint32_t get_uint32_le(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static int16_t swap_endian_and_parse_signed_int(const uint8_t* data) {
    // Little Endian Signed Short
    int16_t val = (int16_t)(data[0] | (data[1] << 8));
    return val;
}

static float get_float_le_safe(const uint8_t* data) {
    uint32_t i = get_uint32_le(data);
    float f;
    memcpy(&f, &i, sizeof(f));
    if (std::isnan(f) || std::isinf(f)) return 0.0f;
    return f;
}

static bool is_flow_on(uint32_t x) {
    return (x & 0b11) >= 0b10;
}

static void logDelta3Data(const Delta3Data& d) {
    ESP_LOGI(TAG, "=== Delta 3 Data ===");
    ESP_LOGI(TAG, "Batt: %.1f%% (In: %.1fW, Out: %.1fW)", d.batteryLevel, d.batteryInputPower, d.batteryOutputPower);
    ESP_LOGI(TAG, "AC In: %.1fW, AC Out: %.1fW", d.acInputPower, d.acOutputPower);
    ESP_LOGI(TAG, "DC In: %.1fW (State: %d, Solar: %.1fW)", d.dcPortInputPower, d.dcPortState, d.solarInputPower);
    ESP_LOGI(TAG, "Total In: %.1fW, Out: %.1fW", d.inputPower, d.outputPower);
    ESP_LOGI(TAG, "12V Out: %.1fW, USB-C: %.1fW/%.1fW, USB-A: %.1fW/%.1fW",
             d.dc12vOutputPower, d.usbcOutputPower, d.usbc2OutputPower, d.usbaOutputPower, d.usba2OutputPower);
    ESP_LOGI(TAG, "SOC Limits: %d%% - %d%%, AC Chg Speed: %dW", d.batteryChargeLimitMin, d.batteryChargeLimitMax, d.acChargingSpeed);
    ESP_LOGI(TAG, "Flags: AC Plugged=%d, Backup=%d, AC Ports=%d, 12V Port=%d", d.pluggedInAc, d.energyBackup, d.acPorts, d.dc12vPort);
}

static void logWave2Data(const Wave2Data& w) {
    ESP_LOGI(TAG, "=== Wave 2 Data ===");
    ESP_LOGI(TAG, "Mode: %d (Sub: %d), PwrMode: %d", w.mode, w.subMode, w.powerMode);
    ESP_LOGI(TAG, "Temps: Env=%.2f, Out=%.2f, Set=%d", w.envTemp, w.outLetTemp, w.setTemp);
    ESP_LOGI(TAG, "Batt: %d%% (Stat: %d), Rem: %dm/%dm", w.batSoc, w.batChgStatus, w.batChgRemainTime, w.batDsgRemainTime);
    ESP_LOGI(TAG, "Power: Bat=%dW, MPPT=%dW, PSDR=%dW", w.batPwrWatt, w.mpptPwrWatt, w.psdrPwrWatt);
    ESP_LOGI(TAG, "Fan: %d, Water: %d, RGB: %d", w.fanValue, w.waterValue, w.rgbState);
    ESP_LOGI(TAG, "Err: %u, BMS Err: %d", w.errCode, w.bmsErr);
}

namespace EcoflowDataParser {

void parsePacket(const Packet& pkt, EcoflowData& data) {

    // V3 Protobuf Packet (Delta 2 / D3)
    if (pkt.getSrc() == 0x02 && pkt.getCmdSet() == 0xFE && (pkt.getCmdId() == 0x11 || pkt.getCmdId() == 0x15)) {
        pd335_sys_DisplayPropertyUpload display_msg = pd335_sys_DisplayPropertyUpload_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());

        if (pb_decode(&stream, pd335_sys_DisplayPropertyUpload_fields, &display_msg)) {
            Delta3Data& d3 = data.delta3;

            if (display_msg.has_cms_batt_soc) d3.batteryLevel = display_msg.cms_batt_soc;
            else if (display_msg.has_bms_batt_soc) d3.batteryLevel = display_msg.bms_batt_soc;

            if (display_msg.has_pow_get_ac_in) d3.acInputPower = display_msg.pow_get_ac_in;
            if (display_msg.has_pow_get_ac_out) d3.acOutputPower = -std::abs(display_msg.pow_get_ac_out); // -round(x, 2)

            if (display_msg.has_pow_in_sum_w) d3.inputPower = display_msg.pow_in_sum_w;
            if (display_msg.has_pow_out_sum_w) d3.outputPower = display_msg.pow_out_sum_w;

            if (display_msg.has_pow_get_12v) d3.dc12vOutputPower = -std::abs(display_msg.pow_get_12v);
            if (display_msg.has_pow_get_pv) d3.dcPortInputPower = display_msg.pow_get_pv;

            if (display_msg.has_plug_in_info_pv_type) d3.dcPortState = (int)display_msg.plug_in_info_pv_type;

            if (display_msg.has_pow_get_typec1) d3.usbcOutputPower = -std::abs(display_msg.pow_get_typec1);
            if (display_msg.has_pow_get_typec2) d3.usbc2OutputPower = -std::abs(display_msg.pow_get_typec2);
            if (display_msg.has_pow_get_qcusb1) d3.usbaOutputPower = -std::abs(display_msg.pow_get_qcusb1);
            if (display_msg.has_pow_get_qcusb2) d3.usba2OutputPower = -std::abs(display_msg.pow_get_qcusb2);

            if (display_msg.has_plug_in_info_ac_charger_flag) d3.pluggedInAc = display_msg.plug_in_info_ac_charger_flag;

            if (display_msg.has_energy_backup_en) d3.energyBackup = display_msg.energy_backup_en;
            if (display_msg.has_energy_backup_start_soc) d3.energyBackupBatteryLevel = display_msg.energy_backup_start_soc;

            if (display_msg.has_pow_get_bms) {
                float bms_pow = display_msg.pow_get_bms;
                d3.batteryInputPower = (bms_pow > 0) ? bms_pow : 0;
                d3.batteryOutputPower = (bms_pow < 0) ? -bms_pow : 0;
            }

            if (display_msg.has_cms_min_dsg_soc) d3.batteryChargeLimitMin = display_msg.cms_min_dsg_soc;
            if (display_msg.has_cms_max_chg_soc) d3.batteryChargeLimitMax = display_msg.cms_max_chg_soc;

            if (display_msg.has_bms_max_cell_temp) d3.cellTemperature = display_msg.bms_max_cell_temp;

            if (display_msg.has_flow_info_12v) d3.dc12vPort = is_flow_on(display_msg.flow_info_12v);
            if (display_msg.has_flow_info_ac_out) d3.acPorts = is_flow_on(display_msg.flow_info_ac_out);

            if (display_msg.has_plug_in_info_ac_in_chg_pow_max) d3.acChargingSpeed = display_msg.plug_in_info_ac_in_chg_pow_max;

            if (d3.dcPortState == 2 && d3.dcPortInputPower > 0) { // 2 = SOLAR
                 d3.solarInputPower = d3.dcPortInputPower;
            } else {
                d3.solarInputPower = 0;
            }

            d3.acOn = d3.acPorts;
            d3.dcOn = d3.dc12vPort;
            if (display_msg.has_flow_info_qcusb1) d3.usbOn = is_flow_on(display_msg.flow_info_qcusb1);

            logDelta3Data(d3);

        } else {
             // RuntimePropertyUpload parsing can be added here if strictly needed, but user asked to focus on Delta 3 Classic list
        }
    }

    // V2 Binary Packet (Wave 2 / KT210)
    else if (pkt.getCmdSet() == 0x42 && pkt.getCmdId() == 0x50) {
        const std::vector<uint8_t>& payload = pkt.getPayload();

        if (payload.size() >= 108) {
            const uint8_t* p = payload.data();
            Wave2Data& w2 = data.wave2;

            w2.mode = p[0];
            w2.subMode = p[1];
            w2.setTemp = p[2];
            w2.fanValue = p[3];
            w2.envTemp = get_float_le_safe(p + 4);
            w2.tempSys = p[8];
            w2.displayIdleTime = get_uint16_le(p + 9);
            w2.displayIdleMode = p[11];
            w2.timeEn = p[12];
            w2.timeSetVal = get_uint16_le(p + 13);
            w2.timeRemainVal = get_uint16_le(p + 15);
            w2.beepEnable = p[17];
            w2.errCode = get_uint32_le(p + 18);

            w2.refEn = p[54];
            w2.bmsPid = get_uint16_le(p + 55);
            w2.wteFthEn = p[57];
            w2.tempDisplay = p[58];
            w2.powerMode = p[59];
            w2.powerSrc = p[60];
            w2.psdrPwrWatt = swap_endian_and_parse_signed_int(p + 61);
            w2.batPwrWatt = swap_endian_and_parse_signed_int(p + 63);
            w2.mpptPwrWatt = swap_endian_and_parse_signed_int(p + 65);
            w2.batDsgRemainTime = get_uint32_le(p + 67);
            w2.batChgRemainTime = get_uint32_le(p + 71);
            w2.batSoc = p[75];
            w2.batChgStatus = p[76];
            w2.outLetTemp = get_float_le_safe(p + 77);
            w2.mpptWork = p[81];
            w2.bmsErr = p[82];
            w2.rgbState = p[83];
            w2.waterValue = p[84];
            w2.bmsBoundFlag = p[85];
            w2.bmsUndervoltage = p[86];
            w2.ver = p[87];

            if (w2.batChgRemainTime > 0 && w2.batChgRemainTime < 6000) w2.remainingTime = w2.batChgRemainTime;
            else if (w2.batDsgRemainTime > 0 && w2.batDsgRemainTime < 6000) w2.remainingTime = w2.batDsgRemainTime;
            else w2.remainingTime = 0;

            logWave2Data(w2);
        }
    }
}

} // namespace EcoflowDataParser
