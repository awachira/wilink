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

#ifndef __UPDATES_DIALOG_H__
#define __UPDATES_DIALOG_H__

#include <QDialog>

#include "updates.h"

class QAbstractButton;
class QDialogButtonBox;
class QLabel;
class QProgressBar;
class QUrl;

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    UpdateDialog(QWidget *parent = NULL);

public slots:
    void check();

private slots:
    void buttonClicked(QAbstractButton *button);
    void checkStarted();
    void checkFinished(const Release &release);
    void downloadProgress(qint64 done, qint64 total);
    void downloadFinished(const Release &release);
    void error(Updates::UpdatesError error, const QString &errorString);

private:
    QDialogButtonBox *buttonBox;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    Updates *updates;
    Release promptRelease;
};

#endif
