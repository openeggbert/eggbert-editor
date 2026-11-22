// SPDX-License-Identifier: GPL-3.0-only
#include "appearance.hpp"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyle>
#include <QStyleFactory>

namespace eggbert {

namespace {

Theme activeTheme = Theme::system;
bool startupCaptured = false;
QPalette startupPalette;
QString startupStyle;

void captureStartup() {
    if (startupCaptured) {
        return;
    }
    startupCaptured = true;
    startupPalette = QApplication::palette();
    startupStyle = QApplication::style()->objectName();
}

QPalette darkPalette() {
    QPalette palette;
    const QColor window(45, 47, 53);
    const QColor base(30, 32, 37);
    const QColor text(230, 232, 236);
    const QColor disabled(130, 133, 140);
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, QColor(38, 40, 46));
    palette.setColor(QPalette::ToolTipBase, QColor(60, 63, 70));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, QColor(58, 61, 68));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, QColor(255, 90, 90));
    palette.setColor(QPalette::Link, QColor(110, 170, 255));
    palette.setColor(QPalette::Highlight, QColor(60, 120, 215));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::PlaceholderText, disabled);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, disabled);
    }
    return palette;
}

QPalette lightPalette() {
    QPalette palette;
    const QColor text(25, 27, 31);
    const QColor disabled(140, 143, 150);
    palette.setColor(QPalette::Window, QColor(240, 241, 243));
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(246, 247, 249));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 225));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, QColor(232, 234, 237));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, QColor(200, 30, 30));
    palette.setColor(QPalette::Link, QColor(20, 90, 200));
    palette.setColor(QPalette::Highlight, QColor(50, 110, 210));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::PlaceholderText, disabled);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, disabled);
    }
    return palette;
}

} // namespace

QString themeKey(Theme theme) {
    switch (theme) {
    case Theme::light:
        return QStringLiteral("light");
    case Theme::dark:
        return QStringLiteral("dark");
    case Theme::system:
        break;
    }
    return QStringLiteral("system");
}

Theme themeFromKey(const QString &key) {
    if (key == QLatin1String("light")) {
        return Theme::light;
    }
    if (key == QLatin1String("dark")) {
        return Theme::dark;
    }
    return Theme::system;
}

void applyTheme(Theme theme) {
    captureStartup();
    activeTheme = theme;
    if (theme == Theme::system) {
        if (auto *style = QStyleFactory::create(startupStyle)) {
            QApplication::setStyle(style);
        }
        QApplication::setPalette(startupPalette);
        return;
    }
    // Fusion follows the palette on every platform.
    if (auto *style = QStyleFactory::create(QStringLiteral("Fusion"))) {
        QApplication::setStyle(style);
    }
    QApplication::setPalette(theme == Theme::dark ? darkPalette() : lightPalette());
}

Theme currentTheme() {
    return activeTheme;
}

QIcon applicationIcon() {
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const double unit = size / 16.0;
        // A rounded tile with a 3 x 3 block pattern and an egg shape.
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(38, 70, 120));
        painter.drawRoundedRect(QRectF(0.5 * unit, 0.5 * unit, 15 * unit, 15 * unit), 3 * unit,
                                3 * unit);
        painter.setBrush(QColor(90, 190, 120));
        painter.drawRect(QRectF(2 * unit, 11 * unit, 12 * unit, 3 * unit));
        painter.setBrush(QColor(200, 150, 70));
        painter.drawRect(QRectF(2 * unit, 8 * unit, 4 * unit, 3 * unit));
        painter.drawRect(QRectF(10 * unit, 8 * unit, 4 * unit, 3 * unit));
        painter.setBrush(QColor(250, 240, 215));
        painter.drawEllipse(QRectF(5.5 * unit, 2 * unit, 5 * unit, 6.5 * unit));
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace eggbert
