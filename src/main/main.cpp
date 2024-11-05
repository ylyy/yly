#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include <WiFiClientSecure.h>
#include "HTTPClient.h"
#include "Audio1.h"
#include "Audio2.h"
#include <ArduinoJson.h>
#include <queue>

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

String APPID = "72e78f96";
String APIKey = "7d40a5a094a70adb709169ed3be2aac1";
String APISecret = "Zjc3ZGM2YjY4M2ExNTJlM2JmNWI2NjJl";

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

// 定义一个队来存储音频URL
std::queue<String> audioQueue;

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

String getUrl(String Spark_url, String host, String path, String Date)
{

    // 拼接字符串
    String signature_origin = "host: " + host + "\n";
    signature_origin += "date: " + Date + "\n";
    signature_origin += "GET " + path + " HTTP/1.1";
    // signature_origin="host: spark-api.xf-yun.com\ndate: Mon, 04 Mar 2024 19:23:20 GMT\nGET /v3.1/chat HTTP/1.1";

    // hmac-sha256 加密
    unsigned char hmac[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const size_t messageLength = signature_origin.length();
    const size_t keyLength = APISecret.length();
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)APISecret.c_str(), keyLength);
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)signature_origin.c_str(), messageLength);
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    // base64 编码
    String signature_sha_base64 = base64::encode(hmac, sizeof(hmac) / sizeof(hmac[0]));

    // 替换Date
    Date.replace(",", "%2C");
    Date.replace(" ", "+");
    Date.replace(":", "%3A");
    String authorization_origin = "api_key=\"" + APIKey + "\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"" + signature_sha_base64 + "\"";
    String authorization = base64::encode(authorization_origin);
    String url = Spark_url + '?' + "authorization=" + authorization + "&date=" + Date + "&host=" + host;
    Serial.println(url);
    return url;
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
// 修改发送音频数据的函数
bool sendAudioData(uint8_t *audioBuffer, size_t bufferSize)
{
    HTTPClient http;
    http.begin("https://srt.ttson.cn:34001/predict");
    http.addHeader("Accept", "application/json");

    // 创建multipart form数据
    String boundary = "----WebKitFormBoundaryABC123";
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

    // 准备WAV头
    WAVHeader wavHeader;
    wavHeader.chunkSize = 36 + bufferSize;
    wavHeader.subchunk2Size = bufferSize;

    // 构建完整的POST数据
    String body = "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"content\"; filename=\"audio.wav\"\r\n";
    body += "Content-Type: audio/wav\r\n\r\n";

    // 计算总长度并分配内存
    size_t totalSize = body.length() + sizeof(WAVHeader) + bufferSize + ("\r\n--" + boundary + "--\r\n").length();
    uint8_t* postData = new uint8_t[totalSize];
    size_t currentPos = 0;

    // 复制表单头
    memcpy(postData, body.c_str(), body.length());
    currentPos += body.length();

    // 复制WAV头
    memcpy(postData + currentPos, &wavHeader, sizeof(WAVHeader));
    currentPos += sizeof(WAVHeader);

    // 复制音频数据
    memcpy(postData + currentPos, audioBuffer, bufferSize);
    currentPos += bufferSize;

    // 复制结束边界
    String endBoundary = "\r\n--" + boundary + "--\r\n";
    memcpy(postData + currentPos, endBoundary.c_str(), endBoundary.length());

    // 发送POST请求
    int httpCode = http.POST(postData, totalSize);
    delete[] postData;

    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
            String transcription = doc["transcription"].as<String>();
            Serial.println("转录结果: " + transcription);
            if (transcription.length() > 0) {
                askquestion = transcription;
                http.end();
                return true;
            }
        }
        else
        {
            Serial.println("转录结果为空");
        }
    } else {
        Serial.printf("HTTP请求失败，错误码: %d\n", httpCode);
    }
    
    http.end();
    return false;
}

