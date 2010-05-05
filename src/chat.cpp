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

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QInputDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QMenuBar>
#include <QMessageBox>
#include <QPluginLoader>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "idle/idle.h"

#include "qxmpp/QXmppConfiguration.h"
#include "qxmpp/QXmppConstants.h"
#include "qxmpp/QXmppLogger.h"
#include "qxmpp/QXmppMessage.h"
#include "qxmpp/QXmppRoster.h"
#include "qxmpp/QXmppRosterIq.h"
#include "qxmpp/QXmppUtils.h"
#include "qxmpp/QXmppVersionIq.h"

#include "qnetio/dns.h"

#include "application.h"
#include "chat.h"
#include "chat_client.h"
#include "chat_dialog.h"
#include "chat_plugin.h"
#include "chat_roster.h"
#include "chat_roster_item.h"
#include "systeminfo.h"

#define AWAY_TIME 300 // set away after 50s

using namespace QNetIO;

static QRegExp jidValidator("[^@]+@[^@]+");

enum StatusIndexes {
    AvailableIndex = 0,
    BusyIndex = 1,
    AwayIndex = 2,
    OfflineIndex = 3,
};

Chat::Chat(QWidget *parent)
    : QMainWindow(parent),
    autoAway(false),
    isBusy(false),
    isConnected(false)
{
    setWindowIcon(QIcon(":/chat.png"));

    client = new ChatClient(this);
    m_rosterModel =  new ChatRosterModel(client);

    /* set up idle monitor */
    idle = new Idle;
    connect(idle, SIGNAL(secondsIdle(int)), this, SLOT(secondsIdle(int)));
    idle->start();

    /* set up logger */
    QXmppLogger *logger = new QXmppLogger(this);
    logger->setLoggingType(QXmppLogger::SignalLogging);
    client->setLogger(logger);

    /* build splitter */
    splitter = new QSplitter;
    splitter->setChildrenCollapsible(false);

    /* left panel */
    rosterView = new ChatRosterView(m_rosterModel);
    connect(rosterView, SIGNAL(clicked(QModelIndex)), this, SLOT(rosterClicked(QModelIndex)));
    connect(rosterView, SIGNAL(itemMenu(QMenu*, QModelIndex)), this, SIGNAL(rosterMenu(QMenu*, QModelIndex)));
    connect(rosterView, SIGNAL(itemMenu(QMenu*, QModelIndex)), this, SLOT(contactsRosterMenu(QMenu*, QModelIndex)));
    connect(rosterView->model(), SIGNAL(modelReset()), this, SLOT(resizeContacts()));
    splitter->addWidget(rosterView);
    splitter->setStretchFactor(0, 0);

    /* right panel */
    conversationPanel = new QStackedWidget;
    conversationPanel->hide();
    connect(conversationPanel, SIGNAL(currentChanged(int)), this, SLOT(panelChanged(int)));
    splitter->addWidget(conversationPanel);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    /* build status bar */
    addButton = new QPushButton;
    addButton->setEnabled(false);
    addButton->setIcon(QIcon(":/add.png"));
    addButton->setToolTip(tr("Add a contact"));
    connect(addButton, SIGNAL(clicked()), this, SLOT(addContact()));
    statusBar()->addWidget(addButton);

    statusCombo = new QComboBox;
    statusCombo->addItem(QIcon(":/contact-available.png"), tr("Available"));
    statusCombo->addItem(QIcon(":/contact-busy.png"), tr("Busy"));
    statusCombo->addItem(QIcon(":/contact-away.png"), tr("Away"));
    statusCombo->addItem(QIcon(":/contact-offline.png"), tr("Offline"));
    statusCombo->setCurrentIndex(OfflineIndex);
    connect(statusCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(statusChanged(int)));
    statusBar()->addPermanentWidget(statusCombo);

    /* create menu */
    QMenu *menu = menuBar()->addMenu("&File");

    optsMenu = menu->addMenu(QIcon(":/options.png"), tr("&Options"));

    QAction *action = optsMenu->addAction(tr("Chat accounts"));
    connect(action, SIGNAL(triggered(bool)), qApp, SLOT(showAccounts()));

    Application *wApp = qobject_cast<Application*>(qApp);
    if (wApp && wApp->isInstalled())
    {
        action = optsMenu->addAction(tr("Open at login"));
        action->setCheckable(true);
        action->setChecked(wApp->openAtLogin());
        connect(action, SIGNAL(toggled(bool)), wApp, SLOT(setOpenAtLogin(bool)));
    }

    action = menu->addAction(QIcon(":/close.png"), tr("&Quit"));
    connect(action, SIGNAL(triggered(bool)), qApp, SLOT(quit()));

    /* set up client */
    connect(client, SIGNAL(error(QXmppClient::Error)), this, SLOT(error(QXmppClient::Error)));
    connect(client, SIGNAL(messageReceived(const QXmppMessage&)), this, SLOT(messageReceived(const QXmppMessage&)));
    connect(client, SIGNAL(presenceReceived(const QXmppPresence&)), this, SLOT(presenceReceived(const QXmppPresence&)));
    connect(client, SIGNAL(connected()), this, SLOT(connected()));
    connect(client, SIGNAL(disconnected()), this, SLOT(disconnected()));

    /* set up keyboard shortcuts */
#ifdef Q_OS_MAC
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_W), this);
    connect(shortcut, SIGNAL(activated()), this, SLOT(close()));
