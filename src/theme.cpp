#include "theme.h"

#include <QDebug>
#include <QColor>
#include <QGuiApplication>
#include <QStyleHints>
#include <QEvent>
#include <QPalette>
#include <cmath>

const QString Theme::DEFAULT_THEME_COLOR = "#248b00";

Theme::Theme(QObject *parent) :
    QObject(parent),
    mColor1("#000000"), mColor2(DEFAULT_THEME_COLOR), mColor3("#4cb328"),
    mColor4("#555555"), mColor5("#888888"), mColor6("#ffffff"),
    mColor7("#cccccc"), mColor8("#eeeeee"), mColor9("#ccffffff")
{
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        auto *hints = QGuiApplication::styleHints();
        mSystemScheme = hints->colorScheme();
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
            mSystemScheme = scheme;
            if (mMode == "system") refreshPalette();
        });
        qApp->installEventFilter(this);
    }
    refreshPalette();
}

void Theme::setThemeColor(QString color)
{
    const QColor c = QColor::fromString(color);
    if (!c.isValid()) return;
    if (mColor2 == c.name()) return;
    mColor2 = c.name();
    refreshPalette();
    emit color2Changed();
}

void Theme::setMode(const QString &mode)
{
    if ((mode != "system" && mode != "light" && mode != "dark") || mMode == mode) return;
    mMode = mode;
    refreshPalette();
    emit modeChanged();
}

bool Theme::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange && mMode == "system"
        && mSystemScheme == Qt::ColorScheme::Unknown) refreshPalette();
    return QObject::eventFilter(object, event);
}

void Theme::refreshPalette()
{
    const QString previous[] = {mColor1, mColor3, mColor4, mColor5, mColor6, mColor7, mColor8, mColor9};
    bool systemDark = mSystemScheme == Qt::ColorScheme::Dark;
    if (mSystemScheme == Qt::ColorScheme::Unknown
        && qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        systemDark = QGuiApplication::palette().color(QPalette::Window).lightnessF() < 0.5;
    mDark = mMode == "dark" || (mMode == "system" && systemDark);
    mColor1 = mDark ? "#f0f0f0" : "#000000";
    mColor4 = mDark ? "#e0e0e0" : "#555555";
    mColor5 = mDark ? "#b5b5b5" : "#888888";
    mColor6 = mDark ? "#181a1b" : "#ffffff";
    mColor7 = mDark ? "#65696c" : "#cccccc";
    mColor8 = mDark ? "#303437" : "#eeeeee";
    mColor9 = mDark ? "#cc181a1b" : "#ccffffff";
    QColor c(mColor2);
    c.setRed(qMin(c.red() + 40, 255));
    c.setGreen(qMin(c.green() + 40, 255));
    c.setBlue(qMin(c.blue() + 40, 255));
    // Keep accent text legible on the dark canvas, including dark custom accents.
    auto luminance = [](const QColor &value) {
        auto channel = [](double n) { return n <= 0.04045 ? n / 12.92 : std::pow((n + 0.055) / 1.055, 2.4); };
        return 0.2126 * channel(value.redF()) + 0.7152 * channel(value.greenF()) + 0.0722 * channel(value.blueF());
    };
    if (mDark) while ((luminance(c) + 0.05) / (luminance(QColor(mColor6)) + 0.05) < 4.5)
        c = c.lighter(115);
    mColor3 = c.name();
    if (previous[0] != mColor1) emit color1Changed();
    if (previous[1] != mColor3) emit color3Changed();
    if (previous[2] != mColor4) emit color4Changed();
    if (previous[3] != mColor5) emit color5Changed();
    if (previous[4] != mColor6) emit color6Changed();
    if (previous[5] != mColor7) emit color7Changed();
    if (previous[6] != mColor8) emit color8Changed();
    if (previous[7] != mColor9) emit color9Changed();
    emit paletteChanged();
}

float Theme::getHue(QString color) {

    QColor c = QColor::fromString(color);
    QColor converted = c.toHsv();
    return converted.hsvHueF();
}

float Theme::getSaturation(QString color) {

    QColor c = QColor::fromString(color);
    QColor converted = c.toHsv();
    return converted.hsvSaturationF();
}

float Theme::getLightness(QString color) {

    QColor c = QColor::fromString(color);
    QColor converted = c.toHsv();
    return converted.valueF();
}
