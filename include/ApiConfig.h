#ifndef API_CONFIG_H
#define API_CONFIG_H

#include <Arduino.h>
#include <HTTPClient.h>

class ApiConfig {
public:
    static const char* const BASE_URL;
    static const char* const AUTH_TOKEN;
    
    // API 端点
    static String getVoiceToVoiceUrl() { return String(BASE_URL) + "/v1/device/voice_to_voice"; }
    static String getSensorToVoiceUrl() { return String(BASE_URL) + "/v1/device/sensor_to_voice"; }
    
    // 授权方法
    static void addAuthHeader(HTTPClient& http) {
        http.setAuthorization(AUTH_TOKEN, "");
    }
};

#endif 