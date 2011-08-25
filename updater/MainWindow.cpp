#include "MainWindow.h"

// Qt includes
#include <QApplication>
#include <QtCore/QTimer>
#include <QtGui/QVBoxLayout>

// KDE includes
#include <KAction>
#include <KActionCollection>
#include <KDebug>

// LibQApt includes
#include <LibQApt/Backend>

// Own includes
#include "ProgressWidget.h"
#include "UpdaterWidget.h"

MainWindow::MainWindow()
    : MuonMainWindow()
{
    initGUI();
    QTimer::singleShot(10, this, SLOT(initObject()));
}

void MainWindow::initGUI()
{
    setWindowTitle(i18nc("@title:window", "Software Updates"));

    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

    m_progressWidget = new ProgressWidget(mainWidget);
    m_progressWidget->hide();

    m_updaterWidget = new UpdaterWidget(mainWidget);
    connect(this, SIGNAL(backendReady(QApt::Backend *)),
            m_updaterWidget, SLOT(setBackend(QApt::Backend *)));

    mainLayout->addWidget(m_progressWidget);
    mainLayout->addWidget(m_updaterWidget);

    setupActions();

    mainWidget->setLayout(mainLayout);
    setCentralWidget(mainWidget);
}

void MainWindow::initObject()
{
    MuonMainWindow::initObject();
    setActionsEnabled(); //Get initial enabled/disabled state

    connect(m_backend, SIGNAL(downloadProgress(int, int, int)),
            m_progressWidget, SLOT(updateDownloadProgress(int, int, int)));
    connect(m_backend, SIGNAL(commitProgress(const QString &, int)),
            m_progressWidget, SLOT(updateCommitProgress(const QString &, int)));
}

void MainWindow::setupActions()
{
    MuonMainWindow::setupActions();

    m_applyAction = actionCollection()->addAction("apply");
    m_applyAction->setIcon(KIcon("dialog-ok-apply"));
    m_applyAction->setText(i18nc("@action Downloads and installs updates", "Install Updates"));
    connect(m_applyAction, SIGNAL(triggered()), this, SLOT(startCommit()));

    setActionsEnabled(false);

    setupGUI((StandardWindowOption)(KXmlGuiWindow::Default & ~KXmlGuiWindow::StatusBar));
}

void MainWindow::workerEvent(QApt::WorkerEvent event)
{
    MuonMainWindow::workerEvent(event);

    switch (event) {
    case QApt::CacheUpdateStarted:
        m_progressWidget->show();
        m_progressWidget->setHeaderText(i18nc("@info", "<title>Updating software sources</title>"));
        connect(m_progressWidget, SIGNAL(cancelDownload()), m_backend, SLOT(cancelDownload()));
        break;
    case QApt::CacheUpdateFinished:
    case QApt::CommitChangesFinished:
        if (m_backend) {
            m_progressWidget->animatedHide();
            m_updaterWidget->setEnabled(true);
            reload();
            setActionsEnabled();
        }
        break;
    case QApt::PackageDownloadStarted:
        m_progressWidget->show();
        m_progressWidget->setHeaderText(i18nc("@info", "<title>Downloading Updates</title>"));
        connect(m_progressWidget, SIGNAL(cancelDownload()), m_backend, SLOT(cancelDownload()));
        QApplication::restoreOverrideCursor();
        break;
    case QApt::CommitChangesStarted:
        m_progressWidget->setHeaderText(i18nc("@info", "<title>Committing Changes</title>"));
        QApplication::restoreOverrideCursor();
        break;
    }
}

void MainWindow::errorOccurred(QApt::ErrorCode error, const QVariantMap &args)
{
    MuonMainWindow::errorOccurred(error, args);

    switch (error) {
    case QApt::UserCancelError:
        QApplication::restoreOverrideCursor();
        break;
    case QApt::AuthError:
        m_updaterWidget->setEnabled(true);
        setActionsEnabled();
        QApplication::restoreOverrideCursor();
    default:
        break;
    }
}

void MainWindow::reload()
{
    m_canExit = false;

    m_updaterWidget->reload();

    m_canExit = true;
}

void MainWindow::setActionsEnabled(bool enabled)
{
    MuonMainWindow::setActionsEnabled(enabled);
    if (!enabled) {
        return;
    }

    //m_downloadListAction->setEnabled(isConnected());

    m_applyAction->setEnabled(m_backend->areChangesMarked());
    m_undoAction->setEnabled(!m_backend->isUndoStackEmpty());
    m_redoAction->setEnabled(!m_backend->isRedoStackEmpty());
    m_revertAction->setEnabled(!m_backend->isUndoStackEmpty());
}

void MainWindow::checkForUpdates()
{
    setActionsEnabled(false);
    m_updaterWidget->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_backend->updateCache();
}

void MainWindow::startCommit()
{
    setActionsEnabled(false);
    m_updaterWidget->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_backend->commitChanges();
}
