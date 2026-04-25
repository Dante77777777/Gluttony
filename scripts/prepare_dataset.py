from __future__ import annotations

import argparse
import tarfile
from pathlib import Path


MVTEC_DOWNLOAD_PAGE = "https://www.mvtec.com/company/research/datasets/mvtec-ad/downloads"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Download or check an MVTec AD category.")
    parser.add_argument("--root", default="data/mvtec_ad", help="MVTec AD root directory.")
    parser.add_argument("--category", default="bottle", help="MVTec AD category, for example bottle or capsule.")
    parser.add_argument("--archive", default=None, help="Path to a downloaded category archive, for example bottle.tar.xz.")
    parser.add_argument(
        "--download",
        action="store_true",
        help="Print the official download page. Direct links change over time, so manual download is recommended.",
    )
    return parser.parse_args()


def category_ready(category_dir: Path) -> bool:
    return all((category_dir / name).exists() for name in ("train", "test", "ground_truth"))


def main() -> None:
    args = parse_args()
    root = Path(args.root)
    category_dir = root / args.category

    if category_ready(category_dir):
        print(f"Dataset is ready: {category_dir}")
        return

    if args.archive:
        root.mkdir(parents=True, exist_ok=True)
        archive_path = Path(args.archive)
        if not archive_path.exists():
            raise FileNotFoundError(f"Archive does not exist: {archive_path}")

        print(f"Extracting {archive_path}")
        with tarfile.open(archive_path, "r:xz") as archive:
            archive.extractall(root)

        if not category_ready(category_dir):
            raise RuntimeError(f"Extraction finished but dataset layout is still invalid: {category_dir}")
        print(f"Dataset is ready: {category_dir}")
        return

    if not args.download:
        print(f"Dataset is not ready: {category_dir}")
        print("Expected subdirectories: train, test, ground_truth")
        print(f"Download the category archive from: {MVTEC_DOWNLOAD_PAGE}")
        print("Then rerun with --archive path\\to\\bottle.tar.xz")
        return

    if args.download and not args.archive:
        print("MVTec changes category download links over time, so this script does not use a hard-coded URL.")
        print(f"Open the official download page and download the '{args.category}' archive:")
        print(MVTEC_DOWNLOAD_PAGE)
        print(f"Then rerun: python scripts/prepare_dataset.py --category {args.category} --archive path\\to\\{args.category}.tar.xz")
        return


if __name__ == "__main__":
    main()
