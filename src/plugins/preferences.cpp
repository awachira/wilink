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

#include <QDialogButtonBox>
#include <QLayout>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>

#include "preferences.h"

class ChatPreferencesList : public QListWidget
{
public:
    QSize sizeHint() const;
};

QSize ChatPreferencesList::sizeHint() const
{
    QSize hint(150, minimumHeight());
    int rowCount = model()->rowCount();
    int rowHeight = rowCount * sizeHintForRow(0);
    if (rowHeight > hint.height())
        hint.setHeight(rowHeight);
    return hint;
}

class ChatPreferencesPrivate
{
public:
    QListWidget *tabList;
    QStackedWidget *tabStack;
};

/** Constructs a new preferences dialog.
 *
 * @param parent
 */
ChatPreferences::ChatPreferences(QWidget *parent)
    : QDialog(parent),
      d(new ChatPreferencesPrivate)
{
    QVBoxLayout *layout = new QVBoxLayout;

    // splitter
    QSplitter *splitter = new QSplitter;
    
    d->tabList = new ChatPreferencesList;
    d->tabList->setIconSize(QSize(32, 32));
    splitter->addWidget(d->tabList);
    splitter->setStretchFactor(0, 0);

    d->tabStack = new QStackedWidget;
    splitter->addWidget(d->tabStack);
    splitter->setStretchFactor(1, 1);

    connect(d->tabList, SIGNAL(currentRowChanged(int)), d->tabStack, SLOT(setCurrentIndex(int)));
    layout->addWidget(splitter);

    // buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(validate()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    layout->addWidget(buttonBox);

    setLayout(layout);
    setWindowTitle(tr("Preferences"));
}

/** Destroys a preferences dialog.
 */
ChatPreferences::~ChatPreferences()
{
    delete d;
}

void ChatPreferences::addTab(ChatPreferencesTab *tab)
{
    QListWidgetItem *item = new QListWidgetItem(tab->windowIcon(), tab->windowTitle());
    d->tabList->addItem(item);
    d->tabStack->addWidget(tab);
}

void ChatPreferences::setCurrentTab(const QString &tabName)
{
    for (int i = 0; i < d->tabStack->count(); ++i) {
        if (d->tabStack->widget(i)->objectName() == tabName) {
            d->tabList->setCurrentRow(i);
            break;
        }
    }
}

/** Validates and applies the new preferences.
 */
void ChatPreferences::validate()
{
    for (int i = 0; i < d->tabStack->count(); ++i) {
        ChatPreferencesTab *tab = qobject_cast<ChatPreferencesTab*>(d->tabStack->widget(i));
        tab->save();
    }
    accept();
}

ChatPreferencesTab::ChatPreferencesTab()
{
}
