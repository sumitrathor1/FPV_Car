"""
FPV Car - AI Watchdog
Monitors camera state from the server. When camera is ON, spawns
the ai.py worker. When camera is OFF, terminates the worker to
save CPU. Runs as a persistent background supervisor.
"""

import subprocess
import sys
import time
from pathlib import Path

import requests


STATE_URL = "http://20.244.113.234/fpv_car/get.php"


def get_state():
    try:
        response = requests.get(STATE_URL, timeout=1.2)
        response.raise_for_status()
        return response.json()
    except Exception:
        return None


def main():
    root   = Path(__file__).resolve().parent
    worker = root / "ai.py"
    process = None

    print("=== FPV Car AI Watchdog Started ===")
    print(f"Monitoring: {STATE_URL}")
    print(f"Worker:     {worker}")

    while True:
        state  = get_state()
        cam_on = bool(state and state.get("cam") == "1")

        # Start worker when camera turns ON
        if cam_on and (process is None or process.poll() is not None):
            print("[watchdog] Camera ON → starting AI worker")
            process = subprocess.Popen(
                [sys.executable, str(worker)],
                cwd=str(root),
            )

        # Stop worker when camera turns OFF
        if not cam_on and process is not None and process.poll() is None:
            print("[watchdog] Camera OFF → stopping AI worker")
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
            process = None

        time.sleep(1.0)


if __name__ == "__main__":
    main()
