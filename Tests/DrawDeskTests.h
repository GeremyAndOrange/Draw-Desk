// 文件用途: 单元测试声明, 覆盖热键解析, 规则匹配与配置序列化
#pragma once

#include <QObject>

class DrawDeskTests : public QObject
{
    Q_OBJECT

private slots:
    void hotkeyParsing();
    void ruleMatching();
    void configRoundTrip();
    void legacyConfigMigration();
    void todoOperations();
};