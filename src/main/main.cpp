#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include <WiFiClientSecure.h>
#include "HTTPClient.h"
#include "AudioRecord.h"
#include "AudioPlay.h"
#include <ArduinoJson.h>
#include <queue>
#include "sensors/ForceSensor.h"
#include "ApiConfig.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <HTTPUpdate.h>

#define key 0
#define ADC 32
#define led3 2
#define led2 18
#define led1 19
const char *wifiData[][2] = {
    {"aaaaa", "88888888"}, // 替换为自己常用的wifi名和密码
    // {"222", "12345678"},
    // 继续添加需要的 Wi-Fi 名称和密码
};
#define SILENCE_THRESHOLD 12  // 静音检测阈值
const int FRAMES_TO_SEND = 6;

// 在全局变量区域添加
enum RecordState {
    IDLE,
    CALIBRATING,
    RECORDING,
    SENDING
};

RecordState recordState = IDLE;
#define FRAME_SIZE 1280
#define TOTAL_BUFFER_SIZE (FRAME_SIZE * FRAMES_TO_SEND)

// 定义静态音频缓冲区
static uint8_t staticAudioBuffer[TOTAL_BUFFER_SIZE] = {0};

// 修改原来的指针定义
// uint8_t* audioBuffer = nullptr;  // 删除这行
uint8_t* audioBuffer = staticAudioBuffer;  // 改用这行

size_t bufferOffset = 0;
int frameCount = 0;
float noiseLevel = 0;
int silence = 0;
int voice = 0;
int voicebegin = 0;
unsigned long lastProcessTime = 0;

bool ledstatus = true;
bool startPlay = false;
bool lastsetence = false;
bool isReady = false;
unsigned long urlTime = 0;
unsigned long pushTime = 0;
int mainStatus = 0;
int receiveFrame = 0;
int adc_start_flag = 0;
HTTPClient https;
std::queue<String> audioQueue;
AudioRecord audioRecord;
AudioPlay audioPlay(false, 3, I2S_NUM_1);

#define I2S_DOUT 25 // DIN
#define I2S_BCLK 27 // BCLK
#define I2S_LRC 26  // LRC
bool sendAudioData(uint8_t *audioBuffer, size_t bufferSize, bool isLastFrame);
void processAudioState();
void playNextAudio();
void performUpdate(const char* firmwareUrl);

// 新增处理HTTP响应的任务函数
bool processHttpResponse(const String& response) {
    int startPos = 0;
    int endPos = 0;
    bool hasAddedAudio = false;

    while ((endPos = response.indexOf("}", startPos)) != -1) {
        String jsonPart = response.substring(startPos, endPos + 1);
        Serial.print(jsonPart);

        DynamicJsonDocument responseDoc(1024);
        DeserializationError error = deserializeJson(responseDoc, jsonPart);
        
        if (!error) {
            if (responseDoc.containsKey("voice_path") && 
                responseDoc.containsKey("url") && 
                responseDoc.containsKey("port")) {
                
                String voicePath = responseDoc["voice_path"].as<String>();
                String baseUrl = responseDoc["url"].as<String>();
                int port = responseDoc["port"].as<int>();
                
                String token = "ht-4b8536b544e0784309820c3d11649a14";
                voicePath = voicePath.substring(0, voicePath.length() - 4) + ".mp3";

                String audioUrl = baseUrl + ":" + String(port) + 
                                "/flashsummary/retrieveFileData?stream=True" + 
                                "&token=" + token + 
                                "&voice_audio_path=" + voicePath;
                
                audioQueue.push(audioUrl);
                hasAddedAudio = true;
            }
        }
        startPos = endPos + 1;
    }

    if (hasAddedAudio && audioPlay.isplaying == 0) {
        playNextAudio();
    }

    return hasAddedAudio;
}

void gain_token(void);
void getText(String role, String content);
void checkLen(JsonArray textArray);
int getLength(JsonArray textArray);
float calculateRMS(uint8_t *buffer, int bufferSize);
void ConnServer();
void ConnServer1();

DynamicJsonDocument doc(2048);
JsonArray text = doc.to<JsonArray>();

String url = "";
String url1 = "";
String Date = "";
DynamicJsonDocument gen_params(const char *appid, const char *domain);

