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
#include <QDesktopServices>
#include <QDomDocument>
#include <QDomElement>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include "QSoundMeter.h"
#include "QSoundPlayer.h"
#include "QXmppRtpChannel.h"
#include "QXmppUtils.h"

#include "application.h"
#include "declarative.h"
#include "history.h"
#include "phone.h"
#include "phone/sip.h"

#define FLAGS_DIRECTION 0x1
#define FLAGS_ERROR 0x2

class PhoneContactItem
{
public:
    PhoneContactItem();
    QByteArray data() const;
    void parse(const QDomElement &element);

    int id;
    QString name;
    QString phone;
    QNetworkReply *reply;
};

PhoneContactItem::PhoneContactItem()
    : id(0),
    reply(0)
{
}

QByteArray PhoneContactItem::data() const
{
    QUrl data;
    data.addQueryItem("name", name);
    data.addQueryItem("phone", phone);
    return data.encodedQuery();
}

void PhoneContactItem::parse(const QDomElement &element)
{
    id = element.firstChildElement("id").text().toInt();
    name = element.firstChildElement("name").text();
    phone = element.firstChildElement("phone").text();
}

PhoneContactModel::PhoneContactModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // set role names
    QHash<int, QByteArray> roleNames;
    roleNames.insert(IdRole, "id");
    roleNames.insert(NameRole, "name");
    roleNames.insert(PhoneRole, "phone");
    setRoleNames(roleNames);

    // http
    m_network = new NetworkAccessManager(this);
}

void PhoneContactModel::addContact(const QString &name, const QString &phone)
{
    PhoneContactItem *item = new PhoneContactItem;
    item->name = name;
    item->phone = phone;

    QNetworkRequest request(m_url);
    request.setRawHeader("Accept", "application/xml");
    request.setRawHeader("Connection", "close");
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    item->reply = m_network->post(request, item->data());
    connect(item->reply, SIGNAL(finished()),
            this, SLOT(_q_handleCreate()));

    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(item);
    endInsertRows();
}

/** Returns the number of columns under the given \a parent.
 *
 * @param parent
 */
int PhoneContactModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant PhoneContactModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || row > m_items.size())
        return QVariant();

    PhoneContactItem *item = m_items.at(row);
    switch (role) {
    case IdRole:
        return item->id;
    case NameRole:
        return item->name;
    case PhoneRole:
        return item->phone;
    default:
        return QVariant();
    }
}

/** Returns the number of rows under the given \a parent.
 *
 * @param parent
 */
int PhoneContactModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_items.size();
}

QUrl PhoneContactModel::url() const
{
    return m_url;
}

void PhoneContactModel::setUrl(const QUrl &url)
{
    if (url != m_url) {
        m_url = url;
        if (m_url.isValid()) {
            QNetworkRequest request(m_url);
            request.setRawHeader("Accept", "application/xml");
            QNetworkReply *reply = m_network->get(request);
            connect(reply, SIGNAL(finished()),
                    this, SLOT(_q_handleList()));
        }
        emit urlChanged(m_url);
    }
}

void PhoneContactModel::_q_handleCreate()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_ASSERT(reply);
}

void PhoneContactModel::_q_handleList()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_ASSERT(reply);

    QDomDocument doc;
    if (reply->error() != QNetworkReply::NoError || !doc.setContent(reply)) {
        qWarning("Failed to retrieve phone contacts: %s", qPrintable(reply->errorString()));
        return;
    }

    QDomElement element = doc.documentElement().firstChildElement("contact");
    while (!element.isNull()) {
        int id = element.firstChildElement("id").text().toInt();
        foreach (PhoneContactItem *item, m_items) {
            if (item->id == id) {
                id = 0;
                break;
            }
        }
        if (id > 0) {
            PhoneContactItem *item = new PhoneContactItem;
            item->parse(element);

            beginInsertRows(QModelIndex(), 0, 0);
            m_items.prepend(item);
            endInsertRows();
        }
        element = element.nextSiblingElement("contact");
    }
}

class PhoneHistoryItem
{
public:
    PhoneHistoryItem();
    QByteArray data() const;
    void parse(const QDomElement &element);

    int id;
    QString address;
    QDateTime date;
    int duration;
    int flags;

