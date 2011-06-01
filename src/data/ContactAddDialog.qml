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
import wiLink 1.2
import 'utils.js' as Utils

Dialog {
    id: dialog

    property string domain: Utils.jidToDomain(window.client.jid)

    title: qsTr('Add a contact')
    minWidth: 250
    minHeight: (help.opacity == 1) ? 250 : 150
    height: (help.opacity == 1) ? 250 : 150
    width: 250

    onAccepted: {
        var jid = contactEdit.text;
        if (jid.match(/^[^@/]+@[^@/]+$/)) {
            console.log("add " + jid);
            window.client.rosterManager.subscribe(jid);
            dialog.hide();
        }
    }

    PanelHelp {
        id: help

        anchors.top: contents.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        opacity: (domain == 'wifirst.net') ? 1 : 0
        text: qsTr('<b>Tip</b>: your wAmis are automatically added to your chat contacts, so the easiest way to add Wifirst contacts is to <a href=\"%1\">add them as wAmis</a>').replace('%1', 'https://www.wifirst.net/w/friends?from=wiLink')
    }

    Text {
        id: title

        anchors.top: (domain == 'wifirst.net') ? help.bottom : contents.top
        anchors.left:  parent.left
        anchors.right: parent.right
        anchors.margins: 8
        text: qsTr('Enter the address of the contact you want to add.')
        wrapMode: Text.WordWrap
    }

    Rectangle {
        id: bar

        anchors.margins: 8
        anchors.top: title.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        border.color: '#c3c3c3'
        border.width: 1
        color: 'white'
        width: 100
        height: contactEdit.paintedHeight + 8

        TextEdit {
            id: contactEdit

            anchors.fill: parent
            anchors.margins: 4
            focus: true
            smooth: true
            text: (domain != '') ? '@' + domain : ''
            textFormat: TextEdit.PlainText

            Keys.onReturnPressed: {
                dialog.accepted();
                return false;
            }
        }
    }
}

