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

#include <QAudioInput>
#include <QAudioOutput>
#include <QByteArrayMatcher>
#include <QCoreApplication>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QPair>
#include <QUdpSocket>
#include <QTimer>

#include "QXmppRtpChannel.h"
#include "QXmppSaslAuth.h"
#include "QXmppSrvInfo.h"
#include "QXmppStun.h"
#include "QXmppUtils.h"

#include "sip.h"

const int RTP_COMPONENT = 1;
const int RTCP_COMPONENT = 2;

#define QXMPP_DEBUG_SIP
#define EXPIRE_SECONDS 300
#define TIMEOUT_SECONDS 30

SdpMessage::SdpMessage(const QByteArray &ba)
{
    const QByteArrayMatcher crlf("\r\n");

    int i = 0;
    while (i < ba.count() - 1) {
        const char field = ba[i];
        int j = i + 1;
        if (ba[j] != '=')
            break;
        j++;

        i = crlf.indexIn(ba, j);
        if (i == -1)
            break;

        m_fields.append(qMakePair(field, ba.mid(j, i - j)));
        i += 2;
    }
}

void SdpMessage::addField(char name, const QByteArray &data)
{
    m_fields.append(qMakePair(name, data));
}

QList<QPair<char, QByteArray> > SdpMessage::fields() const
{
    return m_fields;
}

QByteArray SdpMessage::toByteArray() const
{
    QByteArray ba;
    QList<QPair<char, QByteArray> >::ConstIterator it = m_fields.constBegin(),
                                                        end = m_fields.constEnd();
    for ( ; it != end; ++it) {
        ba += it->first;
        ba += "=";
        ba += it->second;
        ba += "\r\n";
    }
    return ba;
}

class SipCallContext
{
public:
    SipCallContext() : cseq(1) {}

    quint32 cseq;
    QMap<QByteArray, QByteArray> challenge;
    QMap<QByteArray, QByteArray> proxyChallenge;
};

class SipCallPrivate : public SipCallContext
{
public:
    QXmppCall::State state;
    QByteArray id;
    QXmppRtpChannel *channel;
    QAudioInput *audioInput;
    QAudioOutput *audioOutput;
    QHostAddress remoteHost;
    quint16 remotePort;
    QUdpSocket *socket;
    bool invitePending;
    QByteArray inviteRecipient;
    QByteArray inviteUri;
    SipPacket inviteRequest;
    QByteArray remoteRecipient;
    QByteArray remoteRoute;
    QByteArray remoteUri;

    SipCall *q;
    SipClient *client;
    QTimer *timer;
};

class SipClientPrivate : public SipCallContext
{
public:
    QByteArray authorization(const SipPacket &request, const QMap<QByteArray, QByteArray> &challenge) const;
    SipPacket buildRequest(const QByteArray &method, const QByteArray &uri, const QByteArray &id, int seq);
    void sendRequest(SipPacket &request, SipCallContext *ctx);

    QUdpSocket *socket;
    QByteArray baseId;
    QByteArray tag;
    SipPacket registerRequest;
    QTimer *registerTimer;

    // configuration
    QString displayName;
    QString username;
    QString password;
    QString domain;

    SipClient::State state;
    QString rinstance;
    QHostAddress serverAddress;
    QString serverName;
    quint16 serverPort;
    QList<SipCall*> calls;

    QHostAddress reflexiveAddress;
    quint16 reflexivePort;
    SipClient *q;
};

SipCall::SipCall(const QString &recipient, QUdpSocket *socket, SipClient *parent)
    : QXmppLoggable(parent),
    d(new SipCallPrivate)
{
    d->client = parent;
    d->q = this;
    d->state = QXmppCall::OfferState;
    d->invitePending = false;
    d->inviteRecipient = QString("<%1>").arg(recipient).toUtf8();
    d->inviteUri = recipient.toUtf8();
    d->remoteRecipient = QString("<%1>").arg(recipient).toUtf8();
    d->remoteUri = recipient.toUtf8();

    d->id = generateStanzaHash().toLatin1();
    d->channel = new QXmppRtpChannel(this);
    d->audioInput = 0;
    d->audioOutput = 0;
    d->remotePort = 0;
    d->socket = socket;
    d->socket->setParent(this);
    d->timer = new QTimer(this);
    d->timer->setSingleShot(true);

    connect(d->channel, SIGNAL(logMessage(QXmppLogger::MessageType,QString)),
            this, SIGNAL(logMessage(QXmppLogger::MessageType,QString)));
    connect(d->channel, SIGNAL(sendDatagram(QByteArray)),
            this, SLOT(writeToSocket(QByteArray)));
    connect(d->socket, SIGNAL(readyRead()),
            this, SLOT(readFromSocket()));
    connect(d->timer, SIGNAL(timeout()),
            this, SLOT(handleTimeout()));

    d->timer->start(TIMEOUT_SECONDS * 1000);
}

