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

#include "QXmppRtpChannel.h"

#define YCBCR_to_RGB(yp, cb, cr) ((quint8(yp + 1.371 * cr) << 16) | \
                                  (quint8(yp - 0.698 * cr - 0.336 * cb) << 8) | \
                                   quint8(yp + 1.732 * cb))
 
class QVideoGrabberPrivate;
class QVideoGrabberInfoPrivate;

class QVideoGrabber
{
public:
    enum State {
        ActiveState = 0,
        StoppedState = 2,
    };

    QVideoGrabber();
    ~QVideoGrabber();

    QXmppVideoFrame currentFrame();
    bool start();
    State state() const;
    void stop();
    QList<QXmppVideoFrame::PixelFormat> supportedFormats() const;

private:
    QVideoGrabberPrivate *d;
    bool open();
};

class QVideoGrabberInfo
{
public:
    QVideoGrabberInfo();
    QVideoGrabberInfo(const QVideoGrabberInfo &other);
    ~QVideoGrabberInfo();

    QString deviceName() const;
    QList<QXmppVideoFrame::PixelFormat> supportedPixelFormats() const;
    QVideoGrabberInfo &operator=(const QVideoGrabberInfo &other);
    static QList<QVideoGrabberInfo> availableGrabbers();

private:
    QVideoGrabberInfoPrivate *d;
};