int loopcount = 0;
void wifiConnect(const char *wifiData[][2], int numNetworks)
{
    WiFi.disconnect(true);
    for (int i = 0; i < numNetworks; ++i)
    {
        const char *ssid = wifiData[i][0];
        const char *password = wifiData[i][1];

        Serial.print("Connecting to ");
        Serial.println(ssid);

        WiFi.begin(ssid, password);
        uint8_t count = 0;
        while (WiFi.status() != WL_CONNECTED)
        {
            Serial.print(".");
            count++;
            if (count >= 30)
            {
                Serial.printf("\r\n-- wifi connect fail! --");
                break;
            }
            vTaskDelay(100);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.printf("\r\n-- wifi connect success! --\r\n");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
            return; // 如果连接成功，退出函数
        }
    }
}
// 添加WAV文件头结构
struct WAVHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1;      // PCM格式
    uint16_t numChannels = 1;      // 单声道
    uint32_t sampleRate = 4000;    // 修改采样率为8kHz
    uint32_t byteRate = 8000;     // 修改字节率 = sampleRate * numChannels * (bitsPerSample/8)
    uint16_t blockAlign = 2;       // 数据块对齐 = numChannels * (bitsPerSample/8)
    uint16_t bitsPerSample = 16;   // 16位深度
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size;
};
// 新增函播放下一个音频
void playNextAudio() {
    if (!audioQueue.empty()) {
        String audioUrl = audioQueue.front();
        audioQueue.pop();
        Serial.println("开始播放音频: " + audioUrl);
        audioPlay.connecttohost(audioUrl.c_str());
    }
    else
    {
        // Serial.println("音频队列已空");
    }
}
// 修改sendAudioData函数
bool sendAudioData(uint8_t *audioBuffer, size_t bufferSize, bool isLastFrame)
{
    HTTPClient http;
    String url = ApiConfig::getVoiceToVoiceUrl();
    Serial.println("API URL: " + url);
    http.begin(url);
    ApiConfig::addAuthHeader(http);
    
    if (audioBuffer != nullptr && bufferSize > 0) {
        // 创建WAV头和组合缓冲区
        WAVHeader wavHeader;
        size_t totalSize = sizeof(WAVHeader) + bufferSize;
        uint8_t* combinedBuffer = (uint8_t*)malloc(totalSize);
        
        if (!combinedBuffer) {
            Serial.println("内存分配失败");
            return false;
        }
        
        // 设置WAV头参数
        wavHeader.chunkSize = 36 + bufferSize;
        wavHeader.subchunk2Size = bufferSize;
        
        // 复制WAV头和音频数据到组合缓冲区
        memcpy(combinedBuffer, &wavHeader, sizeof(WAVHeader));
        memcpy(combinedBuffer + sizeof(WAVHeader), audioBuffer, bufferSize);
        
        // 创建一个预估大小的JSON文档
        const size_t estimated_base64_size = totalSize * 4 / 3 + 4;
        const size_t json_doc_size = estimated_base64_size + 256;
        Serial.println(String("base64预估大小: ") + String(estimated_base64_size));
        DynamicJsonDocument doc(json_doc_size);
        doc["role_id"] = 1;
        doc["status"] = isLastFrame ? 2 : 1;  // 根据isLastFrame设置状态
        Serial.printf("发送帧状态: %d\n", isLastFrame ? 2 : 1);
        // 对包WAV头的完整音频数据进行base64编码
        String base64Audio = base64::encode(combinedBuffer, totalSize);
        free(combinedBuffer); // 释放组合缓冲区
        
        if (base64Audio.length() > 0) {
            Serial.printf("音频数据编码完成: 原始大小=%d bytes, base64大小=%d bytes\n", 
                totalSize,
                base64Audio.length());
        } else {
            Serial.println("警告: base64编码失败");
        }
        
        // 添加到JSON文档
        doc["chunk"] = base64Audio;

        // 序列化JSON
        String jsonBody;
        serializeJson(doc, jsonBody);
        // 发送请求
        int httpResponseCode = http.POST(jsonBody);
        Serial.printf("HTTP响应码: %d\n", httpResponseCode);
        
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.printf("HTTP响应: %s\n", response.c_str());

            processHttpResponse(response);
            http.end();
            return true;
        }
        
        Serial.printf("HTTP错误: %d\n", httpResponseCode);
        http.end();
        return false;
    }

    http.end();
    return false;
}
// 修改 recordAndSendAudio 函数
void recordAndSendAudio() {
    recordState = CALIBRATING;
    Serial.println("开始录音校准");
}

ForceSensor forceSensor;

// 添加全局变量
AsyncWebServer server(80);  // 创建Web服务器实例,端口80
Preferences preferences;    // 创建Preferences实例用于存储配置

