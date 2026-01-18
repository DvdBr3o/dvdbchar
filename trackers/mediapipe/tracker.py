import cv2
import mediapipe as mp
from mediapipe.tasks.python import vision, BaseOptions
from mediapipe.tasks.python.vision import RunningMode as VisionRunningMode
import time
import numpy as np
import threading
import argparse
from oscpy.client import OSCClient

# argparse
parser = argparse.ArgumentParser()
parser.add_argument("--port")
parser.add_argument("--show")
args = parser.parse_args()

# configs
show_video = True
osc_port = 15784

# osc client
client = OSCClient("127.0.0.1", osc_port)

# mediapipe
latest_result = None
latest_image = None
data_lock = threading.Lock()

def debug_draw_landmarks(np_img, result):
    if not result or not result.face_landmarks:
        return np_img
    client.send_message(b'/', [b'hello!'])
    h, w, _ = np_img.shape
    for face_landmarks in result.face_landmarks:
        for i, landmark in enumerate(face_landmarks):
            cx, cy = int(landmark.x * w), int(landmark.y * h)
            z_val = landmark.z
            radius = 2
            color_val = int(np.clip((z_val + 0.1) * 1275, 0, 255))
            color = (color_val, 0, 255 - color_val) 
            cv2.circle(np_img, (cx, cy), radius, color, -1)
    return np_img

def print_result(result: vision.FaceLandmarkerResult, output_image: mp.Image, timestamp_ms: int): # type: ignore
    global latest_result, latest_image
    with data_lock:
        latest_result = result
        latest_image = output_image.numpy_view().copy()

cap = cv2.VideoCapture(0)
model_path = './face_landmarker.task'
options = vision.FaceLandmarkerOptions(
    base_options=BaseOptions(model_asset_path=model_path),
    running_mode=VisionRunningMode.LIVE_STREAM,
    result_callback=print_result
)

with vision.FaceLandmarker.create_from_options(options) as landmarker:
    start_ns = time.monotonic_ns()

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret: break

        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
        timestamp_ms = (time.monotonic_ns() - start_ns) // 1_000_000
        landmarker.detect_async(mp_image, timestamp_ms)

        display_frame = frame.copy()
        with data_lock:
            if latest_result is not None:
                display_frame = debug_draw_landmarks(display_frame, latest_result)

        cv2.imshow('Face Tracker Debug', display_frame)
        
        if cv2.waitKey(1) == ord('q'):
            break

cap.release()
cv2.destroyAllWindows()