#endif
}

Chat::~Chat()
{
    delete idle;
    foreach (ChatPanel *panel, chatPanels)
        delete panel;

    // disconnect
    client->disconnect();
}

/** Prompt the user for a new contact then add it to the roster.
 */
void Chat::addContact()
{
    bool ok = true;
    QString jid = "@" + client->getConfiguration().domain();
    while (!jidValidator.exactMatch(jid))
    {
        jid = QInputDialog::getText(this, tr("Add a contact"),
            tr("Enter the address of the contact you want to add."),
            QLineEdit::Normal, jid, &ok).toLower();
        if (!ok)
            return;
        jid = jid.trimmed().toLower();
    }

    QXmppPresence packet;
    packet.setTo(jid);
    packet.setType(QXmppPresence::Subscribe);
    client->sendPacket(packet);
}

/** Connect signals for the given panel.
 *
 * @param panel
 */
void Chat::addPanel(ChatPanel *panel)
{
    if (chatPanels.contains(panel))
        return;
    connect(panel, SIGNAL(destroyed(QObject*)), this, SLOT(destroyPanel(QObject*)));
    connect(panel, SIGNAL(hidePanel()), this, SLOT(hidePanel()));
    connect(panel, SIGNAL(notifyPanel()), this, SLOT(notifyPanel()));
    connect(panel, SIGNAL(registerPanel()), this, SLOT(registerPanel()));
    connect(panel, SIGNAL(showPanel()), this, SLOT(showPanel()));
    connect(panel, SIGNAL(unregisterPanel()), this, SLOT(unregisterPanel()));
    chatPanels << panel;
}

void Chat::contactsRosterMenu(QMenu *menu, const QModelIndex &index)
{
    int type = index.data(ChatRosterModel::TypeRole).toInt();
    const QString bareJid = index.data(ChatRosterModel::IdRole).toString();
    
    if (type == ChatRosterItem::Contact)
    {
        QAction *action;

        const QString url = index.data(ChatRosterModel::UrlRole).toString();
        if (!url.isEmpty())
        {
            action = menu->addAction(QIcon(":/diagnostics.png"), tr("Show profile"));
            action->setData(url);
            connect(action, SIGNAL(triggered()), this, SLOT(showContactPage()));
        }

        action = menu->addAction(QIcon(":/options.png"), tr("Rename contact"));
        action->setData(bareJid);
        connect(action, SIGNAL(triggered()), this, SLOT(renameContact()));

        action = menu->addAction(QIcon(":/remove.png"), tr("Remove contact"));
        action->setData(bareJid);
        connect(action, SIGNAL(triggered()), this, SLOT(removeContact()));
    }

}

