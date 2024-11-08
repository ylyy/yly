#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include <WiFiClientSecure.h>
#include "HTTPClient.h"
#include "Audio1.h"
#include "Audio2.h"
#include <ArduinoJson.h>
#include <queue>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

// 在全局变量区域添加
enum RecordState {
    IDLE,
    CALIBRATING,
    RECORDING,
    SENDING
};

RecordState recordState = IDLE;
uint8_t* audioBuffer = nullptr;
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
int noise = 50;
HTTPClient https;

hw_timer_t *timer = NULL;

uint8_t adc_start_flag = 0;
uint8_t adc_complete_flag = 0;

Audio1 audio1;
Audio2 audio2(false, 3, I2S_NUM_1);

#define I2S_DOUT 27 // DIN
#define I2S_BCLK 26 // BCLK
#define I2S_LRC 25  // LRC
float calculateRMS(uint8_t *buffer, int bufferSize);
bool sendAudioData(uint8_t *audioBuffer, size_t bufferSize);
void processAudioState();

// 定义一个队来存储音频URL
std::queue<String> audioQueue;

// 在文件开头添加队列定义
QueueHandle_t audioDataQueue;
const int AUDIO_QUEUE_LENGTH = 5;
const int AUDIO_BUFFER_SIZE = 25600; // 20帧 * 1280字节

// 添加音频数据结构
struct AudioData {
    uint8_t* buffer;
    size_t size;
};

