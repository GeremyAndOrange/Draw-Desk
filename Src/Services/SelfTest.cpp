// 文件用途: 平台能力自检实现, 用受控窗口验证 Cloak, 抽屉切换及相关接口
#include "Services/SelfTest.h"

#include <QDebug>
#include <QRegularExpression>
#include <QString>

#include "Models/Drawer.h"
#include "Platform/HotkeyService.h"
#include "Platform/SingleInstance.h"
#include "Platform/WindowApi.h"
#include "Services/DrawerService.h"

namespace {

DrawDesk::Services::SelfTestResult g_result;

// 记录一项检查结果
void Check(const QString &name, bool ok)
{
    ++g_result.total;
    if (ok) {
        ++g_result.passed;
        qInfo().noquote() << "自检通过:" << name;
    } else {
        qWarning().noquote() << "自检失败:" << name;
    }
}

// 记录一条信息, 不参与通过计数
void Note(const QString &text)
{
    qInfo().noquote() << "自检信息:" << text;
}

}

namespace DrawDesk::Services {

SelfTestResult RunPlatformSelfTest(bool trayAvailable, bool windowVisible)
{
    g_result = SelfTestResult{};

    // 窗口枚举
    const auto windows = WindowApi::EnumTopLevelWindows();
    Check(QStringLiteral("枚举顶层窗口"), !windows.isEmpty());
    Note(QStringLiteral("顶层窗口数量: %1").arg(windows.size()));

    const int sampleCount = qMin(5, windows.size());
    for (int i = 0; i < sampleCount; ++i) {
        const auto &info = windows.at(i);
        Note(QStringLiteral("窗口样例 %1: %2 / %3").arg(i + 1).arg(info.processName, info.title));
    }

    // 受控窗口与抽屉切换
    HWND testA = WindowApi::CreateTestWindow(L"DrawDesk 自检窗口 A");
    HWND testB = WindowApi::CreateTestWindow(L"DrawDesk 自检窗口 B");
    Check(QStringLiteral("创建两个测试窗口"), testA != nullptr && testB != nullptr);

    if (testA && testB) {
        // 基础 Cloak 行为
        Check(QStringLiteral("初始状态未隐藏"),
              !WindowApi::IsCloaked(testA) && !WindowApi::IsCloaked(testB));
        Check(QStringLiteral("执行隐藏调用"), WindowApi::SetCloaked(testA, true));
        Check(QStringLiteral("隐藏状态生效"), WindowApi::IsCloaked(testA));
        Note(QStringLiteral("隐藏后 IsWindowVisible 返回: %1")
                 .arg(IsWindowVisible(testA) ? QStringLiteral("真") : QStringLiteral("假")));
        Check(QStringLiteral("执行恢复调用"), WindowApi::SetCloaked(testA, false));
        Check(QStringLiteral("恢复状态生效"), !WindowApi::IsCloaked(testA));

        // 规则匹配
        const WindowApi::WindowInfo infoA = WindowApi::DescribeWindow(testA);
        const WindowApi::WindowInfo infoB = WindowApi::DescribeWindow(testB);

        Models::WindowRule ruleA;
        ruleA.process = infoA.processName;
        ruleA.titlePattern = infoA.title;

        Models::WindowRule ruleB;
        ruleB.process = infoB.processName;
        ruleB.titlePattern = infoB.title;

        Check(QStringLiteral("规则匹配正确"),
              WindowApi::MatchesRule(infoA, ruleA) && !WindowApi::MatchesRule(infoB, ruleA));

        // 抽屉切换, 使用内存配置, 不写配置文件
        DrawerService service;

        Models::Drawer drawerA;
        drawerA.id = QStringLiteral("testA");
        drawerA.name = QStringLiteral("测试抽屉A");
        drawerA.rules.append(ruleA);

        Models::Drawer drawerB;
        drawerB.id = QStringLiteral("testB");
        drawerB.name = QStringLiteral("测试抽屉B");
        drawerB.rules.append(ruleB);

        QVector<Models::Drawer> drawers;
        drawers.append(drawerA);
        drawers.append(drawerB);

        // 同一窗口允许同时匹配多个抽屉: 匹配即可见, 活动抽屉优先
        QVector<Models::Drawer> sharedDrawers = drawers;
        sharedDrawers[1].rules.append(ruleA);
        DrawerService sharedService;
        sharedService.SetDrawersForTest(sharedDrawers);
        sharedService.SwitchTo(1);
        Check(QStringLiteral("同一窗口匹配多个抽屉时保持显示"),
              !WindowApi::IsCloaked(testA) && !WindowApi::IsCloaked(testB));
        sharedService.SwitchTo(0);
        Check(QStringLiteral("多抽屉匹配时只隐藏不匹配活动抽屉的窗口"),
              !WindowApi::IsCloaked(testA) && WindowApi::IsCloaked(testB));
        sharedService.RestoreAll();
        Check(QStringLiteral("多抽屉匹配恢复全部"),
              !WindowApi::IsCloaked(testA) && !WindowApi::IsCloaked(testB));

        service.SetDrawersForTest(drawers);

        service.SwitchTo(1);
        Check(QStringLiteral("切换到抽屉B: A 隐藏, B 显示"),
              WindowApi::IsCloaked(testA) && !WindowApi::IsCloaked(testB));

        service.SwitchTo(0);
        Check(QStringLiteral("切换回抽屉A: A 显示, B 隐藏"),
              !WindowApi::IsCloaked(testA) && WindowApi::IsCloaked(testB));

        service.RestoreAll();
        Check(QStringLiteral("恢复全部: 两个窗口都显示"),
              !WindowApi::IsCloaked(testA) && !WindowApi::IsCloaked(testB));

        // 布局快照: 隐藏期间位置被改变, 切回时应恢复到保存位置
        const int targetX = 420;
        const int targetY = 340;
        MoveWindow(testA, targetX, targetY, 360, 160, TRUE);

        service.SwitchTo(1);
        MoveWindow(testA, 120, 120, 360, 160, TRUE);
        service.SwitchTo(0);

        RECT restored = {};
        GetWindowRect(testA, &restored);
        Check(QStringLiteral("布局快照恢复位置"),
              qAbs(restored.left - targetX) <= 8 && qAbs(restored.top - targetY) <= 8);

        // 布局快照: 最大化状态保持
        ShowWindow(testA, SW_MAXIMIZE);
        service.SwitchTo(1);
        service.SwitchTo(0);
        Check(QStringLiteral("布局快照恢复最大化状态"), IsZoomed(testA) != FALSE);
        ShowWindow(testA, SW_RESTORE);

        // 抽屉管理: 添加, 重命名, 删除, 内存模式不写配置
        const int initialCount = service.DrawerNames().size();
        service.AddDrawer(QStringLiteral("自检抽屉"));
        Check(QStringLiteral("添加抽屉"), service.DrawerNames().size() == initialCount + 1);

        service.RenameDrawer(service.DrawerNames().size() - 1, QStringLiteral("自检改名"));
        Check(QStringLiteral("重命名抽屉"),
              service.DrawerNames().contains(QStringLiteral("自检改名")));

        service.RemoveDrawer(service.DrawerNames().size() - 1);
        Check(QStringLiteral("删除抽屉"), service.DrawerNames().size() == initialCount);

        // 搜索窗口
        const QStringList found = service.SearchWindows(QStringLiteral("自检窗口"));
        Check(QStringLiteral("搜索窗口"), found.size() >= 2);

        Check(QStringLiteral("销毁测试窗口"),
              WindowApi::DestroyTestWindow(testA) && WindowApi::DestroyTestWindow(testB));
    }

    // 单实例互斥
    Check(QStringLiteral("单实例互斥可被检测"), SingleInstance::VerifyDetectable());

    // 热键状态
    Check(QStringLiteral("热键已注册"), HotkeyService::IsRegistered(0));

    // 托盘与主窗口
    Check(QStringLiteral("托盘可用且已显示"), trayAvailable);
    Check(QStringLiteral("主窗口已显示"), windowVisible);

    qInfo().noquote() << QStringLiteral("自检完成: %1/%2 通过")
                             .arg(g_result.passed)
                             .arg(g_result.total);
    return g_result;
}

}