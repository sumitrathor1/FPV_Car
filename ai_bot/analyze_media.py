from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2

from ai_bot.detector import ObjectDetector, detect_image_file, detect_video_file, summarize


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Analyze image/video and report whether objects are detected.",
    )
    parser.add_argument("--input", required=True, help="Path to image or video file")
    parser.add_argument(
        "--output",
        help="Optional path to save annotated image (works for image inputs)",
    )
    parser.add_argument(
        "--confidence",
        type=float,
        default=0.45,
        help="Confidence threshold between 0 and 1 (default: 0.45)",
    )
    parser.add_argument(
        "--frame-skip",
        type=int,
        default=8,
        help="Process every Nth frame for videos (default: 8)",
    )
    return parser


def main() -> None:
    args = build_parser().parse_args()
    input_path = Path(args.input)

    if not input_path.exists():
        raise SystemExit(f"Input file not found: {input_path}")

    detector = ObjectDetector(confidence_threshold=args.confidence)

    suffix = input_path.suffix.lower()
    image_suffixes = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}

    if suffix in image_suffixes:
        annotated, detections = detect_image_file(detector, input_path)
        result = summarize(detections)

        if args.output:
            output_path = Path(args.output)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            cv2.imwrite(str(output_path), annotated)
    else:
        _, result = detect_video_file(detector, input_path, frame_skip=max(1, args.frame_skip))

    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
