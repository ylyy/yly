import requests
import json
import base64
import wave
import os
from pydub import AudioSegment

def check_file_content(filename):
    with open(filename, 'r') as f:
        content = f.read()
    try:
        data = json.loads(content)
        return data
    except json.JSONDecodeError:
        print(f"文件 {filename} 不是有效的 JSON 格式")
        return None
# 用户名（JWT token）
username = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOjEsInR5cGUiOjEwMywic2NvcGUiOiJhZG1pbiIsImlhdCI6MTcyOTQ4NTQ0MywiZXhwIjoxNzMyMDc3NDQzfQ.SavITbhla70iloEusKJd-2wfYueKywNTXSwRpcGPDfQ"

# 创建Basic认证头
auth_header = base64.b64encode(f"{username}:".encode()).decode()

# 获取JSON数据的URL
json_url = "http://101.35.160.32:5000/v1/device/download-all"

# 设置请求头
headers = {
    "Authorization": f"Basic {auth_header}"
}

# 发送GET请求获取JSON数据
response = requests.get(json_url, headers=headers)
data = response.text

# 尝试解析JSON数据
try:
    data = json.loads(data)
except json.JSONDecodeError:
    print("无法解析JSON数据。服务器返回的原始数据:")
    print(data)
    exit(1)

# 确保data是一个列表
if not isinstance(data, list):
    print("数据不是预期的列表格式。服务器返回的数据:")
    print(data)
    exit(1)

# 遍历JSON数据中的每个项目
for item in data:
    if not isinstance(item, dict):
        print(f"跳过非字典项: {item}")
        continue
    
    download_url = item.get("download_url")
    filename = item.get("filename")
    
    if not download_url or not filename:
        print(f"跳过缺少必要信息的项: {item}")
        continue
    
    # 下载WAV文件
    print(f"正在下载: {filename}")
    audio_response = requests.get(download_url, headers=headers)
    
    # 检查下载是否成功
    if audio_response.status_code == 200:
        # 将文件保存到本地
        with open(filename, "wb") as file:
            file.write(audio_response.content)
        print(f"{filename} 下载完成")
    else:
        print(f"下载 {filename} 失败. 状态码: {audio_response.status_code}")

print("所有文件下载完成")

def concatenate_audio_files(input_files, output_file):
    combined = AudioSegment.empty()
    for infile in input_files:
        try:
            audio = AudioSegment.from_wav(infile)
            combined += audio
        except Exception as e:
            print(f"无法处理文件 {infile}. 错误: {str(e)}")
    
    combined.export(output_file, format="wav")

# 获取当前目录下所有的WAV文件
wav_files = sorted([f for f in os.listdir('.') if f.endswith('.wav')])

for file in wav_files:
    with open(file, 'rb') as f:
        header = f.read(20)
    print(f"文件 {file} 的前20个字节: {header}")
    
    data = check_file_content(file)
    if data:
        print(f"文件 {file} 包含 JSON 数据")
        for item in data:
            if isinstance(item, dict) and 'download_url' in item and 'filename' in item:
                download_url = item['download_url']
                filename = item['filename']
                print(f"正在下载: {filename}")
                
                # 用户名（JWT token）
                username = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOjEsInR5cGUiOjEwMywic2NvcGUiOiJhZG1pbiIsImlhdCI6MTcyOTQ4NTQ0MywiZXhwIjoxNzMyMDc3NDQzfQ.SavITbhla70iloEusKJd-2wfYueKywNTXSwRpcGPDfQ"
                
                # 创建Basic认证头
                auth_header = base64.b64encode(f"{username}:".encode()).decode()
                
                # 设置请求头
                headers = {
                    "Authorization": f"Basic {auth_header}"
                }
                
                # 下载音频文件
                audio_response = requests.get(download_url, headers=headers)
                
                if audio_response.status_code == 200:
                    with open(filename, "wb") as audio_file:
                        audio_file.write(audio_response.content)
                    print(f"{filename} 下载完成")
                else:
                    print(f"下载 {filename} 失败. 状态码: {audio_response.status_code}")
    else:
        print(f"文件 {file} 不是 JSON 数据，可能是音频文件")

print("文件处理完成")

