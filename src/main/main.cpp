#include <M5Atom.h>
#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include <WiFiClientSecure.h>
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <driver/i2s.h>

using namespace websockets;
#define key 0
#define ADC 32
#define led1 19
const char *wifiData[][2] = {
    {"IQOO", "88888888"}, // 替换为自己常用的wifi名和密码
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
int noise = 180;
int isplaying = 0;
HTTPClient https;

hw_timer_t *timer = NULL;

uint8_t adc_start_flag = 0;
uint8_t adc_complete_flag = 0;

// Audio1 audio1;
// Audio2 audio2(false, 3, I2S_NUM_1);

#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

const char *URL="http://120.76.98.140:8000/4427989152-out.mp3";

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;


void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1]=0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}
void updateAudioStream(const char* newURL) {
    if (file) {
        delete file;
    }
    if (buff) {
        delete buff;
    }
    file = new AudioFileSourceICYStream(newURL);
    file->RegisterMetadataCB(MDCallback, (void*)"ICY");
    buff = new AudioFileSourceBuffer(file, 10240);
    buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
    mp3->begin(buff, out);
}
// 减小缓冲区大小或使用动态分配
// uint8_t microphonedata0[1024 * 80];
uint8_t* microphonedata0 = nullptr;

// 在需要时分配内存
void allocateMicrophoneBuffer() {
    if (!microphonedata0) {
        microphonedata0 = (uint8_t*)malloc(1024 * 80);
    }
}

// 在不需要时释放内存
void freeMicrophoneBuffer() {
    if (microphonedata0) {
        free(microphonedata0);
        microphonedata0 = nullptr;
    }
}
uint8_t
    DisBuff[2 + 5 * 5 * 3];  // Used to store RGB color values.  用来存储RBG色值

void setBuff(uint8_t Rdata, uint8_t Gdata,
             uint8_t Bdata) {  // Set the colors of LED, and save the relevant
                               // data to DisBuff[].  设置RGB灯的颜色
    DisBuff[0] = 0x05;
    DisBuff[1] = 0x05;
    for (int i = 0; i < 25; i++) {
        DisBuff[2 + i * 3 + 0] = Rdata;
        DisBuff[2 + i * 3 + 1] = Gdata;
        DisBuff[2 + i * 3 + 2] = Bdata;
    }
}
size_t byte_read = 0;
int16_t *buffptr;
uint32_t data_offset = 0;
#define DATA_SIZE 1024

String SpakeStr;
bool Spakeflag = false;

#define I2S_DOUT 22 // DIN
#define I2S_BCLK 19 // BCLK
#define I2S_LRC 33  // LRC

void gain_token(void);
float calculateRMS(uint8_t *buffer, int bufferSize);
void ConnServer1();

DynamicJsonDocument doc(1000);
JsonArray text = doc.to<JsonArray>();

String url = "";
String url1 = "";
String Date = "";

String askquestion = "";
String Answer = "";

const char *appId1 = "72e78f96"; 
const char *domain1 = "generalv3";
const char *websockets_server = "ws://spark-api.xf-yun.com/v3.1/chat";
const char *websockets_server1 = "ws://192.168.207.174:8765";
using namespace websockets;

#define SPEAK_I2S_NUMBER I2S_NUM_0
#define MODE_MIC 0
#define MODE_SPK 1
#define CONFIG_I2S_BCK_PIN     19
#define CONFIG_I2S_LRCK_PIN    33
#define CONFIG_I2S_DATA_PIN    22
#define CONFIG_I2S_DATA_IN_PIN 23

