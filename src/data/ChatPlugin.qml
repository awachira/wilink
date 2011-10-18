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

import QtQuick 1.0

Plugin {
    name: qsTr('Chat')
    description: qsTr('This plugin allows you to chat with your friends.')
    imageSource: 'chat.png'

    onLoaded: {
        dock.model.add({
            'iconSource': 'dock-chat.png',
            'iconPress': 'chat.png',
            'notified': false,
            'panelSource': 'ChatPanel.qml',
            'priority': 10,
            'text': qsTr('Chat'),
            'visible': true});
        swapper.showPanel('ChatPanel.qml');
    }

    onUnloaded: {
        dock.model.removePanel('ChatPanel.qml');
    }
}