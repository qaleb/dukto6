#include "recentlistitemmodel.h"

#include <QDateTime>
#include <QLocale>

RecentListItemModel::RecentListItemModel() :
    QStandardItemModel(NULL)
{
    QHash<int, QByteArray> roleNames;
    roleNames[Name] = "name";
    roleNames[Value] = "value";
    roleNames[Type] = "type";
    roleNames[TypeIcon] = "typeIcon";
    roleNames[DateTime] = "dateTime";
    roleNames[Sender] = "sender";
    roleNames[Size] = "size";
    setItemRoleNames(roleNames);
}

void RecentListItemModel::addRecent(QString name, QString value, QString type, QString sender, qint64 size)
{
    QStandardItem* it = new QStandardItem();

    // Format timestamp using QLocale
    QDateTime now = QDateTime::currentDateTime();
    QString datetime = QLocale().toString(now, QLocale::ShortFormat);

    // Convert size data
    QString sizeFormatted;
    if (size < 1024)
        sizeFormatted = QString::number(size) + " B";
    else if (size < 1048576)
        sizeFormatted = QString::number(size * 1.0 / 1024, 'f', 1) + " KB";
    else
        sizeFormatted = QString::number(size * 1.0 / 1048576, 'f', 1) + " MB";

    // Icon
    if (type == "text")
        it->setData("qrc:/src/assets/RecentText.svg", RecentListItemModel::TypeIcon);
    else if (type == "file")
        it->setData("qrc:/src/assets/RecentFile.svg", RecentListItemModel::TypeIcon);
    else
        it->setData("qrc:/src/assets/RecentFiles.svg", RecentListItemModel::TypeIcon);

    it->setData(name, RecentListItemModel::Name);
    it->setData(value, RecentListItemModel::Value);
    it->setData(type, RecentListItemModel::Type);
    it->setData(datetime, RecentListItemModel::DateTime);
    it->setData(sender, RecentListItemModel::Sender);
    it->setData(sizeFormatted, RecentListItemModel::Size);
    insertRow(0, it);
    qint64 retained = 0;
    for (int row = 0; row < rowCount(); ++row)
        retained += qint64(item(row)->data(Value).toString().size()) * sizeof(QChar);
    while (rowCount() > 100 || (retained > 32 * 1024 * 1024 && rowCount() > 0)) {
        retained -= qint64(item(rowCount() - 1)->data(Value).toString().size()) * sizeof(QChar);
        removeRow(rowCount() - 1);
    }
}