    SipCall *call;
    QNetworkReply *reply;
    int soundId;
};

PhoneHistoryItem::PhoneHistoryItem()
    : id(0),
    duration(0),
    flags(0),
    call(0),
    reply(0),
    soundId(0)
{
}

QByteArray PhoneHistoryItem::data() const
{
    QUrl data;
    data.addQueryItem("address", address);
    data.addQueryItem("duration", QString::number(duration));
    data.addQueryItem("flags", QString::number(flags));
    return data.encodedQuery();
}

void PhoneHistoryItem::parse(const QDomElement &element)
{
    id = element.firstChildElement("id").text().toInt();
    address = element.firstChildElement("address").text();
    date = datetimeFromString(element.firstChildElement("date").text());
    duration = element.firstChildElement("duration").text().toInt();
    flags = element.firstChildElement("flags").text().toInt();
}

/** Constructs a new model representing the call history.
 *
 * @param network
 * @param parent
 */
PhoneHistoryModel::PhoneHistoryModel(QObject *parent)
    : QAbstractListModel(parent),
    m_enabled(false),
    m_registeredHandler(false)
{
    bool check;
    Q_UNUSED(check);

    // set role names
    QHash<int, QByteArray> roleNames;
    roleNames.insert(ActiveRole, "active");
    roleNames.insert(AddressRole, "address");
    roleNames.insert(DateRole, "date");
    roleNames.insert(DirectionRole, "direction");
    roleNames.insert(DurationRole, "duration");
    roleNames.insert(IdRole, "id");
    roleNames.insert(NameRole, "name");
    roleNames.insert(StateRole, "state");
    setRoleNames(roleNames);

    // http
    m_network = new NetworkAccessManager(this);

    // sip
    m_client = new SipClient;
    m_client->setSoundPlayer(wApp->soundPlayer());
    check = connect(m_client, SIGNAL(callDialled(SipCall*)),
                    this, SLOT(addCall(SipCall*)));
    Q_ASSERT(check);
    m_client->moveToThread(wApp->soundThread());

    // ticker for call durations
    m_ticker = new QTimer(this);
    m_ticker->setInterval(1000);
    connect(m_ticker, SIGNAL(timeout()),
            this, SLOT(callTick()));

    // periodic settings retrieval
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, SIGNAL(timeout()),
            this, SLOT(_q_getSettings()));
    m_timer->start(0);
}

PhoneHistoryModel::~PhoneHistoryModel()
{
    m_ticker->stop();
    m_timer->stop();

    // try to exit SIP client cleanly
    if (m_client->state() == SipClient::ConnectedState)
        QMetaObject::invokeMethod(m_client, "disconnectFromServer");
    m_client->deleteLater();

    // delete items
    foreach (PhoneHistoryItem *item, m_items)
        delete item;
}

QList<SipCall*> PhoneHistoryModel::activeCalls() const
{
    QList<SipCall*> calls;
    for (int i = m_items.size() - 1; i >= 0; --i)
        if (m_items[i]->call)
            calls << m_items[i]->call;
    return calls;
}

void PhoneHistoryModel::addCall(SipCall *call)
{
    PhoneHistoryItem *item = new PhoneHistoryItem;
    item->address = call->recipient();
    item->flags = call->direction();
    item->call = call;
    connect(item->call, SIGNAL(stateChanged(QXmppCall::State)),
            this, SLOT(callStateChanged(QXmppCall::State)));
    if (item->call->direction() == QXmppCall::IncomingDirection)
        item->soundId = wApp->soundPlayer()->play(":/call-incoming.ogg", true);
    else
        connect(item->call, SIGNAL(ringing()), this, SLOT(callRinging()));

    QNetworkRequest request(m_url);
    request.setRawHeader("Accept", "application/xml");
    request.setRawHeader("Connection", "close");
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    item->reply = m_network->post(request, item->data());
    connect(item->reply, SIGNAL(finished()),
            this, SLOT(_q_handleCreate()));

    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(item);
    endInsertRows();

    // schedule periodic refresh
    if (!m_ticker->isActive())
        m_ticker->start();

    // notify change
    emit currentCallsChanged();
}

bool PhoneHistoryModel::call(const QString &address)
{
    if (m_client->state() == SipClient::ConnectedState &&
        !currentCalls()) {
        QMetaObject::invokeMethod(m_client, "call", Q_ARG(QString, address));
        return true;
    }
    return false;
}

