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

#ifndef __SIP_P_H__
#define __SIP_P_H__

#include <QDnsLookup>
#include <QHostAddress>
#include <QMap>
#include <QObject>

#include "sip.h"

class QUdpSocket;
class QTimer;

/** The SipCallContext class represents a SIP dialog.
 */
class SipCallContext
{
public:
    SipCallContext();
    bool handleAuthentication(const SipMessage &reply);

    quint32 cseq;
    QByteArray id;
    QByteArray tag;

    QMap<QByteArray, QByteArray> challenge;
    QMap<QByteArray, QByteArray> proxyChallenge;
    QList<SipTransaction*> transactions;
};

class SipCallPrivate : public SipCallContext
{
public:
    SipCallPrivate(SipCall *qq);
    QString buildSdp() const;
    void handleReply(const SipMessage &reply);
    void handleRequest(const SipMessage &request);
    bool handleSdp(const QString &sdp);
    void onStateChanged();
    void sendInvite();
    void setState(SipCall::State state);

    SipCall::Direction direction;
    QString errorString;
    QDateTime startStamp;
    QDateTime finishStamp;
    SipCall::State state;
    QString activeTime;
    QXmppRtpAudioChannel *audioChannel;
    QXmppIceConnection *iceConnection;
    bool invitePending;
    bool inviteQueued;
    SipMessage inviteRequest;
    QByteArray remoteRecipient;
    QList<QByteArray> remoteRoute;
    QByteArray remoteUri;

    SipClient *client;
    QTimer *durationTimer;
    QTimer *timeoutTimer;

private:
    SipCall *q;
};

class SipClientPrivate : public SipCallContext
{
public:
    SipClientPrivate(SipClient *qq);
    SipMessage buildRequest(const QByteArray &method, const QByteArray &uri, SipCallContext *ctx, int seq);
    SipMessage buildResponse(const SipMessage &request);
    SipMessage buildRetry(const SipMessage &original, SipCallContext *ctx);
    void handleReply(const SipMessage &reply);
    void setState(SipClient::State state);

    // timers
    QTimer *connectTimer;
    QTimer *stunTimer;

    // configuration
    QString displayName;
    QString username;
    QString password;
    QString domain;

    QXmppLogger *logger;
    SipClient::State state;
    QHostAddress serverAddress;
    quint16 serverPort;
    QList<SipCall*> calls;

    // sockets
    QDnsLookup sipDns;
    QHostAddress localAddress;
    QUdpSocket *socket;

    // STUN
    quint32 stunCookie;
    QDnsLookup stunDns;
    bool stunDone;
    QByteArray stunId;
    QHostAddress stunReflexiveAddress;
    quint16 stunReflexivePort;
    QHostAddress stunServerAddress;
    quint16 stunServerPort;

private:
    QByteArray authorization(const SipMessage &request, const QMap<QByteArray, QByteArray> &challenge) const;
    void setContact(SipMessage &request);
    SipClient *q;
};

#endif
