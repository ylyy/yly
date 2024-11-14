from flask import Flask, request, jsonify
import base64
import os
import io
import wave
from datetime import datetime
import logging

app = Flask(__name__)

# 配置日志
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

# 存储音频片段的目录
AUDIO_DIR = "audio_chunks"
if not os.path.exists(AUDIO_DIR):
    os.makedirs(AUDIO_DIR)

# 存储当前正在处理的音频片段
current_audio_chunks = []
current_session_id = None

def combine_wav_files(chunks, output_path):
    """将多个WAV音频片段组合成一个文件"""
    if not chunks:
        logger.warning("没有音频片段可以组合")
        return False
    
    try:
        # 首先保存原始数据用于调试
        debug_dir = "debug_chunks"
        if not os.path.exists(debug_dir):
            os.makedirs(debug_dir)
            
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        # 保存每个原始chunk
        for i, chunk in enumerate(chunks):
            chunk_path = os.path.join(debug_dir, f"chunk_{timestamp}_{i}.raw")
            with open(chunk_path, 'wb') as f:
                f.write(chunk)
            logger.info(f"保存原始chunk {i}: {len(chunk)} 字节到 {chunk_path}")
            
            # 打印chunk的十六进制内容（前100字节）
            logger.info(f"Chunk {i} 头部100字节:")
            logger.info(chunk[:100].hex())
            
        # 继续原来的处理逻辑
        all_audio_data = []
        total_frames = 0
        
        for i, chunk in enumerate(chunks):
            try:
                chunk_stream = io.BytesIO(chunk)
                with wave.open(chunk_stream, 'rb') as wav_chunk:
                    if i == 0:
                        params = wav_chunk.getparams()
                        logger.info(f"第一个chunk的参数:")
                        logger.info(f"  - 采样率: {params.framerate} Hz")
                        logger.info(f"  - 声道数: {params.nchannels}")
                        logger.info(f"  - 采样宽度: {params.sampwidth} 字节")
                        logger.info(f"  - 压缩类型: {params.comptype}")
                        logger.info(f"  - 压缩名称: {params.compname}")
                    
                    frames = wav_chunk.getnframes()
                    audio_data = wav_chunk.readframes(frames)
                    
                    # 保存提取的音频数据
                    audio_path = os.path.join(debug_dir, f"audio_data_{timestamp}_{i}.raw")
                    with open(audio_path, 'wb') as f:
                        f.write(audio_data)
                    logger.info(f"保存音频数据 {i}: {len(audio_data)} 字节到 {audio_path}")
                    
                    all_audio_data.append(audio_data)
                    total_frames += frames
                    
            except Exception as e:
                logger.error(f"处理第 {i+1} 个音频片段时出错: {str(e)}")
                logger.error(f"错误详情:", exc_info=True)
                return False
        
        # 保存合并后的原始音频数据
        combined_raw = b''.join(all_audio_data)
        combined_raw_path = os.path.join(debug_dir, f"combined_{timestamp}.raw")
        with open(combined_raw_path, 'wb') as f:
            f.write(combined_raw)
        logger.info(f"保存合并后的原始音频数据: {len(combined_raw)} 字节到 {combined_raw_path}")
        
        # 写入最终WAV文件
        with wave.open(output_path, 'wb') as output_wav:
            output_wav.setparams(params)
            output_wav.writeframes(combined_raw)
            
        logger.info(f"写入最终WAV文件: {output_path}")
        
        # 验证最终文件
        with wave.open(output_path, 'rb') as final_wav:
            final_frames = final_wav.getnframes()
            final_data = final_wav.readframes(final_frames)
            logger.info(f"最终WAV文件信息:")
            logger.info(f"  - 总帧数: {final_frames}")
            logger.info(f"  - 总数据大小: {len(final_data)} 字节")
            logger.info(f"  - 预期帧数: {total_frames}")
            logger.info(f"  - 预期数据大小: {total_frames * params.sampwidth * params.nchannels} 字节")
        
        return True
        
    except Exception as e:
        logger.error(f"组合音频文件时出错: {str(e)}", exc_info=True)
        return False

