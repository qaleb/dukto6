#pragma once
#include <QString>

namespace DesktopIntegration {
QString shellQuote(const QString &argument);
QString desktopQuote(const QString &argument);
bool install(const QString &executable, QString *error);
bool remove(QString *error);
void ensureInstalled(const QString &executable);
}
