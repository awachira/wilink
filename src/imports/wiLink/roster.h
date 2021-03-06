/*
 * wiLink
 * Copyright (C) 2009-2015 Wifirst
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

#ifndef __WILINK_CHAT_ROSTER_H__
#define __WILINK_CHAT_ROSTER_H__

#include <QQuickImageProvider>
#include <QSet>
#include <QUrl>

#include <QXmppPresence.h>

#include "model.h"

class QXmppDiscoveryIq;
class QXmppPresence;
class QXmppVCardIq;
class QXmppVCardManager;

class ChatClient;
class RosterModel;
class RosterModelPrivate;
class VCardCache;
class VCardCachePrivate;

class RosterImageProvider : public QQuickImageProvider
{
public:
    RosterImageProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);
};

class RosterModel : public ChatModel
{
    Q_OBJECT
    Q_ENUMS(Role)
    Q_PROPERTY(int pendingMessages READ pendingMessages NOTIFY pendingMessagesChanged)

public:
    enum Role {
        NameRole = ChatModel::NameRole,
        StatusRole = ChatModel::UserRole,
        StatusSortRole,
        SubscriptionStatusRole,
        SubscriptionTypeRole
    };

    RosterModel(QObject *parent = 0);
    ~RosterModel();

    int pendingMessages() const;

    // QAbstractItemModel interface
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    QHash<int, QByteArray> roleNames() const;

signals:
    void pendingMessagesChanged();

public slots:
    void addPendingMessage(const QString &bareJid);
    void clearPendingMessages(const QString &bareJid);

private slots:
    void _q_clientCreated(ChatClient *client);
    void _q_clientDestroyed(ChatClient *client);
    void _q_connected();
    void _q_disconnected();
    void _q_itemAdded(const QString &jid);
    void _q_itemChanged(const QString &jid);
    void _q_itemRemoved(const QString &jid);
    void _q_rosterPurge();
    void _q_rosterReceived();

private:
    friend class RosterModelPrivate;
    RosterModelPrivate *d;
};

class VCard : public QObject
{
    Q_OBJECT
    Q_FLAGS(Feature Features)
    Q_PROPERTY(QUrl avatar READ avatar NOTIFY avatarChanged)
    Q_PROPERTY(Features features READ features NOTIFY featuresChanged)
    Q_PROPERTY(QString jid READ jid WRITE setJid NOTIFY jidChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString nickName READ nickName NOTIFY nickNameChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)

public:
    enum Feature {
        ChatStatesFeature = 1,
        FileTransferFeature = 2,
        VersionFeature = 4,
        VoiceFeature = 8,
        VideoFeature = 16,
    };
    Q_DECLARE_FLAGS(Features, Feature)

    VCard(QObject *parent = 0);

    QUrl avatar() const;
    Features features() const;
    QString jid() const;
    void setJid(const QString &jid);
    QString name() const;
    QString nickName() const;
    QString status() const;
    QUrl url() const;

signals:
    void avatarChanged(const QUrl &avatar);
    void featuresChanged();
    void jidChanged(const QString &jid);
    void nameChanged(const QString &name);
    void nickNameChanged(const QString &nickName);
    void statusChanged();
    void urlChanged(const QUrl &url);

public slots:
    QString jidForFeature(Feature feature) const;

private slots:
    void _q_cardChanged(const QString &jid);
    void _q_discoChanged(const QString &jid);
    void _q_presenceChanged(const QString &jid);

private:
    void update();

    VCardCache *m_cache;
    QUrl m_avatar;
    Features m_features;
    QString m_jid;
    QString m_name;
    QString m_nickName;
    QUrl m_url;
};

class VCardCache : public QObject
{
    Q_OBJECT

public:
    static VCardCache *instance();
    ~VCardCache();
    
    QUrl imageUrl(const QString &jid);
    QString presenceStatus(const QString &jid) const;
    QString subscriptionStatus(const QString &jid) const;
    int subscriptionType(const QString &jid) const;

signals:
    void cardChanged(const QString &jid);
    void discoChanged(const QString &jid);
    void presenceChanged(const QString &jid = QString());

public slots:
    void addClient(ChatClient *client);
    bool get(const QString &jid, QXmppVCardIq *iq = 0);

private slots:
    void _q_clientDestroyed(QObject *object);
    void _q_discoveryInfoReceived(const QXmppDiscoveryIq &disco);
    void _q_presenceReceived(const QXmppPresence &presence);
    void _q_vCardReceived(const QXmppVCardIq&);

private:
    VCardCache(QObject *parent = 0);
    ChatClient *client(const QString &jid) const;
    VCard::Features features(const QString &jid) const;

    VCardCachePrivate *d;
    friend class VCard;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VCard::Features)

#endif
