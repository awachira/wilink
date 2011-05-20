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

Rectangle {
    width: 320
    height: 400

    VCard {
        id: ownCard
        jid: client.jid

        onNameChanged: {
            conversation.nickName = name;
            if (!conversation.isJoined) {
                // clear history
                historyModel.clear();

                // send join request
                conversation.join();
            }
        }
    }

    PanelHeader {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        icon: 'chat.png'
        title: '<b>' + conversation.jid.split('@')[0] + '</b>'
        z: 1
    }

    Item {
        anchors.top: header.bottom
        anchors.bottom: chatInput.top
        anchors.left: parent.left
        anchors.right: parent.right
    
        HistoryView {
            id: historyView

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: participantView.left
            model: historyModel

            onParticipantClicked: chatInput.talkAt(participant)
        }

        RoomParticipantView {
            id: participantView

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            model: participantModel
            width: 80

            onParticipantClicked: chatInput.talkAt(participant)
        }
    }

    ChatEdit {
        id: chatInput

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        model: participantModel

        onReturnPressed: {
            var text = chatInput.text;
            if (conversation.sendMessage(text))
                chatInput.text = '';
        }
    }
}
