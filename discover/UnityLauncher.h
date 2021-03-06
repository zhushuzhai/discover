/*
 * Copyright 2016 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef KDEVPLATFORM_UNITYLAUNCHER_H
#define KDEVPLATFORM_UNITYLAUNCHER_H

#include <QObject>

class UnityLauncher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString launcherId READ launcherId WRITE setLauncherId)
    Q_PROPERTY(bool progressVisible READ progressVisible WRITE setProgressVisible)
    Q_PROPERTY(int progress READ progress WRITE setProgress)

public:
    explicit UnityLauncher(QObject *parent = nullptr);
    ~UnityLauncher() override;

    QString launcherId() const;
    void setLauncherId(const QString &launcherId);

    bool progressVisible() const;
    void setProgressVisible(bool progressVisible);

    int progress() const;
    void setProgress(int progress);

private:
    void update(const QVariantMap &properties);

    QString m_launcherId;
    bool m_progressVisible = false;
    int m_progress = 0;

};

#endif // KDEVPLATFORM_UNITYLAUNCHER_H