void PhoneHistoryModel::callRinging()
{
    SipCall *call = qobject_cast<SipCall*>(sender());
    Q_ASSERT(call);

    // find the call
    foreach (PhoneHistoryItem *item, m_items) {
        if (item->call == call && !item->soundId) {
            item->soundId = wApp->soundPlayer()->play(":/call-outgoing.ogg", true);
            break;
        }
    }
}

void PhoneHistoryModel::callStateChanged(QXmppCall::State state)
{
    SipCall *call = qobject_cast<SipCall*>(sender());
    Q_ASSERT(call);

    // find the call
    int row = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i]->call == call) {
            row = i;
            break;
        }
    }
    if (row < 0)
        return;

    PhoneHistoryItem *item = m_items[row];

    if (item->soundId && state != QXmppCall::ConnectingState) {
        wApp->soundPlayer()->stop(item->soundId);
        item->soundId = 0;
    }

    // update the item
    if (state == QXmppCall::ActiveState) {
        connect(call, SIGNAL(inputVolumeChanged(int)),
                this, SIGNAL(inputVolumeChanged(int)));
        connect(call, SIGNAL(outputVolumeChanged(int)),
                this, SIGNAL(outputVolumeChanged(int)));
    }
    else if (state == QXmppCall::FinishedState) {
        call->disconnect(this);
        item->call = 0;
        item->duration = call->duration();
        if (!call->error().isEmpty()) {
            item->flags |= FLAGS_ERROR;
            emit error(call->error());
        }
        emit currentCallsChanged();
        emit inputVolumeChanged(0);
        emit outputVolumeChanged(0);

        QUrl url = m_url;
        url.setPath(url.path() + QString::number(item->id) + "/");

        QNetworkRequest request(url);
        request.setRawHeader("Accept", "application/xml");
        request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
        m_network->put(request, item->data());

        // FIXME: ugly workaround for a race condition causing a crash in
        // PhoneNotification.qml
        QTimer::singleShot(1000, call, SLOT(deleteLater()));
    }
    emit dataChanged(createIndex(row, 0), createIndex(row, 0));
}

void PhoneHistoryModel::callTick()
{
    bool active = false;
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row]->call) {
            active = true;
            emit dataChanged(createIndex(row, 0), createIndex(row, 0));
        }
    }
    if (!active)
        m_ticker->stop();
}

/** Returns the underlying SIP client.
 *
 * \note Use with care as the SIP client lives in a different thread!
 */
SipClient *PhoneHistoryModel::client() const
{
    return m_client;
}

/** Returns the number of columns under the given \a parent.
 *
 * @param parent
 */
int PhoneHistoryModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QUrl PhoneHistoryModel::contactsUrl() const
{
    return m_contactsUrl;
}

int PhoneHistoryModel::currentCalls() const
{
    return activeCalls().size();
}

QVariant PhoneHistoryModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || row > m_items.size())
        return QVariant();
    PhoneHistoryItem *item = m_items[row];

    switch (role) {
    case Qt::ToolTipRole:
        return item->address;
    case AddressRole:
        return item->address;
    case ActiveRole:
        return item->call != 0;
    case DateRole:
        return item->date;
    case DirectionRole:
        return item->flags & FLAGS_DIRECTION;
    case DurationRole:
        return item->call ? item->call->duration() : item->duration;
    case IdRole:
        return item->id;
    case NameRole:
        return sipAddressToName(item->address);
    default:
        return QVariant();
    }
}

/** Returns true if the service is active.
 */
bool PhoneHistoryModel::enabled() const
{
    return m_enabled;
}

/** Hangs up all active calls.
 */
void PhoneHistoryModel::hangup()
{
    for (int i = m_items.size() - 1; i >= 0; --i)
        if (m_items[i]->call)
            m_items[i]->call->hangup();
}

int PhoneHistoryModel::inputVolume() const
{
    QList<SipCall*> calls = activeCalls();
    return calls.isEmpty() ? 0 : calls.first()->inputVolume();
}

int PhoneHistoryModel::maximumVolume() const
{
    return QSoundMeter::maximum();
}

