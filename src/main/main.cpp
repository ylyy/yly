#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include <WiFiClientSecure.h>
#include "HTTPClient.h"
#include "Audio1.h"
#include "Audio2.h"
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
using namespace websockets;
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

// 定义一个队列来存储音频URL
std::queue<String> audioQueue;

void gain_token(void);
void getText(String role, String content);
void checkLen(JsonArray textArray);
int getLength(JsonArray textArray);
float calculateRMS(uint8_t *buffer, int bufferSize);
void ConnServer();
void ConnServer1();

DynamicJsonDocument doc(4000);
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
using namespace websockets;

WebsocketsClient webSocketClient;
WebsocketsClient webSocketClient1;

int loopcount = 0;
void onMessageCallback(WebsocketsMessage message)
{
    StaticJsonDocument<4096> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    if (!error)
    {
        int code = jsonDocument["header"]["code"];
        if (code != 0)
        {
            Serial.print("sth is wrong: ");
            Serial.println(code);
            Serial.println(message.data());
            webSocketClient.close();
        }
        else
        {
            receiveFrame++;
            Serial.print("receiveFrame:");
            Serial.println(receiveFrame);
            Serial.println(message.data());
            //{"code": 0, "data": {"status": 2, "result": "http://localhost:8000//Users/test/Downloads/esp32-chattoys-server/media/4399404064-out.wav"}}
            String result = jsonDocument["data"]["result"];
            if (result.length() > 0 && (audio2.isplaying == 0))
            {
                //"result": "http://localhost:8000//Users/test/Downloads/esp32-chattoys-server/media/4399404064-out.wav"
                // result转换为仅获取最后一个/后的字符串
                String audioStreamURL = "http://localhost:8000/" + result.substring(result.lastIndexOf("/") + 1);
                Serial.println(audioStreamURL);
                audio2.connecttohost(audioStreamURL.c_str());
                startPlay = true;
            }
            getText("assistant", Answer);
        }
    }
}

void onEventsCallback(WebsocketsEvent event, String data)
{
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        Serial.println("Send message to server0!");
        DynamicJsonDocument jsonData = gen_params(appId1, domain1);
        String jsonString;
        serializeJson(jsonData, jsonString);
        Serial.println(jsonString);
        webSocketClient.send(jsonString);
    }
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        Serial.println("Connnection0 Closed");
    }
    else if (event == WebsocketsEvent::GotPing)
    {
        Serial.println("Got a Ping!");
    }
    else if (event == WebsocketsEvent::GotPong)
    {
        Serial.println("Got a Pong!");
    }
}

void onMessageCallback1(WebsocketsMessage message)
{
    StaticJsonDocument<4096> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    if (!error)
    {
        int code = jsonDocument["code"];
        if (code != 0)
        {
            Serial.println(code);
            Serial.println(message.data());
            webSocketClient1.close();
        }
        else
        {
            Serial.println("xunfeiyun return message:");
            Serial.println(message.data());
            JsonArray ws = jsonDocument["data"]["result"]["ws"].as<JsonArray>();

            for (JsonVariant i : ws)
            {
                for (JsonVariant w : i["cw"].as<JsonArray>())
                {
                    askquestion += w["w"].as<String>();
                }
            }
            Serial.println(askquestion);
            int status = jsonDocument["data"]["status"];
            if (status == 2)
            {
                Serial.println("status == 2");
                webSocketClient1.close();
                if (askquestion == "")
                {
                    askquestion = "sorry, i can't hear you";
                    audio2.connecttospeech(askquestion.c_str(), "zh");
                }
                else if (askquestion.substring(0, 9) == "唱歌了" or askquestion.substring(0, 9) == "唱歌啦")
                {

                    if (askquestion.substring(0, 12) == "唱歌了，" or askquestion.substring(0, 12) == "唱歌啦，")
                    { // 自建音乐服务器，按照文件名查找对应歌曲
                        String audioStreamURL = "http://192.168.0.1/mp3/" + askquestion.substring(12, askquestion.length() - 3) + ".mp3";
                        Serial.println(audioStreamURL.c_str());
                        audio2.connecttohost(audioStreamURL.c_str());
                    }
                    else if (askquestion.substring(9) == "。")
                    {
                        askquestion = "好啊, 你想听什么歌？";
                        mainStatus = 1;
                        audio2.connecttospeech(askquestion.c_str(), "zh");
                    }
                    else
                    {
                        String audioStreamURL = "http://192.168.0.1/mp3/" + askquestion.substring(9, askquestion.length() - 3) + ".mp3";
                        Serial.println(audioStreamURL.c_str());
                        audio2.connecttohost(audioStreamURL.c_str());
                    }
                }
                else if (mainStatus == 1)
                {
                    askquestion.trim();
                    if (askquestion.endsWith("。"))
                    {
                        askquestion = askquestion.substring(0, askquestion.length() - 3);
                    }
                    else if (askquestion.endsWith(".") or askquestion.endsWith("?"))
                    {
                        askquestion = askquestion.substring(0, askquestion.length() - 1);
                    }
                    String audioStreamURL = "http://192.168.0.1/mp3/" + askquestion + ".mp3";
                    Serial.println(audioStreamURL.c_str());
                    audio2.connecttohost(audioStreamURL.c_str());
                    mainStatus = 0;
                }
                else
                {
                    getText("user", askquestion);
                    Serial.print("text:");
                    Serial.println(text);
                    Answer = "";
                    lastsetence = false;
                    isReady = true;
                    ConnServer();
                }
            }
        }
    }
    else
    {
        Serial.println("error:");
        Serial.println(error.c_str());
        Serial.println(message.data());
    }
}

