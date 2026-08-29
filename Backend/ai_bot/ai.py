"""
FPV Car - AI Vision Analysis Bot
Fetches the latest camera frame from the server, runs OpenCV-based
obstacle detection (edge density, brightness, near-field analysis),
and publishes real-time telemetry metrics back to the dashboard.

Requires: Python 3.8+, opencv-python-headless, numpy, requests
"""

import random
import time
from collections import deque

import cv2
import numpy as np
import requests


# ======================================================
# Server Endpoints (Updated for /fpv_car/)
# ======================================================
IMG_URL   = "http://20.244.113.234/fpv_car/cam/latest.jpg"
STATE_URL = "http://20.244.113.234/fpv_car/get.php"
SET_URL   = "http://20.244.113.234/fpv_car/set.php"

# ======================================================
# Timing & Thresholds
# ======================================================
LOOP_SLEEP_SEC          = 0.12
CAM_OFF_SLEEP_SEC       = 1.0
STOP_PAUSE_SEC          = 1.0
TURN_COOLDOWN_SEC       = 0.60
OBSTACLE_HOLD_SEC       = 0.75
SCORE_THRESHOLD         = 46.0
NEAR_SCORE_THRESHOLD    = 41.0
NEAR_CONFIRM_THRESHOLD  = 26.0
LOW_BRIGHTNESS_THRESHOLD= 58.0
LATENCY_WEIGHT          = 0.02
MANUAL_BONUS            = 3.0
TURN_90_SEC             = 0.70
TURN_ANGLE_CHOICES      = [45, 50, 90, 155, 180]
REVERSE_MIN_SEC         = 0.22
REVERSE_MAX_SEC         = 0.45
TURN_LOOP_WINDOW_SEC    = 8.0
TURN_LOOP_MIN_COUNT     = 4
CLOSE_TRIGGER_FRAMES    = 2
LOOP_ESCAPE_CONFIRM     = 2
CAMERA_DELAY_SEC        = 2.0
FORWARD_NEAR_THRESHOLD  = 33.0
FORWARD_RISK_THRESHOLD  = 38.0
PREDICT_GAIN_NEAR       = 1.0
PREDICT_GAIN_RISK       = 0.85
EARLY_STOP_HOLD_SEC     = 1.2
EMA_ALPHA               = 0.35
VERY_CLOSE_THRESHOLD    = 48.0
PERSIST_OBSTACLE_REVERSE= 3
TURN_PULSE_SEC          = 0.14
TURN_PAUSE_SEC          = 0.08
FORCED_REVERSE_MIN_SEC  = 0.25
FORCED_REVERSE_MAX_SEC  = 0.50

# ======================================================
# Global State
# ======================================================
last_sent_cmd        = None
last_publish_at      = 0.0
turn_cooldown_until  = 0.0
obstacle_hold_until  = 0.0
last_seen_cam_off    = None
recent_turns         = deque(maxlen=8)
close_hit_streak     = 0
loop_detect_streak   = 0
prev_sample_at       = None
prev_near_score      = None
prev_risk_score      = None
near_ema             = None
risk_ema             = None
obstacle_cycle_count = 0
stop_stuck_since     = None


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
    try:
        requests.get(SET_URL, params={"cmd": cmd}, timeout=1.2)
        last_sent_cmd = cmd
    except Exception:
        pass


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