int PhoneHistoryModel::outputVolume() const
{
    QList<SipCall*> calls = activeCalls();
    return calls.isEmpty() ? 0 : calls.first()->outputVolume();
}

/** Returns the user's own phone number.
 */
QString PhoneHistoryModel::phoneNumber() const
{
    return m_phoneNumber;
}

/** Removes the given \a row under the given \a parent.
 *
 * @param row
 * @param parent
 */
bool PhoneHistoryModel::removeRow(int row, const QModelIndex &parent)
{
    QAbstractListModel::removeRow(row, parent);
}

/** Removes \a count rows starting at the given \a row under the given \a parent.
 *
 * @param row
 * @param count
 * @param parent
 */
bool PhoneHistoryModel::removeRows(int row, int count, const QModelIndex &parent)
{
    int last = row + count - 1;
    if (parent.isValid() || row < 0 || count < 0 || last >= m_items.size())
        return false;

    beginRemoveRows(parent, row, last);
    for (int i = last; i >= row; --i) {
        PhoneHistoryItem *item = m_items[i];
        QUrl url = m_url;
        url.setPath(url.path() + QString::number(item->id) + "/");

        QNetworkRequest request(url);
        request.setRawHeader("Accept", "application/xml");
        m_network->deleteResource(request);
        delete item;
        m_items.removeAt(i);
    }
    endRemoveRows();
    return true;
}

/** Returns the number of rows under the given \a parent.
 *
 * @param parent
 */
int PhoneHistoryModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_items.size();
}

/** Returns the selfcare URL.
 */
QUrl PhoneHistoryModel::selfcareUrl() const
{
    return m_selfcareUrl;
}

/** Starts sending a tone.
 *
 * @param tone
 */
void PhoneHistoryModel::startTone(int toneValue)
{
    if (toneValue < QXmppRtpAudioChannel::Tone_0 || toneValue > QXmppRtpAudioChannel::Tone_D)
        return;
    QXmppRtpAudioChannel::Tone tone = static_cast<QXmppRtpAudioChannel::Tone>(toneValue);
    foreach (SipCall *call, activeCalls()) {
        call->audioChannel()->startTone(tone);
    }
}

/** Stops sending a tone.
 *
 * @param tone
 */
void PhoneHistoryModel::stopTone(int toneValue)
{
    if (toneValue < QXmppRtpAudioChannel::Tone_0 || toneValue > QXmppRtpAudioChannel::Tone_D)
        return;
    QXmppRtpAudioChannel::Tone tone = static_cast<QXmppRtpAudioChannel::Tone>(toneValue);
    foreach (SipCall *call, activeCalls())
        call->audioChannel()->stopTone(tone);
}

/** Returns the voicemail service phone number.
 */
QString PhoneHistoryModel::voicemailNumber() const
{
    return m_voicemailNumber;
}

/** Requests VoIP settings from the server.
 */
void PhoneHistoryModel::_q_getSettings()
{
    qDebug("Phone fetching settings");
    QNetworkRequest req(QUrl("https://www.wifirst.net/wilink/voip"));
    req.setRawHeader("Accept", "application/xml");
    QNetworkReply *reply = m_network->get(req);
    connect(reply, SIGNAL(finished()), this, SLOT(_q_handleSettings()));
}

