#ifndef DETECTIONRESULT_H
#define DETECTIONRESULT_H

#include "DetectionResultUI.h"
#include "DetectionResultModel.h"



class DetectionResult : public QWidget
{
    Q_OBJECT
public:
    explicit DetectionResult(QWidget* parent = nullptr);
private:
    void setup();

private:
    DetectionResultUI* ui;
    DetectionResultModel* model;
};






#endif
