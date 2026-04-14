import random
import time

import cv2
import numpy as np
import requests


IMG_URL = "http://20.244.113.234/robot/cam/latest.jpg"
STATE_URL = "http://20.244.113.234/robot/get.php"
SET_URL = "http://20.244.113.234/robot/set.php"

LOOP_SLEEP_SEC = 0.12
CAM_OFF_SLEEP_SEC = 1.0
STOP_PAUSE_SEC = 1.0
TURN_DURATION_SEC = 0.72
TURN_COOLDOWN_SEC = 0.60
OBSTACLE_HOLD_SEC = 2.0
SCORE_THRESHOLD = 36.0
LOW_BRIGHTNESS_THRESHOLD = 58.0
LATENCY_WEIGHT = 0.06
MANUAL_BONUS = 8.0

last_sent_cmd = None
last_publish_at = 0.0
turn_cooldown_until = 0.0
obstacle_hold_until = 0.0
last_seen_cam_off = None


def get_state():
    try:
        response = requests.get(STATE_URL, timeout=1.2)
        response.raise_for_status()
        return response.json()
    except Exception:
        return None


def send(cmd):
    global last_sent_cmd
    if cmd == last_sent_cmd:
        return
    requests.get(SET_URL, params={"cmd": cmd}, timeout=1.2)
    last_sent_cmd = cmd


def publish(payload, force=False):
    global last_publish_at
    now = time.time()
    if not force and now - last_publish_at < 0.25:
        return
    try:
        requests.get(SET_URL, params=payload, timeout=1.2)
        last_publish_at = now
    except Exception:
        pass


def analyze_frame(image):
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    height, width = gray.shape

    top = int(height * 0.36)
    bottom = int(height * 0.88)
    left = int(width * 0.22)
    right = int(width * 0.78)
    if bottom <= top or right <= left:
        return None

    center = gray[top:bottom, left:right]
    clahe = cv2.createCLAHE(clipLimit=2.2, tileGridSize=(8, 8))
    normalized = clahe.apply(center)
    blurred = cv2.GaussianBlur(normalized, (5, 5), 0)

    median = float(np.median(blurred))
    lower = int(max(18.0, 0.66 * median))
    upper = int(min(220.0, 1.35 * median + 18.0))
    if upper <= lower:
        upper = lower + 20

    edges = cv2.Canny(blurred, lower, upper)
    edge_ratio = float(np.count_nonzero(edges)) / float(edges.size) * 100.0
    brightness = float(np.mean(center))
    contrast = float(np.std(center))
    lap_var = float(cv2.Laplacian(normalized, cv2.CV_64F).var())

    dark_cutoff = max(45.0, brightness * 0.72)
    dark_ratio = float(np.mean(center < dark_cutoff)) * 100.0

    score = (
        edge_ratio * 2.2
        + min(lap_var / 18.0, 18.0)
        + contrast * 0.22
        + dark_ratio * 0.12
        + max(0.0, 105.0 - brightness) * 0.12
    )

    return brightness, edge_ratio, contrast, lap_var, dark_ratio, score


def publish_metrics(state, obstacle, action, turn, brightness, edge_ratio, score, latency_ms):
    publish(
        {
            "ai_worker": "1",
            "ai_obstacle": "1" if obstacle else "0",
            "ai_action": action,
            "ai_turn": turn,
            "ai_brightness": f"{brightness:.1f}",
            "ai_edge": f"{edge_ratio:.1f}",
            "ai_score": f"{score:.1f}",
            "ai_latency": str(latency_ms),
        }
    )


