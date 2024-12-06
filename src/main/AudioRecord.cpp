#include "AudioRecord.h"

AudioRecord::AudioRecord()
{
  // 캯нĳʼڴĲ
  wavData = nullptr;
  i2s = nullptr;
  i2s = new I2S();

  // wavData = new char*[wavDataSize/dividedWavDataSize];
  // for (int i = 0; i < wavDataSize/dividedWavDataSize; ++i) wavData[i] = new char[dividedWavDataSize];
  // i2s = new I2S(micType);
}

AudioRecord::~AudioRecord()
{
  for (int i = 0; i < wavDataSize / dividedWavDataSize; ++i)
    delete[] wavData[i];
  delete[] wavData;
  delete i2s;
}

void AudioRecord::init()
{
  wavData = new char *[1];
  for (int i = 0; i < 1; ++i)
    wavData[i] = new char[1280];
}

void AudioRecord::clear()
{
  i2s->clear();
}

void AudioRecord::CreateWavHeader(byte *header, int waveDataSize)
{
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSizeMinus8 = waveDataSize + 44 - 8;
  header[4] = (byte)(fileSizeMinus8 & 0xFF);
  header[5] = (byte)((fileSizeMinus8 >> 8) & 0xFF);
  header[6] = (byte)((fileSizeMinus8 >> 16) & 0xFF);
  header[7] = (byte)((fileSizeMinus8 >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10; // linear PCM
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01; // linear PCM
  header[21] = 0x00;
  header[22] = 0x01; // monoral
  header[23] = 0x00;
  header[24] = 0x80; // sampling rate 8000
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00; // Byte/sec = 16000x2x1 = 32000
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02; // 16bit monoral
  header[33] = 0x00;
  header[34] = 0x10; // 16bit
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(waveDataSize & 0xFF);
  header[41] = (byte)((waveDataSize >> 8) & 0xFF);
  header[42] = (byte)((waveDataSize >> 16) & 0xFF);
  header[43] = (byte)((waveDataSize >> 24) & 0xFF);
}

void AudioRecord::Record()
{
  i2s->Read(i2sBuffer, i2sBufferSize);
  const float gain = 12;  // 增益系数，可以根据需要调整

  for (int i = 0; i < i2sBufferSize / 8; ++i)
  {
    int16_t sample = (i2sBuffer[8 * i + 3] << 8) | i2sBuffer[8 * i + 2];
    sample = static_cast<int16_t>(sample * gain);  // 应用增益

    // 确保增益后的值不超过16位整数的范围
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;

    wavData[0][2 * i] = sample & 0xFF;
    wavData[0][2 * i + 1] = (sample >> 8) & 0xFF;
  }
}

String AudioRecord::parseJSON(const char *jsonResponse)
{
  DynamicJsonDocument doc(1024);

  // 解析JSON响应
  DeserializationError error = deserializeJson(doc, jsonResponse);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return String("");
  }

  // 提取并返?"question"
  const char *question = doc["result"][0];
  return String(question);
}

float AudioRecord::calculateRMS(uint8_t *buffer, int bufferSize)
{
  float sum = 0;
  int16_t sample;

  // ÿεֽڣ16λ
  for (int i = 0; i < bufferSize; i += 2)
  {
    // ӻлȡ16λǵֽ˳
    sample = (buffer[i + 1] << 8) | buffer[i];

    // ƽۼ
    sum += sample * sample;
  }

  // ƽֵ
  sum /= (bufferSize / 2);

  // RMSֵ
  return sqrt(sum);
}