void PhoneHistoryModel::_q_handleSettings()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_ASSERT(reply);

    if (reply->error() != QNetworkReply::NoError) {
        qWarning("Phone failed to retrieve settings: %s", qPrintable(reply->errorString()));
        // retry in 5mn
        m_timer->start(300000);
        return;
    }

    QDomDocument doc;
    doc.setContent(reply);
    QDomElement settings = doc.documentElement();

    // parse settings from server
    const bool enabled = settings.firstChildElement("enabled").text() == QLatin1String("true");
    const QString domain = settings.firstChildElement("domain").text();
    const QString username = settings.firstChildElement("username").text();
    const QString password = settings.firstChildElement("password").text();
    const QString number = settings.firstChildElement("number").text();
    const QString callsUrl = settings.firstChildElement("calls-url").text();
    const QUrl contactsUrl = QUrl(settings.firstChildElement("contacts-url").text());
    const QUrl selfcareUrl = QUrl(settings.firstChildElement("selfcare-url").text());
    const QString voicemailNumber = settings.firstChildElement("voicemail-number").text();

    // update contacts url
    if (contactsUrl != m_contactsUrl) {
        m_contactsUrl = contactsUrl;
        emit contactsUrlChanged(contactsUrl);
    }

    // update phone number
    if (number != m_phoneNumber) {
        m_phoneNumber = number;
        emit phoneNumberChanged(m_phoneNumber);
    }

    // update selfcare url
    if (selfcareUrl != m_selfcareUrl) {
        m_selfcareUrl = selfcareUrl;
        emit selfcareUrlChanged(m_selfcareUrl);
    }

    // update voicemail number
    if (voicemailNumber != m_voicemailNumber) {
        m_voicemailNumber = voicemailNumber;
        emit voicemailNumberChanged(m_voicemailNumber);
    }

    // check service is activated
    const bool wasEnabled = m_enabled;
    if (enabled && !domain.isEmpty() && !username.isEmpty() && !password.isEmpty()) {
        // connect to SIP server
        if (m_client->displayName() != number ||
            m_client->domain() != domain ||
            m_client->username() != username ||
            m_client->password() != password)
        {
            m_client->setDisplayName(number);
            m_client->setDomain(domain);
            m_client->setUsername(username);
            m_client->setPassword(password);
            QMetaObject::invokeMethod(m_client, "connectToServer");
        }

        // retrieve call history
        if (!callsUrl.isEmpty()) {
            m_url = callsUrl;

            QNetworkRequest request(m_url);
            request.setRawHeader("Accept", "application/xml");
            QNetworkReply *reply = m_network->get(request);
            connect(reply, SIGNAL(finished()), this, SLOT(_q_handleList()));
        }

        // register URL handler
        if (!m_registeredHandler) {
            HistoryMessage::addTransform(QRegExp("^(.*\\s)?(\\+?[0-9]{4,})(\\s.*)?$"),
                QString("\\1<a href=\"sip:\\2@%1\">\\2</a>\\3").arg(m_client->domain()));
            QDesktopServices::setUrlHandler("sip", this, "_q_openUrl");
            m_registeredHandler = true;
        }

        m_enabled = true;
    } else { 
        m_enabled = false;
    }
    if (m_enabled != wasEnabled)
        emit enabledChanged(m_enabled);
}

void PhoneHistoryModel::_q_handleCreate()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_ASSERT(reply);

    // find the item
    PhoneHistoryItem *item = 0;
    int row = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i]->reply == reply) {
            item = m_items[i];
            item->reply = 0;
            row = i;
            break;
        }
    }
    if (!item)
        return;

    // update the item
    QDomDocument doc;
    if (reply->error() != QNetworkReply::NoError || !doc.setContent(reply)) {
        qWarning("Failed to create phone call: %s", qPrintable(reply->errorString()));
        return;
    }
    item->parse(doc.documentElement());
    emit dataChanged(createIndex(row, 0), createIndex(row, 0));
}

void PhoneHistoryModel::_q_handleList()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_ASSERT(reply);

    QDomDocument doc;
    if (reply->error() != QNetworkReply::NoError || !doc.setContent(reply)) {
        qWarning("Failed to retrieve phone calls: %s", qPrintable(reply->errorString()));
        return;
    }

    QDomElement element = doc.documentElement().firstChildElement("call");
    while (!element.isNull()) {
        int id = element.firstChildElement("id").text().toInt();
        foreach (PhoneHistoryItem *item, m_items) {
            if (item->id == id) {
                id = 0;
                break;
            }
        }
        if (id > 0) {
            PhoneHistoryItem *item = new PhoneHistoryItem;
            item->parse(element);

            beginInsertRows(QModelIndex(), 0, 0);
            m_items.prepend(item);
            endInsertRows();
        }
        element = element.nextSiblingElement("call");
    }
}

void PhoneHistoryModel::_q_openUrl(const QUrl &url)
{
    if (url.scheme() != "sip") {
        qWarning("PhoneHistoryModel got a non-SIP URL!");
        return;
    }

    if (!url.path().isEmpty()) {
        const QString phoneNumber = url.path().split('@').first();
        const QString recipient = QString("\"%1\" <%2>").arg(phoneNumber, url.toString());
        call(recipient);
    }
    // FIXME
    //emit showPanel();
}


