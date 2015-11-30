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

import QtQuick 2.3
import wiLink 2.5

Item {
    id: chatEdit

    property int chatState: QXmppMessage.None
    property alias callButton: callButton
    property alias text: input.text
    property Component menuComponent
    property QtObject model
    signal returnPressed
    signal tabPressed
    signal callTriggered

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
                if (bits[i] === '@' + participant)
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

    height: wrapper.height + 2 * wrapper.anchors.margins

    // This mousearea prevents hover effects on items behind ChatEdit
    MouseArea {
        anchors.fill: parent;
        hoverEnabled: true;
    }

    Rectangle {
        id: wrapper

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: menuButton.left
        anchors.rightMargin: appStyle.margin.normal
        border.color: '#c3c3c3'
        border.width: 1
        color: 'white'
        height: input.paintedHeight + 2 * input.anchors.margins

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
            interval: 5000

            onTriggered: {
                if (chatEdit.chatState === QXmppMessage.Composing) {
                    chatEdit.chatState = QXmppMessage.Paused;
                }
            }
        }

        TextEdit {
            id: input

            anchors.margins: appStyle.margin.large
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            focus: appStyle.isMobile ? false : true
            font.pixelSize: appStyle.font.normalSize
            selectByMouse: true
            smooth: true
            textFormat: TextEdit.PlainText
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

            Keys.onEnterPressed: {
                chatEdit.returnPressed();
            }

            Keys.onReturnPressed: {
                if (event.modifiers === Qt.NoModifier) {
                    chatEdit.returnPressed();
                } else {
                    event.accepted = false;
                }
            }

            Keys.onTabPressed: {
                if (!Qt.isQtObject(chatEdit.model))
                    return;

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
                for (var i = 0; i < chatEdit.model.count; i += 1) {
                    var name = chatEdit.model.getProperty(i, 'name');
                    if (name.slice(0, needle.length).toLowerCase() === needle) {
                        matches[matches.length] = name;
                    }
                }
                if (matches.length === 1) {
                    var replacement = matches[0] + ': ';
                    input.text = text.slice(0, start) + replacement + text.slice(end);
                    input.cursorPosition = start + replacement.length;
                }
            }
        }

        MouseArea {
            function showMenu(mouse) {
                var pos = mapToItem(menuLoader.parent, mouse.x, mouse.y);
                var component = Qt.createComponent('InputMenu.qml');

                function finishCreation() {
                    if (component.status != Component.Ready)
                        return;

                    menuLoader.sourceComponent = component;
                    menuLoader.item.target = input;
                    menuLoader.show(pos.x, pos.y - menuLoader.item.height);
                }

                if (component.status === Component.Loading)
                    component.statusChanged.connect(finishCreation);
                else
                    finishCreation();
            }

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: {
                if (mouse.button === Qt.LeftButton) {
                    if (!input.activeFocus)
                        input.forceActiveFocus();
                    mouse.accepted = false;
                } else if (mouse.button === Qt.RightButton) {
                    showMenu(mouse);
                }
            }
        }
    }

    Button {
        id: menuButton

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: callButton.left
        anchors.rightMargin: appStyle.margin.normal
        iconStyle: 'icon-reorder'

        onClicked: {
            // show context menu
            var pos = mapToItem(menuLoader.parent, width, 0);
            menuLoader.sourceComponent = menuComponent;
            menuLoader.show(pos.x - menuLoader.item.width, pos.y - menuLoader.item.height);
        }
    }

    Button {
        id: callButton

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: sendButton.left
        anchors.rightMargin: visible ? appStyle.margin.normal : 0
        iconStyle: 'icon-phone'
        iconColor: 'white'
        iconFontSize: 20
        style: 'success'
        visible: false

        onClicked: chatEdit.callTriggered();
    }

    Button {
        id: sendButton

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        style: 'primary'
        visible: parent.width > 0
        text: qsTr('Send')
        onClicked: chatEdit.returnPressed()
    }
}
