// 文件用途: 日志模块实现, 把 Qt 消息写入数据目录下的日志文件
#include "Platform/Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>

namespace {

QMutex g_mutex;
QFile *g_file = nullptr;
QTextStream *g_stream = nullptr;

// 把消息级别转换为固定文本
const char *LevelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "INFO";
}

// 写入一行日志并立即落盘
void WriteLine(QtMsgType type, const QString &text)
{
    QMutexLocker locker(&g_mutex);
    if (!g_stream)
        return;

    const QString line = QStringLiteral("%1 [%2] %3")
                             .arg(QDateTime::currentDateTime().toString(
                                      QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  QString::fromLatin1(LevelName(type)), text);

    *g_stream << line << '\n';
    g_stream->flush();
}

// 处理 Qt 消息, 写入文件并输出到标准错误
void MessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    WriteLine(type, msg);
    std::fprintf(stderr, "%s\n", qUtf8Printable(msg));
}

}

namespace DrawDesk::Log {

QString DataDir()
{
    const QString overrideDir = qEnvironmentVariable("DRAWDESK_DATA_DIR");
    if (!overrideDir.isEmpty())
        return overrideDir;

    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

void Init()
{
    const QString logDir = DataDir() + QStringLiteral("/Logs");
    QDir().mkpath(logDir);

    const QString logFile = logDir + QStringLiteral("/DrawDesk-")
                            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd"))
                            + QStringLiteral(".log");

    g_file = new QFile(logFile);
    if (g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        g_stream = new QTextStream(g_file);
        g_stream->setEncoding(QStringConverter::Utf8);
    }

    qInstallMessageHandler(MessageHandler);
}

}