bool InitI2SSpeakOrMic(int mode) {
    esp_err_t err = ESP_OK;

    i2s_driver_uninstall(SPEAK_I2S_NUMBER);
    i2s_config_t i2s_config = {
        .mode        = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = 16000,
        .bits_per_sample =
            I2S_BITS_PER_SAMPLE_16BIT,  // is fixed at 12bit, stereo, MSB
        .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 1, 0)
        .communication_format =
            I2S_COMM_FORMAT_STAND_I2S,  // Set the format of the communication.
#else                                   // 设置通讯格式
        .communication_format = I2S_COMM_FORMAT_I2S,
#endif
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count    = 6,
        .dma_buf_len      = 60,
    };
    if (mode == MODE_MIC) {
        i2s_config.mode =
            (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    } else {
        i2s_config.mode     = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_config.use_apll = false;
        i2s_config.tx_desc_auto_clear = true;
    }

    Serial.println("Init i2s_driver_install");

    err += i2s_driver_install(SPEAK_I2S_NUMBER, &i2s_config, 0, NULL);
    i2s_pin_config_t tx_pin_config;

#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 3, 0))
    tx_pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
#endif
    tx_pin_config.bck_io_num   = CONFIG_I2S_BCK_PIN;
    tx_pin_config.ws_io_num    = CONFIG_I2S_LRCK_PIN;
    tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
    tx_pin_config.data_in_num  = CONFIG_I2S_DATA_IN_PIN;

    Serial.println("Init i2s_set_pin");
    err += i2s_set_pin(SPEAK_I2S_NUMBER, &tx_pin_config);
    Serial.println("Init i2s_set_clk");
    err += i2s_set_clk(SPEAK_I2S_NUMBER, 16000, I2S_BITS_PER_SAMPLE_16BIT,
                       I2S_CHANNEL_MONO);

    return true;
}


WebsocketsClient webSocketClient;
WebsocketsClient webSocketClient1;

int loopcount = 0;
void onMessageCallback(WebsocketsMessage message)
{
    StaticJsonDocument<1024> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, message.data());
    Serial.println(message.data());
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
            if (result.length() > 0 && (isplaying == 0))
            {
                //"result": "http://localhost:8000//Users/test/Downloads/esp32-chattoys-server/media/4399404064-out.wav"
                //result转换为仅获取最后一个/后的字符串
                // InitI2SSpeakOrMic(MODE_SPK);
                i2s_driver_uninstall(SPEAK_I2S_NUMBER);

                String audioStreamURL = "http://192.168.207.174:8000/" + result.substring(result.lastIndexOf("/") + 1);
                Serial.println(audioStreamURL);
                URL = audioStreamURL.c_str();
                // audio2.connecttohost(audioStreamURL.c_str());
                updateAudioStream(URL);
                startPlay = true;
            }
        }
    }
}

void onEventsCallback1(WebsocketsEvent event, String data)
{
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        Serial.println("Send message to coze");
        setBuff(0x40, 0x00, 0x00);
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
            allocateMicrophoneBuffer();
            i2s_read(SPEAK_I2S_NUMBER, (char *)(microphonedata0),
                     DATA_SIZE, &byte_read, (100 / portTICK_RATE_MS));
            // for (int i = 0; i < audio1.i2sBufferSize / 8; ++i)
            // {
            //     audio1.wavData[0][2 * i] = audio1.i2sBuffer[8 * i + 2];
            //     audio1.wavData[0][2 * i + 1] = audio1.i2sBuffer[8 * i + 3];
            // }
            float rms = calculateRMS((uint8_t *)microphonedata0, DATA_SIZE);
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
                //jsonString发送{'data': {'status': 2, 'format': 'audio/L16;rate=8000', 'audio': '', 'encoding': 'raw'}}
                String jsonString;
                serializeJson(doc, jsonString);
                Serial.println("send_2:");
                webSocketClient1.send(jsonString);
                Serial.println("send_2 end");
                // Serial.println(jsonString);
                setBuff(0x00, 0x40, 0x00);
                delay(40);
                break;
            }
            if (firstframe == 1)
            {
                data["status"] = 0;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)microphonedata0, DATA_SIZE);
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
                freeMicrophoneBuffer();
            }
            else
            {
                data["status"] = 1;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)microphonedata0, DATA_SIZE);
                data["encoding"] = "raw";

                String jsonString;
                serializeJson(doc, jsonString);
                Serial.println("send_1:");
                // Serial.println(jsonString);
                webSocketClient1.send(jsonString);
                Serial.println("send_1 end");
                delay(40);
                freeMicrophoneBuffer();
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


