#ifndef LOGMESSAGE_H
#define LOGMESSAGE_H


#include <QObject>
#include <QWidget>
#include <QFile>
#include <QString>
#include <QDebug>
#include <QTextStream>
#include <QMutex>
#include <QDir>
#include <QDateTime>

#define QDEBUG(message) qDebug()<<" [FUNCTION]: " <<__FUNCTION__<< " [LINE]: " <<__LINE__<<  " [LOG] " << message;
#define QWARNING(message) qWarning()<<" [FUNCTION]: " <<__FUNCTION__<< " [LINE]: " <<__LINE__<<  " [LOG] " << message;
#define QCRITICAL(message) qCritical()<<" [FUNCTION]: " <<__FUNCTION__<< " [LINE]: " <<__LINE__<<  " [LOG] " << message;
#define QFATAL(message) qFatal()<<" [FUNCTION]: " <<__FUNCTION__<< " [LINE]: " <<__LINE__<<  " [LOG] " << message;


class LogMessage : public QObject
{
    Q_OBJECT
public:
    static LogMessage& getInstance()
    {
        static LogMessage instance;
        return instance;
    }
    LogMessage(LogMessage&) = delete;
    LogMessage& operator=(LogMessage&) = delete;

    LogMessage(LogMessage&&) = delete;
    LogMessage& operator=(LogMessage&&) = delete;

    QString logPath();
    QString logName();

    void installMessageHandler();
    void uninstallMessageHandler();
private:
    explicit LogMessage(QObject* parent = nullptr);
    ~LogMessage();

signals:
    void sigDebugHtmlData(const QString &);
    void sigDebugStrData(const QString &);

};










#endif
