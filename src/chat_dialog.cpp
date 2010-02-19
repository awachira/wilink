/*
 * wDesktop
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

#include <QDateTime>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QShortcut>

#include "qxmpp/QXmppArchiveManager.h"
#include "qxmpp/QXmppConstants.h"
#include "qxmpp/QXmppMessage.h"

#include "chat_dialog.h"
#include "chat_edit.h"
#include "chat_history.h"

ChatDialog::ChatDialog(QXmppClient *xmppClient, const QString &jid, QWidget *parent)
    : ChatConversation(jid, parent), client(xmppClient)
{
    connect(client, SIGNAL(messageReceived(const QXmppMessage&)), this, SLOT(messageReceived(const QXmppMessage&)));
    connect(&client->getArchiveManager(), SIGNAL(archiveChatReceived(const QXmppArchiveChat &)), this, SLOT(archiveChatReceived(const QXmppArchiveChat &)));
    connect(&client->getArchiveManager(), SIGNAL(archiveListReceived(const QList<QXmppArchiveChat> &)), this, SLOT(archiveListReceived(const QList<QXmppArchiveChat> &)));
}

void ChatDialog::archiveChatReceived(const QXmppArchiveChat &chat)
{
    if (chat.with.split("/").first() != chatRemoteJid)
        return;

    foreach (const QXmppArchiveMessage &msg, chat.messages)
    {
        ChatHistoryMessage message;
        message.archived = true;
        message.body = msg.body;
        message.datetime = msg.datetime;
        message.from = msg.local ? chatLocalName : chatRemoteName;
        message.local = msg.local;
        chatHistory->addMessage(message);
    }
}

void ChatDialog::archiveListReceived(const QList<QXmppArchiveChat> &chats)
{
    for (int i = chats.size() - 1; i >= 0; i--)
        if (chats[i].with.split("/").first() == chatRemoteJid)
            client->getArchiveManager().retrieveCollection(chats[i].with, chats[i].start);
}

bool ChatDialog::isRoom() const
{
    return false;
}

/** List archives for the past week.
 */
void ChatDialog::join()
{
    client->getArchiveManager().listCollections(chatRemoteJid,
        QDateTime::currentDateTime().addDays(-7));
}

void ChatDialog::leave()
{
}

void ChatDialog::messageReceived(const QXmppMessage &msg)
{
    if (msg.getFrom().split("/").first() != chatRemoteJid)
        return;

    ChatHistoryMessage message;
    message.body = msg.getBody();
    if (msg.getExtension().attribute("xmlns") == ns_delay)
    {
        const QString str = msg.getExtension().attribute("stamp");
        message.datetime = QDateTime::fromString(str, "yyyyMMddThh:mm:ss");
        message.datetime.setTimeSpec(Qt::UTC);
    } else {
        message.datetime = QDateTime::currentDateTime();
    }
    message.from = chatRemoteName;
    message.local = false;
    chatHistory->addMessage(message);
}

void ChatDialog::sendMessage(const QString &text)
{
    // add message to history
    ChatHistoryMessage message;
    message.body = text;
    message.datetime = QDateTime::currentDateTime();
    message.from = chatLocalName;
    message.local = true;
    chatHistory->addMessage(message);

    // send message
    QXmppMessage msg;
    msg.setBody(text);
    msg.setTo(chatRemoteJid);
    client->sendPacket(msg);
}

