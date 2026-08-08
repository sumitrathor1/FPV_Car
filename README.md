# Simple AIML Object Detection Project

This repository now contains a clean AIML setup with two parts:

1. Browser frontend for image/video upload object detection.
2. Refactored Python `ai_bot` module for structured media analysis.

## Frontend (No backend needed)

### What it does
- Upload an image or video from your device.
- Detect objects using AI in browser (TensorFlow.js + COCO-SSD).
- Show object presence, total objects, top match, and labels.

### Run
1. Open `index.html` in a browser.
2. If direct open is blocked, run:
  - `python -m http.server 8000`
  - Open `http://localhost:8000`

## Python ai_bot (Structured)

### What changed
- Removed old robot/car control dependencies and remote PHP polling logic.
- Kept only AIML object detection responsibilities.

### Files
- `ai_bot/config.py`: model and class configuration
- `ai_bot/detector.py`: detector and summary utilities
- `ai_bot/analyze_media.py`: CLI to analyze image/video

### Install
1. `pip install -r ai_bot/requirements.txt`

### Run image analysis
1. `python -m ai_bot.analyze_media --input "sample.jpg" --output "out/annotated.jpg"`

### Run video analysis
1. `python -m ai_bot.analyze_media --input "sample.mp4" --frame-skip 8`

### Output
The Python script prints JSON:
- `object_present`
- `total_objects`
- `top_match`
- `labels`
