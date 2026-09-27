// 文件用途: 程序入口, 初始化单实例, 日志, 托盘, 热键与抽屉服务, 加载主界面与悬浮窗
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

#include "Platform/ConfigStore.h"
#include "Platform/HotkeyService.h"
#include "Platform/Log.h"
#include "Platform/SingleInstance.h"
#include "Platform/TrayIcon.h"
#include "Platform/WindowApi.h"
#include "Services/DrawerService.h"
#include "Services/SelfTest.h"
#include "Ui/AppIcon.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("DrawDesk"));
    QCoreApplication::setApplicationName(QStringLiteral("DrawDesk"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.1"));

    // 窗口与任务栏图标
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/DrawDesk/Src/Ui/AppIcon.svg")));

    DrawDesk::Log::Init();
    DrawDesk::ConfigStore::EnsureDefaultConfig();
    qInfo().noquote() << "DrawDesk 启动, 数据目录:" << DrawDesk::Log::DataDir();

    // 单实例检查
    if (!DrawDesk::SingleInstance::Acquire()) {
        qWarning("检测到 DrawDesk 已在运行, 本次启动退出");
        return 0;
    }

    // 前台窗口跟踪, 用于添加窗口到抽屉
    DrawDesk::WindowApi::StartForegroundWatcher();

    // 抽屉服务
    DrawDesk::Services::DrawerService drawerService;
    drawerService.Load();

    // 界面根对象, 加载后赋值, 供托盘菜单回调使用
    QObject *uiRoot = nullptr;

    // 诊断开关: 允许临时跳过托盘或热键初始化
    const bool skipTray = qEnvironmentVariableIsSet("DRAWDESK_NO_TRAY");
    const bool skipHotkey = qEnvironmentVariableIsSet("DRAWDESK_NO_HOTKEY");

    // 托盘图标, 菜单包含恢复全部, 悬浮窗开关与退出
    bool trayReady = false;
    if (!skipTray) {
        const HICON appIcon = DrawDesk::Ui::CreateAppHIcon();
        qInfo().noquote() << "托盘图标已生成:" << (appIcon != nullptr);

        DrawDesk::TrayIcon::Callbacks callbacks;
        callbacks.onShowMainWindow = [&uiRoot]() {
            if (uiRoot)
                QMetaObject::invokeMethod(uiRoot, "openMainWindow");
        };
        callbacks.onToggleFloating = [&uiRoot]() {
            if (uiRoot)
                QMetaObject::invokeMethod(uiRoot, "toggleFloatingWindow");
        };

        callbacks.onQuit = [&uiRoot]() {
            if (!uiRoot || !QMetaObject::invokeMethod(uiRoot, "quitApplication"))
                QCoreApplication::quit();
        };

        trayReady = DrawDesk::TrayIcon::Create(GetModuleHandleW(nullptr), appIcon, callbacks);
    }
    qInfo().noquote() << "托盘状态:" << (trayReady ? "已显示" : "不可用");

    // 注册全局热键, 快捷键配置变化后会自动重新注册
    auto registerHotkeys = [&drawerService, skipHotkey]() {
        DrawDesk::HotkeyService::Stop();
        if (skipHotkey)
            return;

        if (!DrawDesk::HotkeyService::Start()) {
            qWarning("热键服务启动失败");
            return;
        }

        const QString hotkeyPrefix = QStringLiteral("Ctrl+Alt");
        int drawerHotkeyCount = 0;
        for (int i = 0; i < drawerService.DrawerNames().size(); ++i) {
            const QString keyText = drawerService.DrawerHotkey(i);
            if (keyText.isEmpty())
                continue;

            UINT drawerModifiers = 0;
            UINT drawerKey = 0;
            if (!DrawDesk::HotkeyService::ParseHotkey(hotkeyPrefix + QLatin1Char('+') + keyText,
                                                       &drawerModifiers, &drawerKey)) {
                qWarning().noquote() << "抽屉热键无效:" << drawerService.DrawerNames().at(i)
                                     << keyText;
                continue;
            }

            const bool ok = DrawDesk::HotkeyService::Register(
                100 + i, drawerModifiers, drawerKey,
                [&drawerService, i]() { drawerService.SwitchTo(i); });
            if (ok)
                ++drawerHotkeyCount;
            qInfo().noquote() << QStringLiteral("热键 抽屉%1 注册:").arg(i + 1)
                              << (ok ? "成功" : "失败");
        }
        qInfo().noquote() << "热键 抽屉注册数:" << drawerHotkeyCount;

        UINT addModifiers = 0;
        UINT addKey = 0;
        const bool addOk =
            DrawDesk::HotkeyService::ParseHotkey(drawerService.HotkeyAddWindow(), &addModifiers,
                                                 &addKey)
            && DrawDesk::HotkeyService::Register(
                0, addModifiers, addKey, [&drawerService]() {
                    drawerService.AddForegroundWindowToDrawer(drawerService.ActiveIndex());
                });
        qInfo().noquote() << "热键 加入窗口 注册:" << (addOk ? "成功" : "失败");

        UINT removeModifiers = 0;
        UINT removeKey = 0;
        const bool removeOk =
            DrawDesk::HotkeyService::ParseHotkey(drawerService.HotkeyRemoveWindow(),
                                                 &removeModifiers, &removeKey)
            && DrawDesk::HotkeyService::Register(
                1, removeModifiers, removeKey, [&drawerService]() {
                    drawerService.RemoveForegroundWindowFromDrawer(drawerService.ActiveIndex());
                });
        qInfo().noquote() << "热键 移出窗口 注册:" << (removeOk ? "成功" : "失败");    };

    registerHotkeys();
    QObject::connect(&drawerService, &DrawDesk::Services::DrawerService::hotkeySettingsChanged,
                     &app, registerHotkeys);
    // 抽屉数量变化时同步注册对应数量的切换热键
    QObject::connect(&drawerService, &DrawDesk::Services::DrawerService::drawersChanged, &app,
                     registerHotkeys);

    // 工具条界面
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("drawerService"), &drawerService);
    engine.loadFromModule("DrawDesk", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCritical("主界面加载失败");
        DrawDesk::HotkeyService::Stop();
        DrawDesk::TrayIcon::Destroy();
        return 1;
    }
    qInfo().noquote() << "主界面已加载, 根对象数量:" << engine.rootObjects().size();

    uiRoot = engine.rootObjects().first();

    // 启动时打开主窗口, 悬浮窗由托盘菜单控制显示
    if (uiRoot)
        QMetaObject::invokeMethod(uiRoot, "openMainWindow");

    // 稍后记录主窗口可见状态
    QTimer::singleShot(500, &app, [&uiRoot]() {
        const bool mainVisible = uiRoot && uiRoot->property("mainWindowVisible").toBool();
        qInfo().noquote() << "主窗口可见:" << mainVisible;
    });

    if (qEnvironmentVariableIsSet("DRAWDESK_SELFTEST")) {
        // 自检模式: 执行平台自检后按结果退出
        QTimer::singleShot(700, &app, [&app, &uiRoot, trayReady]() {
            const bool mainVisible = uiRoot && uiRoot->property("mainWindowVisible").toBool();
            const auto result = DrawDesk::Services::RunPlatformSelfTest(trayReady, mainVisible);
            app.exit(result.Ok() ? 0 : 3);
        });
    } else if (qEnvironmentVariableIsSet("DRAWDESK_SMOKE")) {
        // 冒烟模式: 三秒后自动退出
        QTimer::singleShot(3000, &app, &QCoreApplication::quit);
    }

    const int exitCode = app.exec();

    // 退出前恢复全部被隐藏的窗口
    drawerService.RestoreAll();
    DrawDesk::HotkeyService::Stop();
    DrawDesk::TrayIcon::Destroy();
    qInfo().noquote() << "DrawDesk 退出, 退出码:" << exitCode;
    return exitCode;
}