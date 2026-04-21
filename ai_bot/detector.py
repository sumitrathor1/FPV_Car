from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable
from urllib.request import urlretrieve

import cv2
import numpy as np

try:
    from .config import COCO_CLASS_NAMES, CONFIG_FILE, CONFIG_URL, MODEL_FILE, MODEL_URL, MODELS_DIR
except ImportError:
    from config import COCO_CLASS_NAMES, CONFIG_FILE, CONFIG_URL, MODEL_FILE, MODEL_URL, MODELS_DIR


@dataclass
class Detection:
    label: str
    confidence: float
    box: tuple[int, int, int, int]


def ensure_model_files() -> None:
    MODELS_DIR.mkdir(parents=True, exist_ok=True)

    if not MODEL_FILE.exists():
        urlretrieve(MODEL_URL, str(MODEL_FILE))

    if not CONFIG_FILE.exists():
        urlretrieve(CONFIG_URL, str(CONFIG_FILE))


class ObjectDetector:
    def __init__(self, confidence_threshold: float = 0.45) -> None:
        ensure_model_files()
        self.confidence_threshold = confidence_threshold
        self.model = cv2.dnn_DetectionModel(str(MODEL_FILE), str(CONFIG_FILE))
        self.model.setInputSize(300, 300)
        self.model.setInputScale(1.0 / 127.5)
        self.model.setInputMean((127.5, 127.5, 127.5))
        self.model.setInputSwapRB(True)

    def detect(self, image: np.ndarray) -> list[Detection]:
        class_ids, confidences, boxes = self.model.detect(
            image,
            confThreshold=self.confidence_threshold,
            nmsThreshold=0.35,
        )

        detections: list[Detection] = []
        if len(class_ids) == 0:
            return detections

        for class_id, confidence, box in zip(class_ids.flatten(), confidences.flatten(), boxes):
            label = (
                COCO_CLASS_NAMES[class_id]
                if 0 <= class_id < len(COCO_CLASS_NAMES)
                else f"class_{class_id}"
            )
            x, y, w, h = map(int, box)
            detections.append(Detection(label=label, confidence=float(confidence), box=(x, y, w, h)))

        detections.sort(key=lambda item: item.confidence, reverse=True)
        return detections


def draw_detections(image: np.ndarray, detections: Iterable[Detection]) -> np.ndarray:
    output = image.copy()

    for item in detections:
        x, y, w, h = item.box
        cv2.rectangle(output, (x, y), (x + w, y + h), (40, 140, 255), 2)
        text = f"{item.label} {item.confidence * 100:.1f}%"
        cv2.putText(
            output,
            text,
            (x, max(20, y - 8)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            (20, 30, 40),
            2,
            cv2.LINE_AA,
        )

    return output


def summarize(detections: list[Detection]) -> dict[str, object]:
    labels = sorted({item.label for item in detections})
    top = detections[0] if detections else None

    return {
        "object_present": bool(detections),
        "total_objects": len(detections),
        "top_match": {
            "label": top.label,
            "confidence": round(top.confidence, 4),
        }
        if top
        else None,
        "labels": labels,
    }


def detect_image_file(detector: ObjectDetector, image_path: Path) -> tuple[np.ndarray, list[Detection]]:
    frame = cv2.imread(str(image_path))
    if frame is None:
        raise ValueError(f"Could not read image: {image_path}")

    detections = detector.detect(frame)
    annotated = draw_detections(frame, detections)
    return annotated, detections


def detect_video_file(
    detector: ObjectDetector,
    video_path: Path,
    frame_skip: int = 8,
) -> tuple[list[Detection], dict[str, object]]:
    capture = cv2.VideoCapture(str(video_path))
    if not capture.isOpened():
        raise ValueError(f"Could not read video: {video_path}")

    combined: list[Detection] = []
    frame_index = 0

    try:
        while True:
            ok, frame = capture.read()
            if not ok:
                break

            if frame_index % frame_skip == 0:
                combined.extend(detector.detect(frame))

            frame_index += 1
    finally:
        capture.release()

    best_by_label: dict[str, Detection] = {}
    for item in combined:
        current = best_by_label.get(item.label)
        if current is None or item.confidence > current.confidence:
            best_by_label[item.label] = item

    merged = sorted(best_by_label.values(), key=lambda item: item.confidence, reverse=True)
    return merged, summarize(merged)


def _direct_run_message() -> None:
    print("This file is a library module and is not meant to be run directly.")
    print("Use the CLI entry point instead:")
    print("python -m ai_bot.analyze_media --input <image_or_video_path>")


if __name__ == "__main__":
    _direct_run_message()
