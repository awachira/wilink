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

#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#include <QApplication>
#include <QString>
#include <QUrl>

#ifdef USE_SYSTRAY
#include <QSystemTrayIcon>
#endif

#define HELP_URL "https://www.wifirst.net/wilink/faq"

class UpdatesDialog;
class QAuthenticator;
class QAbstractNetworkCache;

class ApplicationPrivate;

class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int &argc, char **argv);
    ~Application();

    static void alert(QWidget *widget);
    static void platformInit();
    void createSystemTrayIcon();
    QAbstractNetworkCache *networkCache();
    UpdatesDialog *updatesDialog();
    void setUpdatesDialog(UpdatesDialog *updatesDialog);

    bool isInstalled();
    bool openAtLogin() const;
    void showMessage(QWidget *context, const QString &title, const QString &message);
    bool showOfflineContacts() const;

signals:
    void messageClicked(QWidget *context);
    void openAtLoginChanged(bool run);
    void showOfflineContactsChanged(bool show);

public slots:
    void openUrl(const QUrl &url);
    void setOpenAtLogin(bool run);
    void setShowOfflineContacts(bool show);

private slots:
    void messageClicked();
    void resetChats();
    void showAccounts();
    void showChats();
#ifdef USE_SYSTRAY
    void trayActivated(QSystemTrayIcon::ActivationReason reason);
#endif

private:
    static QString executablePath();
    void migrateFromWdesktop();

    ApplicationPrivate * const d;
};

#endif
