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
import QXmpp 0.4
import wiLink 1.2
import 'utils.js' as Utils

FocusScope {
    id: panel

    property bool acceptableInput: (jidInput.acceptableInput && passwordInput.acceptableInput)
    property string domain: ''
    property ListModel model
    property string testJid
    property string testPassword

    signal accepted(string jid, string password)
    signal close

    PanelHelp {
        id: help

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        text: panel.domain != '' ? qsTr("Enter the username and password for your '%1' account.").replace('%1', panel.domain) : qsTr('Enter the address and password for the account you want to add.')
    }

    Item {
        id: jidRow

        anchors.top: help.bottom
        anchors.topMargin: appStyle.spacing.vertical
        anchors.left: parent.left
        anchors.right: parent.right
        height: jidInput.height

        Text {
            id: jidLabel

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            font.bold: true
            text: panel.domain != '' ? qsTr('Username') : qsTr('Address')
            width: 100
        }

        InputBar {
            id: jidInput

            anchors.top: parent.top
            anchors.left: jidLabel.right
            anchors.leftMargin: appStyle.spacing.horizontal
            anchors.right: parent.right
            focus: false
            validator: RegExpValidator {
                regExp: panel.domain != '' ? /^[^@/ ]+$/ : /^[^@/ ]+@[^@/ ]+$/
            }

            onTextChanged: {
                if (panel.state == 'dupe') {
                    panel.state = '';
                }
            }
        }
    }

    Item {
        id: passwordRow

        anchors.top: jidRow.bottom
        anchors.topMargin: appStyle.spacing.vertical
        anchors.left: parent.left
        anchors.right: parent.right
        height: passwordInput.height

        Text {
            id: passwordLabel

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            font.bold: true
            text: qsTr('Password')
            width: 100
        }


        InputBar {
            id: passwordInput

            anchors.top: parent.top
            anchors.left: passwordLabel.right
            anchors.leftMargin: appStyle.spacing.horizontal
            anchors.right: parent.right
            echoMode: TextInput.Password
            validator: RegExpValidator {
                regExp: /.+/
            }
        }
    }

    Text {
        id: statusLabel

        property string dupeText: qsTr("You already have an account for '%1'.").replace('%1', Utils.jidToDomain(jidInput.text));
        property string failedText: qsTr('Could not connect, please check your username and password.')
        property string testingText: qsTr('Checking your username and password..')

        anchors.top: passwordRow.bottom
        anchors.topMargin: appStyle.spacing.vertical
        anchors.left: parent.left
        anchors.right: parent.right
        wrapMode: Text.WordWrap
    }

    Row {
        anchors.top: statusLabel.bottom
        anchors.topMargin: appStyle.spacing.vertical
        anchors.horizontalCenter: parent.horizontalCenter
        height: 32
        spacing: appStyle.spacing.horizontal

        Button {
            id: addButton

            iconSource: 'add.png'
            text: qsTr('Add')

            onClicked: {
                if (!jidInput.acceptableInput || !passwordInput.acceptableInput)
                    return;

                var jid = jidInput.text;
                if (panel.domain != '')
                    jid += '@' + panel.domain;

                // check for duplicate account
                for (var i = 0; i < model.count; i++) {
                    if (Utils.jidToDomain(model.get(i).jid) == Utils.jidToDomain(jid)) {
                        panel.state = 'dupe';
                        return;
                    }
                }
                panel.state = 'testing';
                panel.testJid = jid;
                panel.testPassword = passwordInput.text;
                testClient.connectToServer(panel.testJid + '/AccountCheck', panel.testPassword);
            }
        }

        Button {
            iconSource: 'back.png'
            text: qsTr('Cancel')

            onClicked: {
                panel.state = '';
                jidInput.text = '';
                passwordInput.Text = '';
                testClient.disconnectFromServer();
                panel.close();
            }
        }
    }

    Client {
        id: testClient

        onConnected: {
            if (panel.state == 'testing') {
                panel.state = '';
                panel.accepted(panel.testJid, panel.testPassword);
            }
        }

        onDisconnected: {
            if (panel.state == 'testing') {
                panel.state = 'failed';
            }
        }
    }

    states: [
        State {
            name: 'dupe'
            PropertyChanges { target: statusLabel; color: 'red'; text: statusLabel.dupeText }
        },
        State {
            name: 'failed'
            PropertyChanges { target: statusLabel; color: 'red'; text: statusLabel.failedText }
        },
        State {
            name: 'testing'
            PropertyChanges { target: addButton; enabled: false }
            PropertyChanges { target: statusLabel; text: statusLabel.testingText }
            PropertyChanges { target: jidInput; enabled: false }
            PropertyChanges { target: passwordInput; enabled: false }
        }
    ]

    Component.onCompleted: {
        jidInput.focus = true;
    }

    Keys.onReturnPressed: {
        if (addButton.enabled) {
            addButton.clicked();
        }
    }

    Keys.onTabPressed: {
        if (jidInput.activeFocus)
            passwordInput.focus = true;
        else
            jidInput.focus = true;
    }
}

