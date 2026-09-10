#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"

class TelemetryLogger {
public:
    TelemetryLogger();

    bool begin();
    bool logSample(const SensorSample &sample);
    void flush();
    
    size_t getLogFileSize() const;
    void printDiagnostics() const;

private:
    bool _isInitialized;
    String _logFilePath;

    void ensureHeaderExists();
};

#endif // LOGGER_H
