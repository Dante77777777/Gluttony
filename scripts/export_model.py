from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parents[1] / "src"))

from gluttony.anomalib_factory import build_datamodule, build_engine, build_model, find_latest_checkpoint
from gluttony.config import load_config
from gluttony.io import write_json


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export a trained PatchCore checkpoint.")
    parser.add_argument("--config", default="configs/patchcore_mvtec.yaml", help="Path to YAML config.")
    parser.add_argument("--ckpt", default=None, help="Checkpoint path. Defaults to latest checkpoint under run dir.")
    parser.add_argument("--format", default=None, choices=["openvino", "onnx", "torch"], help="Export format.")
    return parser.parse_args()


def export_type_from_name(name: str):
    from anomalib.deploy import ExportType

    normalized = name.lower()
    if normalized == "openvino":
        return ExportType.OPENVINO
    if normalized == "onnx":
        return ExportType.ONNX
    if normalized == "torch":
        return ExportType.TORCH
    raise ValueError(f"Unsupported export format: {name}")


def main() -> None:
    args = parse_args()
    config = load_config(args.config)
    ckpt_path = Path(args.ckpt) if args.ckpt else find_latest_checkpoint(config.run_dir)
    export_format = args.format or config.export.format

    datamodule = build_datamodule(config)
    model = build_model(config)
    engine = build_engine(config)

    export_type = export_type_from_name(export_format)
    try:
        export_path = engine.export(
            model=model,
            export_type=export_type,
            ckpt_path=str(ckpt_path),
            input_size=config.export.input_size,
            datamodule=datamodule,
        )
    except TypeError:
        # Current Anomalib docs show the compact form. The richer call above is
        # accepted by some releases, while others infer size from the model.
        export_path = engine.export(
            model=model,
            export_type=export_type,
            ckpt_path=str(ckpt_path),
        )

    manifest_path = write_json(
        config.run_dir / "export_manifest.json",
        {
            "format": export_format,
            "checkpoint_path": str(ckpt_path),
            "export_path": str(export_path),
            "input_size": list(config.export.input_size),
        },
    )
    print(f"Export finished: {export_path}")
    print(f"Manifest: {manifest_path}")


if __name__ == "__main__":
    main()
