#pragma once

#include <QString>
#include <QList>

struct ReleaseAsset
{
    QString name;
    QString downloadUrl;
    qint64 size = 0;
};
