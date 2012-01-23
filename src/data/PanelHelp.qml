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

import QtQuick 1.1

Rectangle {
    property alias text: label.text
    property int iconSize: appStyle.icon.tinySize

    border.color: '#93b9f2'
    border.width: 1
    gradient: Gradient {
        GradientStop { position: 0; color: '#e7effd' }
        GradientStop { position: 1; color: '#cbdaf1' }
    }
    height: visible ? label.height + 2 * label.anchors.margins : 0
    visible: label.text != ''

    Image {
        id: icon
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        height: iconSize
        width: iconSize
        source: 'tip.png'
    }

    Label {
        id: label

        anchors.left: icon.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        wrapMode: Text.WordWrap

        onLinkActivated: Qt.openUrlExternally(link)
    }
}
