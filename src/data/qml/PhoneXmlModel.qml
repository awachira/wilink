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

XmlListModel {
    id: xmlModel

    property url url
    property string username
    property string password

    function reload() {
        console.log("PhoneXmlModel reload " + xmlModel.url);
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    xmlModel.xml = xhr.responseText;
                }
            }
        };
        xhr.open('GET', xmlModel.url, true, xmlModel.username, xmlModel.password);
        xhr.setRequestHeader('Accept', 'application/xml');
        xhr.send();
    }

    function addItem(props) {
        var url = xmlModel.url;
        var data = '';
        for (var name in props) {
            if (data.length)
                data += '&';
            data += name + '=' + encodeURIComponent(props[name]);
        }

        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            console.log("addItem readyState " + xhr.readyState);
            // FIXME: for some reason, we never get past this state
            if (xhr.readyState == 3) {
                xhr.abort();
                xmlModel.reload();
            }
        };
        xhr.open('POST', url, true, xmlModel.username, xmlModel.password);
        xhr.setRequestHeader("Accept", "application/xml");
        xhr.setRequestHeader("Connection", "close");
        xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
        xhr.send(data);
    }

    function removeItem(id) {
        var url = xmlModel.url + id + '/';
        var data = '_method=delete';

        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    console.log("PhoneXmlModel deleted item " + url);
                    xmlModel.reload();
                } else {
                    console.log("PhoneXmlModel failed to delete item " + url + ": " + xhr.status + "/" + xhr.statusText);
                }
            }
        }
        // FIXME: why does QML not like the "DELETE" method?
        xhr.open('POST', url, true, xmlModel.username, xmlModel.password);
        xhr.setRequestHeader('Accept', 'application/xml');
        xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
        xhr.send(data);
    }

    function updateItem(id, props) {
        var url = xmlModel.url + id + '/';
        var data = '';
        for (var name in props) {
            if (data.length)
                data += '&';
            data += name + '=' + encodeURIComponent(props[name]);
        }

        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    console.log("PhoneXmlModel updated item " + url);
                    xmlModel.reload();
                } else {
                    console.log("PhoneXmlModel failed to update item " + url + ": " + xhr.status + "/" + xhr.statusText);
                }
            }
        };
        xhr.open('POST', url, true, xmlModel.username, xmlModel.password);
        xhr.setRequestHeader("Accept", "application/xml");
        xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
        xhr.send(data);
    }
}
