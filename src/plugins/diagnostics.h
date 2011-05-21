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

#ifndef __WILINK_DIAGNOSTICS_H__
#define __WILINK_DIAGNOSTICS_H__

#include "QXmppClientExtension.h"

#include "chat_panel.h"
#include "diagnostics/iq.h"

class Chat;
class DiagnosticsExtension;
class QTimer;

/** The DiagnosticsPanel represents panel for displaying diagnostics results.
 */
class DiagnosticsPanel : public ChatPanel
{
    Q_OBJECT

public:
    DiagnosticsPanel(Chat *chatWindow, QXmppClient *client);

private slots:
    void slotShow();

private:
    QXmppClient *m_client;
    bool m_displayed;
    DiagnosticsExtension *m_manager;
};

class DiagnosticsExtension : public QXmppClientExtension
{
    Q_OBJECT
    Q_PROPERTY(QString html READ html NOTIFY htmlChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)

public:
    DiagnosticsExtension(QXmppClient *client);

    QString html() const;
    bool running() const;

    /// \cond
    QStringList discoveryFeatures() const;
    bool handleStanza(const QDomElement &element);
    /// \endcond

signals:
    void htmlChanged(const QString &html);
    void runningChanged(bool running);

public slots:
    void refresh();

private slots:
    void diagnosticsServerFound(const QString &diagServer);
    void handleResults(const DiagnosticsIq &results);

private:
    void run(const DiagnosticsIq &request);

    QString m_diagnosticsServer;
    QString m_html;
    QThread *m_thread;
};

class DiagnosticsAgent : public QObject
{
    Q_OBJECT

public:
    DiagnosticsAgent(QObject *parent = 0) : QObject(parent) {};

private slots:
    void handle(const DiagnosticsIq &request);
    void transfersFinished(const QList<Transfer> &transfers);

signals:
    void finished(const DiagnosticsIq &results);

private:
    DiagnosticsIq iq;
};

#endif