void onEventsCallback1(WebsocketsEvent event, String data)
{
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        Serial.println("Send message to xunfeiyun");
        digitalWrite(led2, HIGH);
        int silence = 0;
        int firstframe = 1;
        int j = 0;
        int voicebegin = 0;
        int voice = 0;
        DynamicJsonDocument doc(2500);
        while (1)
        {
            doc.clear();
            JsonObject data = doc.createNestedObject("data");
            audio1.Record();
            float rms = calculateRMS((uint8_t *)audio1.wavData[0], 1280);
            printf("%d %f\n", 0, rms);
            if (rms < noise)
            {
                if (voicebegin == 1)
                {
                    silence++;
                    // Serial.print("noise:");
                    // Serial.println(noise);
                }
            }
            else
            {
                voice++;
                if (voice >= 5)
                {
                    voicebegin = 1;
                }
                else
                {
                    voicebegin = 0;
                }
                silence = 0;
            }
            if (silence == 6)
            {
                data["status"] = 2;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = "";
                data["encoding"] = "raw";
                j++;
                // jsonString发送{'data': {'status': 2, 'format': 'audio/L16;rate=8000', 'audio': '', 'encoding': 'raw'}}
                String jsonString;
                serializeJson(doc, jsonString);
                Serial.println("send_2:");
                webSocketClient1.send(jsonString);
                Serial.println("send_2 end");
                Serial.println(jsonString);
                digitalWrite(led2, LOW);
                delay(40);
                break;
            }
            if (firstframe == 1)
            {
                data["status"] = 0;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)audio1.wavData[0], 1280);
                data["encoding"] = "raw";
                j++;

                JsonObject common = doc.createNestedObject("common");
                common["app_id"] = appId1;

                JsonObject business = doc.createNestedObject("business");
                business["domain"] = "iat";
                business["language"] = "zh_cn";
                business["accent"] = "mandarin";
                business["vinfo"] = 1;
                business["vad_eos"] = 1000;

                String jsonString;
                serializeJson(doc, jsonString);
                Serial.println("send_0:");
                Serial.println(jsonString);
                webSocketClient1.send(jsonString);
                Serial.println("send_0 end");
                firstframe = 0;
                delay(40);
            }
            else
            {
                data["status"] = 1;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)audio1.wavData[0], 1280);
                data["encoding"] = "raw";

                String jsonString;
                serializeJson(doc, jsonString);
                Serial.println("send_1:");
                Serial.println(jsonString);
                webSocketClient1.send(jsonString);
                Serial.println("send_1 end");
                delay(40);
            }
        }
    }
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        Serial.println("Connnection1 Closed");
    }
    else if (event == WebsocketsEvent::GotPing)
    {
        Serial.println("Got a Ping!");
    }
    else if (event == WebsocketsEvent::GotPong)
    {
        Serial.println("Got a Pong!");
    }
}
void ConnServer()
{
    Serial.println("url:" + url);

    webSocketClient.onMessage(onMessageCallback);
    webSocketClient.onEvent(onEventsCallback);
    // Connect to WebSocket
    Serial.println("Begin connect to server0......");
    if (webSocketClient.connect(url.c_str()))
    {
        Serial.println("Connected to server0!");
    }
    else
    {
        Serial.println("Failed to connect to server0!");
    }
}

