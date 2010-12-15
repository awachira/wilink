/*
 * wiLink
 * Copyright (C) 2009-2010 Bolloré telecom
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

#ifndef __WILINK_CHAT_SOUND_H__
#define __WILINK_CHAT_SOUND_H__

#include <QAudioOutput>
#include <QIODevice>
#include <QMap>

class QFile;
class ChatSoundFile;

class ChatSoundPlayer : public QObject
{
    Q_OBJECT

public:
    ChatSoundPlayer(QObject *parent = 0);
    int play(const QString &name, bool repeat = false);
    void stop(int id);

private slots:
    void readerFinished();

private:
    int m_readerId;
    QMap<int, ChatSoundFile*> m_readers;
};

/** The ChatSoundFile class represents a WAV-format file.
 */
class ChatSoundFile : public QIODevice
{
    Q_OBJECT

public:
    ChatSoundFile(const QString &name, QObject *parent = 0);
    void close();
    QAudioFormat format() const;
    void setFormat(const QAudioFormat &format);
    bool open(QIODevice::OpenMode mode);
    bool repeat() const;
    void setRepeat(bool repeat);

signals:
    void finished();

protected:
    qint64 readData(char * data, qint64 maxSize);
    qint64 writeData(const char * data, qint64 maxSize);

private:
    bool readHeader();
    bool writeHeader();

    QFile *m_file;
    QAudioFormat m_format;
    qint64 m_beginPos;
    qint64 m_endPos;
    bool m_repeat;
    int m_repeatCount;
    int m_repeatLeft;

    friend class ChatSoundPlayer;
};

#endif
