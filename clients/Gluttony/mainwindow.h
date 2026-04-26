#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QBoxLayout>
#include <QFile>

#include "ToolBar/ToolBar.h"
#include "ModelWidget/ModelWidget.h"
#include "ParamWidget/ParamWidget.h"


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setupUI();
private:
    QWidget* centralWidget;
    ToolBar* topToolBar;
    ModelWidget* modelWidget;
    ParamWidget* paramWidget;
    QHBoxLayout* mainLayout;
    QVBoxLayout* leftLayout;
    QVBoxLayout* rightLayout;
    QVBoxLayout* dataLayout;
    QHBoxLayout* viewLayout;

};
#endif // MAINWINDOW_H