// 修改 recordAndSendAudio 函数
void recordAndSendAudio()
{
    const int FRAME_SIZE = 1280;  // 每帧数据大小
    const int MAX_BUFFER_SIZE = FRAME_SIZE * 25;  // 从50帧减少到25帧
  // 检查可用内存
    if (ESP.getFreeHeap() < MAX_BUFFER_SIZE + 1024) {  // 预留1KB缓冲
        Serial.println("内存不足，无法开始录音");
        return;
    }
    
    uint8_t* audioBuffer = new uint8_t[MAX_BUFFER_SIZE];
    if (!audioBuffer) {
        Serial.println("内存分配失败");
        return;
    }
    
    Serial.println("开始录音和发送");
    digitalWrite(led2, HIGH);
    

    int bufferPos = 0;
    const int CALIBRATION_FRAMES = 10;  // 校准帧数
    float noiseLevel = 0;
    
    // 先采集环境噪音
    for(int i = 0; i < CALIBRATION_FRAMES; i++) {
        audio1.Record();
        float rms = calculateRMS((uint8_t *)audio1.wavData[0], FRAME_SIZE);
        noiseLevel += rms;
        delay(30);
    }
    noiseLevel /= CALIBRATION_FRAMES;
    
    // VAD 相关参数
    const float VOICE_THRESHOLD = noiseLevel * 1.5;  // 动态阈值
    const int VOICE_HOLD_FRAMES = 10;  // 保持检测的帧数
    const int MAX_SILENCE_FRAMES = 30;  // 最大静音帧数
    
    int silenceCount = 0;
    int voiceCount = 0;
    bool hasVoiceStarted = false;
    unsigned long startTime = millis();
    
    while ((millis() - startTime) < 20000) { // 最长录音20秒
        audio1.Record();
        float rms = calculateRMS((uint8_t *)audio1.wavData[0], FRAME_SIZE);
        Serial.println(rms);
        // 检测到声音
        if (rms > VOICE_THRESHOLD) {
            voiceCount++;
            silenceCount = 0;
            
            if (voiceCount >= VOICE_HOLD_FRAMES) {
                hasVoiceStarted = true;
            }
        } else {
            voiceCount = 0;
            if (hasVoiceStarted) {
                silenceCount++;
            }
        }
        
        if (hasVoiceStarted && bufferPos < MAX_BUFFER_SIZE) {
            memcpy(audioBuffer + bufferPos, audio1.wavData[0], FRAME_SIZE);
            bufferPos += FRAME_SIZE;
        }
        
        if ((hasVoiceStarted && silenceCount >= MAX_SILENCE_FRAMES) || 
            bufferPos >= MAX_BUFFER_SIZE) {
            break;
        }
        
        delay(30);
    }
    
    if (bufferPos > 0 && hasVoiceStarted) {
        sendAudioData(audioBuffer, bufferPos);
    }
    
    heap_caps_free(audioBuffer);  // 使用heap_caps_free释放内存
    digitalWrite(led2, LOW);
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

    // String Date = "Fri, 22 Mar 2024 03:35:56 GMT";
    url = getUrl("ws://localhost:8765", "localhost", "/wss", Date);
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
}

void loop()
{

    // webSocketClient.poll();
    // webSocketClient1.poll();
    // delay(10);
    audio2.loop();

    if (audio2.isplaying == 1)
    {
        digitalWrite(led3, HIGH);
    }
    else
    {
        digitalWrite(led3, LOW);
        // 当前音频播放结束，检查是否有下一个音频需要播放
        playNextAudio();
    }

    // if (digitalRead(17) == HIGH)

    if (digitalRead(key) == 1)
    {
        audio2.isplaying = 0;
        startPlay = false;
        isReady = false;
        Answer = "";
        Serial.printf("开始识别\r\n\r\n");

        adc_start_flag = 1;

        askquestion = "";
        // audio2.connecttospeech(askquestion.c_str(), "zh");
        recordAndSendAudio();
        adc_complete_flag = 0;

        // Serial.println(text);
        // checkLen(text);
    }
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


