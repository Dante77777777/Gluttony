from __future__ import annotations

import argparse
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Load an exported OpenVINO model to verify deployment readiness.")
    parser.add_argument("--model", required=True, help="Path to exported .xml or .onnx model.")
    parser.add_argument("--device", default="CPU", help="OpenVINO device name, for example CPU.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    model_path = Path(args.model)
    if not model_path.exists():
        raise FileNotFoundError(f"Model file does not exist: {model_path}")

    try:
        from openvino import Core
    except ImportError:
        from openvino.runtime import Core

    core = Core()
    model = core.read_model(model_path)
    compiled_model = core.compile_model(model, args.device)

    print(f"OpenVINO model loaded: {model_path}")
    print(f"Device: {args.device}")
    print("Inputs:")
    for input_port in compiled_model.inputs:
        print(f"  {input_port.get_any_name()}: {input_port.partial_shape}")
    print("Outputs:")
    for output_port in compiled_model.outputs:
        print(f"  {output_port.get_any_name()}: {output_port.partial_shape}")


if __name__ == "__main__":
    main()