/** When a panel is destroyed, from it from our list of panels.
 *
 * @param obj
 */
void Chat::destroyPanel(QObject *obj)
{
    chatPanels.removeAll(static_cast<ChatPanel*>(obj));
}

/** Hide a panel.
 */
void Chat::hidePanel()
{
    QWidget *panel = qobject_cast<QWidget*>(sender());
    if (conversationPanel->indexOf(panel) < 0)
        return;

    // close view
    if (conversationPanel->count() == 1)
    {
        conversationPanel->hide();
        QTimer::singleShot(100, this, SLOT(resizeContacts()));
    }
    conversationPanel->removeWidget(panel);
}

/** Notify the user of activity on a panel.
 */
void Chat::notifyPanel()
{
    QWidget *panel = qobject_cast<QWidget*>(sender());
    if (conversationPanel->indexOf(panel) < 0)
        return;

    // add pending message
    if (!isActiveWindow() || conversationPanel->currentWidget() != panel)
        m_rosterModel->addPendingMessage(panel->objectName());

    // show the chat window
    if (!isVisible())
    {
#ifdef Q_OS_MAC
        show();
#else
        showMinimized();
#endif
    }

    /* NOTE : in Qt built for Mac OS X using Cocoa, QApplication::alert
     * only causes the dock icon to bounce for one second, instead of
     * bouncing until the user focuses the window. To work around this
     * we implement our own version.
     */
    Application::alert(panel);
}

/** Register a panel in the roster list.
  */
void Chat::registerPanel()
{
    ChatPanel *panel = qobject_cast<ChatPanel*>(sender());
    if (!panel)
        return;

    m_rosterModel->addItem(panel->objectType(),
        panel->objectName(),
        panel->windowTitle(),
        panel->windowIcon());
}

/** Show a panel.
 */
void Chat::showPanel()
{
    ChatPanel *panel = qobject_cast<ChatPanel*>(sender());
    if (!panel)
        return;

    // register panel
    m_rosterModel->addItem(panel->objectType(),
        panel->objectName(),
        panel->windowTitle(),
        panel->windowIcon());

    // add panel
    if (conversationPanel->indexOf(panel) < 0)
    {
        conversationPanel->addWidget(panel);
        conversationPanel->show();
        if (conversationPanel->count() == 1)
            resizeContacts();
    }
    conversationPanel->setCurrentWidget(panel);
}

/** Handle switching between panels.
 *
 * @param index
 */
void Chat::panelChanged(int index)
{
    QWidget *widget = conversationPanel->widget(index);
    if (!widget)
        return;
    m_rosterModel->clearPendingMessages(widget->objectName());
    rosterView->selectContact(widget->objectName());
    widget->setFocus();
}

/** When the window is activated, pass focus to the active chat.
 *
 * @param event
 */
void Chat::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow())
    {
        int index = conversationPanel->currentIndex();
        if (index >= 0)
            panelChanged(index);
    }
}

/** Handle successful connection to the chat server.
 */
void Chat::connected()
{
    isConnected = true;
    addButton->setEnabled(true);
    statusCombo->setCurrentIndex(isBusy ? BusyIndex : AvailableIndex);
}

/** Handle disconnection from the chat server.
 */
void Chat::disconnected()
{
    isConnected = false;
    addButton->setEnabled(false);
    statusCombo->setCurrentIndex(OfflineIndex);
}

/** Handle an error talking to the chat server.
 *
 * @param error
 */
void Chat::error(QXmppClient::Error error)
{
    if(error == QXmppClient::XmppStreamError &&
       client->getXmppStreamError() == QXmppClient::ConflictStreamError)
    {
        // if we received a resource conflict, exit
        qWarning("Received a resource conflict from chat server");
        qApp->quit();
    }
}

void Chat::messageReceived(const QXmppMessage &msg)
{
    const QString bareJid = jidToBareJid(msg.from());

    if (msg.type() == QXmppMessage::Chat && !panel(bareJid) && !msg.body().isEmpty())
    {
        ChatDialog *dialog = new ChatDialog(client, m_rosterModel, bareJid);
        addPanel(dialog);
        dialog->messageReceived(msg);
    }
}

