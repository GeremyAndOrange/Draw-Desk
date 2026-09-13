// 文件用途: 配置模块接口, 提供配置路径, 默认配置与抽屉读写
#pragma once

#include <QString>
#include <QVector>

#include "Models/Drawer.h"

namespace DrawDesk::ConfigStore {

// 返回配置文件完整路径
QString ConfigPath();

// 返回窗口隐藏状态文件路径, 供恢复脚本读取
QString CloakedStatePath();

// 若配置文件不存在, 写入默认配置
void EnsureDefaultConfig();

// 返回默认抽屉列表
QVector<Models::Drawer> DefaultDrawers();

// 读取抽屉列表, 文件缺失或列表为空时返回默认抽屉
QVector<Models::Drawer> LoadDrawers();

// 保存抽屉列表, 保留配置中的其他字段
bool SaveDrawers(const QVector<Models::Drawer> &drawers);

// 管理窗口尺寸, 0 表示未设置
int ManagerWindowWidth();
int ManagerWindowHeight();

// 保存管理窗口尺寸
void SetManagerWindowSize(int width, int height);

// 快捷键配置, 前缀固定为 Ctrl+Alt
QString HotkeyAddWindow();
QString HotkeyRemoveWindow();
void SetWindowHotkeys(const QString &addWindow, const QString &removeWindow);

// 悬浮窗位置, -1 表示未设置
int FloatingWindowX();
int FloatingWindowY();

// 保存悬浮窗位置
void SetFloatingPosition(int x, int y);

}