# ======================================================
# Core Vision Analysis (OpenCV)
# ======================================================
def analyze_frame(image):
    """
    Analyses a BGR camera frame and returns:
      (brightness, edge_ratio, contrast, lap_var, dark_ratio,
       score, near_score, very_close_score)

    Uses CLAHE normalization, Canny edge detection, and
    multi-zone ROI analysis (center, near-field, very-near-field).
    """
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    height, width = gray.shape

    top    = int(height * 0.36)
    bottom = int(height * 0.88)
    left   = int(width * 0.22)
    right  = int(width * 0.78)
    if bottom <= top or right <= left:
        return None

    center     = gray[top:bottom, left:right]
    clahe      = cv2.createCLAHE(clipLimit=2.2, tileGridSize=(8, 8))
    normalized = clahe.apply(center)
    blurred    = cv2.GaussianBlur(normalized, (5, 5), 0)

    median = float(np.median(blurred))
    lower  = int(max(18.0, 0.66 * median))
    upper  = int(min(220.0, 1.35 * median + 18.0))
    if upper <= lower:
        upper = lower + 20

    edges      = cv2.Canny(blurred, lower, upper)
    edge_ratio = float(np.count_nonzero(edges)) / float(edges.size) * 100.0
    brightness = float(np.mean(center))
    contrast   = float(np.std(center))
    lap_var    = float(cv2.Laplacian(normalized, cv2.CV_64F).var())

    dark_cutoff = max(45.0, brightness * 0.72)
    dark_ratio  = float(np.mean(center < dark_cutoff)) * 100.0

    score = (
        edge_ratio * 2.2
        + min(lap_var / 18.0, 18.0)
        + contrast * 0.22
        + dark_ratio * 0.12
        + max(0.0, 105.0 - brightness) * 0.12
    )

    # Near-field zone (bottom 42% of ROI)
    near         = center[int(center.shape[0] * 0.58):, :]
    near_blurred = cv2.GaussianBlur(near, (5, 5), 0)
    near_edges   = cv2.Canny(near_blurred, lower, upper)
    near_edge_ratio  = float(np.count_nonzero(near_edges)) / float(near_edges.size) * 100.0
    near_brightness  = float(np.mean(near))
    near_dark_cutoff = max(40.0, near_brightness * 0.72)
    near_dark_ratio  = float(np.mean(near < near_dark_cutoff)) * 100.0
    near_score = (
        near_edge_ratio * 2.8
        + near_dark_ratio * 0.22
        + max(0.0, 95.0 - near_brightness) * 0.18
        + min(lap_var / 25.0, 10.0)
    )

    # Very-near-field zone (bottom 22% of ROI)
    very_near         = center[int(center.shape[0] * 0.78):, :]
    very_near_blurred = cv2.GaussianBlur(very_near, (5, 5), 0)
    very_near_edges   = cv2.Canny(very_near_blurred, lower, upper)
    very_near_edge_ratio = (
        float(np.count_nonzero(very_near_edges)) / float(very_near_edges.size) * 100.0
    )
    very_near_brightness  = float(np.mean(very_near))
    very_near_dark_cutoff = max(38.0, very_near_brightness * 0.75)
    very_near_dark_ratio  = float(np.mean(very_near < very_near_dark_cutoff)) * 100.0
    very_close_score = (
        very_near_edge_ratio * 3.2
        + very_near_dark_ratio * 0.28
        + max(0.0, 90.0 - very_near_brightness) * 0.24
    )

    return (
        brightness, edge_ratio, contrast, lap_var, dark_ratio,
        score, near_score, very_close_score,
    )


# ======================================================
# Turn Loop Detection & Escape
# ======================================================
def register_turn(turn_cmd):
    now = time.time()
    recent_turns.append((now, turn_cmd))
    while recent_turns and now - recent_turns[0][0] > TURN_LOOP_WINDOW_SEC:
        recent_turns.popleft()


def turn_loop_detected():
    now = time.time()
    while recent_turns and now - recent_turns[0][0] > TURN_LOOP_WINDOW_SEC:
        recent_turns.popleft()
    if len(recent_turns) < TURN_LOOP_MIN_COUNT:
        return False
    left_count  = sum(1 for _, cmd in recent_turns if cmd == "L")
    right_count = sum(1 for _, cmd in recent_turns if cmd == "R")
    return abs(left_count - right_count) <= 1


def pick_turn_profile():
    angle    = random.choice(TURN_ANGLE_CHOICES)
    scale    = random.uniform(0.9, 1.15)
    duration = TURN_90_SEC * (angle / 90.0) * scale
    return angle, max(0.28, min(1.85, duration))


def execute_turn_pulse(turn_cmd, duration):
    started = time.time()
    while time.time() - started < duration:
        send(turn_cmd)
        time.sleep(TURN_PULSE_SEC)
        send("S")
        time.sleep(TURN_PAUSE_SEC)


