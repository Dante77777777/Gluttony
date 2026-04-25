from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parents[1] / "src"))

from gluttony.anomalib_factory import build_datamodule, build_engine, build_model, find_latest_checkpoint
from gluttony.config import load_config
from gluttony.io import ensure_dir, to_jsonable, write_json


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train PatchCore on MVTec AD with Anomalib.")
    parser.add_argument("--config", default="configs/patchcore_mvtec.yaml", help="Path to YAML config.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    config = load_config(args.config)
    ensure_dir(config.run_dir)

    datamodule = build_datamodule(config)
    model = build_model(config)
    engine = build_engine(config)

    engine.fit(model=model, datamodule=datamodule)
    test_results = engine.test(model=model, datamodule=datamodule)
    checkpoint_path = find_latest_checkpoint(config.run_dir)

    write_json(config.run_dir / "metrics.json", to_jsonable(test_results))
    write_json(
        config.run_dir / "run_summary.json",
        {
            "category": config.dataset.category,
            "image_size": config.dataset.image_size,
            "checkpoint_path": str(checkpoint_path),
            "note": "PatchCore builds a normal feature memory bank instead of training CNN weights with backpropagation.",
        },
    )

    print(f"Training finished. Checkpoint: {checkpoint_path}")
    print(f"Metrics: {config.run_dir / 'metrics.json'}")


if __name__ == "__main__":
    main()
