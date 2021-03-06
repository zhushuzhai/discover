/***************************************************************************
 *   Copyright © 2013 Aleix Pol Gonzalez <aleixpol@blue-systems.com>       *
 *   Copyright © 2017 Jan Grulich <jgrulich@redhat.com>                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "FlatpakJobTransaction.h"
#include "FlatpakBackend.h"
#include "FlatpakResource.h"
#include "FlatpakTransactionThread.h"

#include <QDebug>
#include <QTimer>

extern "C" {
#include <flatpak.h>
#include <gio/gio.h>
#include <glib.h>
}

FlatpakJobTransaction::FlatpakJobTransaction(FlatpakResource *app, Role role, bool delayStart)
    : FlatpakJobTransaction(app, nullptr, role, delayStart)
{
}

FlatpakJobTransaction::FlatpakJobTransaction(FlatpakResource *app, FlatpakResource *runtime, Transaction::Role role, bool delayStart)
    : Transaction(app->backend(), app, role, {})
    , m_app(app)
    , m_runtime(runtime)
{
    setCancellable(true);
    setStatus(QueuedStatus);

    if (!delayStart) {
        QTimer::singleShot(0, this, &FlatpakJobTransaction::start);
    }
}

FlatpakJobTransaction::~FlatpakJobTransaction()
{
    for(auto job : m_jobs) {
        if (!job->isFinished()) {
            connect(job, &QThread::finished, job, &QObject::deleteLater);
        } else
            delete job;
    }
}

void FlatpakJobTransaction::cancel()
{
    //note it's possible to have a job cancelled before it ever started as
    //sometimes we're delaying the start until we have the runtime
    foreach (const QPointer<FlatpakTransactionThread> &job, m_jobs) {
        job->cancel();
    }
    setStatus(CancelledStatus);
}

void FlatpakJobTransaction::setRuntime(FlatpakResource *runtime)
{
    m_runtime = runtime;
}

void FlatpakJobTransaction::start()
{
    setStatus(CommittingStatus);
    if (m_runtime) {
        QPointer<FlatpakTransactionThread> job = new FlatpakTransactionThread(m_runtime, {}, role());
        connect(job, &FlatpakTransactionThread::finished, this, &FlatpakJobTransaction::onJobFinished);
        connect(job, &FlatpakTransactionThread::progressChanged, this, &FlatpakJobTransaction::onJobProgressChanged);
        m_jobs << job;

        processRelatedRefs(m_runtime);
    }

    // App job will be added everytime
    m_appJob = new FlatpakTransactionThread(m_app, {}, role());
    connect(m_appJob, &FlatpakTransactionThread::finished, this, &FlatpakJobTransaction::onJobFinished);
    connect(m_appJob, &FlatpakTransactionThread::progressChanged, this, &FlatpakJobTransaction::onJobProgressChanged);
    m_jobs << m_appJob;

    processRelatedRefs(m_app);


    // Now start all the jobs together
    foreach (const QPointer<FlatpakTransactionThread> &job, m_jobs) {
        job->start();
    }
}

void FlatpakJobTransaction::processRelatedRefs(FlatpakResource* resource)
{
    g_autoptr(GPtrArray) refs = nullptr;
    g_autoptr(GError) error = nullptr;
    g_autoptr(GCancellable) cancellable = g_cancellable_new();;
    QList<FlatpakResource> additionalResources;

    g_autofree gchar *ref = g_strdup_printf ("%s/%s/%s/%s",
                           resource->typeAsString().toUtf8().constData(),
                           resource->flatpakName().toUtf8().constData(),
                           resource->arch().toUtf8().constData(),
                           resource->branch().toUtf8().constData());

    if (role() == Transaction::Role::InstallRole) {
        if (resource->state() == AbstractResource::Upgradeable) {
            refs = flatpak_installation_list_installed_related_refs_sync(resource->installation(), resource->origin().toUtf8().constData(), ref, cancellable, &error);
            if (error) {
                qWarning() << "Failed to list installed related refs for update: " << error->message;
            }
        } else {
            refs = flatpak_installation_list_remote_related_refs_sync(resource->installation(), resource->origin().toUtf8().constData(), ref, cancellable, &error);
            if (error) {
                qWarning() << "Failed to list related refs for installation: " << error->message;
            }
        }
    } else if (role() == Transaction::Role::RemoveRole) {
        refs = flatpak_installation_list_installed_related_refs_sync(resource->installation(), resource->origin().toUtf8().constData(), ref, cancellable, &error);
        if (error) {
            qWarning() << "Failed to list installed related refs for removal: " << error->message;
        }
    }

    if (refs) {
        for (uint i = 0; i < refs->len; i++) {
            FlatpakRef *flatpakRef = FLATPAK_REF(g_ptr_array_index(refs, i));
            if (flatpak_related_ref_should_download(FLATPAK_RELATED_REF(flatpakRef))) {
                QPointer<FlatpakTransactionThread> job = new FlatpakTransactionThread(resource, QPair<QString, uint>(QString::fromUtf8(flatpak_ref_get_name(flatpakRef)), flatpak_ref_get_kind(flatpakRef)), role());
                connect(job, &FlatpakTransactionThread::finished, this, &FlatpakJobTransaction::onJobFinished);
                connect(job, &FlatpakTransactionThread::progressChanged, this, &FlatpakJobTransaction::onJobProgressChanged);
                // Add to the list of all jobs
                m_jobs << job;
            }
        }
    }
}

void FlatpakJobTransaction::onJobFinished()
{
    FlatpakTransactionThread *job = static_cast<FlatpakTransactionThread*>(sender());

    if (job != m_appJob) {
        if (!job->result()) {
            Q_EMIT passiveMessage(job->errorMessage());
        }

        // Mark runtime as installed
        if (m_runtime && job->app()->flatpakName() == m_runtime->flatpakName() && !job->isRelated() && role() != Transaction::Role::RemoveRole) {
            if (job->result()) {
                m_runtime->setState(AbstractResource::Installed);
            }
        }
    }

    foreach (const QPointer<FlatpakTransactionThread> &job, m_jobs) {
        if (job->isRunning()) {
            return;
        }
    }

    // No other job is running → finish transaction
    finishTransaction();
}

void FlatpakJobTransaction::onJobProgressChanged(int progress)
{
    Q_UNUSED(progress);

    int total = 0;

    // Count progress from all the jobs
    foreach (const QPointer<FlatpakTransactionThread> &job, m_jobs) {
        total += job->progress();
    }

    setProgress(total / m_jobs.count());
}

void FlatpakJobTransaction::finishTransaction()
{
    if (m_appJob->result()) {
        AbstractResource::State newState = AbstractResource::None;
        switch(role()) {
        case InstallRole:
        case ChangeAddonsRole:
            newState = AbstractResource::Installed;
            break;
        case RemoveRole:
            newState = AbstractResource::None;
            break;
        }

        m_app->setState(newState);

        setStatus(DoneStatus);
    } else {
        setStatus(DoneWithErrorStatus);
    }
}
