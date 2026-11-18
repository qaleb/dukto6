#include <QtTest>
#include <QGuiApplication>
#include <QStyleHints>
#include "theme.h"

class ThemeTest : public QObject
{
    Q_OBJECT
    void systemScheme(Qt::ColorScheme scheme)
    {
        // Model a platform notification without changing the user's OS preference.
        QVERIFY(QMetaObject::invokeMethod(QGuiApplication::styleHints(), "colorSchemeChanged",
                                         Qt::DirectConnection, Q_ARG(Qt::ColorScheme, scheme)));
    }
private slots:
    void systemTracksPlatformChanges()
    {
        Theme theme;
        QCOMPARE(theme.mode(), QString("system"));
        systemScheme(Qt::ColorScheme::Light);
        QVERIFY(!theme.dark());
        systemScheme(Qt::ColorScheme::Dark);
        QVERIFY(theme.dark());
        systemScheme(Qt::ColorScheme::Light);
        QVERIFY(!theme.dark());
    }
    void explicitChoiceSurvivesPlatformChange()
    {
        Theme theme;
        theme.setMode("light");
        systemScheme(Qt::ColorScheme::Dark);
        QVERIFY(!theme.dark());
        theme.setMode("dark");
        systemScheme(Qt::ColorScheme::Light);
        QVERIFY(theme.dark());
        theme.setMode("system");
        QVERIFY(!theme.dark());
    }
    void modeSwitchKeepsAccentAndPanelText()
    {
        Theme theme;
        theme.setThemeColor("#123456");
        theme.setMode("light");
        const auto lightSurface = theme.color6();
        theme.setMode("dark");
        QCOMPARE(theme.color2(), QString("#123456"));
        QVERIFY(theme.color6() != lightSurface);
        QCOMPARE(theme.accentText(), QString("#ffffff"));
        QVERIFY(QColor(theme.color4()).lightnessF() > 0.75);
        QVERIFY(QColor(theme.color6()).lightnessF() < 0.15);
        theme.setMode("light");
        QCOMPARE(theme.color2(), QString("#123456"));
        QCOMPARE(theme.color6(), lightSurface);
    }
    void veryDarkAccentRemainsLegible()
    {
        Theme theme;
        theme.setThemeColor("#000000");
        theme.setMode("dark");
        QVERIFY(QColor(theme.accentForeground()).lightnessF() > 0.4);
        QCOMPARE(theme.color2(), QString("#000000"));
    }
    void invalidPreferencesDoNotReplaceCurrentChoice()
    {
        Theme theme;
        theme.setMode("dark");
        QSignalSpy changed(&theme, &Theme::modeChanged);
        const auto accent = theme.color2();
        theme.setMode("invalid");
        theme.setThemeColor("not a color");
        QCOMPARE(theme.mode(), QString("dark"));
        QCOMPARE(theme.color2(), accent);
        QCOMPARE(changed.count(), 0);
    }
};

QTEST_MAIN(ThemeTest)
#include "theme_tests.moc"