// 添加配网相关的处理函数
void handleSetConfig(AsyncWebServerRequest *request) {
    String wifi_name = request->getParam("wifi_name")->value();
    String wifi_pwd = request->getParam("wifi_pwd")->value();
    
    // 保存WiFi信息到preferences
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", wifi_name);
    preferences.putString("password", wifi_pwd);
    preferences.end();

    // 返回成功响应
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"success\":true}");
    request->send(response);

    // 延迟重启ESP32
    delay(1000);
    ESP.restart();
}

// 添加WiFi扫描处理函数
void handleWifiScan(AsyncWebServerRequest *request) {
    int n = WiFi.scanComplete();
    if(n == -2){
        WiFi.scanNetworks(true); // 异步扫描
        request->send(200, "application/json", "{\"success\":true,\"data\":[]}");
        return;
    }
    
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();
    
    for (int i = 0; i < n; ++i) {
        JsonObject wifi = array.createNestedObject();
        wifi["ssid"] = WiFi.SSID(i);
        wifi["rssi"] = WiFi.RSSI(i);
        wifi["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", "{\"success\":true,\"data\":" + response + "}");
    WiFi.scanDelete();
    
    // 开始新的扫描
    WiFi.scanNetworks(true);
}

void setupWebServer() {
    // 初始化SPIFFS
    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS挂载失败");
        return;
    }

    // 设置路由
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });

    server.on("/set_config", HTTP_GET, handleSetConfig);
    
    server.on("/scan_wifi", HTTP_GET, handleWifiScan);
    
    // 启动服务器
    server.begin();
    Serial.println("HTTP服务器启动");
}

const char* FIRMWARE_VERSION = "1.0.0";  // 当前固件版本
const char* OTA_UPDATE_URL = "http://your-server.com/firmware.bin";  // 替换为您的固件服务器地址

// 修改检查更新的函数
void checkForUpdates() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi未连接，跳过固件检查");
        return;
    }

    Serial.println("检查固件更新...");
    HTTPClient http;
    http.begin("http://your-server.com/version.json");  // 替换为您的版本检查接口
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            const char* latestVersion = doc["version"];
            const char* firmwareUrl = doc["url"];
            
            Serial.printf("当前版本: %s, 最新版本: %s\n", FIRMWARE_VERSION, latestVersion);
            
            if (String(latestVersion) != String(FIRMWARE_VERSION)) {
                Serial.println("发现新版本固件，开始更新...");
                performUpdate(firmwareUrl);
            } else {
                Serial.println("当前已是最新版本");
            }
        }
    } else {
        Serial.printf("检查更新失败，HTTP错误码: %d\n", httpCode);
    }
    http.end();
}

// 添加执行更新的函数
void performUpdate(const char* firmwareUrl) {
    Serial.println("开始固件更新...");
    
    WiFiClientSecure client;
    client.setInsecure();  // 如果使用HTTPS但不需要验证证书
    
    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);
    
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", 
                httpUpdate.getLastError(),
                httpUpdate.getLastErrorString().c_str());
            break;
            
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;
            
        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            // 更新成功后会自动重启
            break;
    }
}

void setup() {
    // 尝试从preferences读取WiFi配置
    preferences.begin("wifi-config", true);
    String saved_ssid = preferences.getString("ssid", "");
    String saved_password = preferences.getString("password", "");
    preferences.end();

    // if (saved_ssid.length() > 0) {
    //     // 如果有保存的WiFi信息,尝试连接
    //     WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
        
    //     int retry_count = 0;
    //     while (WiFi.status() != WL_CONNECTED && retry_count < 30) {
    //         delay(500);
    //         Serial.print(".");
    //         retry_count++;
    //     }
    // }

    // if (WiFi.status() != WL_CONNECTED) {
    //     // 如果无法连接已保存的WiFi,启动配网模式
    //     WiFi.mode(WIFI_AP);
    //     WiFi.softAP("ESP32_Config", "12345678"); // 创建AP热点
        
    //     Serial.println("启动配网模式");
    //     Serial.print("AP IP地址: ");
    //     Serial.println(WiFi.softAPIP());
        
    //     setupWebServer();
    // } else {
    //     Serial.println("WiFi连接成功");
    //     Serial.print("IP地址: ");
    //     Serial.println(WiFi.localIP());
        
    //     // WiFi连接成功后立即检查更新
    //     checkForUpdates();
    // }

    // String Date = "Fri, 22 Mar 2024 03:35:56 GMT";
    Serial.begin(115200);
    // pinMode(ADC,ANALOG);
    pinMode(17, INPUT);
    pinMode(key, INPUT_PULLUP);
    pinMode(34, INPUT_PULLUP);
    pinMode(35, INPUT_PULLUP);
    audioRecord.init();

    int numNetworks = sizeof(wifiData) / sizeof(wifiData[0]);
    wifiConnect(wifiData, numNetworks);

    // 添加I2S初始化检查
    if (!audioPlay.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT)) {
        Serial.println("I2S引脚配置失败");
        // 可以在这里添加重试逻辑
    }
    
    // 设置较小的音量以减少内存使用
    audioPlay.setVolume(15);  // 从20降到15

    forceSensor.begin();

    // 添加OTA配置
    httpUpdate.setLedPin(LED_BUILTIN, LOW);
    
    // 可选：设置证书（如果需要HTTPS验证）
    // httpUpdate.setCACert(rootCACertificate);
}

