/*
 * wiLink
 * Copyright (C) 2009-2012 Wifirst
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

import QtQuick 1.1
import QXmpp 0.4
import wiLink 2.0

FocusScope {
    id: root

    AccountModel {
        id: accountModel

        function clientForJid(jid) {
            var panel = swapper.findPanel('ChatPanel.qml', {accountJid: jid});
            return panel ? panel.client : undefined;
        }

        onModelReset: {
            appPlugins.unload();
            if (accountModel.count) {
                appPlugins.load();
            } else {
                dialogSwapper.showPanel('SetupDialog.qml');
            }
        }
    }

    QXmppLogger {
        id: appLogger
        loggingType: QXmppLogger.SignalLogging
    }

    PluginLoader {
        id: appPlugins
    }

    PreferenceModel {
        id: appPreferences
    }

    Style {
        id: appStyle
        isMobile: application.isMobile
    }

    Rectangle {
        anchors.fill: parent
        color: 'pink'
    }

    Loader {
        id: loader

        anchors.fill: parent
    }

    Dock {
        id: dock

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        z: 1
    }

    PanelSwapper {
        id: swapper

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: dock.right
        anchors.right: parent.right
        focus: true
    }

    MouseArea {
        id: cancelArea

        anchors.fill: parent
        enabled: false
        z: 11

        onClicked: menuLoader.hide()
    }

    PanelSwapper {
        id: dialogSwapper

        opacity: 0
        z: 10

        onCurrentItemChanged: {
            if (currentItem) {
                dialogSwapper.focus = true;
                x = Math.max(0, Math.floor((parent.width - currentItem.width) / 2));
                y = Math.max(0, Math.floor((parent.height - currentItem.height) / 2));
                opacity = 1;
            } else {
                swapper.focus = true;
                opacity = 0;
            }
        }
    }

    Loader {
        id: menuLoader

        z: 12

        function hide() {
            cancelArea.enabled = false;
            menuLoader.item.opacity = 0;
        }

        function show(x, y) {
            cancelArea.enabled = true;
            menuLoader.x = x;
            menuLoader.y = y;
            menuLoader.item.opacity = 1;
        }

        Connections {
            target: menuLoader.item
            onItemClicked: menuLoader.hide()
        }
    }

    Component.onCompleted: {
        window.minimumWidth = 360;
        window.minimumHeight = 360;
        window.fullScreen = application.isMobile && !application.isAndroid;
        window.showAndRaise();

        application.showWindows.connect(function() {
            window.showAndRaise();
        });

        if (accountModel.count) {
            appPlugins.load();
        } else {
            dialogSwapper.showPanel('SetupDialog.qml');
        }
    }

    Keys.forwardTo: dock
}
