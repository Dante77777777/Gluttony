from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parents[1] / "src"))

from gluttony.anomalib_factory import build_datamodule, build_engine, build_model, find_latest_checkpoint
from gluttony.config import load_config
from gluttony.io import to_jsonable, write_json


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Evaluate a trained PatchCore checkpoint.")
    parser.add_argument("--config", default="configs/patchcore_mvtec.yaml", help="Path to YAML config.")
    parser.add_argument("--ckpt", default=None, help="Checkpoint path. Defaults to latest checkpoint under run dir.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    config = load_config(args.config)
    ckpt_path = Path(args.ckpt) if args.ckpt else find_latest_checkpoint(config.run_dir)

    datamodule = build_datamodule(config)
    model = build_model(config)
    engine = build_engine(config)
    results = engine.test(model=model, datamodule=datamodule, ckpt_path=str(ckpt_path))

    output_path = write_json(config.run_dir / "evaluation.json", to_jsonable(results))
    print(f"Evaluation finished: {output_path}")


if __name__ == "__main__":
    main()
