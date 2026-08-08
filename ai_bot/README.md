# ai_bot (Refactored)

This folder now contains only useful AIML object detection code.

## Why this refactor
Old code was tied to robot control and remote PHP endpoints. For a simple AIML project, that logic was unnecessary.

## Files
- `config.py`: model paths, URLs, and class names
- `detector.py`: model loading + detection + summary utilities
- `analyze_media.py`: CLI for image/video analysis
- `requirements.txt`: dependencies

## Install
```powershell
pip install -r ai_bot/requirements.txt
```

## Run on image
```powershell
python -m ai_bot.analyze_media --input "sample.jpg" --output "out/annotated.jpg"
```

## Run on video
```powershell
python -m ai_bot.analyze_media --input "sample.mp4" --frame-skip 8
```

The output is JSON with:
- `object_present`
- `total_objects`
- `top_match`
- `labels`
