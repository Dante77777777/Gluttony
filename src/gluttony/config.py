from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml


@dataclass(frozen=True)
class ProjectConfig:
    name: str
    seed: int
    output_dir: Path


@dataclass(frozen=True)
class DatasetConfig:
    root: Path
    category: str
    image_size: int
    train_batch_size: int
    eval_batch_size: int
    num_workers: int


@dataclass(frozen=True)
class ModelConfig:
    backbone: str
    layers: list[str]
    coreset_sampling_ratio: float
    num_neighbors: int


@dataclass(frozen=True)
class TrainerConfig:
    accelerator: str
    devices: int
    max_epochs: int
    enable_progress_bar: bool


@dataclass(frozen=True)
class ExportConfig:
    format: str
    input_size: tuple[int, int]


@dataclass(frozen=True)
class AppConfig:
    project: ProjectConfig
    dataset: DatasetConfig
    model: ModelConfig
    trainer: TrainerConfig
    export: ExportConfig

    @property
    def run_dir(self) -> Path:
        return self.project.output_dir / self.dataset.category


def load_config(path: str | Path) -> AppConfig:
    config_path = Path(path)
    with config_path.open("r", encoding="utf-8") as file:
        raw: dict[str, Any] = yaml.safe_load(file)

    return AppConfig(
        project=ProjectConfig(
            name=raw["project"]["name"],
            seed=int(raw["project"].get("seed", 42)),
            output_dir=Path(raw["project"].get("output_dir", "outputs/patchcore_mvtec")),
        ),
        dataset=DatasetConfig(
            root=Path(raw["dataset"].get("root", "data/mvtec_ad")),
            category=raw["dataset"].get("category", "bottle"),
            image_size=int(raw["dataset"].get("image_size", 256)),
            train_batch_size=int(raw["dataset"].get("train_batch_size", 4)),
            eval_batch_size=int(raw["dataset"].get("eval_batch_size", 4)),
            num_workers=int(raw["dataset"].get("num_workers", 0)),
        ),
        model=ModelConfig(
            backbone=raw["model"].get("backbone", "wide_resnet50_2"),
            layers=list(raw["model"].get("layers", ["layer2", "layer3"])),
            coreset_sampling_ratio=float(raw["model"].get("coreset_sampling_ratio", 0.01)),
            num_neighbors=int(raw["model"].get("num_neighbors", 9)),
        ),
        trainer=TrainerConfig(
            accelerator=raw["trainer"].get("accelerator", "cpu"),
            devices=int(raw["trainer"].get("devices", 1)),
            max_epochs=int(raw["trainer"].get("max_epochs", 1)),
            enable_progress_bar=bool(raw["trainer"].get("enable_progress_bar", True)),
        ),
        export=ExportConfig(
            format=raw["export"].get("format", "openvino"),
            input_size=tuple(raw["export"].get("input_size", [256, 256])),
        ),
    )
