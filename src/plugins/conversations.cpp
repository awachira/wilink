/*
 * wiLink
 * Copyright (C) 2009-2011 Bolloré telecom
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
#include <QTimer>

#include "QXmppArchiveIq.h"
#include "QXmppArchiveManager.h"
#include "QXmppConstants.h"
#include "QXmppMessage.h"
#include "QXmppUtils.h"

#include "application.h"
#include "client.h"
#include "conversations.h"
#include "history.h"
#include "roster.h"

#ifdef WILINK_EMBEDDED
#define HISTORY_DAYS 7
#else
#define HISTORY_DAYS 14
#endif

Conversation::Conversation(QObject *parent)
    : QObject(parent),
    m_archivesFetched(false),
    m_client(0),
    m_historyModel(0),
    m_localState(QXmppMessage::None),
    m_remoteState(QXmppMessage::None)
{
    m_historyModel = new HistoryModel(this);
}

ChatClient *Conversation::client() const
{
    return m_client;
}

void Conversation::setClient(ChatClient *client)
{
    bool check;
    Q_UNUSED(check);

    if (client != m_client) {
        m_client = client;
        m_historyModel->setClient(client);

        check = connect(client, SIGNAL(messageReceived(QXmppMessage)),
                        this, SLOT(messageReceived(QXmppMessage)));
        Q_ASSERT(check);

        emit clientChanged(client);

        // try to fetch archives
        fetchArchives();
    }
}

HistoryModel *Conversation::historyModel() const
{
    return m_historyModel;
}

QString Conversation::jid() const
{
    return m_jid;
}

void Conversation::setJid(const QString &jid)
{
    if (jid != m_jid) {
        m_jid = jid;
        m_historyModel->setJid(jid);
        emit jidChanged(jid);

        // try to fetch archives
        fetchArchives();
    }
}

int Conversation::localState() const
{
    return m_localState;
}

void Conversation::setLocalState(int state)
{
    if (state != m_localState) {
        m_localState = static_cast<QXmppMessage::State>(state);

        // notify state change
        if (m_client) {
            QXmppMessage message;
            message.setTo(m_jid);
            message.setState(m_localState);
            m_client->sendPacket(message);
        }

        emit localStateChanged(m_localState);
    }
}

int Conversation::remoteState() const
{
    return m_remoteState;
}

void Conversation::fetchArchives()
{
    if (m_archivesFetched || !m_client || !m_historyModel || m_jid.isEmpty())
        return;

    m_client->archiveManager()->listCollections(m_jid,
        m_client->serverTime().addDays(-HISTORY_DAYS));
    m_archivesFetched = true;
}

void Conversation::messageReceived(const QXmppMessage &msg)
{
    if (msg.type() != QXmppMessage::Chat ||
        jidToBareJid(msg.from()) != m_jid)
        return;

    // handle chat state
    if (msg.state() != m_remoteState) {
        m_remoteState = msg.state();
        emit remoteStateChanged(m_remoteState);
    }

    // handle message body
    if (msg.body().isEmpty())
        return;

    HistoryMessage message;
    message.body = msg.body();
    message.date = msg.stamp();
    if (!message.date.isValid())
        message.date = m_client->serverTime();
    message.jid = m_jid;
    message.received = true;
    if (m_historyModel)
        m_historyModel->addMessage(message);
}

bool Conversation::sendMessage(const QString &body)
{
    if (m_jid.isEmpty() || !m_client || !m_client->isConnected())
        return false;

    // send message
    QXmppMessage message;
    message.setTo(m_jid);
    message.setBody(body);
    message.setState(QXmppMessage::Active);
    if (!m_client->sendPacket(message))
        return false;

    // update state
    if (m_localState != QXmppMessage::Active) {
        m_localState = QXmppMessage::Active;
        emit localStateChanged(m_localState);
    }

    // add message to history
    if (m_historyModel) {
        HistoryMessage message;
        message.body = body;
        message.date = m_client->serverTime();
        message.jid = m_client->configuration().jidBare();
        message.received = false;
        m_historyModel->addMessage(message);
    }

    return true;
}

