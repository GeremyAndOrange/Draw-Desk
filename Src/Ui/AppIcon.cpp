// 文件用途: 应用图标实现, 绘制 SVG 并转换为 Windows 图标句柄
#include "Ui/AppIcon.h"

#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <cstring>

namespace {

constexpr const char *SVG_PATH = ":/qt/qml/DrawDesk/Src/Ui/AppIcon.svg";
constexpr int ICON_SIZE = 32;

// 绘制图标图像, SVG 不可用时退化为简单图形
QImage RenderIconImage()
{
    QImage image(ICON_SIZE, ICON_SIZE, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QSvgRenderer renderer(QString::fromLatin1(SVG_PATH));
    if (renderer.isValid()) {
        renderer.render(&painter);
        painter.end();
        return image;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x4C, 0x8D, 0xFF));
    painter.drawRoundedRect(QRectF(2, 2, 28, 28), 7, 7);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(QRectF(7, 9, 18, 6), 2, 2);
    painter.drawRoundedRect(QRectF(7, 17, 18, 6), 2, 2);
    painter.end();
    return image;
}

// 把图像转换为图标句柄
HICON ToHIcon(const QImage &image)
{
    const QImage source = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int width = source.width();
    const int height = source.height();

    BITMAPV5HEADER header = {};
    header.bV5Size = sizeof(header);
    header.bV5Width = width;
    header.bV5Height = -height;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;

    HDC deviceContext = GetDC(nullptr);
    void *pixels = nullptr;
    HBITMAP colorBitmap = CreateDIBSection(deviceContext, reinterpret_cast<BITMAPINFO *>(&header),
                                           DIB_RGB_COLORS, &pixels, nullptr, 0);
    ReleaseDC(nullptr, deviceContext);

    if (!colorBitmap || !pixels) {
        if (colorBitmap)
            DeleteObject(colorBitmap);
        return nullptr;
    }

    std::memcpy(pixels, source.constBits(), static_cast<size_t>(width) * height * 4);

    HBITMAP maskBitmap = CreateBitmap(width, height, 1, 1, nullptr);
    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = colorBitmap;
    iconInfo.hbmMask = maskBitmap;

    HICON icon = CreateIconIndirect(&iconInfo);

    DeleteObject(colorBitmap);
    if (maskBitmap)
        DeleteObject(maskBitmap);
    return icon;
}

}

namespace DrawDesk::Ui {

HICON CreateAppHIcon()
{
    return ToHIcon(RenderIconImage());
}

}
