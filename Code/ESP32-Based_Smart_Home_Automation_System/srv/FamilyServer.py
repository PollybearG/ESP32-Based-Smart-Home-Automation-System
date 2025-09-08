from flask import Flask, request
import cv2
import time
import concurrent.futures;
import requests

app = Flask(__name__)

# Telegram 配置
bot_token = "YOUR_TOKEN_HERE"  # bot_token is the access token for Telegram Bot API, It needs the user to insert their own API to build server
CHAT_ID = '-4641405520'

executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)

# 上次触发时间
last_trigger_time = 0
MIN_INTERVAL = 60  # 秒


def record_and_send_video():
    filename="output.mp4"
    cap = cv2.VideoCapture(0)
    fourcc = cv2.VideoWriter_fourcc(*'avc1')  # H264 编码
    out = cv2.VideoWriter(filename, fourcc, 20.0, (640, 480))

    if not out.isOpened():
        print("❌ VideoWriter open failed")
        return

    print("🎥 Starting recoding video...")
    start = time.time()
    while time.time() - start < 10:
        ret, frame = cap.read()
        if not ret:
            break
        out.write(frame)

    cap.release()
    out.release()
    print(f"✅ Video saved: {filename}")
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendVideo"
    with open(filename, "rb") as video_file:
        files = {'video': video_file}
        data = {'chat_id': CHAT_ID, 'caption': "📹 10 Seconds Video From Family Server"}
        response = requests.post(url, data=data, files=files)
        if response.status_code == 200:
            print("✅ Video sent successfully!")
        else:
            print(f"❌ Video sent failed: {response.status_code}, {response.text}")





@app.route("/record", methods=["GET"])
def handle_esp32_request():
    global last_trigger_time
    now = time.time()
    interval = now - last_trigger_time

    if interval > MIN_INTERVAL:
        last_trigger_time = now
        print("🚨 Received request to record video")
        executor.submit(record_and_send_video)
        return "📡 The task has been triggered and is running in the background.", 200
    else:
        print(f"⚠️ Request interval  {interval:.1f}s is too short, ignored")
        return "⚠️ Request is too frequent, ignored.", 200


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080, debug=True)