void ConnServer1()
{
    // Serial.println("url1:" + url1);
    webSocketClient1.onMessage(onMessageCallback);
    webSocketClient1.onEvent(onEventsCallback1);
    // Connect to WebSocket
    Serial.println("Begin connect to server1...... url1:" + url1);
    if (webSocketClient1.connect(url1.c_str()))
    {
        Serial.println("Connected to server1!");
    }
    else
    {
        Serial.println("Failed to connect to server1!");
    }
}

void voicePlay()
{
    if ((audio2.isplaying == 0) && Answer != "")
    {
        // String subAnswer = "";
        // String answer = "";
        // if (Answer.length() >= 100)
        //     subAnswer = Answer.substring(0, 100);
        // else
        // {
        //     subAnswer = Answer.substring(0);
        //     lastsetence = true;
        //     // startPlay = false;
        // }

        // Serial.print("subAnswer:");
        // Serial.println(subAnswer);
        int firstPeriodIndex = Answer.indexOf("。");
        int secondPeriodIndex = 0;

        if (firstPeriodIndex != -1)
        {
            secondPeriodIndex = Answer.indexOf("。", firstPeriodIndex + 1);
            if (secondPeriodIndex == -1)
                secondPeriodIndex = firstPeriodIndex;
        }
        else
        {
            secondPeriodIndex = firstPeriodIndex;
        }

        if (secondPeriodIndex != -1)
        {
            String answer = Answer.substring(0, secondPeriodIndex + 1);
            Serial.print("answer: ");
            Serial.println(answer);
            Answer = Answer.substring(secondPeriodIndex + 2);
            audio2.connecttospeech(answer.c_str(), "zh");
        }
        else
        {
            const char *chinesePunctuation = "？，：；,.";

            int lastChineseSentenceIndex = -1;

            for (int i = 0; i < Answer.length(); ++i)
            {
                char currentChar = Answer.charAt(i);

                if (strchr(chinesePunctuation, currentChar) != NULL)
                {
                    lastChineseSentenceIndex = i;
                }
            }

            if (lastChineseSentenceIndex != -1)
            {
                String answer = Answer.substring(0, lastChineseSentenceIndex + 1);
                audio2.connecttospeech(answer.c_str(), "zh");
                Answer = Answer.substring(lastChineseSentenceIndex + 2);
            }
        }
        startPlay = true;
    }
    else
    {
        // digitalWrite(led3, LOW);
    }
}

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
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = 8000;
    uint32_t byteRate = 16000;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
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

    String username = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOjEsInR5cGUiOjEwMywic2NvcGUiOiJhZG1pbiIsImlhdCI6MTcyOTQ4NTQ0MywiZXhwIjoxNzMyMDc3NDQzfQ.SavITbhla70iloEusKJd-2wfYueKywNTXSwRpcGPDfQ";
    String password = "";
    http.setAuthorization(username.c_str(), password.c_str());
    http.addHeader("Content-Type", "application/json");

    if (audioBuffer != nullptr && bufferSize > 0) {
        // 创建WAV头
        WAVHeader header;
        header.chunkSize = 36 + bufferSize;
        header.subchunk2Size = bufferSize;

        // 创建包含WAV头和音频数据的缓冲区
        size_t totalSize = sizeof(WAVHeader) + bufferSize;
        uint8_t* wavBuffer = new uint8_t[totalSize];
        memcpy(wavBuffer, &header, sizeof(WAVHeader));
        memcpy(wavBuffer + sizeof(WAVHeader), audioBuffer, bufferSize);

        // 将WAV数据转换为base64
        String base64Audio = base64::encode(wavBuffer, totalSize);
        delete[] wavBuffer;

        // 构建JSON请求体
        DynamicJsonDocument doc(16384);
        doc["role_id"] = 2;
        doc["chunk"] = base64Audio;

        String jsonBody;
        serializeJson(doc, jsonBody);
        Serial.println("发送请求: " + jsonBody);

        int httpResponseCode = http.POST(jsonBody);
        Serial.println("服务器响应码: " + String(httpResponseCode));
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("服务器响应: " + response);
            
            // 分割响应字符串
            int startPos = 0;
            int endPos = 0;
            bool hasAddedAudio = false;

            while ((endPos = response.indexOf("}", startPos)) != -1) {
                String jsonPart = response.substring(startPos, endPos + 1);
                
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, jsonPart);
                
                if (!error) {
                    if (doc.containsKey("voice_path") && doc.containsKey("url") && doc.containsKey("port")) {
                        String voicePath = doc["voice_path"].as<String>();
                        String baseUrl = doc["url"].as<String>();
                        int port = doc["port"].as<int>();
                        
                        // 构建完整的音频URL
                        String token = "ht-4b8536b544e0784309820c3d11649a14";
                        voicePath = voicePath.substring(0, voicePath.length() - 4) + ".mp3";

                        String audioUrl = baseUrl + ":" + String(port) + 
                                          "/flashsummary/retrieveFileData?stream=True" + "&token=" + token + "&voice_audio_path=" + voicePath;
                        
                        Serial.println("添加音频到队列: " + audioUrl);
                        
                        // 将音频URL添加到队列
                        audioQueue.push(audioUrl);
                        hasAddedAudio = true;
                    }
                }
                
                startPos = endPos + 1;
            }

            // 如果当前没有音频在播放，开始播放队列中的第一个音频
            if (hasAddedAudio && audio2.isplaying == 0) {
                playNextAudio();
            }
            
            http.end();
            return hasAddedAudio;
        } else {
            Serial.print("发送POST请求时出错: ");
            Serial.println(httpResponseCode);
            http.end();
            return false;
        }
    } 

    http.end();
    return false;
}

