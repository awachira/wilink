/*
 * wiLink
 * Copyright (C) 2009-2010 Bolloré telecom
 * See AUTHORS file for a full list of contributors.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __WILINK_SHARES_H__
#define __WILINK_SHARES_H__

#include <QTextDocument>

#include "QXmppLogger.h"
#include "QXmppShareIq.h"
#include "QXmppTransferManager.h"

#include "chat_panel.h"

class Chat;
class ChatRosterModel;
class SharesModel;
class SharesTab;
class SharesView;
class ChatTransfers;
class ChatTransfersView;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QStatusBar;
class QTabWidget;
class QTimer;
class QXmppPacket;
class QXmppPresence;
class QXmppShareDatabase;

/** The shares panel.
 */
class SharesPanel : public ChatPanel
{
    Q_OBJECT

public:
    SharesPanel(Chat *chat, QXmppShareDatabase *sharesDb, QWidget *parent = 0);
    void setClient(QXmppClient *client);
    void setRoster(ChatRosterModel *model);
    void setTransfers(ChatTransfers *transfers);

signals:
    void findFinished(bool found);
    void logMessage(QXmppLogger::MessageType type, const QString &msg);

private slots:
    void disconnected();
    void getFailed(const QString &packetId);
    void transferAbort(QXmppShareItem *item);
    void transferDestroyed(QObject *obj);
    void transferDoubleClicked(const QModelIndex &index);
    void transferFinished(QXmppTransferJob *job);
    void transferProgress(qint64, qint64);
    void transferRemoved();
    void transferStarted(QXmppTransferJob *job);
    void find(const QString &needle, QTextDocument::FindFlags flags, bool changed);
    void downloadItem();
    void itemContextMenu(const QModelIndex &index, const QPoint &globalPos);
    void itemDoubleClicked(const QModelIndex &index);
    void itemExpandRequested(const QModelIndex &index);
    void presenceReceived(const QXmppPresence &presence);
    void processDownloadQueue();
    void showOptions();
    void shareSearchIqReceived(const QXmppShareSearchIq &searchIq);
    void shareServerFound(const QString &server);
    void indexStarted();
    void indexFinished(double elapsed, int updated, int removed);
    void tabChanged(int index);
    void directoryChanged(const QString &path);

private:
    void queueItem(QXmppShareItem *item);

private:
    QString shareServer;

    Chat *chatWindow;
    QXmppClient *client;
    QXmppShareDatabase *db;
    ChatRosterModel *rosterModel;
    SharesModel *queueModel;
    QMap<QString, QWidget*> searches;

    QTabWidget *tabWidget;

    SharesView *sharesView;
    SharesTab *sharesWidget;
    QString sharesFilter;

    SharesView *downloadsView;
    SharesTab *downloadsWidget;
    QList<QXmppTransferJob*> downloadJobs;

    ChatTransfersView *uploadsView;
    SharesTab *uploadsWidget;

    QPushButton *downloadButton;
    QPushButton *indexButton;
    QPushButton *removeButton;
    QStatusBar *statusBar;

    QAction *menuAction;
};

class SharesTab : public QWidget
{
    Q_OBJECT

public:
    SharesTab(QWidget *parent = 0);
    void addWidget(QWidget *widget);
    void setText(const QString &text);

signals:
    void showOptions();

private:
    QLabel *label;
};

#endif
