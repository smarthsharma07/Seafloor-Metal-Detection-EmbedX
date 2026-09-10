#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"

class WiFiRecoveryManager {
public:
    WiFiRecoveryManager();

    bool begin(const char *ssid = "MoES_Sensor_Recovery", const char *password = "Seafloor2026");
    void handleClient();
    void stop();

    bool isRunning() const { return _isRunning; }
    void printDiagnostics() const;

private:
    WebServer _server;
    bool _isRunning;

    void setupRoutes();
    void handleRoot();
    void handleStatus();
    void handleConfig();
    void handleCalibration();
    void handleFileList();
    void handleDownload();
    void handleNotFound();
};

#endif // WIFI_MANAGER_H
