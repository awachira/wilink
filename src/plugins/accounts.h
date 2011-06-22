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

#ifndef __WILINK_CHAT_ACCOUNTS_H__
#define __WILINK_CHAT_ACCOUNTS_H__

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QXmppClient;

class ChatAccountPrompt : public QDialog
{
    Q_OBJECT

public:
    ChatAccountPrompt(QWidget *parent = 0);

    QString jid() const;
    QString password() const;

    void setAccounts(const QStringList &accounts);
    void setDomain(const QString &domain);

private slots:
    void testAccount();
    void testFailed();

private:
    void showMessage(const QString &message, bool isError);

    QString m_domain;
    QStringList m_accounts;
    QString m_errorStyle;

    QDialogButtonBox *m_buttonBox;
    QLineEdit *m_jidEdit;
    QLabel *m_jidLabel;
    QLineEdit *m_passwordEdit;
    QLabel *m_promptLabel;
    QLabel *m_statusLabel;
    QXmppClient *m_testClient;
};

class ChatPasswordPrompt : public QDialog
{
    Q_OBJECT

public:
    ChatPasswordPrompt(const QString &jid, QWidget *parent = 0);
    QString password() const;

private:
    QLineEdit *m_passwordEdit;
};

#endif