// 添加录音任务函数
void recordTask(void *parameter) {
    while(1) {
        if (adc_start_flag) {
            Serial.println("开始录音");
            digitalWrite(led2, HIGH);
            
            // 噪音校准
            float noiseLevel = 0;
            const int CALIBRATION_FRAMES = 10;
            
            for (int i = 0; i < CALIBRATION_FRAMES; i++) {
                audio1.Record();
                float rms = calculateRMS((uint8_t *)audio1.wavData[0], 1280);
                noiseLevel += rms;
                delay(40);
            }
            noiseLevel = (noiseLevel / CALIBRATION_FRAMES) * 1.5;
            Serial.printf("噪音基准: %f\n", noiseLevel);

            int silence = 0;
            int voice = 0;
            int voicebegin = 0;
            int frameCount = 0;
            const int FRAMES_TO_SEND = 10;
            const size_t FRAME_SIZE = 1280;
            
            uint8_t* combinedBuffer = (uint8_t*)malloc(FRAME_SIZE * FRAMES_TO_SEND);
            if (!combinedBuffer) {
                Serial.println("内存分配失败");
                digitalWrite(led2, LOW);
                adc_start_flag = 0;
                continue;
            }
            
            size_t bufferOffset = 0;
            
            while (adc_start_flag) {
                audio1.Record();
                float rms = calculateRMS((uint8_t *)audio1.wavData[0], FRAME_SIZE);

                if (rms > noiseLevel) {
                    voice++;
                    if (voice >= 3) {
                        voicebegin = 1;
                        silence = 0;
                    }
                } else {
                    if (voicebegin) {
                        silence++;
                    }
                }

                if (voicebegin == 1) {
                    memcpy(combinedBuffer + bufferOffset, audio1.wavData[0], FRAME_SIZE);
                    bufferOffset += FRAME_SIZE;
                    frameCount++;

                    if (frameCount >= FRAMES_TO_SEND) {
                        AudioData* audioData = new AudioData;
                        audioData->buffer = (uint8_t*)malloc(bufferOffset);
                        memcpy(audioData->buffer, combinedBuffer, bufferOffset);
                        audioData->size = bufferOffset;
                        
                        if (xQueueSend(audioDataQueue, &audioData, 0) != pdTRUE) {
                            free(audioData->buffer);
                            delete audioData;
                        }
                        
                        bufferOffset = 0;
                        frameCount = 0;
                    }
                }

                if (silence > 20) {  // 静音超过一定时间，结束录音
                    adc_start_flag = 0;
                }
                
                vTaskDelay(pdMS_TO_TICKS(40));
            }
            
            free(combinedBuffer);
            digitalWrite(led2, LOW);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 添加发送任务函数
void sendTask(void *parameter) {
    while(1) {
        AudioData* audioData;
        if (xQueueReceive(audioDataQueue, &audioData, portMAX_DELAY) == pdTRUE) {
            sendAudioData(audioData->buffer, audioData->size);
            free(audioData->buffer);
            delete audioData;
        }
    }
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

String askquestion = "";
String Answer = "";

const char *appId1 = "72e78f96";
const char *domain1 = "generalv3";
const char *websockets_server = "ws://spark-api.xf-yun.com/v3.1/chat";
// 修改 websockets_server1 的定义方式
char websockets_server1[100]; // 先声明为 char 数组
// 使用 sprintf 格式化字符串
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
            digitalWrite(led1, ledstatus);
            ledstatus = !ledstatus;
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
    uint32_t sampleRate = 16000;   // 采样率改为16kHz
    uint32_t byteRate = 32000;     // 字节率 = sampleRate * numChannels * (bitsPerSample/8)
    uint16_t blockAlign = 2;       // 数据块对齐 = numChannels * (bitsPerSample/8)
    uint16_t bitsPerSample = 16;   // 16位深度
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size;
};
// 新增函数：播放下一个音频
void playNextAudio() {
    if (!audioQueue.empty()) {
        String audioUrl = audioQueue.front();
        audioQueue.pop();
        Serial.println("开始播放音频: " + audioUrl);
        audio2.connecttohost(audioUrl.c_str());
    }
    else
    {
        // Serial.println("音频队列已空");
    }
}
// 修改sendAudioData函数
bool sendAudioData(uint8_t *audioBuffer, size_t bufferSize)
{
    HTTPClient http;
    String url = "http://101.35.160.32:5000/v1/device/voice_to_voice";
    http.begin(url);

    String username = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOjEsInR5cGUiOjExMCwic2NvcGUiOlsxXSwiaWF0IjoxNzMwODI0Mjc2LCJleHAiOjE3MzM0MTYyNzZ9.N4J4eJUM8Ombg7esoKhCQUEqPXRw8C6MWpag6BJ46e8";
    http.setAuthorization(username.c_str(), "");
    http.addHeader("Content-Type", "application/json");
    
    if (audioBuffer != nullptr && bufferSize > 0) {
        // 创建WAV头
        WAVHeader header;
        header.chunkSize = 36 + bufferSize;
        header.subchunk2Size = bufferSize;

        // 使用较小的缓冲区大小
        const size_t CHUNK_SIZE = 512;
        uint8_t* chunk = (uint8_t*)malloc(CHUNK_SIZE);
        if (!chunk) {
            Serial.println("缓冲区分配失败");
            http.end();
            return false;
        }

        // 创建一个预估大小的JSON文档
        const size_t estimated_base64_size = (bufferSize + sizeof(WAVHeader)) * 4 / 3 + 4; // base64编码后的预估大小
        const size_t json_doc_size = estimated_base64_size + 256; // 添加一些额外空间用于JSON结构
        
        DynamicJsonDocument doc(json_doc_size);
        doc["role_id"] = 2;
        
        // 分块构建base64字符串
        String base64Audio = "";
        
        // 首先编码WAV头
        memcpy(chunk, &header, sizeof(WAVHeader));
        base64Audio = base64::encode(chunk, sizeof(WAVHeader));
        
        // 分块编码音频数据
        size_t offset = 0;
        while (offset < bufferSize) {
            size_t current_chunk_size = min(CHUNK_SIZE, bufferSize - offset);
            memcpy(chunk, audioBuffer + offset, current_chunk_size);
            base64Audio += base64::encode(chunk, current_chunk_size);
            offset += current_chunk_size;
        }
        
        free(chunk);
        
        // 添加到JSON文档
        doc["chunk"] = base64Audio;
        doc["silent_threshold"] = 1000;

        // 序列化JSON
        String jsonBody;
        serializeJson(doc, jsonBody);
        //打印发送的json
        Serial.println(jsonBody);

        // 发送请求
        int httpResponseCode = http.POST(jsonBody);
        Serial.printf("HTTP响应码: %d\n", httpResponseCode);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("服务器响应: " + response);
            // 分割响应字符串并处理音频URL
            int startPos = 0;
            int endPos = 0;
            bool hasAddedAudio = false;

            while ((endPos = response.indexOf("}", startPos)) != -1) {
                String jsonPart = response.substring(startPos, endPos + 1);
                
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

            if (hasAddedAudio && audio2.isplaying == 0) {
                playNextAudio();
            }
            
            http.end();
            return hasAddedAudio;
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
    digitalWrite(led2, HIGH);
    Serial.println("开始录音校准");
}

void getTimeFromServer()
{
    String timeurl = "https://www.baidu.com";
    HTTPClient http;
    http.begin(timeurl);
    const char *headerKeys[] = {"Date"};
    http.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));
    int httpCode = http.GET();
    Date = http.header("Date");
    Serial.println(Date);
    http.end();
    // delay(50); // 可以根据实际情况调整延时时间
}

void setup()
{
    // 在其他初始化代码之前添加
    if(psramFound()) {
        Serial.println("PSRAM 已找到并初始化");
        Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    } else {
        Serial.println("未找到PSRAM，将使用普通内存");
    }
    
    // String Date = "Fri, 22 Mar 2024 03:35:56 GMT";
    Serial.begin(115200);
    // pinMode(ADC,ANALOG);
    pinMode(17, INPUT);
    pinMode(key, INPUT_PULLUP);
    pinMode(34, INPUT_PULLUP);
    pinMode(35, INPUT_PULLUP);
    pinMode(led1, OUTPUT);
    pinMode(led2, OUTPUT);
    pinMode(led3, OUTPUT);
    audio1.init();

    int numNetworks = sizeof(wifiData) / sizeof(wifiData[0]);
    wifiConnect(wifiData, numNetworks);

    getTimeFromServer();

    // 添加I2S初始化检查
    if (!audio2.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT)) {
        Serial.println("I2S引脚配置失败");
        // 可以在这里添加重试逻辑
    }
    
    // 设置较小的音量以减少内存使用
    audio2.setVolume(15);  // 从20降到15

    // url1 = getUrl("ws://localhost:8765", "localhost", "/wss", Date);
    //  ip最后一个.后面的字符替换为174
    String ip = WiFi.localIP().toString();
    ip.replace(ip.substring(ip.lastIndexOf(".") + 1), "174");
    url1 = "ws://" + ip + ":8765";
    Serial.println(url1);
    urlTime = millis();

    ///////////////////////////////////

    // 添加内存检查
    Serial.printf("Total heap: %d\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

    // 创建音频数据队列
    audioDataQueue = xQueueCreate(AUDIO_QUEUE_LENGTH, sizeof(AudioData*));
    
    // 创建录音和发送任务
    xTaskCreatePinnedToCore(
        recordTask,          // 任务函数
        "RecordTask",        // 任务名称
        8192,               // 堆栈大小
        NULL,               // 务参数
        1,                  // 任务优先级
        NULL,               // 任务句柄
        0                   // 在核心0上运行
    );
    
    xTaskCreatePinnedToCore(
        sendTask,           // 任务函数
        "SendTask",         // 任务名称
        8192,               // 堆栈大小
        NULL,               // 任务参数
        1,                  // 任务优先级
        NULL,               // 任务句柄
        1                   // 在核心1上运行
    );
}

void loop()
{
    audio2.loop();
    
    if (audio2.isplaying == 1) {
        digitalWrite(led3, HIGH);
    } else {
        digitalWrite(led3, LOW);
        playNextAudio();
    }
    
    if (digitalRead(key) == 0) {
        audio2.isplaying = 0;
        startPlay = false;
        isReady = false;
        Answer = "";
        Serial.printf("开始识别\r\n\r\n");
        recordAndSendAudio();
    }
    
    processAudioState();  // 处理录音状态机
}

void getText(String role, String content)
{
    checkLen(text);
    DynamicJsonDocument jsoncon(1024);
    jsoncon["role"] = role;
    jsoncon["content"] = content;
    text.add(jsoncon);
    jsoncon.clear();
    String serialized;
    serializeJson(text, serialized);
    Serial.print("text: ");
    Serial.println(serialized);
    // serializeJsonPretty(text, Serial);
}

int getLength(JsonArray textArray)
{
    int length = 0;
    for (JsonObject content : textArray)
    {
        const char *temp = content["content"];
        int leng = strlen(temp);
        length += leng;
    }
    return length;
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
    const int FRAME_SIZE = 1280;
    const int FRAMES_TO_SEND = 10;
    
    switch(recordState) {
        case CALIBRATING: {
            static int calibrationCount = 0;
            static float totalNoise = 0;
            
            if (millis() - lastProcessTime >= 40) {  // 每40ms处理一次
                lastProcessTime = millis();
                
                audio1.Record();
                float rms = calculateRMS((uint8_t *)audio1.wavData[0], FRAME_SIZE);
                totalNoise += rms;
                calibrationCount++;
                
                if (calibrationCount >= 10) {
                    noiseLevel = (totalNoise / calibrationCount) * 1.5;
                    Serial.printf("噪音基准: %f\n", noiseLevel);
                    
                    // 准备录音缓冲区
                    if (audioBuffer == nullptr) {
                        audioBuffer = (uint8_t*)malloc(FRAME_SIZE * FRAMES_TO_SEND);
                    }
                    
                    if (audioBuffer == nullptr) {
                        Serial.println("内存分配失败");
                        recordState = IDLE;
                        digitalWrite(led2, LOW);
                    } else {
                        recordState = RECORDING;
                        bufferOffset = 0;
                        frameCount = 0;
                        silence = 0;
                        voice = 0;
                        voicebegin = 0;
                    }
                    
                    calibrationCount = 0;
                    totalNoise = 0;
                }
            }
            break;
        }
        
        case RECORDING: {
            if (millis() - lastProcessTime >= 40) {  // 每40ms处理一次
                lastProcessTime = millis();
                
                audio1.Record();
                float rms = calculateRMS((uint8_t *)audio1.wavData[0], FRAME_SIZE);
                
                if (rms > noiseLevel) {
                    voice++;
                    if (voice >= 3) {
                        voicebegin = 1;
                        silence = 0;
                    }
                } else {
                    if (voicebegin) {
                        silence++;
                    }
                }
                
                if (voicebegin == 1) {
                    memcpy(audioBuffer + bufferOffset, audio1.wavData[0], FRAME_SIZE);
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
                if (sendAudioData(audioBuffer, bufferOffset)) {
                    Serial.println("音频数据已发送并加入队列，停止录音");
                    bufferOffset = 0;
                    frameCount = 0;
                    recordState = IDLE;  // 有音频返回，停止录音
                } else {
                    // 返回false表示继续录音
                    bufferOffset = 0;
                    frameCount = 0;
                    recordState = RECORDING;  // 继续录音
                }
            } else {
                recordState = IDLE;
            }
            
            if (recordState == IDLE) {
                if (audioBuffer != nullptr) {
                    free(audioBuffer);
                    audioBuffer = nullptr;
                }
                digitalWrite(led2, LOW);
            }
            break;
        }
        
        default:
            break;
    }
}


