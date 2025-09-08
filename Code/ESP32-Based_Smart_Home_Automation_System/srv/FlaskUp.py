from flask import Flask, request
import requests
from io import BytesIO
import time
import cv2
from concurrent.futures import ThreadPoolExecutor, as_completed
import uuid
import os

app = Flask(__name__)
# url = f"https://api.telegram.org/bot{bot_token}/getUpdates"
# res = requests.get(url)
# print(res.json())
bot_token = "YOUR_TOKEN_HERE"  # bot_token is the access token for Telegram Bot API, It needs the user to insert their own API to build server
#CHAT_IDs =[ "7533611600", "5597232596", "7579687369"] #Me, Fan, Han
CHAT_IDs = ["-4641405520"]  # Group Chat ID
#CHAT_IDs = ["7533611600"]  # Group Chat ID

def record_and_send_video(chat_id, stream_url, duration_sec=10, output_path=None):
    try:
        # 为每个 chat_id 生成唯一的临时文件名
        if output_path is None:
            output_path = f"esp32_video_{uuid.uuid4().hex}.mp4"

        # 打开视频流
        cap = cv2.VideoCapture(stream_url)
        if not cap.isOpened():
            print(f"❌ 无法连接到 ESP32 视频流: {stream_url}")
            return False, f"无法连接到视频流: {stream_url}"

        # 获取分辨率和帧率
        width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        fps = cap.get(cv2.CAP_PROP_FPS)

        # 如果获取失败，使用默认值
        if width <= 0 or height <= 0:
            width, height = 320, 240  # ESP32-CAM 常用分辨率
        if fps <= 0:
            fps = 5  # 默认 5 fps，ESP32-CAM 常见帧率

        # 初始化视频写入器
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        out = cv2.VideoWriter(output_path, fourcc, fps, (width, height))
        if not out.isOpened():
            cap.release()
            print(f"❌ 无法创建视频文件: {output_path}")
            return False, f"无法创建视频文件: {output_path}"

        print(f"🎥 正在录制 {duration_sec} 秒视频来自 {stream_url} (分辨率: {width}x{height}, 帧率: {fps})")
        start_time = time.time()
        max_retries = 3
        retry_delay = 2  # 每次重试间隔 2 秒

        while (time.time() - start_time) < duration_sec:
            for attempt in range(max_retries):
                ret, frame = cap.read()
                if ret:
                    out.write(frame)
                    break
                else:
                    print(f"⚠️ 读取视频帧失败，尝试 {attempt + 1}/{max_retries}")
                    if attempt < max_retries - 1:
                        time.sleep(retry_delay)
                        # 尝试重新连接
                        cap.release()
                        cap = cv2.VideoCapture(stream_url)
                        if not cap.isOpened():
                            print(f"❌ 重连失败: {stream_url}")
                            out.release()
                            return False, f"重连视频流失败: {stream_url}"
                    else:
                        print("❌ 无法读取视频帧，放弃录制")
                        cap.release()
                        out.release()
                        return False, "无法读取视频帧"

        cap.release()
        out.release()
        print(f"✅ 视频录制完成: {output_path}")

        # 发送视频到 Telegram
        max_retries = 3
        for attempt in range(max_retries):
            try:
                with open(output_path, 'rb') as video_file:
                    resp = requests.post(
                        f"https://api.telegram.org/bot{BOT_TOKEN}/sendVideo",
                        data={'chat_id': chat_id, 'caption': f'🎬 ESP32 {stream_url} 的视频'},
                        files={'video': ('video.mp4', video_file, 'video/mp4')},
                        timeout=20
                    )

                if resp.status_code == 200:
                    print(f"✅ 成功发送视频到 Chat ID {chat_id}")
                    break
                elif resp.status_code == 429:  # Too Many Requests
                    retry_after = resp.json().get('parameters', {}).get('retry_after', 5)
                    print(f"⚠️ Chat ID {chat_id} 尝试 {attempt + 1}/{max_retries} 失败: 速率限制，等待 {retry_after} 秒")
                    time.sleep(retry_after)
                else:
                    print(f"⚠️ Chat ID {chat_id} 尝试 {attempt + 1}/{max_retries} 失败: {resp.status_code} {resp.text}")
                    if attempt == max_retries - 1:
                        return False, f"发送视频失败: {resp.text}"
                    time.sleep(2)
            except requests.exceptions.RequestException as e:
                print(f"⚠️ Chat ID {chat_id} 尝试 {attempt + 1}/{max_retries} 失败: {str(e)}")
                if attempt == max_retries - 1:
                    return False, f"发送视频失败: {str(e)}"
                time.sleep(2)

        return True, "成功发送视频"
    except Exception as e:
        return False, f"发生错误: {str(e)}"
    finally:
        # 清理临时文件
        if os.path.exists(output_path):
            try:
                os.remove(output_path)
                print(f"🗑️ 已删除临时文件: {output_path}")
            except Exception as e:
                print(f"⚠️ 删除临时文件失败: {str(e)}")

