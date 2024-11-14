#ifndef FORCE_SENSOR_H
#define FORCE_SENSOR_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class ForceSensor {
public:
    ForceSensor();
    void begin();
    void update();

private:
    static const int NUM_SENSORS = 4;
    const uint8_t SENSOR_PINS[NUM_SENSORS] = {22, 23, 24, 25};  // 腿，背，胸，私处
    const char* BODY_PARTS[NUM_SENSORS] = {"腿部", "背部", "胸部", "私处"};
    
    // 力度阈值（根据实际情况调整）
    const float LIGHT_THRESHOLD = 1.1;   // 轻度阈值
    const float MEDIUM_THRESHOLD = 2.2;   // 中度阈值
    
    String lastForceLevel[NUM_SENSORS];
    unsigned long lastUpdateTime[NUM_SENSORS];
    static const unsigned long UPDATE_INTERVAL = 500; // 500ms更新间隔
    
    float readVoltage(uint8_t pin);
    String getForceLevel(float voltage);
    void sendForceUpdate(const char* part, const String& force);
};

#endif 