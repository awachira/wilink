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

Rectangle {
    id: chatEdit

    property int chatState: QXmppMessage.None
    property alias text: input.text
    property alias model: listHelper.model
    signal returnPressed
    signal tabPressed

    function talkAt(participant) {
        var text = input.text;
        var newText = '';

        var pattern = /@([^,:]+[,:] )+/;
        var start = text.search(pattern);
        if (start >= 0) {
            var m = text.match(pattern)[0];
            var bits = m.split(/[,:] /);
            if (start > 0)
                newText += text.slice(0, start);
            for (var i in bits) {
                if (bits[i] == '@' + participant)
                    return;
                if (bits[i].length)
                    newText += bits[i] + ', ';
            }
        } else {
            newText = text;
        }
        newText += '@' + participant + ': ';
        input.text = newText;
        input.cursorPosition = newText.length;
    }

    color: '#dfdfdf'
    height: wrapper.height + 8

    Rectangle {
        id: wrapper

        border.color: '#c3c3c3'
        border.width: 1
        color: 'white'
        x: 4
        y: 4
        height: input.paintedHeight + 16
        width: parent.width - sendButton.width - 12

        ListHelper {
            id: listHelper
        }

        Timer {
            id: inactiveTimer
            interval: 120000
            running: true

            onTriggered: {
                if (chatEdit.chatState != QXmppMessage.Inactive) {
                    chatEdit.chatState = QXmppMessage.Inactive;
                }
            }
        }

        Timer {
            id: pausedTimer
            interval: 30000

            onTriggered: {
                if (chatEdit.chatState == QXmppMessage.Composing) {
                    chatEdit.chatState = QXmppMessage.Paused;
                }
            }
        }

        TextEdit {
            id: input

            focus: true
            x: 8
            y: 8
            selectByMouse: true
            smooth: true
            textFormat: TextEdit.PlainText
            width: parent.width - 16
            wrapMode: TextEdit.WordWrap

            onActiveFocusChanged: {
                if (chatEdit.chatState != QXmppMessage.Active &&
                    chatEdit.chatState != QXmppMessage.Composing &&
                    chatEdit.chatState != QXmppMessage.Paused) {
                    chatEdit.chatState = QXmppMessage.Active;
                }
                inactiveTimer.restart();
            }

            onTextChanged: {
                inactiveTimer.stop();
                if (text.length) {
                    if (chatEdit.chatState != QXmppMessage.Composing) {
                        chatEdit.chatState = QXmppMessage.Composing;
                    }
                    pausedTimer.restart()
                } else {
                    if (chatEdit.chatState != QXmppMessage.Active) {
                        chatEdit.chatState = QXmppMessage.Active;
                    }
                    pausedTimer.stop()
                }
            }

            Keys.onReturnPressed: {
                if (event.modifiers == Qt.NoModifier) {
                    chatEdit.returnPressed();
                } else {
                    event.accepted = false;
                }
            }

            Keys.onTabPressed: {
                // select word, including 'at' sign
                var text = input.text;
                var end = input.cursorPosition;
                var start = end;
                while (start >= 0 && text.charAt(start) != '@') {
                    start -= 1;
                }
                if (start < 0)
                    return;
                start += 1;

                // search matching participants
                var needle = input.text.slice(start, end).toLowerCase();
                var matches = [];
                for (var i = 0; i < listHelper.count; i += 1) {
                    var name = listHelper.get(i).name;
                    if (name.slice(0, needle.length).toLowerCase() == needle) {
                        matches[matches.length] = name;
                    }
                }
                if (matches.length == 1) {
                    var replacement = matches[0] + ': ';
                    input.text = text.slice(0, start) + replacement + text.slice(end);
                    input.cursorPosition = start + replacement.length;
                }
            }
        }

        MouseArea {
            anchors.fill: input
            acceptedButtons: Qt.RightButton
            onPressed: {
                var pos = mapToItem(chatEdit, mouse.x, mouse.y);
                menu.model.clear();
                menu.model.append({'action': 'cut', 'text': qsTr('Cut')});
                menu.model.append({'action': 'copy', 'text': qsTr('Copy')});
                menu.model.append({'action': 'paste', 'text': qsTr('Paste')});
                menu.show(pos.x, pos.y - menu.height);
            }
        }
    }

    Button {
        id: sendButton

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 4
        iconSource: 'upload.png'
        onClicked: chatEdit.returnPressed()
    }

    Menu {
        id: menu
        opacity: 0

        onItemClicked: {
            var item = menu.model.get(index);
            if (item.action == 'cut') {
                input.cut();
            } else if (item.action == 'copy') {
                input.copy();
            } else if (item.action == 'paste') {
                input.paste();
            }
        }
    }
}
