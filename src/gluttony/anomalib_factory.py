from __future__ import annotations

from pathlib import Path
from typing import Any

from .config import AppConfig


def build_datamodule(config: AppConfig) -> Any:
    """Create an Anomalib MVTec AD datamodule with CPU/Windows-friendly defaults."""
    from anomalib.data import MVTecAD

    kwargs = {
        "root": config.dataset.root,
        "category": config.dataset.category,
        "train_batch_size": config.dataset.train_batch_size,
        "eval_batch_size": config.dataset.eval_batch_size,
        "num_workers": config.dataset.num_workers,
    }

    try:
        return MVTecAD(image_size=(config.dataset.image_size, config.dataset.image_size), **kwargs)
    except TypeError:
        # Anomalib 2.x moved most image transforms into model preprocessors.
        return MVTecAD(**kwargs)


def build_model(config: AppConfig) -> Any:
    from anomalib.models import Patchcore

    kwargs = {
        "backbone": config.model.backbone,
        "layers": config.model.layers,
        "coreset_sampling_ratio": config.model.coreset_sampling_ratio,
        "num_neighbors": config.model.num_neighbors,
    }

    try:
        pre_processor = Patchcore.configure_pre_processor(
            image_size=(config.dataset.image_size, config.dataset.image_size)
        )
        return Patchcore(pre_processor=pre_processor, **kwargs)
    except (AttributeError, TypeError):
        return Patchcore(**kwargs)


def build_engine(config: AppConfig) -> Any:
    from anomalib.engine import Engine

    engine_kwargs = {
        "default_root_dir": config.run_dir,
        "accelerator": config.trainer.accelerator,
        "devices": config.trainer.devices,
        "max_epochs": config.trainer.max_epochs,
        "enable_progress_bar": config.trainer.enable_progress_bar,
    }

    try:
        return Engine(**engine_kwargs)
    except TypeError:
        engine_kwargs.pop("default_root_dir", None)
        return Engine(**engine_kwargs)


def find_latest_checkpoint(run_dir: str | Path) -> Path:
    candidates = sorted(Path(run_dir).rglob("*.ckpt"), key=lambda item: item.stat().st_mtime, reverse=True)
    if not candidates:
        raise FileNotFoundError(f"No .ckpt file found under {run_dir}. Train the model first.")
    return candidates[0]
