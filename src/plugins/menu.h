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

#ifndef __WILINK_MENU_H__
#define __WILINK_MENU_H__

#include <QMap>
#include <QObject>
#include <QStringList>
#include <QUrl>

class QAction;
class QMenu;
class QMenuBar;
class QNetworkAccessManager;
class QNetworkReply;

class Menu : public QObject
{
    Q_OBJECT

public:
    Menu(QMenuBar *bar);

private slots:
    void fetchMenu();
    void openUrl();
    void showIcon();
    void showMenu();

private:
    void fetchIcon(const QUrl &url, QAction *action);

    QMap<QNetworkReply *, QAction *> icons;
    QNetworkAccessManager *network;
    int refreshInterval;
    QStringList seenMessages;
    QByteArray userAgent;

private:
    QMenu *servicesMenu;
    QMenuBar *menuBar;
};

#endif
