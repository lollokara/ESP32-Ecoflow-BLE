#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <Arduino.h>
#include <vector>
#include <string>
#include <mutex>
#include "esp_log.h"

struct LogMessage {
    uint32_t timestamp;
    esp_log_level_t level;
    String tag;
    String message;
};

class LogBuffer {
public:
    static LogBuffer& getInstance();

    void begin();
    void setLoggingEnabled(bool enabled);
    bool isLoggingEnabled() const;

    void setGlobalLevel(esp_log_level_t level);
    void setTagLevel(const String& tag, esp_log_level_t level);

    std::vector<LogMessage> getLogs(size_t fromIndex = 0);
    size_t getLogCount() const;
    void clearLogs();

    // Internal use for the vprintf hook
    void addLog(esp_log_level_t level, const char* tag, const char* format, va_list args);

private:
    LogBuffer();
    LogBuffer(const LogBuffer&) = delete;
    LogBuffer& operator=(const LogBuffer&) = delete;

    bool _enabled = false;
    std::vector<LogMessage> _logs;
    const size_t _maxLogs = 50; // Limit buffer size to save RAM
    mutable SemaphoreHandle_t _mutex;
};

#endif // LOG_BUFFER_H
