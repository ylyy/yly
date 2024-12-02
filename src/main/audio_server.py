from flask import Flask, request, jsonify, send_file, url_for
import base64
import os
import io
import wave
from datetime import datetime
import logging
from pydub import AudioSegment
import uuid

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

# 配置静态文件目录
MP3_DIR = os.path.abspath("temp/tts")
app.config['MP3_DIR'] = MP3_DIR
if not os.path.exists(MP3_DIR):
    os.makedirs(MP3_DIR)

def get_server_info(request=None):
    """获取当前服务器的访问地址和端口"""
    if request:
        # 在请求上下文中使用request对象
        host = request.host.split(':')[0]  # 获取主机地址
        port = request.host.split(':')[1] if ':' in request.host else '5000'  # 获取端口
        scheme = request.scheme  # 获取协议(http/https)
        return f"{scheme}://{host}", port
    else:
        # 在服务启动时使用默认值
        return "http://localhost", "5000"

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

def wav_to_mp3(wav_path):
    """将WAV文件转换为MP3格式"""
    try:
        # 生成日期目录
        date_dir = os.path.join(MP3_DIR, datetime.now().strftime("%Y%m%d"))
        if not os.path.exists(date_dir):
            os.makedirs(date_dir)
            
        # 生成唯一文件名
        mp3_filename = f"{uuid.uuid4().hex[:8]}.mp3"
        mp3_path = os.path.join(date_dir, mp3_filename)
        logger.info(mp3_path)

        # 转换格式
        audio = AudioSegment.from_wav(wav_path)
        audio.export(mp3_path, format="mp3")
        
        return mp3_path
    except Exception as e:
        logger.error(f"WAV转MP3失败: {str(e)}")
        return None

@app.route('/v1/device/voice_to_voice', methods=['POST'])
def receive_audio():
    global current_audio_chunks, current_session_id
    
    start_time = datetime.now()  # 添加开始时间记录
    
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
                # 转换为MP3
                mp3_path = wav_to_mp3(output_path)
                if not mp3_path:
                    return jsonify({
                        "code": 1,
                        "message": "MP3转换失败",
                        "data": None
                    }), 500
                
                # 计算相对路径
                server_url, server_port = get_server_info()
                relative_mp3_path = f"/audio/{datetime.now().strftime('%Y%m%d')}/{os.path.basename(mp3_path)}"
                
                elapsed_time = (datetime.now() - start_time).total_seconds()
                logger.info(f"音频处理成功，总耗时: {elapsed_time:.3f} 秒")
                
                # 清空当前会话
                current_audio_chunks = []
                current_session_id = None
                
                return jsonify({
                    "code": 0,
                    "message": "音频处理成功",
                    "data": {
                        "voice_path": relative_mp3_path,
                        "url": server_url,
                        "port": server_port,
                        "process_time": f"{elapsed_time:.3f}"
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
        elapsed_time = (datetime.now() - start_time).total_seconds()
        logger.info(f"片段理完成，耗时: {elapsed_time:.3f} 秒")
        return jsonify({
            "code": 0,
            "message": "片段接收成功",
            "data": {
                "process_time": f"{elapsed_time:.3f}"
            }
        })
        
    except Exception as e:
        elapsed_time = (datetime.now() - start_time).total_seconds()
        logger.error(f"处理请求时发生错误，耗时: {elapsed_time:.3f} 秒", exc_info=True)
        return jsonify({
            "code": 1,
            "message": f"处理失败: {str(e)}",
            "data": None
        }), 500

# 添加静态文件服务路由
@app.route('/audio/<path:filename>')
def serve_audio(filename):
    """提供MP3文件的访问服务"""
    try:
        # 构建完整的文件路径
        file_path = os.path.join(app.config['MP3_DIR'], filename)
        if os.path.exists(file_path):
            return send_file(file_path, mimetype='audio/mpeg')
        else:
            logger.error(f"文件不存在: {file_path}")
            return jsonify({"code": 1, "message": "文件不存在"}), 404
    except Exception as e:
        logger.error(f"音频文件访问失败: {str(e)}")
        return jsonify({"code": 1, "message": str(e)}), 404

if __name__ == '__main__':
    host, port = get_server_info()  # 不传入request参数
    logger.info(f"服务器启动在: {host}:{port}")
    app.run(host='0.0.0.0', port=5000, debug=True)
