from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

import numpy as np

sys.path.append(str(Path(__file__).resolve().parents[1] / "src"))

from gluttony.anomalib_factory import build_engine, build_model, find_latest_checkpoint
from gluttony.config import load_config
from gluttony.io import ensure_dir, to_jsonable, write_json
from gluttony.visualization import save_heatmap_overlay


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Predict a single image with a trained PatchCore checkpoint.")
    parser.add_argument("--config", default="configs/patchcore_mvtec.yaml", help="Path to YAML config.")
    parser.add_argument("--image", required=True, help="Image path.")
    parser.add_argument("--ckpt", default=None, help="Checkpoint path. Defaults to latest checkpoint under run dir.")
    parser.add_argument("--output-dir", default=None, help="Prediction output directory.")
    return parser.parse_args()


def first_prediction(predictions: Any) -> Any:
    if predictions is None:
        raise RuntimeError("Anomalib returned no predictions.")
    if isinstance(predictions, list):
        if not predictions:
            raise RuntimeError("Anomalib returned an empty prediction list.")
        first = predictions[0]
        if isinstance(first, list):
            return first[0]
        return first
    return predictions


def get_field(prediction: Any, *names: str) -> Any:
    for name in names:
        if isinstance(prediction, dict) and name in prediction:
            return prediction[name]
        if hasattr(prediction, name):
            return getattr(prediction, name)
    return None


def to_float(value: Any) -> float | None:
    if value is None:
        return None
    if hasattr(value, "detach"):
        value = value.detach().cpu()
    if hasattr(value, "item"):
        return float(value.item())
    if isinstance(value, (int, float)):
        return float(value)
    return None


def to_numpy(value: Any) -> np.ndarray | None:
    if value is None:
        return None
    if hasattr(value, "detach"):
        value = value.detach().cpu()
    if hasattr(value, "numpy"):
        value = value.numpy()
    array = np.asarray(value)
    return np.squeeze(array)


def main() -> None:
    args = parse_args()
    config = load_config(args.config)
    image_path = Path(args.image)
    ckpt_path = Path(args.ckpt) if args.ckpt else find_latest_checkpoint(config.run_dir)
    output_dir = ensure_dir(args.output_dir or config.run_dir / "predictions" / image_path.stem)

    from anomalib.data import PredictDataset

    model = build_model(config)
    engine = build_engine(config)
    try:
        dataset = PredictDataset(
            path=image_path,
            image_size=(config.dataset.image_size, config.dataset.image_size),
        )
    except TypeError:
        dataset = PredictDataset(path=image_path)

    predictions = engine.predict(model=model, dataset=dataset, ckpt_path=str(ckpt_path))
    prediction = first_prediction(predictions)

    score = to_float(get_field(prediction, "pred_score", "score", "anomaly_score"))
    label = get_field(prediction, "pred_label", "label")
    label_value = to_float(label)
    is_anomaly = bool(label_value) if label_value is not None else None

    anomaly_map = to_numpy(get_field(prediction, "anomaly_map", "pred_mask"))
    image_outputs = {}
    if anomaly_map is not None:
        image_outputs = save_heatmap_overlay(image_path, anomaly_map, output_dir)

    result = {
        "image_path": str(image_path),
        "checkpoint_path": str(ckpt_path),
        "prediction": "NG" if is_anomaly else "OK" if is_anomaly is not None else "UNKNOWN",
        "is_anomaly": is_anomaly,
        "anomaly_score": score,
        "raw_prediction": to_jsonable(prediction),
        "outputs": image_outputs,
    }
    result_path = write_json(output_dir / "prediction.json", result)

    print(f"Prediction: {result['prediction']}")
    print(f"Anomaly score: {score}")
    print(f"Result JSON: {result_path}")


if __name__ == "__main__":
    main()
