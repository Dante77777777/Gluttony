# Gluttony

工业视觉异常检测项目，覆盖 Python 模型训练与评估、C++/Qt 客户端推理、TCP 相机数据接入和 SQLite 检测记录管理。

当前版本使用 **Anomalib + PatchCore + MVTec AD**。PatchCore 不从零训练 CNN 参数，而是使用 ImageNet 预训练网络提取正常样本的 patch 特征，建立正常特征库；推理时通过最近邻距离生成异常分数和热力图。

系统包含 Python 训练与验证工具，以及 Qt 检测客户端。客户端可以加载 OpenVINO IR 或 ONNX 模型，处理单张图片、文件夹和 TCP 相机图像流；SQLite 保存检测批次、检测结果与历史记录，热力图等图像文件保存在本地目录。

## 目录结构

```text
configs/              # 默认训练配置
database/             # SQLite 数据库迁移
docs/                 # 训练、持久化和系统设计文档
clients/Gluttony/     # C++/Qt 检测客户端
scripts/              # 命令行入口
src/gluttony/         # Python 工程代码
outputs/              # 训练、评估、预测、导出结果
```

## 环境安装

建议使用 Anaconda 单独创建环境，不要直接使用 base 环境。

```powershell
conda create -n gluttony python=3.10 -y
conda activate gluttony
cd D:\code\Gluttony
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

PyCharm 的解释器应选择：

```text
D:\Anaconda\envs\gluttony\python.exe
```

不要选择：

```text
D:\Anaconda\python.exe
D:\Anaconda\condabin\conda.bat
```

## 准备数据集

默认使用 MVTec AD 的 `bottle` 类别。先让脚本打印官方下载页面：

```powershell
python scripts/prepare_dataset.py --category bottle --download
```

从 MVTec 官网下载 `bottle.tar.xz` 后，用脚本解压：

```powershell
python scripts/prepare_dataset.py --category bottle --archive D:\Downloads\bottle.tar.xz
```

也可以手动解压到：

```text
D:\code\Gluttony\data\mvtec_ad\bottle
```

目录应包含：

```text
train
test
ground_truth
```

## 训练 PatchCore

```powershell
python scripts/train_patchcore.py --config configs/patchcore_mvtec.yaml
```

训练完成后会在 `outputs/patchcore_mvtec/bottle/` 下保存：

- Anomalib 运行结果和 checkpoint
- `metrics.json`
- `run_summary.json`

## 单图预测

```powershell
python scripts/predict_image.py --image data/mvtec_ad/bottle/test/broken_large/000.png
```

预测会输出：

- OK/NG 判定
- 异常分数
- `prediction.json`
- `heatmap.png`
- `overlay.png`

## 评估

```powershell
python scripts/evaluate.py
```

## 导出模型

```powershell
python scripts/export_model.py --format openvino
```

导出的 OpenVINO/ONNX 模型可由 C++/Qt 客户端通过 OpenVINO Runtime 直接加载推理。

导出后可以先用 Python 验证模型能否被 OpenVINO Runtime 加载：

```powershell
python scripts/verify_openvino_export.py --model outputs/patchcore_mvtec/bottle/weights/openvino/model.xml
python scripts/verify_openvino_export.py --model outputs/patchcore_mvtec/bottle/weights/onnx/model.onnx
```

Qt 客户端的 “Load Model” 文件选择器同时支持：

- OpenVINO IR：`*.xml`（同目录需保留对应的 `*.bin`）
- ONNX：`*.onnx`

ONNX 仍由现有的 OpenVINO Runtime 执行，因此客户端无需再链接 ONNX Runtime。

## SQLite 持久化

客户端使用 SQLite 保存检测批次、逐张检测结果、状态和相机流统计；原始图像与热力图等大文件存储在文件系统中。数据库结构通过版本化迁移维护，当前迁移包括基础检测记录和 TCP 相机数据源。

运行 Python 标准库数据库约束测试：

```powershell
python -m unittest discover -s tests -v
```

迁移说明、Qt 客户端接入状态和 C++ 冒烟测试命令见 [database/README.md](database/README.md)。

## Qt 客户端

客户端源码位于 `clients/Gluttony/`，包含 OpenVINO 推理、图片与相机流处理、TCP 服务端及 SQLite 持久化工作器。C++ 构建配置位于 `clients/Gluttony/CMakeLists.txt`；数据库测试和迁移说明见 [database/README.md](database/README.md)。

## 学习路线

训练过程解释见 [docs/training_process.md](docs/training_process.md)，持久化需求和表结构设计见 [docs/persistence_requirements.md](docs/persistence_requirements.md) 与 [docs/sqlite_schema_design.md](docs/sqlite_schema_design.md)。