void setup()
{
    // String Date = "Fri, 22 Mar 2024 03:35:56 GMT";
    Serial.begin(115200);
    M5.begin(true, false,
             true); 
    // pinMode(ADC,ANALOG);
    // WiFi.mode(WIFI_AP_STA);   // Set the wifi mode to the mode compatible with
    //                           // the AP and Station, and start intelligent
    //                           // network configuration
    // WiFi.beginSmartConfig();  // 设置wifi模式为AP 与 Station
    int numNetworks = sizeof(wifiData) / sizeof(wifiData[0]);
    wifiConnect(wifiData, numNetworks);
    setBuff(0xff, 0x00, 0x00);
    M5.dis.displaybuff(DisBuff);
    // WiFi.mode(WIFI_AP_STA);   
    // if(WiFi.beginSmartConfig())
    // {
    //     Serial.println("beginSmartConfig success");
    // }
    // else
    // {
    //     Serial.println("beginSmartConfig fail");
    // }
    // while (!WiFi.smartConfigDone()) {  // If the smart network is not completed.
    //                                    // 若智能配网没有完成
    //     delay(500);
    //     Serial.println("smartConfig not done");
    // }
    
    // 配网成功后，立即请求接口
    String bssid = WiFi.BSSIDstr(); // 获取 BSSID
    String uuid = String(ESP.getEfuseMac()); // 获取 UUID
    HTTPClient http;
    http.begin("http://120.76.98.140/app/api/v1/toy/save-uuid");
    
    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    String form = "--" + boundary + "\r\n";
    form += "Content-Disposition: form-data; name=\"wifi_bssid\"\r\n\r\n" + bssid + "\r\n";
    form += "--" + boundary + "\r\n";
    form += "Content-Disposition: form-data; name=\"toy_uuid\"\r\n\r\n" + uuid + "\r\n";
    form += "--" + boundary + "--\r\n";
    Serial.println(form);
    

    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    
    int httpCode = http.POST(form);
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println("Response: " + payload);
        } else {
            Serial.printf("Error: %d\n", httpCode);
        }
    } else {
        Serial.printf("HTTP请求失败，错误原因: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
       Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

    // String Date = "Fri, 22 Mar 2024 03:35:56 GMT";
    url = "ws://192.168.207.174:8765";
    //url1 = getUrl("ws://192.168.207.174:8765", "120.76.98.140", "/wss", Date);
    url1 = "ws://192.168.207.174:8765";
    audioLogger = &Serial;
    file = new AudioFileSourceICYStream(URL);
    file->RegisterMetadataCB(MDCallback, (void*)"ICY");
    buff = new AudioFileSourceBuffer(file, 10240);
    buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
    out = new AudioOutputI2S();
    mp3 = new AudioGeneratorMP3();
    mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
    ///////////////////////////////////
}
void loop()
{
    M5.update();
    //webSocketClient.poll();
    webSocketClient1.poll();
    // delay(10);

    // audio2.loop();

    if (isplaying == 1)
    {
        Serial.printf("Start setBuff red green\r\n\r\n");
        setBuff(0x40, 0x40, 0x00);
        M5.dis.displaybuff(DisBuff);
    }
    else
    {
        setBuff(0x00, 0x00, 0x40);
        M5.dis.displaybuff(DisBuff);
    }
    static int lastms = 0;
    
    if (mp3->isRunning()) {
        M5.dis.drawpix(0, 0x00ffff);
        if (millis()-lastms > 1000) {
        lastms = millis();
        Serial.printf("Running for %d ms...\n", lastms);
        Serial.flush();
        }
        if (!mp3->loop()) mp3->stop();
    } else {
        M5.dis.drawpix(0, 0xffffff);
    }
    if (M5.Btn.isPressed())
    {
        Serial.printf("Btn is pressed\r\n");

        isplaying = 0;
        startPlay = false;
        isReady = false;
        Answer = "";
        Serial.printf("Start recognition\r\n\r\n");

        adc_start_flag = 1;
        // Serial.println(esp_get_free_heap_size());
        data_offset = 0;
        Spakeflag   = false;
        InitI2SSpeakOrMic(MODE_MIC);
        
        askquestion = "";
        // audio2.connecttospeech(askquestion.c_str(), "zh");
        ConnServer1();

        // delay(6000);
        // audio1.Record();
        adc_complete_flag = 0;

        // Serial.println(text);
        // checkLen(text);
    }
}

float calculateRMS(uint8_t *buffer, int bufferSize)
{
    float sum = 0;
    int16_t sample;

    for (int i = 0; i < bufferSize; i ++)
    {

        sample = (buffer[i]) ;
        sum += sample * sample;
    }

    sum /= (bufferSize / 2);

    return sqrt(sum);
}