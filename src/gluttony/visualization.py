from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np


def normalize_map(anomaly_map: np.ndarray) -> np.ndarray:
    anomaly_map = anomaly_map.astype(np.float32)
    min_value = float(anomaly_map.min())
    max_value = float(anomaly_map.max())
    if max_value - min_value < 1e-8:
        return np.zeros_like(anomaly_map, dtype=np.uint8)
    normalized = (anomaly_map - min_value) / (max_value - min_value)
    return (normalized * 255).clip(0, 255).astype(np.uint8)


def save_heatmap_overlay(image_path: str | Path, anomaly_map: np.ndarray, output_dir: str | Path) -> dict[str, str]:
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=True)

    image = cv2.imread(str(image_path), cv2.IMREAD_COLOR)
    if image is None:
        raise ValueError(f"Could not read image: {image_path}")

    heatmap_gray = normalize_map(anomaly_map)
    heatmap_gray = cv2.resize(heatmap_gray, (image.shape[1], image.shape[0]))
    heatmap_color = cv2.applyColorMap(heatmap_gray, cv2.COLORMAP_JET)
    overlay = cv2.addWeighted(image, 0.55, heatmap_color, 0.45, 0)

    heatmap_path = output / "heatmap.png"
    overlay_path = output / "overlay.png"
    cv2.imwrite(str(heatmap_path), heatmap_color)
    cv2.imwrite(str(overlay_path), overlay)
    return {"heatmap": str(heatmap_path), "overlay": str(overlay_path)}
