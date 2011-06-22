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

#ifndef __WILINK_DECLARATIVE_H__
#define __WILINK_DECLARATIVE_H__

#include <QAbstractItemModel>
#include <QDeclarativeNetworkAccessManagerFactory>
#include <QDeclarativeExtensionPlugin>
#include <QFileDialog>
#include <QNetworkAccessManager>
#include <QSortFilterProxyModel>

#include "QXmppMessage.h"
#include "QXmppPresence.h"

class QDeclarativeFileDialog : public QFileDialog
{
    Q_OBJECT
    Q_PROPERTY(QStringList selectedFiles READ selectedFiles)

public:
    QDeclarativeFileDialog(QWidget *parent = 0) : QFileDialog(parent) {}
    Q_INVOKABLE void setDirectory(const QString &directory);
};

class QDeclarativeSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged)

public:
    QDeclarativeSortFilterProxyModel(QObject *parent = 0);
    void setSourceModel(QAbstractItemModel *model);

public slots:
    void sort(int column);

signals:
    void sourceModelChanged(QAbstractItemModel *sourceModel);
};

class QXmppDeclarativeMessage : public QObject
{
    Q_OBJECT
    Q_ENUMS(State)

public:
    enum State {
        None = QXmppMessage::None,
        Active = QXmppMessage::Active,
        Inactive = QXmppMessage::Inactive,
        Gone = QXmppMessage::Gone,
        Composing = QXmppMessage::Composing,
        Paused = QXmppMessage::Paused,
    };
};

class QXmppDeclarativePresence : public QObject
{
    Q_OBJECT
    Q_ENUMS(Status)

public:
    enum Status {
        Offline = QXmppPresence::Status::Offline,
        Online = QXmppPresence::Status::Online,
        Away = QXmppPresence::Status::Away,
        XA = QXmppPresence::Status::XA,
        DND = QXmppPresence::Status::DND,
        Chat = QXmppPresence::Status::Chat,
        Invisible = QXmppPresence::Status::Invisible,
    };
};

class ListHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)

public:
    ListHelper(QObject *parent = 0);

    int count() const;
    Q_INVOKABLE QVariant get(int row) const;
    Q_INVOKABLE QVariant getProperty(int row, const QString &name) const;

    QAbstractItemModel *model() const;
    void setModel(QAbstractItemModel *model);

signals:
    void countChanged();
    void modelChanged(QAbstractItemModel *model);

private:
    QAbstractItemModel *m_model;
};

class NetworkAccessManager : public QNetworkAccessManager
{
public:
    NetworkAccessManager(QObject *parent = 0);

protected:
    virtual QNetworkReply *createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData = 0);
};

class NetworkAccessManagerFactory : public QDeclarativeNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject * parent)
    {
        return new NetworkAccessManager(parent);
    }
};

class DeclarativeWallet : public QObject
{
    Q_OBJECT

public:
    DeclarativeWallet(QObject *parent = 0);
    static QString realm(const QString &jid);

public slots:
    QString get(const QString &jid) const;
    void remove(const QString &jid);
    void set(const QString &jid, const QString &password);
};

class Plugin : public QDeclarativeExtensionPlugin
{
    Q_OBJECT

public:
     void registerTypes(const char *uri);
};

#endif
