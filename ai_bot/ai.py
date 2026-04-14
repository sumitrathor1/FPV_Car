import time

import cv2
import numpy as np
import requests


IMG_URL = "http://20.244.113.234/robot/cam/latest.jpg"
STATE_URL = "http://20.244.113.234/robot/get.php"
SET_URL = "http://20.244.113.234/robot/set.php"


last_mode = None


def get_state():
    try:
        response = requests.get(STATE_URL, timeout=1.2)
        response.raise_for_status()
        return response.json()
    except Exception:
        return None


def send(cmd):
    requests.get(SET_URL, params={"cmd": cmd}, timeout=1.2)


def analyze_frame(image):
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    height, width = gray.shape

    top = int(height * 0.4)
    bottom = int(height * 0.85)
    left = int(width * 0.25)
    right = int(width * 0.75)
    if bottom <= top or right <= left:
        return None

    center = gray[top:bottom, left:right]
    blurred = cv2.GaussianBlur(center, (5, 5), 0)
    edges = cv2.Canny(blurred, 45, 130)

    brightness = float(np.mean(center))
    edge_ratio = float(np.count_nonzero(edges)) / float(edges.size) * 100.0
    score = (edge_ratio * 1.6) + max(0.0, 140.0 - brightness) * 0.4

    return brightness, edge_ratio, score


def process_once():
    global last_mode

    state = get_state()
    if not state or state.get("ai") != "1":
        last_mode = None
        return

    if state.get("cam") != "1":
        if last_mode != "stop":
            send("S")
            last_mode = "stop"
        return

    try:
        response = requests.get(IMG_URL, timeout=1.5)
        response.raise_for_status()

        image_array = np.frombuffer(response.content, np.uint8)
        image = cv2.imdecode(image_array, cv2.IMREAD_COLOR)
        if image is None:
            print("frame decode failed")
            return

        metrics = analyze_frame(image)
        if metrics is None:
            print("invalid roi")
            return

        brightness, edge_ratio, score = metrics
        print(f"brightness={brightness:.1f} edge={edge_ratio:.1f}% score={score:.1f}")

        obstacle = score > 55.0 or brightness < 78.0

        if obstacle:
            if last_mode != "avoid":
                print("obstacle -> stop and turn left")
                send("S")
                time.sleep(0.25)
                send("L")
                last_mode = "avoid"
        else:
            if last_mode != "forward":
                print("clear -> forward")
                send("F")
                last_mode = "forward"

    except Exception as exc:
        print("error:", exc)


def main():
    while True:
        process_once()
        time.sleep(0.2)


if __name__ == "__main__":
    main()