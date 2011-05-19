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

Column {
    anchors.fill: parent

    RosterView {
        id: rooms

        model: roomModel
        title: qsTr('My rooms')
        height: parent.height / 3
        width: parent.width

        Connections {
            onItemClicked: {
                var url = 'xmpp://' + window.objectName + '/' + model.jid + '?join';
                Qt.openUrlExternally(url);
            }
        }
    }

    RosterView {
        id: contacts

        model: contactModel
        title: qsTr('My contacts')
        height: 2 * parent.height / 3
        width: parent.width

        Menu {
            id: menu
            opacity: 0
        }

        Connections {
            onAddClicked: {
                var domain = window.objectName.split('@')[1];
                var tip = (domain == 'wifirst.net') ? '<p>' + qsTr('<b>Tip</b>: your wAmis are automatically added to your chat contacts, so the easiest way to add Wifirst contacts is to <a href=\"%1\">add them as wAmis</a>').replace('%1', 'https://www.wifirst.net/w/friends?from=wiLink') + '</p>' : '';

                var dialog = window.inputDialog();
                dialog.windowTitle = qsTr('Add a contact');
                dialog.labelText = tip + '<p>' + qsTr('Enter the address of the contact you want to add.') + '</p>';
                dialog.textValue = '@' + domain;

                var jid = '';
                while (!jid.match(/^[^@/]+@[^@/]+$/)) {
                    if (!dialog.exec())
                        return;
                    jid = dialog.textValue;
                }
                console.log("add " + jid);
                client.rosterManager.subscribe(jid);
            }

            onItemClicked: {
                var url = 'xmpp://' + window.objectName + '/' + model.jid + '?message';
                Qt.openUrlExternally(url);
            }

            onItemContextMenu: {
                menu.model.clear()
                if (model.url != undefined && model.url != '') {
                    menu.model.append({
                        'action': 'profile',
                        'icon': 'diagnostics.png',
                        'text': qsTr('Show profile'),
                        'url': model.url});
                }
                menu.model.append({
                    'action': 'rename',
                    'icon': 'options.png',
                    'name': model.name,
                    'text': qsTr('Rename contact'),
                    'jid': model.jid});
                menu.model.append({
                    'action': 'remove',
                    'icon': 'remove.png',
                    'name': model.name,
                    'text': qsTr('Remove contact'),
                    'jid': model.jid});
                menu.x = 16;
                menu.y = point.y - 16;
                menu.state = 'visible';
            }
        }

        Connections {
            target: client.rosterManager
            onSubscriptionReceived: {
                var box = window.messageBox();
                box.windowTitle = qsTr('Invitation from %1').replace('%1', bareJid);
                box.text = qsTr('%1 has asked to add you to his or her contact list.\n\nDo you accept?').replace('%1', bareJid);
                box.standardButtons = QMessageBox.Yes | QMessageBox.No;
                if (box.exec() == QMessageBox.Yes) {
                    // accept subscription
                    client.rosterManager.acceptSubscription(bareJid);

                    // request reciprocal subscription
                    client.rosterManager.subscribe(bareJid);
                } else {
                    // refuse subscription
                    client.rosterManager.refuseSubscription(bareJid);
                }
            }
        }

        Connections {
            target: menu
            onItemClicked: {
                var item = menu.model.get(index);
                if (item.action == 'profile') {
                    Qt.openUrlExternally(item.url);
                } else if (item.action == 'rename') {
                    var dialog = window.inputDialog();
                    dialog.windowTitle = qsTr('Rename contact');
                    dialog.labelText = qsTr("Enter the name for this contact.");
                    dialog.textValue = item.name;
                    if (dialog.exec()) {
                        console.log("rename " + item.jid + ": " + dialog.textValue);
                        client.rosterManager.renameItem(item.jid, dialog.textValue);
                    }
                } else if (item.action == 'remove') {
                    var box = window.messageBox();
                    box.windowTitle = qsTr("Remove contact");
                    box.text = qsTr('Do you want to remove %1 from your contact list?').replace('%1', item.name);
                    box.standardButtons = QMessageBox.Yes | QMessageBox.No;
                    if (box.exec() == QMessageBox.Yes) {
                        console.log("remove " + item.jid);
                        client.rosterManager.removeItem(item.jid);
                    }
                }
            }
        }
    }
}