void Chat::presenceReceived(const QXmppPresence &presence)
{
    QXmppPresence packet;
    packet.setTo(presence.from());
    const QString bareJid = jidToBareJid(presence.from());

    switch (presence.getType())
    {
    case QXmppPresence::Subscribe:
        {
            QXmppRoster::QXmppRosterEntry entry = client->getRoster().getRosterEntry(presence.from());
            QXmppRoster::QXmppRosterEntry::SubscriptionType type = entry.subscriptionType();

            /* if the contact is in our roster accept subscribe */
            if (type == QXmppRoster::QXmppRosterEntry::To || type == QXmppRoster::QXmppRosterEntry::Both)
            {
                qDebug("Subscribe accepted");
                packet.setType(QXmppPresence::Subscribed);
                client->sendPacket(packet);

                packet.setType(QXmppPresence::Subscribe);
                client->sendPacket(packet);
                return;
            }

            /* otherwise ask user */
            ChatRosterPrompt *dlg = new ChatRosterPrompt(client, presence.from(), this);
            dlg->show();
        }
        break;
    default:
        break;
    }
}

/** Return this window's chat client.
 */
ChatClient *Chat::chatClient()
{
    return client;
}

/** Return this window's chat roster model.
 */
ChatRosterModel *Chat::rosterModel()
{
    return m_rosterModel;
}

/** Open the connection to the chat server.
 *
 * @param jid
 * @param password
 */
bool Chat::open(const QString &jid, const QString &password, bool ignoreSslErrors)
{
    QXmppConfiguration config;
    config.setResource(qApp->applicationName());

    /* get user and domain */
    if (!jidValidator.exactMatch(jid))
    {
        qWarning("Cannot connect to chat server using invalid JID");
        return false;
    }
    QStringList bits = jid.split("@");
    config.setPasswd(password);
    config.setUser(bits[0]);
    config.setDomain(bits[1]);

    /* get the server */
    QList<ServiceInfo> results;
    if (ServiceInfo::lookupService("_xmpp-client._tcp." + config.domain(), results))
    {
        config.setHost(results[0].hostName());
        config.setPort(results[0].port());
    } else {
        config.setHost(config.domain());
    }

    /* set security parameters */
    config.setAutoAcceptSubscriptions(false);
    //config.setStreamSecurityMode(QXmppConfiguration::TLSRequired);
    config.setIgnoreSslErrors(ignoreSslErrors);

    /* set keep alive */
    config.setKeepAliveInterval(60);
    config.setKeepAliveTimeout(15);

    /* connect to server */
    client->connectToServer(config);

    /* load plugins */
    QObjectList plugins = QPluginLoader::staticInstances();
    foreach (QObject *object, plugins)
    {
        ChatPlugin *plugin = qobject_cast<ChatPlugin*>(object);
        if (plugin)
            plugin->initialize(this);
    }
    return true;
}

/** Return this window's "Options" menu.
 */
QMenu *Chat::optionsMenu()
{
    return optsMenu;
}

/** Find a panel by its object name.
 *
 * @param objectName
 */
ChatPanel *Chat::panel(const QString &objectName)
{
    foreach (ChatPanel *panel, chatPanels)
        if (panel->objectName() == objectName)
            return panel;
    return 0;
}

/** Prompt the user for confirmation then remove a contact.
 */
