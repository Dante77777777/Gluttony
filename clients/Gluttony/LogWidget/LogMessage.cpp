#include "LogMessage.h"


void outputLog(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QMutex mutex;
    QMutexLocker<QMutex> lock(&mutex);

    QString text;
    QString color = "red";

    switch (type) {
    case QtDebugMsg:
    {
        text = QString("[Debug]");
        color = "darkblue";
        break;
    }
    case QtWarningMsg:
    {
        text = QString("[Warning]");
        color = "darkorange";
        break;
    }
    case QtFatalMsg:
    {
        text = QString("[FATAL]");
        color = "red";
        break;
    }
    case QtCriticalMsg:
    {
        text = QString("[Critical]");
        color = "red";
        break;
    }
    default:
    {
        text = QString("[DEFAULT]");
        color = "black";
        break;
    }
    }

    LogMessage& log = LogMessage::getInstance();
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString currentDate = QString("[%1]").arg(currentDateTime);

    QString message = QString("%1%2%3").arg(currentDate).arg(text).arg(msg);
    QFile file(log.logPath() + log.logName());

    QString messageHTML = QString("<font color=%1>" + message + "</font>").arg(color);

    file.open(QIODevice::WriteOnly | QIODevice::Append);

    QTextStream textStream(&file);
    textStream << message << "\r\n";
    file.flush();
    file.close();

    emit log.sigDebugHtmlData(messageHTML);
    emit log.sigDebugStrData(message);

    if(file.size() < 1024*1024)
    {
        return ;
    }

    for(int i = 1; i < 100; i++)
    {
        QString filename = QString("%1/log_%2").arg(log.logPath()).arg(i);
        QFile logFile(filename);
        if(logFile.size() < 4)
        {
            file.rename(filename);
            return ;
        }
    }
}


LogMessage::LogMessage(QObject* parent)
    :QObject(parent)
{

}

LogMessage::~LogMessage()
{

}


QString LogMessage::logName()
{
    return "log.text";
}


QString LogMessage::logPath()
{
    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QDir dir(QString("log/%1").arg(currentDate));

    if(!dir.exists())
    {
        dir.mkpath("./");
    }
    return dir.path() + "/";
}


void LogMessage::installMessageHandler()
{
    qInstallMessageHandler(outputLog);
}


void LogMessage::uninstallMessageHandler()
{
    qInstallMessageHandler(nullptr);
}
