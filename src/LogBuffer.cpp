#include "LogBuffer.h"

// We need a separate static function for the hook
static vprintf_like_t old_vprintf = nullptr;

static int silent_vprintf(const char *fmt, va_list args) {
    return 0; // Do nothing
}

static int buffer_vprintf(const char *fmt, va_list args) {
    // Format the message
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (len > 0) {
        String logLine = String(buffer);

        // Basic parsing logic
        esp_log_level_t level = ESP_LOG_INFO;
        String tag = "SYS";
        String msg = logLine;

        char levelChar = logLine.charAt(0);
        if (levelChar == 'E') level = ESP_LOG_ERROR;
        else if (levelChar == 'W') level = ESP_LOG_WARN;
        else if (levelChar == 'I') level = ESP_LOG_INFO;
        else if (levelChar == 'D') level = ESP_LOG_DEBUG;
        else if (levelChar == 'V') level = ESP_LOG_VERBOSE;

        // Extract Tag: "L (time) Tag: Message"
        int closeParen = logLine.indexOf(')');
        int colon = logLine.indexOf(':', closeParen);
        if (closeParen > 0 && colon > closeParen) {
            tag = logLine.substring(closeParen + 2, colon);
            msg = logLine.substring(colon + 2);
        }

        LogBuffer::getInstance().addLog(level, tag.c_str(), logLine.c_str(), args);
    }

    // Forward to original handler (UART)
    if (old_vprintf) {
        return old_vprintf(fmt, args);
    } else {
        return vprintf(fmt, args);
    }
}

LogBuffer& LogBuffer::getInstance() {
    static LogBuffer instance;
    return instance;
}

LogBuffer::LogBuffer() {
    _mutex = xSemaphoreCreateMutex();
}

void LogBuffer::begin() {
    // Ensure we start in the correct state
    // If _enabled is false (default), ensure we are silent
    if (!_enabled) {
        setLoggingEnabled(false);
    }
}

void LogBuffer::setLoggingEnabled(bool enabled) {
    _enabled = enabled;
    if (_enabled) {
        // Enable Logging: Restore Global Level and Switch to Buffer+UART
        esp_log_level_set("*", ESP_LOG_INFO);

        // Restore NimBLE specific tags to INFO
        esp_log_level_set("NimBLE", ESP_LOG_INFO);
        esp_log_level_set("NimBLEScan", ESP_LOG_INFO);
        esp_log_level_set("NimBLEClient", ESP_LOG_INFO);
        esp_log_level_set("NimBLEAdvertisedDevice", ESP_LOG_INFO);

        vprintf_like_t current = esp_log_set_vprintf(buffer_vprintf);
        if (current != silent_vprintf && current != buffer_vprintf) {
             old_vprintf = current;
        }
    } else {
        // Disable Logging: Mute Global Level and Switch to Silent Hook
        esp_log_level_set("*", ESP_LOG_NONE);

        // Explicitly mute NimBLE components which might ignore global * sometimes or be verbose
        esp_log_level_set("NimBLE", ESP_LOG_NONE);
        esp_log_level_set("NimBLEScan", ESP_LOG_NONE);
        esp_log_level_set("NimBLEClient", ESP_LOG_NONE);
        esp_log_level_set("NimBLEAdvertisedDevice", ESP_LOG_NONE);

        vprintf_like_t current = esp_log_set_vprintf(silent_vprintf);
        if (current != silent_vprintf && current != buffer_vprintf) {
            old_vprintf = current;
        }
    }
}

bool LogBuffer::isLoggingEnabled() const {
    return _enabled;
}

void LogBuffer::setGlobalLevel(esp_log_level_t level) {
    esp_log_level_set("*", level);
}

void LogBuffer::setTagLevel(const String& tag, esp_log_level_t level) {
    // Exclusive Tag Mode: Mute everything else
    esp_log_level_set("*", ESP_LOG_NONE);

    // Also mute NimBLE explicitly if we are filtering for something else
    esp_log_level_set("NimBLE", ESP_LOG_NONE);
    esp_log_level_set("NimBLEScan", ESP_LOG_NONE);

    esp_log_level_set(tag.c_str(), level);
}

void LogBuffer::addLog(esp_log_level_t level, const char* tag, const char* message, va_list args) {
    if (!_enabled) return;

    xSemaphoreTake(_mutex, portMAX_DELAY);

    if (_logs.size() >= _maxLogs) {
        _logs.erase(_logs.begin()); // Remove oldest
    }

    LogMessage lm;
    lm.timestamp = millis();
    lm.level = level;
    lm.tag = String(tag);
    lm.message = String(message); // Already formatted
    if (lm.message.endsWith("\n")) lm.message.remove(lm.message.length()-1);

    _logs.push_back(lm);

    xSemaphoreGive(_mutex);
}

std::vector<LogMessage> LogBuffer::getLogs(size_t fromIndex) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    std::vector<LogMessage> result;
    // Check if fromIndex is valid
    // If the client asks for index 50, but we dropped logs and now start at "logical index" X?
    // The vector is a sliding window.
    // Simple approach: The client tracks count.
    // If we have 50 logs, and client asks for index 40, we return last 10?
    // No, indices are relative to the current buffer.
    // Better: Client sends "I have 0 logs". Server returns all 50. Client has 50.
    // Client sends "I have 50 logs". Server has 50 (same). Return empty?
    // But since we erase logs, the "count" isn't stable index.

    // Let's assume fromIndex is "number of logs client already has" IF the buffer was append-only.
    // But buffer rotates.
    // If we rotate, the client's index is invalid.
    // It's easier to just return *new* logs if we had a sequence ID.

    // Compromise: We will use the `fromIndex` as "Skip this many items from the BEGINNING of current buffer".
    // If fromIndex >= _logs.size(), return empty.

    if (fromIndex < _logs.size()) {
        // Return sub-vector
        result.insert(result.end(), _logs.begin() + fromIndex, _logs.end());
    } else if (fromIndex > _logs.size()) {
        // Client index is out of sync (e.g. buffer cleared or client restart)
        // In a more complex system we would return all logs or an error.
        // For now, returning empty is safe, client will naturally just wait or clear.
    }

    xSemaphoreGive(_mutex);
    return result;
}

size_t LogBuffer::getLogCount() const {
    return _logs.size();
}

void LogBuffer::clearLogs() {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _logs.clear();
    xSemaphoreGive(_mutex);
}
