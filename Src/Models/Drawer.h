// 文件用途: 抽屉数据模型定义
#pragma once

#include <QRect>
#include <QString>
#include <QVector>

namespace DrawDesk::Models {

// 窗口匹配规则, 进程名精确匹配, 标题按字面或正则匹配;标题为空时只匹配进程
struct WindowRule {
    QString process;
    QString titlePattern;
    int processId = 0;
    bool useRegex = false;
};

// 窗口布局快照
struct WindowLayout {
    QString process;
    QString titlePattern;
    int processId = 0;
    QRect rect;
    int showCommand = 1;
    bool useRegex = false;
};

// 抽屉, 代表一个任务场景
struct Drawer {
    QString id;
    QString name;
    QString hotkey;   // 切换键位, 与全局前缀组合使用, 空表示不设置
    QVector<WindowRule> rules;
    QVector<WindowLayout> layout;
};

}