@app.route('/v1/device/voice_to_voice', methods=['POST'])
def receive_audio():
    global current_audio_chunks, current_session_id
    
    try:
        logger.info("收到新的音频请求")
        logger.info(f"请求头: {dict(request.headers)}")
        
        if not request.is_json:
            logger.error("请求不是JSON格式")
            return jsonify({"code": 1, "message": "需要JSON格式的请求"}), 400
        
        data = request.json
        logger.info(f"请求数据: {data.keys()}")
        
        status = data.get('status', 0)
        audio_base64 = data.get('chunk', '')
        
        logger.info(f"状态码: {status}")
        logger.info(f"Base64数据长度: {len(audio_base64)}")
        
        try:
            # 解码音频数据
            audio_data = base64.b64decode(audio_base64)
            logger.info(f"解码后的音频数据大小: {len(audio_data)} 字节")
            
            # 详细分析WAV头部
            riff = audio_data[0:4].decode('ascii')
            file_size = int.from_bytes(audio_data[4:8], 'little')
            wave_format = audio_data[8:12].decode('ascii')
            fmt = audio_data[12:16].decode('ascii')
            
            logger.info(f"WAV头部分析:")
            logger.info(f"RIFF标识: {riff}")
            logger.info(f"文件大小: {file_size} 字节")
            logger.info(f"WAVE标识: {wave_format}")
            logger.info(f"fmt 标识: {fmt}")
            
            # 验证是否为有效的WAV格式
            try:
                test_stream = io.BytesIO(audio_data)
                with wave.open(test_stream, 'rb') as test_wav:
                    params = test_wav.getparams()
                    actual_frames = test_wav.getnframes()
                    actual_audio_data = test_wav.readframes(actual_frames)
                    logger.info(f"实际音频数据:")
                    logger.info(f"帧数: {actual_frames}")
                    logger.info(f"音频数据大小: {len(actual_audio_data)} 字节")
                    logger.info(f"理论数据大小: {actual_frames * params.sampwidth * params.nchannels} 字节")
            except wave.Error as e:
                logger.error(f"无效的WAV格式: {str(e)}")
                return jsonify({"code": 1, "message": "无效的WAV格式"}), 400
                
        except Exception as e:
            logger.error(f"Base64解码或数据分析失败: {str(e)}")
            return jsonify({"code": 1, "message": "数据处理失败"}), 400
        
        # 将音频片段添加到当前会话
        current_audio_chunks.append(audio_data)
        logger.info(f"当前累积的音频片段数: {len(current_audio_chunks)}")
        
        # 如果是最后一个片段，组合所有音频
        if status == 2:
            logger.info("收到最后一个音频片段，开始组合")
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_path = os.path.join(AUDIO_DIR, f"combined_{timestamp}.wav")
            
            if combine_wav_files(current_audio_chunks, output_path):
                logger.info("音频组合成功")
                # 清空当前会话
                current_audio_chunks = []
                current_session_id = None
                
                return jsonify({
                    "code": 0,
                    "message": "音频处理成功",
                    "data": {
                        "file_path": output_path
                    }
                })
            else:
                logger.error("音频组合失败")
                return jsonify({
                    "code": 1,
                    "message": "音频组合失败",
                    "data": None
                }), 500
        
        # 如果不是最后一个片段，返回继续接收的响应
        logger.info("成功接收音频片段")
        return jsonify({
            "code": 0,
            "message": "片段接收成功",
            "data": None
        })
        
    except Exception as e:
        logger.error(f"处理请求时发生错误: {str(e)}", exc_info=True)
        return jsonify({
            "code": 1,
            "message": f"处理失败: {str(e)}",
            "data": None
        }), 500

if __name__ == '__main__':
    logger.info("服务器启动在 http://0.0.0.0:5000")
    app.run(host='0.0.0.0', port=5000, debug=True)