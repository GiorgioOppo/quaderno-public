#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

QList<QByteArray> renderEpubPages(const QString &path, QString &error);
