# Gluttony

工业异常检测训练端项目，面向“Python 模型训练 + C++/Qt 客户端部署”的机器视觉作品集。

当前版本使用 **Anomalib + PatchCore + MVTec AD**。PatchCore 不从零训练 CNN 参数，而是使用 ImageNet 预训练网络提取正常样本的 patch 特征，建立正常特征库；推理时通过最近邻距离生成异常分数和热力图。

## 目录结构

```text
configs/              # 默认训练配置
docs/                 # 训练过程学习文档
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

导出的 OpenVINO/ONNX 模型用于后续 C++/Qt 客户端加载推理。

导出后可以先用 Python 验证 OpenVINO 模型能否加载：

```powershell
python scripts/verify_openvino_export.py --model outputs/patchcore_mvtec/bottle/weights/openvino/model.xml
```

## 学习路线

训练过程解释见 [docs/training_process.md](docs/training_process.md)。
