import subprocess
import sys
import time
from pathlib import Path

import requests


STATE_URL = "http://20.244.113.234/robot/get.php"


def get_state():
    try:
        response = requests.get(STATE_URL, timeout=1.2)
        response.raise_for_status()
        return response.json()
    except Exception:
        return None


def main():
    root = Path(__file__).resolve().parent
    worker = root / "ai.py"
    process = None

    while True:
        state = get_state()
        cam_on = bool(state and state.get("cam") == "1")

        if cam_on and (process is None or process.poll() is not None):
            process = subprocess.Popen([sys.executable, str(worker)], cwd=str(root))

        if not cam_on and process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
            process = None

        time.sleep(1.0)


if __name__ == "__main__":
    main()