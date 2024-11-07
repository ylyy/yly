from flask import Flask, request, jsonify
import base64
import os
from datetime import datetime

app = Flask(__name__)

# 设置保存音频文件的目录
UPLOAD_FOLDER = 'uploads'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@app.route('/upload_audio', methods=['POST'])
def upload_audio():
    try:
        data = request.json
        if not data:
            print("Error: Request data is not JSON or is empty:", request.data)
            return jsonify({'error': 'Invalid JSON data'}), 400

        if 'chunk' not in data:
            print("Error: No 'chunk' field in the request data:", data)
            return jsonify({'error': 'No audio data provided'}), 400

        # 获取base64编码的音频数据
        base64_audio = data['chunk']
        
        # 解码base64数据
        audio_data = base64.b64decode(base64_audio)
        print(f"音频数据大小: {len(audio_data)} 字节")
        
        # 获取当前时间并格式化为字符串
        timestamp = datetime.now().strftime('%H%M%S')
        
        # 保存音频文件到项目目录下的uploads文件夹，文件名包含时间戳
        file_path = os.path.join(UPLOAD_FOLDER, f'audio_{timestamp}.wav')
        with open(file_path, 'wb') as f:
            f.write(audio_data)
        
        return jsonify({'message': 'Audio received and saved successfully'}), 200
    except Exception as e:
        print("Exception occurred:", str(e))
        print("Request data:", request.data)  # 打印原始请求数据
        return jsonify({'error': 'An error occurred'}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)