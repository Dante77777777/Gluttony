# PatchCore 训练过程说明

本文档用于帮助你理解并讲清楚这个项目的模型训练部分。

## 1. 项目目标

工业缺陷检测经常遇到一个现实问题：正常样本很多，缺陷样本少，而且缺陷类型不可穷举。PatchCore 适合这种场景，因为它主要学习“正常是什么样”，然后把偏离正常分布的区域判为异常。

本项目使用：

- 数据集：MVTec AD
- 模型：PatchCore
- 框架：Anomalib
- 部署方向：OpenVINO/ONNX，后续给 C++/Qt 客户端使用

## 2. 数据阶段

MVTec AD 每个类别都有类似结构：

```text
bottle/
  train/
    good/
  test/
    good/
    broken_large/
    broken_small/
    contamination/
  ground_truth/
    broken_large/
    broken_small/
    contamination/
```

训练时主要使用 `train/good` 中的正常图片。测试时同时使用正常图片和缺陷图片，并用 `ground_truth` 中的 mask 评估像素级定位效果。

## 3. 特征提取阶段

PatchCore 使用 ImageNet 预训练 CNN，例如 `wide_resnet50_2`。它不会从零训练这个 CNN，而是从中间层提取局部 patch 特征。

常用层是：

- `layer2`：保留较多局部纹理和边缘信息
- `layer3`：语义更强，感受野更大

把两层特征结合起来后，每张图片会变成很多个 patch 向量。

## 4. 建立正常特征库

训练阶段的核心不是更新参数，而是把正常样本的 patch 特征保存成 memory bank。

如果把所有 patch 都保存下来，内存和推理速度会变差。因此 PatchCore 使用 coreset sampling，只保留一部分代表性特征。配置里的 `coreset_sampling_ratio: 0.01` 表示只采样一小部分正常 patch 特征，适合 CPU 环境先跑通。

你可以这样解释：

> 训练阶段相当于建立一套“正常零件局部纹理数据库”。推理时，如果某个区域找不到相似的正常特征，就说明它可能是缺陷。

## 5. 推理阶段

输入一张测试图像后：

1. 按同样的预处理缩放到 256x256。
2. 用预训练 backbone 提取 patch 特征。
3. 每个测试 patch 到 memory bank 中查找最近邻。
4. 最近邻距离越大，说明越不像正常样本。
5. 所有 patch 距离还原成异常热力图。

图片级异常分数通常来自最异常的 patch 或若干高异常 patch 的组合。

## 6. 阈值判定

模型会输出异常分数和热力图，但工业客户端最终需要一个清晰结论：`OK` 或 `NG`。

Anomalib 的后处理会根据评估数据确定阈值。推理时：

- 分数低于阈值：OK
- 分数高于阈值：NG

在 Qt 客户端里，可以把这个阈值做成参数，让用户按实际产线要求调节误检率和漏检率。

## 7. 评估指标

建议在简历和 README 中展示这些指标：

- Image-level AUROC：整张图是否能正确判定 OK/NG
- Pixel-level AUROC：缺陷热力图是否能覆盖真实缺陷区域
- F1 或阈值下准确率：更接近工程使用
- 单张推理耗时：后续客户端部署关注

## 8. 和 Qt 客户端的关系

Python 训练端负责：

- 准备数据
- 建立 PatchCore memory bank
- 评估指标
- 导出模型
- 保存阈值和配置

C++/Qt 客户端负责：

- 读取图片或相机帧
- 使用 OpenCV 预处理
- 使用 OpenVINO/ONNX Runtime 推理
- 生成热力图叠加显示
- 输出 OK/NG 和检测报告

这样项目就形成了完整闭环：训练、评估、部署、可视化、工业客户端。
