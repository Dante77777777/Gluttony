#ifndef VIEWWIDGET_H
#define VIEWWIDGET_H

#include "ViewWidgetUI.h"
#include <QString>


class ViewWidget : public QWidget
{
    Q_OBJECT
public:
    ViewWidget(QWidget* parent = nullptr);
    ~ViewWidget();
    void setupUI();
    // 仅记录文件名；图像尺寸和像素颜色仍由 GraphicsView 内的浮层显示。
    void setOriginalImagePath(const QString& imagePath);
public:
    ViewWidgetUI* ui;

private:
    void updateOriginalFooter();

    QString originalFileName;
    double originalZoomScale = 0.0;
};







#endif