SipCall::~SipCall()
{
    delete d;
}

QByteArray SipCall::id() const
{
    return d->id;
}

void SipCall::sendInvite()
{
    SdpMessage sdp;
    sdp.addField('v', "0");
    sdp.addField('o', "- 1289387706660194 1 IN IP4 " + d->socket->localAddress().toString().toUtf8());
    sdp.addField('s', qApp->applicationName().toUtf8());
    sdp.addField('c', "IN IP4 " + d->client->d->socket->localAddress().toString().toUtf8());
    sdp.addField('t', "0 0");
#ifdef USE_ICE
    sdp.addField('a', "ice-ufrag:" + call->iceConnection->localUser().toUtf8());
    sdp.addField('a', "ice-pwd:" + call->iceConnection->localPassword().toUtf8());
#endif
    // RTP profile
    QByteArray profiles = "RTP/AVP";
    QList<QByteArray> attrs;
    foreach (const QXmppJinglePayloadType &payload, d->channel->supportedPayloadTypes()) {
        profiles += " " + QByteArray::number(payload.id());
        attrs << "rtpmap:" + QByteArray::number(payload.id()) + " " + payload.name().toUtf8() + "/" + QByteArray::number(payload.clockrate());
    }
    profiles += " 101";
    attrs << "rtpmap:101 telephone-event/8000";
    attrs << "fmtp:101 0-15";
    attrs << "sendrecv";

    sdp.addField('m', "audio " + QByteArray::number(d->socket->localPort()) + " "  + profiles);
    foreach (const QByteArray &attr, attrs)
        sdp.addField('a', attr);
#ifdef USE_ICE
    foreach (const QXmppJingleCandidate &candidate, call->iceConnection->localCandidates()) {
        QByteArray ba = "candidate:" + QByteArray::number(candidate.foundation()) + " ";
        ba += QByteArray::number(candidate.component()) + " ";
        ba += "UDP ";
        ba += QByteArray::number(candidate.priority()) + " ";
        ba += candidate.host().toString().toUtf8() + " ";
        ba += QByteArray::number(candidate.port()) + " ";
        ba += "typ host";
        sdp.addField('a', ba);
    }
#endif

    SipPacket request = d->client->d->buildRequest("INVITE", d->inviteUri, d->id, d->cseq++);
    request.setHeaderField("To", d->inviteRecipient);
    request.setHeaderField("Content-Type", "application/sdp");
    request.setBody(sdp.toByteArray());
    d->client->d->sendRequest(request, d);
    d->invitePending = true;
    d->inviteRequest = request;
}

QString SipCall::recipient() const
{
    return QString::fromUtf8(d->remoteRecipient);
}