// 修改录音和发送逻辑
void recordAndSendAudio()
{
    Serial.println("开始录音和发送");
    digitalWrite(led2, HIGH);
    int silence = 0;
    int firstframe = 1;
    int voicebegin = 0;
    int voice = 0;
    DynamicJsonDocument doc(2500);

    while (1)
    {
        doc.clear();
        JsonObject data = doc.createNestedObject("data");
        audio1.Record();
        float rms = calculateRMS((uint8_t *)audio1.wavData[0], 1280);
        printf("RMS: %f\n", rms);

        if (rms < noise)
        {
            if (voicebegin == 1)
            {
                silence++;
            }
        }
        else
        {
            voice++;
            if (voice >= 4)
            {
                voicebegin = 1;
            }
            else
            {
                voicebegin = 0;
            }
            silence = 0;
        }

        // if (silence == 6)
        // {
        //     // 发送结束标志
        //     sendAudioData(nullptr, 0); // 发送空数据表示结束
        //     digitalWrite(led2, LOW);
        //     Serial.println("录音结束");
        //     break;
        // }

        if (firstframe == 1)
        {
            firstframe = 0;
        }

        // 发送录音数据
        if (sendAudioData(reinterpret_cast<uint8_t *>(audio1.wavData[0]), 1280))
        {
            break;
        }
        // delay(40);
    }
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

    audio2.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio2.setVolume(20);

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
}

void loop()
{

    // webSocketClient.poll();
    webSocketClient1.poll();
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

    if (digitalRead(key) == 0)
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
        // ConnServer();
        // delay(6000);
        // audio1.Record();
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

void checkLen(JsonArray textArray)
{
    while (getLength(textArray) > 3000)
    {
        textArray.remove(0);
    }
    // return textArray;
}

DynamicJsonDocument gen_params(const char *appid, const char *domain)
{
    DynamicJsonDocument data(2048);

    JsonObject header = data.createNestedObject("header");
    header["app_id"] = appid;
    header["uid"] = "1234";

    JsonObject parameter = data.createNestedObject("parameter");
    JsonObject chat = parameter.createNestedObject("chat");
    chat["domain"] = domain;
    chat["temperature"] = 0.5;
    chat["max_tokens"] = 1024;

    JsonObject payload = data.createNestedObject("payload");
    JsonObject message = payload.createNestedObject("message");

    JsonArray textArray = message.createNestedArray("text");
    for (const auto &item : text)
    {
        textArray.add(item);
    }
    return data;
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