void loop()
{
    audioPlay.loop();
    
    if (audioPlay.isplaying == 1) {
        // 删除 digitalWrite(led3, HIGH);
    } else {
        // 删除 digitalWrite(led3, LOW);
        playNextAudio();
    }
    
    if (digitalRead(key) == 0) {
        audioPlay.isplaying = 0;
        startPlay = false;
        isReady = false;
        adc_start_flag = 1;
        Serial.printf("开始识别\r\n\r\n");
        recordAndSendAudio();
    }
    processAudioState();  // 处理录音状态机

    forceSensor.update();
}

float calculateRMS(uint8_t *buffer, int bufferSize)
{
    float sum = 0;
    int16_t sample;

    for (int i = 0; i < bufferSize; i += 2)
    {

        sample = (buffer[i + 1] << 8) | buffer[i];
        sum += sample * sample;
    }

    sum /= (bufferSize / 2);

    return sqrt(sum);
}

// 在 loop 函数中添加状态机处理
void processAudioState() {
    
    switch(recordState) {
        case CALIBRATING: {
            static int calibrationCount = 0;
            static float totalNoise = 0;
            
            if (millis() - lastProcessTime >= 40) {  // 每40ms处理一次
                lastProcessTime = millis();
                
                audioRecord.Record();
                float rms = calculateRMS((uint8_t *)audioRecord.wavData[0], FRAME_SIZE);
                totalNoise += rms;
                calibrationCount++;
                
                if (calibrationCount >= 10) {
                    noiseLevel = 65;
                    Serial.printf("噪音基准: %f\n", noiseLevel);
                    
                    // 使用静态缓冲区替代动态分配
                    audioBuffer = staticAudioBuffer;
                    recordState = RECORDING;
                    bufferOffset = 0;
                    frameCount = 0;
                    silence = 0;
                    voice = 0;
                    voicebegin = 0;
                    
                    calibrationCount = 0;
                    totalNoise = 0;
                }
            }
            break;
        }
        
        case RECORDING: {
            if (millis() - lastProcessTime >= 40) {  // 每40ms处理一次
                lastProcessTime = millis();
                Serial.println("录音中");
                audioRecord.Record();
                float rms = calculateRMS((uint8_t *)audioRecord.wavData[0], FRAME_SIZE);
                
                if (rms > noiseLevel) {
                    voice++;
                    if (voice >= 2) {
                        voicebegin = 1;
                        Serial.println("开始录音");
                        silence = 0;
                    }
                } else {
                    if (voicebegin) {
                        silence++;
                        Serial.println("静音");
                    }
                }
                
                if (voicebegin == 1) {
                    memcpy(audioBuffer + bufferOffset, audioRecord.wavData[0], FRAME_SIZE);
                    bufferOffset += FRAME_SIZE;
                    frameCount++;
                    
                    if (frameCount >= FRAMES_TO_SEND) {
                        recordState = SENDING;
                    }
                }
            }
            break;
        }
        
        case SENDING: {
            if (bufferOffset > 0) {
                bool isLastFrame = silence >= 20 ;
                if (sendAudioData(audioBuffer, bufferOffset, isLastFrame)) {
                    
                    if (isLastFrame) {
                        Serial.println("音频数已发送并加入队列，停止录音");
                        bufferOffset = 0;
                        frameCount = 0;
                        recordState = IDLE;
                    } else {
                        bufferOffset = 0;
                        frameCount = 0;
                        recordState = RECORDING;
                    }
                }
            } else {
                recordState = IDLE;
            }
            break;
        }
        
        default:
            break;
    }
}