# ======================================================
# Telemetry Publisher
# ======================================================
def publish_metrics(
    state, obstacle, action, turn, brightness, edge_ratio,
    score, latency_ms, near_score=0.0, near_ema_value=0.0,
    very_close_score=0.0, pred_near=0.0, pred_risk=0.0,
    hit=0, cmd="S",
):
    publish({
        "ai_worker":    "1",
        "ai_obstacle":  "1" if obstacle else "0",
        "ai_action":    action,
        "ai_turn":      turn,
        "ai_brightness": f"{brightness:.1f}",
        "ai_edge":      f"{edge_ratio:.1f}",
        "ai_near":      f"{near_score:.1f}",
        "ai_near_ema":  f"{near_ema_value:.1f}",
        "ai_vclose":    f"{very_close_score:.1f}",
        "ai_pred_near": f"{pred_near:.1f}",
        "ai_pred_risk": f"{pred_risk:.1f}",
        "ai_hit":       str(int(hit)),
        "ai_cmd":       cmd,
        "ai_score":     f"{score:.1f}",
        "ai_latency":   str(latency_ms),
    })


# ======================================================
# Main Processing Loop (One Iteration)
# ======================================================
def process_once():
    global turn_cooldown_until, obstacle_hold_until, last_seen_cam_off
    global close_hit_streak, loop_detect_streak
    global prev_sample_at, prev_near_score, prev_risk_score
    global near_ema, risk_ema, obstacle_cycle_count
    global stop_stuck_since

    state = get_state()
    if not state:
        return LOOP_SLEEP_SEC

    cam_on     = state.get("cam") == "1"
    ai_enabled = state.get("ai") == "1"
    manual_cmd = str(state.get("cmd") or "S").upper()

    # Camera OFF → zero-load idle
    if not cam_on:
        last_seen_cam_off = last_seen_cam_off or time.time()
        send("S")
        publish({
            "ai_worker": "0", "ai_obstacle": "0", "ai_action": "CAMERA_OFF",
            "ai_turn": "-", "ai_brightness": "0.0", "ai_edge": "0.0",
            "ai_near": "0.0", "ai_near_ema": "0.0", "ai_vclose": "0.0",
            "ai_pred_near": "0.0", "ai_pred_risk": "0.0", "ai_hit": "0",
            "ai_cmd": "S", "ai_score": "0.0", "ai_latency": "0",
        }, force=True)
        return CAM_OFF_SLEEP_SEC

    last_seen_cam_off = None

    try:
        # Fetch frame from server
        fetch_started = time.time()
        response = requests.get(IMG_URL, timeout=1.6)
        response.raise_for_status()
        latency_ms = int((time.time() - fetch_started) * 1000)

        image_array = np.frombuffer(response.content, np.uint8)
        image = cv2.imdecode(image_array, cv2.IMREAD_COLOR)
        if image is None:
            publish({
                "ai_worker": "1", "ai_obstacle": "0", "ai_action": "FRAME_DECODE_FAIL",
                "ai_turn": "-", "ai_brightness": "0.0", "ai_edge": "0.0",
                "ai_near": "0.0", "ai_near_ema": "0.0", "ai_vclose": "0.0",
                "ai_pred_near": "0.0", "ai_pred_risk": "0.0", "ai_hit": "0",
                "ai_cmd": manual_cmd, "ai_score": "0.0", "ai_latency": str(latency_ms),
            }, force=True)
            return LOOP_SLEEP_SEC

        # Run vision analysis
        metrics = analyze_frame(image)
        if metrics is None:
            publish({
                "ai_worker": "1", "ai_obstacle": "0", "ai_action": "ROI_INVALID",
                "ai_turn": "-", "ai_brightness": "0.0", "ai_edge": "0.0",
                "ai_near": "0.0", "ai_near_ema": "0.0", "ai_vclose": "0.0",
                "ai_pred_near": "0.0", "ai_pred_risk": "0.0", "ai_hit": "0",
                "ai_cmd": manual_cmd, "ai_score": "0.0", "ai_latency": str(latency_ms),
            }, force=True)
            return LOOP_SLEEP_SEC

        (
            brightness, edge_ratio, contrast, lap_var, dark_ratio,
            base_score, near_score, very_close_score,
        ) = metrics

        moving_bonus = MANUAL_BONUS if manual_cmd in ("F", "B") else 0.0
        risk_score   = base_score + (latency_ms * LATENCY_WEIGHT) + moving_bonus

        # EMA smoothing
        if near_ema is None:
            near_ema = near_score
        else:
            near_ema = (EMA_ALPHA * near_score) + ((1.0 - EMA_ALPHA) * near_ema)

        if risk_ema is None:
            risk_ema = risk_score
        else:
            risk_ema = (EMA_ALPHA * risk_score) + ((1.0 - EMA_ALPHA) * risk_ema)

        # Predictive scoring
        now = time.time()
        dt  = LOOP_SLEEP_SEC if prev_sample_at is None else max(0.05, now - prev_sample_at)

        near_rate = 0.0
        risk_rate = 0.0
        if prev_near_score is not None:
            near_rate = (near_ema - prev_near_score) / dt
        if prev_risk_score is not None:
            risk_rate = (risk_ema - prev_risk_score) / dt

        pred_near = near_ema + max(0.0, near_rate) * CAMERA_DELAY_SEC * PREDICT_GAIN_NEAR
        pred_risk = risk_ema + max(0.0, risk_rate) * CAMERA_DELAY_SEC * PREDICT_GAIN_RISK

        prev_sample_at  = now
        prev_near_score = near_ema
        prev_risk_score = risk_ema

        forward_intent = manual_cmd == "F" or ai_enabled

        # Obstacle detection (multi-signal fusion)
        near_hit = near_score >= NEAR_SCORE_THRESHOLD
        confirmed_hit = (
            risk_score >= SCORE_THRESHOLD
            and near_score >= NEAR_CONFIRM_THRESHOLD
            and edge_ratio >= 1.2
        )
        predictive_hit = (
            forward_intent
            and (pred_near >= FORWARD_NEAR_THRESHOLD or pred_risk >= FORWARD_RISK_THRESHOLD)
        )
        emergency_close = very_close_score >= VERY_CLOSE_THRESHOLD

        raw_obstacle = near_hit or confirmed_hit or predictive_hit or emergency_close
        if brightness <= LOW_BRIGHTNESS_THRESHOLD and near_score >= NEAR_CONFIRM_THRESHOLD:
            raw_obstacle = True

        if raw_obstacle:
            close_hit_streak = min(
                close_hit_streak + (2 if predictive_hit or emergency_close else 1), 8
            )
            hold_for = EARLY_STOP_HOLD_SEC if predictive_hit else OBSTACLE_HOLD_SEC
            obstacle_hold_until = time.time() + hold_for
        else:
            close_hit_streak = max(0, close_hit_streak - 1)

        trigger_frames = 1 if forward_intent else CLOSE_TRIGGER_FRAMES
        obstacle = close_hit_streak >= trigger_frames or (
            close_hit_streak > 0 and time.time() < obstacle_hold_until
        )

        # Console log
        print(
            f"brightness={brightness:.1f} edge={edge_ratio:.1f}% score={risk_score:.1f} "
            f"near={near_score:.1f} nearEma={near_ema:.1f} vClose={very_close_score:.1f} "
            f"predNear={pred_near:.1f} predRisk={pred_risk:.1f} hit={close_hit_streak} "
            f"latency={latency_ms}ms cmd={manual_cmd}"
        )

        # Publish telemetry to dashboard
        publish_metrics(
            state, obstacle,
            "OBSTACLE" if obstacle else "CLEAR", "-",
            brightness, edge_ratio, risk_score, latency_ms,
            near_score=near_score, near_ema_value=near_ema,
            very_close_score=very_close_score,
            pred_near=pred_near, pred_risk=pred_risk,
            hit=close_hit_streak, cmd=manual_cmd,
        )

        # AI-controlled driving
        if obstacle:
            obstacle_cycle_count += 1
            send("S")

            if ai_enabled and time.time() >= turn_cooldown_until:
                if turn_loop_detected():
                    loop_detect_streak += 1
                else:
                    loop_detect_streak = 0

                must_reverse = (
                    loop_detect_streak >= LOOP_ESCAPE_CONFIRM
                    or obstacle_cycle_count >= PERSIST_OBSTACLE_REVERSE
                )

                if must_reverse:
                    reverse_sec = random.uniform(REVERSE_MIN_SEC, REVERSE_MAX_SEC)
                    send("B")
                    publish_metrics(
                        state, True, "ESCAPE_REVERSE", "-",
                        brightness, edge_ratio, risk_score, latency_ms,
                        near_score=near_score, near_ema_value=near_ema,
                        very_close_score=very_close_score,
                        pred_near=pred_near, pred_risk=pred_risk,
                        hit=close_hit_streak, cmd="B",
                    )
                    time.sleep(reverse_sec)
                    send("S")
                    time.sleep(0.2)
                    loop_detect_streak   = 0
                    obstacle_cycle_count = 0

                time.sleep(STOP_PAUSE_SEC)
                turn_cmd = random.choice(["L", "R"])
                turn_angle, turn_duration = pick_turn_profile()
                publish_metrics(
                    state, True, f"TURN_{turn_cmd}_{turn_angle}", turn_cmd,
                    brightness, edge_ratio, risk_score, latency_ms,
                    near_score=near_score, near_ema_value=near_ema,
                    very_close_score=very_close_score,
                    pred_near=pred_near, pred_risk=pred_risk,
                    hit=close_hit_streak, cmd=turn_cmd,
                )
                register_turn(turn_cmd)
                execute_turn_pulse(turn_cmd, turn_duration)
                send("S")
                turn_cooldown_until = time.time() + TURN_COOLDOWN_SEC

            if stop_stuck_since is None:
                stop_stuck_since = time.time()
            elif ai_enabled and (time.time() - stop_stuck_since) > 4.0:
                send("B")
                publish_metrics(
                    state, True, "FORCED_ESCAPE", "-",
                    brightness, edge_ratio, risk_score, latency_ms,
                    near_score=near_score, near_ema_value=near_ema,
                    very_close_score=very_close_score,
                    pred_near=pred_near, pred_risk=pred_risk,
                    hit=close_hit_streak, cmd="B",
                )
                time.sleep(random.uniform(FORCED_REVERSE_MIN_SEC, FORCED_REVERSE_MAX_SEC))
                send("S")
                time.sleep(0.15)
                turn_cmd = random.choice(["L", "R"])
                _, turn_duration = pick_turn_profile()
                execute_turn_pulse(turn_cmd, min(turn_duration, 0.9))
                send("S")
                stop_stuck_since = time.time()
            return LOOP_SLEEP_SEC

        # Path is clear
        loop_detect_streak   = 0
        obstacle_cycle_count = 0
        stop_stuck_since     = None

        if ai_enabled:
            send("F")
            publish_metrics(
                state, False, "AUTO_FORWARD", "-",
                brightness, edge_ratio, risk_score, latency_ms,
                near_score=near_score, near_ema_value=near_ema,
                very_close_score=very_close_score,
                pred_near=pred_near, pred_risk=pred_risk,
                hit=close_hit_streak, cmd="F",
            )
        else:
            publish_metrics(
                state, False, "MANUAL_SAFE", "-",
                brightness, edge_ratio, risk_score, latency_ms,
                near_score=near_score, near_ema_value=near_ema,
                very_close_score=very_close_score,
                pred_near=pred_near, pred_risk=pred_risk,
                hit=close_hit_streak, cmd=manual_cmd,
            )

        return LOOP_SLEEP_SEC

    except Exception as exc:
        print("error:", exc)
        publish({
            "ai_worker": "0", "ai_obstacle": "0", "ai_action": "ERROR",
            "ai_turn": "-", "ai_brightness": "0.0", "ai_edge": "0.0",
            "ai_near": "0.0", "ai_near_ema": "0.0", "ai_vclose": "0.0",
            "ai_pred_near": "0.0", "ai_pred_risk": "0.0", "ai_hit": "0",
            "ai_cmd": "S", "ai_score": "0.0", "ai_latency": "0",
        }, force=True)
        return 0.35


def main():
    print("=== FPV Car AI Vision Bot Started ===")
    print(f"Image URL:  {IMG_URL}")
    print(f"State URL:  {STATE_URL}")
    print(f"Set URL:    {SET_URL}")
    print("Polling every ~120ms...")
    while True:
        time.sleep(process_once())


if __name__ == "__main__":
    main()