void SipCall::handleReply(const SipPacket &reply)
{
    const QByteArray command = reply.headerField("CSeq").split(' ').last();

    // store information
    if (!reply.headerField("To").isEmpty())
        d->remoteRecipient = reply.headerField("To");
    if (!reply.headerField("Contact").isEmpty())
        d->remoteUri = reply.headerField("Contact").replace("<", "").replace(">", "");
    if (!reply.headerField("Record-Route").isEmpty())
        d->remoteRoute = reply.headerField("Record-Route");

    // send ack
    if  (command == "INVITE" && reply.statusCode() >= 200) {
        d->invitePending = false;

        SipPacket request;
        request.setMethod("ACK");

        QList<QByteArray> fields;
        fields << "Via" << "Contact" << "Max-Forwards" << "Proxy-Authorization" << "Authorization";
        foreach (const QByteArray &field, fields)
        {
            if (!d->inviteRequest.headerField(field).isEmpty())
                request.setHeaderField(field, d->inviteRequest.headerField(field));
        }
        quint32 cseq = d->inviteRequest.sequenceNumber();

        request.setUri(d->remoteUri);
        request.setHeaderField("Route", d->remoteRoute);
        request.setHeaderField("To", d->remoteRecipient);
        request.setHeaderField("From", reply.headerField("From"));
        request.setHeaderField("Call-Id", reply.headerField("Call-Id"));
        request.setHeaderField("CSeq", QByteArray::number(cseq) + " ACK");
        d->client->d->sendRequest(request, d);
    }

    // handle authentication
    if (reply.statusCode() == 407) {

        const QByteArray auth = reply.headerField("Proxy-Authenticate");
        if (!auth.startsWith("Digest ") || !d->inviteRequest.headerField("Proxy-Authorization").isEmpty())  {
            warning("Proxy authentication failed");
            setState(QXmppCall::FinishedState);
            return;
        }
        d->proxyChallenge = QXmppSaslDigestMd5::parseMessage(auth.mid(7));

        SipPacket request = d->inviteRequest;
        request.setHeaderField("CSeq", QString::number(d->cseq++).toLatin1() + ' ' + request.method());
        d->client->d->sendRequest(request, d);
        d->invitePending = true;
        d->inviteRequest = request;
        return;
    }

    if (command == "BYE") {

        if (d->invitePending) {
            SipPacket request = d->client->d->buildRequest("CANCEL", d->inviteUri, d->id, d->inviteRequest.sequenceNumber());
            request.setHeaderField("To", d->inviteRecipient);
            request.setHeaderField("Via", d->inviteRequest.headerField("Via"));
            request.removeHeaderField("Contact");
            d->client->d->sendRequest(request, d);
        } else {
            setState(QXmppCall::FinishedState);
        }

    } else if (command == "CANCEL") {

        setState(QXmppCall::FinishedState);

    } else if (command == "INVITE") {

        if (reply.statusCode() == 180)
        {
            emit ringing();
        }
        else if (reply.statusCode() == 200)
        {
            debug(QString("Call %1 established").arg(QString::fromUtf8(d->id)));
            d->timer->stop();

            if (reply.headerField("Content-Type") == "application/sdp" && !d->audioOutput)
            {
                // parse descriptor
                SdpMessage sdp(reply.body());
                QPair<char, QByteArray> field;
                foreach (field, sdp.fields()) {
                    if (field.first == 'c') {
                        // determine remote host
                        if (field.second.startsWith("IN IP4 ")) {
                            d->remoteHost = QHostAddress(QString::fromUtf8(field.second.mid(7)));
                        }
                    } else if (field.first == 'm') {
                        QList<QByteArray> bits = field.second.split(' ');
                        if (bits.size() < 3 || bits[0] != "audio" || bits[2] != "RTP/AVP")
                            continue;

                        // determine remote port
                        d->remotePort = bits[1].toUInt();

                        // determine codec
                        for (int i = 3; i < bits.size() && !d->channel->isOpen(); ++i)
                        {
                            foreach (const QXmppJinglePayloadType &payload, d->channel->supportedPayloadTypes()) {
                                bool ok = false;
                                if (payload.id() == bits[i].toInt(&ok) && ok) {
                                    d->channel->setPayloadType(payload);
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!d->channel->isOpen()) {
                    warning("Could not negociate a common codec");
                    return;
                }

                // prepare audio format
                QAudioFormat format;
                format.setFrequency(d->channel->payloadType().clockrate());
                format.setChannels(d->channel->payloadType().channels());
                format.setSampleSize(16);
                format.setCodec("audio/pcm");
                format.setByteOrder(QAudioFormat::LittleEndian);
                format.setSampleType(QAudioFormat::SignedInt);

                int packetSize = (format.frequency() * format.channels() * (format.sampleSize() / 8)) * d->channel->payloadType().ptime() / 1000;

                // initialise audio output
                d->audioOutput = new QAudioOutput(format, this);
                d->audioOutput->setBufferSize(2 * packetSize);
                d->audioOutput->start(d->channel);

                // initialise audio input
                d->audioInput = new QAudioInput(format, this);
                d->audioInput->setBufferSize(2 * packetSize);
                d->audioInput->start(d->channel);

                setState(QXmppCall::ActiveState);
            }

        } else if (reply.statusCode() >= 300) {

            warning(QString("Call %1 failed").arg(
                QString::fromUtf8(d->id)));
            d->timer->stop();
            setState(QXmppCall::FinishedState);
        }
    }
}

void SipCall::handleRequest(const SipPacket &request)
{
    debug(QString("Got request %1 %2").arg(
        QString::fromUtf8(request.method()),
        QString::fromUtf8(request.uri())));

    SipPacket response;
    response.setStatusCode(200);
    response.setReasonPhrase("OK");
    foreach (const QByteArray &via, request.headerFieldValues("Via"))
        response.addHeaderField("Via", via);
    response.setHeaderField("From", request.headerField("From"));
    response.setHeaderField("To", request.headerField("To"));
    response.setHeaderField("Call-Id", request.headerField("Call-Id"));
    response.setHeaderField("CSeq", request.headerField("CSeq"));
    response.setHeaderField("Record-Route", request.headerField("Record-Route"));
    response.setHeaderField("Contact", QString("<sip:%1@%2:%3>").arg(
        d->client->d->username,
        d->client->d->reflexiveAddress.toString(),
        QString::number(d->client->d->reflexivePort)).toUtf8());

    d->client->d->sendRequest(response, d);

    if (request.method() == "BYE") {
        setState(QXmppCall::FinishedState);
    }
}

void SipCall::handleTimeout()
{
    warning(QString("Call %1 timed out").arg(QString::fromUtf8(d->id)));
    setState(QXmppCall::FinishedState);
}

void SipCall::hangup()
{
    if (d->state == QXmppCall::DisconnectingState ||
        d->state == QXmppCall::FinishedState)
        return;

    debug(QString("Call %1 hangup").arg(
            QString::fromUtf8(d->id)));
    setState(QXmppCall::DisconnectingState);

    // stop audio
    if (d->audioInput)
    {
        d->audioInput->stop();
        delete d->audioInput;
        d->audioInput = 0;
    }
    if (d->audioOutput)
    {
        d->audioOutput->stop();
        delete d->audioOutput;
        d->audioOutput = 0;
    }
    d->socket->close();

    SipPacket request = d->client->d->buildRequest("BYE", d->remoteUri, d->id, d->cseq++);
    request.setHeaderField("To", d->remoteRecipient);
    request.setHeaderField("Route", d->remoteRoute);
    d->client->d->sendRequest(request, d);
}

void SipCall::readFromSocket()
{
    while (d->socket && d->socket->hasPendingDatagrams())
    {
        QByteArray ba(d->socket->pendingDatagramSize(), '\0');
        QHostAddress remoteHost;
        quint16 remotePort = 0;
        d->socket->readDatagram(ba.data(), ba.size(), &remoteHost, &remotePort);
        d->channel->datagramReceived(ba);
    }
}

void SipCall::setState(QXmppCall::State state)
{
    if (d->state != state)
    {
        d->state = state;
        emit stateChanged(d->state);

        if (d->state == QXmppCall::ActiveState)
            emit connected();
        else if (d->state == QXmppCall::FinishedState)
        {
            debug(QString("Call %1 finished").arg(QString::fromUtf8(d->id)));
            emit finished();
        }
    }
}

void SipCall::writeToSocket(const QByteArray &ba)
{
    if (!d->socket || d->remoteHost.isNull() || !d->remotePort)
        return;

    d->socket->writeDatagram(ba, d->remoteHost, d->remotePort);
}

QByteArray SipClientPrivate::authorization(const SipPacket &request, const QMap<QByteArray, QByteArray> &challenge) const
{
    QXmppSaslDigestMd5 digest;
    digest.setCnonce(digest.generateNonce());
    digest.setNc("00000001");
    digest.setNonce(challenge.value("nonce"));
    digest.setQop(challenge.value("qop"));
    digest.setRealm(challenge.value("realm"));
    digest.setUsername(username.toUtf8());
    digest.setPassword(password.toUtf8());

    const QByteArray A1 = digest.username() + ':' + digest.realm() + ':' + password.toUtf8();
    const QByteArray A2 = request.method() + ':' + request.uri();

    QMap<QByteArray, QByteArray> response;
    response["username"] = digest.username();
    response["realm"] = digest.realm();
    response["nonce"] = digest.nonce();
    response["uri"] = request.uri();
    response["response"] = digest.calculateDigest(A1, A2);
    response["cnonce"] = digest.cnonce();
    response["qop"] = digest.qop();
    response["nc"] = digest.nc();
    response["algorithm"] = "MD5";

    return QByteArray("Digest ") + QXmppSaslDigestMd5::serializeMessage(response);
}

SipPacket SipClientPrivate::buildRequest(const QByteArray &method, const QByteArray &uri, const QByteArray &callId, int seqNum)
{
    QString addr;
    if (!displayName.isEmpty())
        addr += QString("\"%1\"").arg(displayName);
    addr += QString("<sip:%1@%2>").arg(username, domain);
    const QByteArray branch = "z9hG4bK-" + generateStanzaHash().toLatin1();
    const QString via = QString("SIP/2.0/UDP %1:%2;branch=%3;rport").arg(
        socket->localAddress().toString(),
        QString::number(socket->localPort()),
        branch);

    SipPacket packet;
    packet.setMethod(method);
    packet.setUri(uri);
    packet.setHeaderField("Via", via.toLatin1());
    packet.setHeaderField("Max-Forwards", "70");
    packet.setHeaderField("Call-ID", callId);
    packet.setHeaderField("CSeq", QString::number(seqNum).toLatin1() + ' ' + method);
    packet.setHeaderField("Contact", QString("<sip:%1@%2:%3;rinstance=%4>").arg(
        username, socket->localAddress().toString(),
        QString::number(socket->localPort()), rinstance).toUtf8());
    packet.setHeaderField("To", addr.toUtf8());
    packet.setHeaderField("From", addr.toUtf8() + ";tag=" + tag);
    if (method != "ACK" && method != "CANCEL")
        packet.setHeaderField("Allow", "INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO");
    return packet;
}

void SipClientPrivate::sendRequest(SipPacket &request, SipCallContext *ctx)
{
    if (request.isRequest()) {
        if (!ctx->challenge.isEmpty())
            request.setHeaderField("Authorization", authorization(request, ctx->challenge));
        if (!ctx->proxyChallenge.isEmpty())
            request.setHeaderField("Proxy-Authorization", authorization(request, ctx->proxyChallenge));
    }
    request.setHeaderField("User-Agent", QString("%1/%2").arg(qApp->applicationName(), qApp->applicationVersion()).toUtf8());

#ifdef QXMPP_DEBUG_SIP
    q->logMessage(QXmppLogger::SentMessage,
        QString("SIP packet to %1:%2\n%3").arg(
            serverAddress.toString(),
            QString::number(serverPort),
            QString::fromUtf8(request.toByteArray())));
#endif

    socket->writeDatagram(request.toByteArray(), serverAddress, serverPort);
}

SipClient::SipClient(QObject *parent)
    : QXmppLoggable(parent),
    d(new SipClientPrivate)
{
    d->q = this;
    d->state = DisconnectedState;
    d->rinstance = generateStanzaHash(16);
    d->reflexivePort = 0;
    d->socket = new QUdpSocket(this);
    connect(d->socket, SIGNAL(readyRead()),
            this, SLOT(datagramReceived()));

    d->registerTimer = new QTimer(this);
    connect(d->registerTimer, SIGNAL(timeout()),
            this, SLOT(connectToServer()));
}

SipClient::~SipClient()
{
    if (d->state == ConnectedState)
        disconnectFromServer();
    delete d;
}

SipCall *SipClient::call(const QString &recipient)
{
    if (d->state != ConnectedState) {
        warning("Cannot dial call, not connected to server");
        return 0;
    }

    QUdpSocket *socket = new QUdpSocket;
    if (!socket->bind(d->socket->localAddress(), 0))
    {
        warning("Could not start listening for RTP");
        delete socket;
        return 0;
    }

    SipCall *call = new SipCall(recipient, socket, this);
    connect(call, SIGNAL(destroyed(QObject*)),
            this, SLOT(callDestroyed(QObject*)));
    d->calls << call;
    call->sendInvite();

    return call;
}

void SipClient::callDestroyed(QObject *object)
{
    d->calls.removeAll(static_cast<SipCall*>(object));
}

void SipClient::connectToServer()
{
    debug(QString("Looking up server for domain %1").arg(d->domain));
    QXmppSrvInfo::lookupService("_sip._udp." + d->domain, this,
                                SLOT(connectToServer(QXmppSrvInfo)));
}

void SipClient::connectToServer(const QXmppSrvInfo &serviceInfo)
{
    if (!serviceInfo.records().isEmpty()) {
        d->serverName = serviceInfo.records().first().target();
        d->serverPort = serviceInfo.records().first().port();
    } else {
        d->serverName = d->domain;
        d->serverPort = 5060;
    }

    QHostInfo info = QHostInfo::fromName(d->serverName);
    if (info.addresses().isEmpty()) {
        warning(QString("Could not lookup SIP server %1").arg(d->serverName));
        return;
    }
    d->serverAddress = info.addresses().first();
    d->baseId = generateStanzaHash().toLatin1();
    d->tag = generateStanzaHash(8).toLatin1();

    // bind socket
    if (d->socket->state() == QAbstractSocket::UnconnectedState) {
        QHostAddress bindAddress;
        foreach (const QHostAddress &ip, QNetworkInterface::allAddresses())
        {
            if (ip.protocol() == QAbstractSocket::IPv4Protocol &&
                ip != QHostAddress::Null &&
                ip != QHostAddress::LocalHost &&
                ip != QHostAddress::LocalHostIPv6 &&
                ip != QHostAddress::Broadcast &&
                ip != QHostAddress::Any &&
                ip != QHostAddress::AnyIPv6)
            {
                bindAddress = ip;
                break;
            }
        }

        debug(QString("Listening for SIP on %1").arg(bindAddress.toString()));
        if(!d->socket->bind(bindAddress, 0))
        {
            warning("Could not start listening for SIP");
            return;
        }
    }

    // register
    debug(QString("Connecting to SIP server %1:%2").arg(d->serverName, QString::number(d->serverPort)));

    const QByteArray uri = QString("sip:%1").arg(d->serverName).toUtf8();
    SipPacket request = d->buildRequest("REGISTER", uri, d->baseId, d->cseq++);
    request.setHeaderField("Expires", QByteArray::number(EXPIRE_SECONDS));
    d->sendRequest(request, d);
    d->registerRequest = request;
    d->registerTimer->start((EXPIRE_SECONDS - TIMEOUT_SECONDS) * 1000);

    setState(ConnectingState);
}

void SipClient::disconnectFromServer()
{
    d->registerTimer->stop();

    // terminate calls
    foreach (SipCall *call, d->calls)
        call->hangup();

    // unregister
    if (d->state == SipClient::ConnectedState) {
        const QByteArray uri = QString("sip:%1").arg(d->serverName).toUtf8();
        SipPacket request = d->buildRequest("REGISTER", uri, d->baseId, d->cseq++);
        request.setHeaderField("Contact", request.headerField("Contact") + ";expires=0");
        d->sendRequest(request, d);
        d->registerRequest = request;

        setState(DisconnectingState);
    }
}

void SipClient::datagramReceived()
{
    if (!d->socket->hasPendingDatagrams())
        return;

    // receive datagram
    const qint64 size = d->socket->pendingDatagramSize();
    QByteArray buffer(size, 0);
    QHostAddress remoteHost;
    quint16 remotePort;
    d->socket->readDatagram(buffer.data(), buffer.size(), &remoteHost, &remotePort);

#ifdef QXMPP_DEBUG_SIP
    emit logMessage(QXmppLogger::ReceivedMessage,
        QString("SIP packet from %1\n%2").arg(remoteHost.toString(), QString::fromUtf8(buffer)));
#endif

    // parse packet
    SipPacket reply(buffer);

    // find corresponding call
    SipCall *currentCall = 0;
    const QByteArray callId = reply.headerField("Call-ID");
    if (callId != d->baseId)
    {
        foreach (SipCall *potentialCall, d->calls)
        {
            if (potentialCall->id() == callId)
            {
                currentCall = potentialCall;
                break;
            }
        }
        if (!currentCall)
        {
            warning("Received a reply for unknown call " + callId);
            return;
        }
    }

    // check whether it's a request or a response
    if (reply.isRequest()) {
        if (currentCall)
            currentCall->handleRequest(reply);
    } else if (reply.isReply()) {
        if (currentCall)
            currentCall->handleReply(reply);
        else
            handleReply(reply);
    } else if (!reply.isReply()) {
        warning("SIP packet is neither request nor reply");
    }
}

void SipClient::handleReply(const SipPacket &reply)
{
    const QByteArray command = reply.headerField("CSeq").split(' ').last();

    if (reply.statusCode() == 401) {
        // handle authentication
        const QByteArray auth = reply.headerField("WWW-Authenticate");
        if (!auth.startsWith("Digest ") || !d->registerRequest.headerField("Authorization").isEmpty())  {
            warning("Authentication failed");
            setState(DisconnectedState);
            return;
        }
        d->challenge = QXmppSaslDigestMd5::parseMessage(auth.mid(7));

        SipPacket request = d->registerRequest;
        request.setHeaderField("CSeq", QString::number(d->cseq++).toLatin1() + ' ' + request.method());
        d->sendRequest(request, d);
        d->registerRequest = request;
    }
    else if (command == "REGISTER" && reply.statusCode() == 200) {
        if (d->state == DisconnectingState) {
            setState(DisconnectedState);
        } else {
            QMap<QByteArray, QByteArray> params = reply.headerFieldParameters("Via");
            if (params.contains("received"))
                d->reflexiveAddress = QHostAddress(QString::fromLatin1(params.value("received")));
            if (params.contains("rport"))
                d->reflexivePort = params.value("rport").toUInt();
            const QByteArray uri = QString("sip:%1@%2").arg(d->username, d->domain).toUtf8();
            SipPacket request = d->buildRequest("SUBSCRIBE", uri, d->baseId, d->cseq++);
            request.setHeaderField("Expires", QByteArray::number(EXPIRE_SECONDS));
            d->sendRequest(request, d);
        }
    }
    else if (command == "SUBSCRIBE" && reply.statusCode() == 200) {
        setState(ConnectedState);
    }
}


QString SipClient::serverName() const
{
    return d->serverName;
}

void SipClient::setDisplayName(const QString &displayName)
{
    d->displayName = displayName;
}

void SipClient::setDomain(const QString &domain)
{
    d->domain = domain;
}

void SipClient::setPassword(const QString &password)
{
    d->password = password;
}

void SipClient::setUsername(const QString &username)
{
    d->username = username;
}

void SipClient::setState(SipClient::State state)
{
    if (d->state != state)
    {
        d->state = state;
        emit stateChanged(d->state);

        if (d->state == SipClient::ConnectedState)
            emit connected();
        else if (d->state == SipClient::DisconnectedState)
            emit disconnected();
    }
}

SipPacket::SipPacket(const QByteArray &bytes)
    : m_statusCode(0)
{
    const QByteArrayMatcher crlf("\r\n");
    const QByteArrayMatcher colon(":");

    int j = crlf.indexIn(bytes);
    if (j < 0)
        return;

    // parse status
    const QByteArray status = bytes.left(j);
    if (status.size() >= 10 && status.startsWith("SIP/2.0 ")) {
        m_statusCode = status.mid(8, 3).toInt();
        m_reasonPhrase = QString::fromUtf8(status.mid(12));
    }
    else if (status.size() > 10 && status.endsWith(" SIP/2.0")) {
        int n = status.indexOf(' ');
        m_method = status.left(n);
        m_uri = status.mid(n + 1, j - n - 9);
    } else {
        j = -1;
    }

    // parse headers
    const QByteArray header = bytes.mid(j+1);
    int i = 0;
    while (i < header.count()) {
        int n = crlf.indexIn(header, i);

        if (n < 0) {
            // something is wrong
            qWarning("Missing end of line in SIP header");
            return;
        }
        else if (n == i) {
            // end of header
            i = n + 2;
            break;
        }

        // parse header field
        int j = colon.indexIn(header, i);
        if (j == -1)
            break;
        QByteArray field = header.mid(i, j - i).trimmed();
        j++;
        QByteArray value = header.mid(j, n - j).trimmed();

        // expand shortcuts
        if (field == "c")
            field = "Content-Type";
        else if (field == "f")
            field = "From";
        else if (field == "i")
            field = "Call-ID";
        else if (field == "k")
            field = "Supported";
        else if (field == "l")
            field = "Content-Length";
        else if (field == "m")
            field = "Contact";
        else if (field == "t")
            field = "To";
        else if (field == "v")
            field = "Via";
        m_fields.append(qMakePair(field, value));

        i = n + 2;
    }
    if (i >= 0)
        m_body = header.mid(i);
}

QByteArray SipPacket::headerField(const QByteArray &name, const QByteArray &defaultValue) const
{
    QList<QByteArray> allValues = headerFieldValues(name);
    if (allValues.isEmpty())
        return defaultValue;

    QByteArray result;
    bool first = true;
    foreach (const QByteArray &value, allValues) {
        if (!first)
            result += ", ";
        first = false;
        result += value;
    }
    return result;
}

QList<QByteArray> SipPacket::headerFieldValues(const QByteArray &name) const
{
    QList<QByteArray> result;
    QList<QPair<QByteArray, QByteArray> >::ConstIterator it = m_fields.constBegin(),
                                                        end = m_fields.constEnd();
    for ( ; it != end; ++it)
        if (qstricmp(name.constData(), it->first) == 0)
            result += it->second;

    return result;
}

QMap<QByteArray, QByteArray> SipPacket::headerFieldParameters(const QByteArray &name) const
{
    const QByteArray value = headerField(name);
    QMap<QByteArray, QByteArray> params;
    // FIXME: this is a very, very naive implementation
    QList<QByteArray> bits = value.split(';');
    if (bits.size() > 1)
    {
        bits.removeFirst();
        foreach (const QByteArray &bit, bits)
        {
            int i = bit.indexOf('=');
            if (i >= 0)
                params[bit.left(i)] = bit.mid(i+1);
        }
    }
    return params;
}

void SipPacket::addHeaderField(const QByteArray &name, const QByteArray &data)
{
    m_fields.append(qMakePair(name, data));
}

void SipPacket::removeHeaderField(const QByteArray &name)
{
    QList<QPair<QByteArray, QByteArray> >::Iterator it = m_fields.begin();
    while (it != m_fields.end()) {
        if (qstricmp(name.constData(), it->first) == 0)
            it = m_fields.erase(it);
        else
            ++it;
    }
}

void SipPacket::setHeaderField(const QByteArray &name, const QByteArray &data)
{
    removeHeaderField(name);
    m_fields.append(qMakePair(name, data));
}

bool SipPacket::isReply() const
{
    return m_statusCode != 0;
}

bool SipPacket::isRequest() const
{
    return !m_method.isEmpty() && !m_uri.isEmpty();
}

QByteArray SipPacket::body() const
{
    return m_body;
}

void SipPacket::setBody(const QByteArray &body)
{
    m_body = body;
}

QByteArray SipPacket::method() const
{
    return m_method;
}

void SipPacket::setMethod(const QByteArray &method)
{
    m_method = method;
}

QByteArray SipPacket::uri() const
{
    return m_uri;
}

void SipPacket::setUri(const QByteArray &uri)
{
    m_uri = uri;
}

QString SipPacket::reasonPhrase() const
{
    return m_reasonPhrase;
}

void SipPacket::setReasonPhrase(const QString &reasonPhrase)
{
    m_reasonPhrase = reasonPhrase;
}

quint32 SipPacket::sequenceNumber() const
{
    return headerField("CSeq").split(' ').first().toUInt();
}

int SipPacket::statusCode() const
{
    return m_statusCode;
}

void SipPacket::setStatusCode(int statusCode)
{
    m_statusCode = statusCode;
}

QByteArray SipPacket::toByteArray() const
{
    QByteArray ba;

    if (!m_method.isEmpty()) {
        ba += m_method;
        ba += ' ';
        ba += m_uri;
        ba += " SIP/2.0\r\n";
    } else {
        ba += "SIP/2.0 ";
        ba += QByteArray::number(m_statusCode);
        ba += ' ';
        ba += m_reasonPhrase.toUtf8();
        ba += "\r\n";
    }

    bool hasLength = false;
    QList<QPair<QByteArray, QByteArray> >::ConstIterator it = m_fields.constBegin(),
                                                        end = m_fields.constEnd();
    for ( ; it != end; ++it) {
        if (qstricmp("Content-Length", it->first) == 0)
            hasLength = true;
        ba += it->first;
        ba += ": ";
        ba += it->second;
        ba += "\r\n";
    }
    if (!hasLength)
        ba += "Content-Length: " + QByteArray::number(m_body.size()) + "\r\n";

    ba += "\r\n";
    return ba + m_body;
}


