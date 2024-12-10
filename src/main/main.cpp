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
#include "time.h"
#include <asyncHTTPrequest.h>

#define key 12
#define ADC 32
#define led3 2
#define led2 18
#define led1 19
const char *wifiData[][2] = {
    {"aaaaa", "88888888"}, // 替换为自己常用的wifi名和密码
    // {"222", "12345678"},
    // 继续添加需要的 Wi-Fi 名称和密码
};
const int FRAMES_TO_SEND =2;

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

#define I2S_DOUT 27 // DIN
#define I2S_BCLK 26 // BCLK
#define I2S_LRC 25  // LRC
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
        Serial.print("处理JSON片段: ");
        Serial.println(jsonPart);

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
                Serial.printf("添加音频URL到队列，当前队列大小: %d\n", audioQueue.size());
                
                // 如果当前没有在播放，立即开始播放
                if (audioPlay.isplaying == 0) {
                    Serial.println("立即开始播放新音频");
                    playNextAudio();
                }
            }
        }
        startPos = endPos + 1;
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
// 修改WAVHeader结构体定义，确保内存对齐
#pragma pack(push, 1)  // 添加1字节对齐
struct WAVHeader {
    // 成员变量保持不变，但使用数组初始化语法
    uint8_t riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;           // 后面再设置具体大小
    uint8_t wave[4] = {'W', 'A', 'V', 'E'};
    uint8_t fmt[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = 4000;
    uint32_t byteRate = 8000;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    uint8_t data[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size;       // 后面再设置具体大小
};
#pragma pack(pop)  // 恢复默认对齐

// 新增函播放下一个音频
void playNextAudio() {
    if (!audioQueue.empty()) {
        String audioUrl = audioQueue.front();
        audioQueue.pop();
        
        Serial.println("\n=== 开始新音频播放流程 ===");
        Serial.println("队列剩余数量: " + String(audioQueue.size()));
        
        if (audioPlay.connecttohost(audioUrl.c_str())) {
            Serial.println("成功连接到新音频");
        } else {
            Serial.println("音频连接失败，尝试下一个");
            // 如果连接失败，立即尝试播放队列中的下一个
            if (!audioQueue.empty()) {
                playNextAudio();
            }
        }
    }
}
// 修改sendAudioData函数
bool sendAudioData(uint8_t *audioBuffer, size_t bufferSize, bool isLastFrame)
{
    digitalWrite(led2, HIGH);  // 发送开始，LED2亮起
    String url = ApiConfig::getVoiceToVoiceUrl();
    String ip = WiFi.localIP().toString();
    ip.replace(ip.substring(ip.lastIndexOf(".") + 1), "174");
    url1 = "http://" + ip + ":5000" + "/v1/device/voice_to_voice";
    static asyncHTTPrequest asyncHttp;

    // 1. 添加延时，避免请求过于频繁
    static unsigned long lastRequestTime = 0;
    if (millis() - lastRequestTime < 80) { // 确保请求间隔至少100ms
        delay(30);
    }
    lastRequestTime = millis();
    
    // 2. 确保之前的请求已完全清理
    if(asyncHttp.readyState() != 0) {
        asyncHttp.abort();
        delay(50); // 给予系统一些时间来清理资源
    }
    
    // 3. 打印完整的 URL 用于调试
    Serial.println("Sending request to URL: " + url1);
    
    // // 4. 使用try-catch包装请求操作
    // try {
    //     if(!asyncHttp.open("POST", url1.c_str())) {
    //         Serial.println("请求打开失败");
    //         asyncHttp.abort();
    //         delay(100);
    //         return false;
    //     }
    // } catch(...) {
    //     Serial.println("请求过程发生异常");
    //     asyncHttp.abort();
    //     delay(100);
    //     return false;
    // }
    
    // 替换为:
    String credentials = String(ApiConfig::USERNAME) + ":" + String(ApiConfig::PASSWORD);
    String authHeader = "Basic " + base64::encode(credentials);
    asyncHttp.setReqHeader("Authorization", authHeader.c_str());
    asyncHttp.setReqHeader("Content-Type", "application/json");

    if (audioBuffer != nullptr && bufferSize > 0) {
        WAVHeader wavHeader;  // 创建WAV头实例
        // 设置变的大小参数
        wavHeader.chunkSize = 36 + bufferSize;  // 文件总长度减去8字节
        wavHeader.subchunk2Size = bufferSize;   // 音频数据的大小
        
        // 分配内存并复制数据
        size_t totalSize = sizeof(WAVHeader) + bufferSize;
        uint8_t* combinedBuffer = (uint8_t*)malloc(totalSize);
        if (!combinedBuffer) {
            Serial.println("内存分配失败! 尝试清理内存...");
            return false;
        }
        
        // 复制数据
        memcpy(combinedBuffer, &wavHeader, sizeof(WAVHeader));
        memcpy(combinedBuffer + sizeof(WAVHeader), audioBuffer, bufferSize);
        
        // base64编码
        String base64Audio;
        try {
            base64Audio = base64::encode(combinedBuffer, totalSize);
        } catch (...) {
            Serial.println("Base64编码过程中发生错误!");
            free(combinedBuffer);
            return false;
        }
        
        // 检查base64编码结果
        if (base64Audio.length() == 0) {
            Serial.println("警告: base64编码结果为空!");
            free(combinedBuffer);
            return false;
        }
        
        // 创建JSON并发送
        const size_t json_doc_size = base64Audio.length() + 256;  // 为JSON结构预留空间
        
        DynamicJsonDocument doc(json_doc_size);
        doc["role_id"] = 3;
        doc["status"] = isLastFrame ? 2 : 1;
        doc["chunk"] = base64Audio;
        doc["min_recognizable"] = 500;
        
        String jsonBody;
        serializeJson(doc, jsonBody);
        
        // 只有当isLastFrame为true时，才执行下面的响应函数
        if(isLastFrame) {
            asyncHttp.onReadyStateChange([](void* optParm, asyncHTTPrequest* request, int readyState) {
                if (readyState == 4) {  // 请求完成
                    digitalWrite(led2, LOW);
                
                // 1. 安全获取响应
                if (!request) {
                    Serial.println("无效的请求对象");
                    return;
                }
                
                // 2. 使用局部变量存储响应
                String response;
                try {
                    response = request->responseText();
                } catch (...) {
                    Serial.println("获取响应文本时发生错误");
                    return;
                }
                
                // 3. 验证响应内容
                if (response.length() == 0) {
                    Serial.println("空响应");
                    return;
                }
                
                // 4. 安全处理响应
                if (response.indexOf("voice_path") != -1) {
                    // 使用try-catch包装处理过程
                    try {
                        struct tm timeinfo;
                        if (getLocalTime(&timeinfo)) {
                            char timeString[64];
                            strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
                            Serial.println("找到voice_path，当前时间:" + String(timeString));
                        }
                        
                        // 确保在处理响应前验证audioQueue未满
                        if (audioQueue.size() < 10) { // 设置一个最大队列大小
                            processHttpResponse(response);
                            playNextAudio();
                        } else {
                            Serial.println("音频队列已满，跳过处理");
                        }
                    } catch (...) {
                        Serial.println("处理响应时发生错误");
                    }
                } else {
                    Serial.println("服务器返回消息：" + response);
                    }
                }
            });
        }
        // 发送请求
        asyncHttp.send(jsonBody);
        
        free(combinedBuffer);
        return true;
    }

    asyncHttp.abort();  // 使用 abort 替代 end
    return false;
}

// 修改 recordAndSendAudio 函数
void recordAndSendAudio() {
    // 直接进入录音状态，不需要校准
    audioBuffer = staticAudioBuffer;
    recordState = RECORDING;
    bufferOffset = 0;
    frameCount = 0;
    silence = 0;
    voice = 0;
    voicebegin = 0;
}

ForceSensor forceSensor;

void handleHttpResponse(const String& response) {
    processHttpResponse(response);
}

// 在全局变量区域添加
unsigned long lastKeyPressTime = 0;
const unsigned long KEY_DEBOUNCE_DELAY = 500; // 500ms防抖延时

void setup() {
    Serial.begin(115200);
    pinMode(17, INPUT);
    pinMode(key, INPUT);
    pinMode(34, INPUT_PULLUP);
    pinMode(35, INPUT_PULLUP);
    pinMode(led1, OUTPUT);  // 添加LED1引脚初始化
    pinMode(led2, OUTPUT);  // 添加LED2引脚初始化
    digitalWrite(led1, HIGH);  // 配网开始，LED1亮起
    audioRecord.init();

    int numNetworks = sizeof(wifiData) / sizeof(wifiData[0]);
    wifiConnect(wifiData, numNetworks);
    
    digitalWrite(led1, LOW);  // 配网完成，LED1熄灭

    // 在WiFi连接之前添加噪音校准
    Serial.println("开始环境噪音校准...");
    float totalNoise = 0;
    const int calibrationSamples = 10;  // 采20次以获得更稳定的结果
    //delay1s
        delay(500);  // 等待初始化电压稳定

    for(int i = 0; i < calibrationSamples; i++) {
        audioRecord.Record();
        float rms = calculateRMS((uint8_t *)audioRecord.wavData[0], FRAME_SIZE);
        totalNoise += rms;
        delay(40);  // 每次采样间隔40ms
    }
    
    // noiseLevel = (totalNoise / calibrationSamples) * 2;  // 设置噪音基准为平均值的1.5倍
    noiseLevel=650;
    Serial.printf("环境噪音基准设置为: %f\n", noiseLevel);

    // 添加I2S初始化检查
    if (!audioPlay.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT)) {
        Serial.println("I2S引脚配置失败");
        // 可以在这里添加重试逻辑
    }
    
    // 设置小的音量以减少内存使用
    audioPlay.setVolume(2);  // 从20降到15

    forceSensor.begin();
    forceSensor.setHttpResponseCallback(handleHttpResponse);
    // 可选：设置证书（如果需要HTTPS验证）
    // httpUpdate.setCACert(rootCACertificate);

    // 在WiFi连接成功后添加
    configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");  // 设置中国时区(UTC+8)
}

void loop() {
    audioPlay.loop();
    
    // 当前音频播放完成后，立即播放下一个
    if (!audioQueue.empty() && audioPlay.isplaying == 0) {
        playNextAudio();
    }
    
    // 添加播放状态检查
    if (digitalRead(key) == HIGH && audioPlay.isplaying == 0) {  // 只有在不播放时才允许唤醒
        unsigned long currentTime = millis();
        if (currentTime - lastKeyPressTime >= KEY_DEBOUNCE_DELAY) {
            lastKeyPressTime = currentTime;
            
            audioPlay.stopSong();
            while (!audioQueue.empty()) {
                audioQueue.pop();
            }
            startPlay = false;
            isReady = false;
            adc_start_flag = 1;
            Serial.printf("开始识别\r\n\r\n");
            lastProcessTime = millis();
            recordAndSendAudio();
        }
    }
    processAudioState();
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
        case RECORDING: {
            if (millis() - lastProcessTime >= 30) {
                lastProcessTime = millis();
                audioRecord.Record();
                float rms = calculateRMS((uint8_t *)audioRecord.wavData[0], FRAME_SIZE);
                // Serial.println("rms:" + String(rms));
                if (rms > noiseLevel) {
                    voice++;
                    if (voice > 1) {    
                        voicebegin = 1;
                        silence = 0;
                    }
                } else {
                    if (voicebegin) {
                        silence++;
                        Serial.println("silence:" + String(silence));
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
                bool isLastFrame = silence >= 8;
                
                // 强制进行垃圾回收
                ESP.getFreeHeap();
                
                if (sendAudioData(audioBuffer, bufferOffset, isLastFrame)) {
                    if (isLastFrame) {
                        Serial.println("音频发送完成，重置状态");
                        bufferOffset = 0;
                        frameCount = 0;
                        voice = 0;
                        voicebegin = 0;
                        silence = 0;
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
        
        case IDLE: {
            // 在空闲状态下进行内存清理
            ESP.getFreeHeap();
            break;
        }
        
        default:
            break;
    }
}
