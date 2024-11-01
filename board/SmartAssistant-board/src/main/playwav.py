import requests

# 构造正确的URL
base_url = "https://u95167-9b1c-2697c52f.bjc1.seetacloud.com"
port = 8443
voice_path = "./temp/tts/20241025/325de52f.wav"

url = f"{base_url}:{port}{voice_path}"

# 发送GET请求
response = requests.get(url)

# 检查请求是否成功
if response.status_code == 200:
    # 保存文件
    with open("downloaded_audio.wav", "wb") as file:
        file.write(response.content)
    print("文件下载成功!")
else:
    print(f"下载失败。状态码: {response.status_code}")