def process_once():
    global turn_cooldown_until, obstacle_hold_until, last_seen_cam_off

    state = get_state()
    if not state:
        return LOOP_SLEEP_SEC

    cam_on = state.get("cam") == "1"
    ai_enabled = state.get("ai") == "1"
    manual_cmd = str(state.get("cmd") or "S").upper()

    if not cam_on:
        last_seen_cam_off = last_seen_cam_off or time.time()
        send("S")
        publish(
            {
                "ai_worker": "0",
                "ai_obstacle": "0",
                "ai_action": "CAMERA_OFF",
                "ai_turn": "-",
                "ai_brightness": "0.0",
                "ai_edge": "0.0",
                "ai_score": "0.0",
                "ai_latency": "0",
            },
            force=True,
        )
        return CAM_OFF_SLEEP_SEC

    last_seen_cam_off = None

    try:
        fetch_started = time.time()
        response = requests.get(IMG_URL, timeout=1.6)
        response.raise_for_status()
        latency_ms = int((time.time() - fetch_started) * 1000)

        image_array = np.frombuffer(response.content, np.uint8)
        image = cv2.imdecode(image_array, cv2.IMREAD_COLOR)
        if image is None:
            publish(
                {
                    "ai_worker": "1",
                    "ai_obstacle": "0",
                    "ai_action": "FRAME_DECODE_FAIL",
                    "ai_turn": "-",
                    "ai_brightness": "0.0",
                    "ai_edge": "0.0",
                    "ai_score": "0.0",
                    "ai_latency": str(latency_ms),
                },
                force=True,
            )
            return LOOP_SLEEP_SEC

        metrics = analyze_frame(image)
        if metrics is None:
            publish(
                {
                    "ai_worker": "1",
                    "ai_obstacle": "0",
                    "ai_action": "ROI_INVALID",
                    "ai_turn": "-",
                    "ai_brightness": "0.0",
                    "ai_edge": "0.0",
                    "ai_score": "0.0",
                    "ai_latency": str(latency_ms),
                },
                force=True,
            )
            return LOOP_SLEEP_SEC

        brightness, edge_ratio, contrast, lap_var, dark_ratio, base_score = metrics
        moving_bonus = MANUAL_BONUS if manual_cmd in ("F", "B") else 0.0
        risk_score = base_score + (latency_ms * LATENCY_WEIGHT) + moving_bonus

        raw_obstacle = risk_score >= SCORE_THRESHOLD or brightness <= LOW_BRIGHTNESS_THRESHOLD
        if raw_obstacle:
            obstacle_hold_until = time.time() + OBSTACLE_HOLD_SEC

        obstacle = raw_obstacle or time.time() < obstacle_hold_until

        print(
            f"brightness={brightness:.1f} edge={edge_ratio:.1f}% score={risk_score:.1f} "
            f"latency={latency_ms}ms contrast={contrast:.1f} lap={lap_var:.1f} dark={dark_ratio:.1f}% cmd={manual_cmd}"
        )

        publish_metrics(state, obstacle, "OBSTACLE" if obstacle else "CLEAR", "-", brightness, edge_ratio, risk_score, latency_ms)

        if obstacle:
            send("S")
            if ai_enabled and time.time() >= turn_cooldown_until:
                time.sleep(STOP_PAUSE_SEC)
                turn_cmd = random.choice(["L", "R"])
                send(turn_cmd)
                publish_metrics(state, True, "TURNING", turn_cmd, brightness, edge_ratio, risk_score, latency_ms)
                time.sleep(TURN_DURATION_SEC)
                send("S")
                turn_cooldown_until = time.time() + TURN_COOLDOWN_SEC
            return LOOP_SLEEP_SEC

        if ai_enabled:
            send("F")
            publish_metrics(state, False, "AUTO_FORWARD", "-", brightness, edge_ratio, risk_score, latency_ms)
        else:
            publish_metrics(state, False, "MANUAL_SAFE", "-", brightness, edge_ratio, risk_score, latency_ms)

        return LOOP_SLEEP_SEC

    except Exception as exc:
        print("error:", exc)
        publish(
            {
                "ai_worker": "0",
                "ai_obstacle": "0",
                "ai_action": "ERROR",
                "ai_turn": "-",
                "ai_brightness": "0.0",
                "ai_edge": "0.0",
                "ai_score": "0.0",
                "ai_latency": "0",
            },
            force=True,
        )
        return 0.35


def main():
    while True:
        time.sleep(process_once())


if __name__ == "__main__":
    main()