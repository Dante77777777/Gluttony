#include "mainwindow.h"
#include "LogWidget/LogMessage.h"
#include <QApplication>

void applyQSS()
{
    QFile file(":/style.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet); // 为整个应用程序设置
        file.close();
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    applyQSS();
    LogMessage::getInstance().installMessageHandler();
    qRegisterMetaType<InferenceResult>("InferenceResult");
    w.show();
    return a.exec();
}