@app.route('/record', methods=['POST'])
def record():
    # 获取 ESP32 的 IP
    esp32_ip = request.remote_addr
    stream_url = f"http://{esp32_ip}:81/stream"
    print(f"📡 收到录制请求，视频流 URL: {stream_url}")

    # 使用线程池处理多个 chat_id
    success_count = 0
    error_messages = []
    with ThreadPoolExecutor(max_workers=3) as executor:
        future_to_chat_id = {
            executor.submit(record_and_send_video, chat_id, stream_url): chat_id
            for chat_id in CHAT_IDs
        }
        for future in as_completed(future_to_chat_id):
            chat_id = future_to_chat_id[future]
            try:
                success, message = future.result()
                if success:
                    success_count += 1
                else:
                    error_messages.append(f"Chat ID {chat_id}: {message}")
            except Exception as e:
                error_messages.append(f"Chat ID {chat_id}: 线程执行失败: {str(e)}")

    # 等待所有线程完成后再返回响应
    if success_count == len(CHAT_IDs):
        return f"✅ 成功录制并发送视频到 {success_count}/{len(CHAT_IDs)} 个 chat_id!", 200
    else:
        return f"⚠️ 部分成功，录制并发送到 {success_count}/{len(CHAT_IDs)} 个 chat_id. 错误: {'; '.join(error_messages)}", 500

@app.route('/upload', methods=['POST'])
def upload():
    if 'photo' not in request.files:
        return "No photo part", 400

    photo = request.files['photo']
    if not photo:
        return "Photo is empty", 400

    caption = request.form.get('caption', '来自ESP32的照片')

    # 读取照片数据到内存（避免重复读取文件流）
    photo_data = photo.read()
    photo_stream = BytesIO(photo_data)
    photo_filename = photo.filename
    photo_mimetype = photo.mimetype

    telegram_url = f'https://api.telegram.org/bot{BOT_TOKEN}/sendPhoto'
    success_count = 0
    error_messages = []

    # 向每个 chat_id 发送照片
    for chat_id in CHAT_IDs:
        max_retries = 3
        for attempt in range(max_retries):
            try:
                # 重置文件流位置
                photo_stream.seek(0)
                response = requests.post(
                    telegram_url,
                    data={'chat_id': chat_id, 'caption': caption},
                    files={'photo': (photo_filename, photo_stream, photo_mimetype)},
                    timeout=10
                )

                if response.status_code == 200:
                    success_count += 1
                    break
                elif response.status_code == 429:  # Too Many Requests
                    retry_after = response.json().get('parameters', {}).get('retry_after', 5)
                    error_messages.append(
                        f"Chat ID {chat_id} 尝试 {attempt + 1}/{max_retries} 失败: 速率限制，等待 {retry_after} 秒")
                    time.sleep(retry_after)
                else:
                    error_messages.append(f"Chat ID {chat_id} 尝试 {attempt + 1}/{max_retries} 失败: {response.text}")
                    if attempt == max_retries - 1:
                        error_messages.append(f"Chat ID {chat_id} 最终失败")
                    time.sleep(2)  # 每次失败后等待 2 秒
            except requests.exceptions.RequestException as e:
                error_messages.append(f"Chat ID {chat_id} 尝试 {attempt + 1}/{max_retries} 失败: {str(e)}")
                if attempt == max_retries - 1:
                    error_messages.append(f"Chat ID {chat_id} 最终失败")
                time.sleep(2)  # 每次失败后等待 2 秒

        time.sleep(5)  # 每个 chat_id 之间等待 5 秒，避免触发 Telegram 速率限制

    # 关闭文件流
    photo_stream.close()

    # 返回结果
    if success_count == len(CHAT_IDs):
        return f"✅ 成功发送到 {success_count}/{len(CHAT_IDs)} 个 chat_id!", 200
    else:
        return f"⚠️ 部分成功，发送到 {success_count}/{len(CHAT_IDs)} 个 chat_id. 错误: {'; '.join(error_messages)}", 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080, debug=True)