#include "ForceSensor.h"
#include "ApiConfig.h"

ForceSensor::ForceSensor() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        lastForceLevel[i] = "";
        lastUpdateTime[i] = 0;
    }
}

void ForceSensor::begin() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        pinMode(SENSOR_PINS[i], INPUT);
    }
}

float ForceSensor::readVoltage(uint8_t pin) {
    int rawValue = analogRead(pin);
    return (rawValue * 3.3) / 4095.0;  // ESP32的ADC是12位的
}

String ForceSensor::getForceLevel(float voltage) {
    if (voltage < LIGHT_THRESHOLD) {
        return "轻";
    } else if (voltage < MEDIUM_THRESHOLD) {
        return "中";
    } else {
        return "重";
    }
}

void ForceSensor::sendForceUpdate(const char* part, const String& force) {
    HTTPClient http;
    http.begin(ApiConfig::getSensorToVoiceUrl());
    http.addHeader("Content-Type", "application/json");
    
    // 添加授权头
    ApiConfig::addAuthHeader(http);
    
    // 准备JSON数据
    DynamicJsonDocument doc(200);
    doc["role_id"] = 1;
    doc["part"] = part;
    doc["force"] = force;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.printf("HTTP Response: %s\n", response.c_str());
    } else {
        Serial.printf("HTTP Error: %d\n", httpResponseCode);
    }
    
    http.end();
}

void ForceSensor::update() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (currentTime - lastUpdateTime[i] >= UPDATE_INTERVAL) {
            float voltage = readVoltage(SENSOR_PINS[i]);
            String currentForceLevel = getForceLevel(voltage);
            
            // 如果力度等级发生变化，发送更新
            if (lastForceLevel[i] != currentForceLevel && voltage > 0.1) {  // 添加最小阈值避免噪声
                Serial.printf("传感器 %s: 电压 %.2fV, 力度 %s\n", 
                            BODY_PARTS[i], voltage, currentForceLevel.c_str());
                sendForceUpdate(BODY_PARTS[i], currentForceLevel);
                lastForceLevel[i] = currentForceLevel;
            }
            
            lastUpdateTime[i] = currentTime;
        }
    }
} 