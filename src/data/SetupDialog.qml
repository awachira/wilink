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
import wiLink 2.0
import 'utils.js' as Utils

Dialog {
    id: dialog

    focus: true
    opacity: 1
    title: qsTr('Add an account')
    footerComponent: PanelHelp {
        anchors.margins: 8
        text: qsTr('If you need help, please refer to the <a href="%1">%2 FAQ</a>.').replace('%1', 'https://www.wifirst.net/wilink/faq').replace('%2', application.applicationName)
    }

    AccountAddPanel {
        id: panel

        anchors.fill: dialog.contents
        domain: 'wifirst.net'
        focus: true
        model: AccountModel {}
        opacity: 1

        onAccepted: {
            model.append({'jid': jid, 'password': password});
            model.submit();
        }
        onClose: application.quit()
    }

    Connections {
        target: window

        onShowHelp: Qt.openUrlExternally('https://www.wifirst.net/wilink/faq')
    }

    Component.onCompleted: {
        panel.forceActiveFocus();
    }
}