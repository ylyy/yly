from urllib.request import HTTPBasicAuthHandler
import requests
from requests.auth import HTTPBasicAuth  # 添加这行
import json

# 获取JSON数据的URL
json_url = "http://101.35.160.32:5000/v1/device/download-all"
username="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOjEsInR5cGUiOjEwMywic2NvcGUiOiJhZG1pbiIsImlhdCI6MTcyOTQ4NTQ0MywiZXhwIjoxNzMyMDc3NDQzfQ.SavITbhla70iloEusKJd-2wfYueKywNTXSwRpcGPDfQ"
#username

# 发送GET请求获取JSON数据,用basic认证
response = requests.get(json_url, auth=HTTPBasicAuth(username, ''))
data = response.json()

print("API返回的数据:")
print(json.dumps(data, indent=2, ensure_ascii=False))

# 修改这部分代码来处理列表格式的响应
if isinstance(data, list):
    for item in data:
        if isinstance(item, dict):
            download_url = item.get("download_url")
            filename = item.get("filename")
            
            if download_url and filename:
                # 下载WAV文件,使用Basic认证
                print(f"正在下载: {filename}")
                audio_response = requests.get(download_url, auth=HTTPBasicAuth(username, ''))
                
                # 检查下载是否成功
                if audio_response.status_code == 200:
                    # 将文件保存到本地
                    with open(filename, "wb") as file:
                        file.write(audio_response.content)
                    print(f"{filename} 下载完成")
                else:
                    print(f"下载 {filename} 失败. 状态码: {audio_response.status_code}")
            else:
                print("项目缺少下载URL或文件名")
        else:
            print("无效的项目格式")
else:
    print("API返回的数据不是预期的列表格式")

print("所有文件下载完成")