void Chat::removeContact()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action)
        return;
    QString jid = action->data().toString();

    if (QMessageBox::question(this, tr("Remove contact"),
        tr("Do you want to remove %1 from your contact list?").arg(jid),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    {
        QXmppRosterIq::Item item;
        item.setBareJid(jid);
        item.setSubscriptionType(QXmppRosterIq::Item::Remove);
        QXmppRosterIq packet;
        packet.setType(QXmppIq::Set);
        packet.addItem(item);
        client->sendPacket(packet);
    }
}

/** Prompt the user to rename a contact.
 */
void Chat::renameContact()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action)
        return;
    QString jid = action->data().toString();

    bool ok = true;
    QXmppRosterIq::Item item = client->getRoster().getRosterEntry(jid);
    QString name = QInputDialog::getText(this, tr("Rename contact"),
        tr("Enter the name for this contact."),
        QLineEdit::Normal, item.name(), &ok);
    if (ok)
    {
        item.setName(name);
        QXmppRosterIq packet;
        packet.setType(QXmppIq::Set);
        packet.addItem(item);
        client->sendPacket(packet);
    }
}

/** Try to resize the window to fit the contents of the contacts list.
 */
void Chat::resizeContacts()
{
    QSize hint = rosterView->sizeHint();
    hint.setHeight(hint.height() + 32);
    if (conversationPanel->isVisible())
        hint.setWidth(hint.width() + 500);

    // respect current size
    if (conversationPanel->isVisible() && hint.width() < size().width())
        hint.setWidth(size().width());
    if (hint.height() < size().height())
        hint.setHeight(size().height());

    /* Make sure we do not resize to a size exceeding the desktop size
     * + some padding for the window title.
     */
    QDesktopWidget *desktop = QApplication::desktop();
    const QRect &available = desktop->availableGeometry(this);
    if (hint.height() > available.height() - 100)
    {
        hint.setHeight(available.height() - 100);
        hint.setWidth(hint.width() + 32);
    }

    resize(hint);
}

void Chat::rosterClicked(const QModelIndex &index)
{
    int type = index.data(ChatRosterModel::TypeRole).toInt();
    const QString jid = index.data(ChatRosterModel::IdRole).toString();

    // create conversation if necessary
    if (type == ChatRosterItem::Contact && !panel(jid))
        addPanel(new ChatDialog(client, m_rosterModel, jid));

    // show requested panel
    ChatPanel *chatPanel = panel(jid);
    if (chatPanel)
        QTimer::singleShot(0, chatPanel, SIGNAL(showPanel()));
}

/** Handle user inactivity.
 *
 * @param secs
 */
void Chat::secondsIdle(int secs)
{
    if (secs >= AWAY_TIME)
    {
        if (statusCombo->currentIndex() == AvailableIndex)
        {
            statusCombo->setCurrentIndex(AwayIndex);
            autoAway = true;
        }
    } else if (autoAway) {
        const int oldIndex = isBusy ? BusyIndex : AvailableIndex;
        statusCombo->setCurrentIndex(oldIndex);
    }
}

void Chat::showContactPage()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action)
        return;

    QString url = action->data().toString();
    if (!url.isEmpty())
        QDesktopServices::openUrl(url);
}

void Chat::statusChanged(int currentIndex)
{
    autoAway = false;
    if (currentIndex == AvailableIndex)
    {
        isBusy = false;
        if (isConnected)
        {
            QXmppPresence presence;
            presence.setType(QXmppPresence::Available);
            client->sendPacket(presence);
        }
        else
            client->connectToServer(client->getConfiguration());
    } else if (currentIndex == AwayIndex) {
        if (isConnected)
        {
            QXmppPresence presence;
            presence.setType(QXmppPresence::Available);
            presence.setStatus(QXmppPresence::Status::Away);
            client->sendPacket(presence);
        }
    } else if (currentIndex == BusyIndex) {
        isBusy = true;
        if (isConnected)
        {
            QXmppPresence presence;
            presence.setType(QXmppPresence::Available);
            presence.setStatus(QXmppPresence::Status::DND);
            client->sendPacket(presence);
        }
        else
            client->connectToServer(client->getConfiguration());
    } else if (currentIndex == OfflineIndex) {
        if (isConnected)
            client->disconnect();
    }
}

/** Unregister a panel from the roster list.
  */
void Chat::unregisterPanel()
{
    QWidget *panel = qobject_cast<QWidget*>(sender());
    if (!panel)
        return;

    m_rosterModel->removeItem(panel->objectName());
}

