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

#include <Foundation/NSAutoreleasePool.h>
#include <QTKit/QTCaptureDevice.h>
#include <QTKit/QTCaptureDeviceInput.h>
#include <QTKit/QTCaptureSession.h>
#include <QTKit/QTCaptureDecompressedVideoOutput.h>
#include <QTKit/QTFormatDescription.h>
#include <QTKit/QTMedia.h>

#include <QString>

#include "QVideoGrabber.h"
#include "QVideoGrabber_p.h"

class AutoReleasePool
{
public:
    AutoReleasePool()
    {
        pool = [[NSAutoreleasePool alloc] init];
    }

    ~AutoReleasePool()
    {
        [pool release];
    }

private:
    NSAutoreleasePool *pool;
};

@interface QVideoGrabberDelegate : NSObject {
@public
    QXmppVideoFrame currentFrame;
    QVideoGrabber *q;
}

-(void) captureOutput:(QTCaptureOutput *)captureOutput
                    didOutputVideoFrame:(CVImageBufferRef)videoFrame
                    withSampleBuffer:(QTSampleBuffer *)sampleBuffer
                    fromConnection:(QTCaptureConnection *)connection;

@end

@implementation QVideoGrabberDelegate

- (void) captureOutput:(QTCaptureOutput *)captureOutput
                    didOutputVideoFrame:(CVImageBufferRef)videoFrame
                    withSampleBuffer:(QTSampleBuffer *)sampleBuffer
                    fromConnection:(QTCaptureConnection *)connection
{
    Q_UNUSED(captureOutput);
    Q_UNUSED(sampleBuffer);
    Q_UNUSED(connection);
    if (CVPixelBufferLockBaseAddress(videoFrame, 0) == kCVReturnSuccess) {
        const int length = CVPixelBufferGetDataSize(videoFrame);
        memcpy(currentFrame.bits(), CVPixelBufferGetBaseAddress(videoFrame), length);
        CVPixelBufferUnlockBaseAddress(videoFrame, 0);
    }
    
    QMetaObject::invokeMethod(q, "frameAvailable", Q_ARG(QXmppVideoFrame, currentFrame));
}

@end

inline QString cfstringRefToQstring(CFStringRef cfStringRef) {
    QString retVal;
    CFIndex maxLength = 2 * CFStringGetLength(cfStringRef) + 1/*zero term*/; // max UTF8
    char *cstring = new char[maxLength];
    if (CFStringGetCString(CFStringRef(cfStringRef), cstring, maxLength, kCFStringEncodingUTF8)) {
        retVal = QString::fromUtf8(cstring);
    }
    delete cstring;
    return retVal;
}

inline CFStringRef qstringToCFStringRef(const QString &string)
{
    return CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar *>(string.unicode()),
                                        string.length());
}

inline NSString *qstringToNSString(const QString &qstr)
{
    return [reinterpret_cast<const NSString *>(qstringToCFStringRef(qstr)) autorelease];
}

inline QString nsstringToQString(const NSString *nsstr)
{
    return cfstringRefToQstring(reinterpret_cast<const CFStringRef>(nsstr));
}

class QVideoGrabberPrivate
{
public:
    QVideoGrabberPrivate(QVideoGrabber *qq);
    bool open();
    void close();

    QXmppVideoFormat videoFormat;
    QVideoGrabberDelegate *delegate;
    QTCaptureDevice *device;
    QTCaptureDeviceInput *deviceInput;
    QTCaptureDecompressedVideoOutput *deviceOutput;
    QTCaptureSession *session;

private:
    QVideoGrabber *q;
};

QVideoGrabberPrivate::QVideoGrabberPrivate(QVideoGrabber *qq)
    : delegate(0),
    device(0),
    deviceInput(0),
    deviceOutput(0),
    session(0),
    q(qq)
{
}

void QVideoGrabberPrivate::close()
{
    AutoReleasePool pool;

    if (session) {
        [session release];
        session = 0;
    }

    if (device) {
        if ([device isOpen])
            [device close];
        [device release];
        device = 0;
    }
}

bool QVideoGrabberPrivate::open()
{
    AutoReleasePool pool;

    // check format
    if (videoFormat.pixelFormat() != QXmppVideoFrame::Format_YUYV) {
        qWarning("QVideoGrabber: invalid pixel format requested");
        return false;
    }

    // create device
    device = [QTCaptureDevice defaultInputDeviceWithMediaType:QTMediaTypeVideo];
    if (!device)
        return false;

    // open device
    NSError *error;
    if (![device open:&error]) {
        NSLog(@"%@\n", error);
        close();
        return false;
    }

    session = [[QTCaptureSession alloc] init];

    // add capture input
    deviceInput = [[QTCaptureDeviceInput alloc] initWithDevice:device];
    if (![session addInput:deviceInput error:&error]) {
        NSLog(@"%@\n", error);
        close();
        return false;
    }

    // add capture output
    deviceOutput = [[QTCaptureDecompressedVideoOutput alloc] init];
    if (![session addOutput:deviceOutput error:&error]) {
        NSLog(@"%@\n", error);
        close();
        return false;
    }
    [deviceOutput setMinimumVideoFrameInterval:1.0/videoFormat.frameRate()];
    [deviceOutput setPixelBufferAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithUnsignedInt:kYUVSPixelFormat], (id)kCVPixelBufferPixelFormatTypeKey,
        [NSNumber numberWithUnsignedInt:videoFormat.frameWidth()], (id)kCVPixelBufferWidthKey,
        [NSNumber numberWithUnsignedInt:videoFormat.frameHeight()], (id)kCVPixelBufferHeightKey,
        nil]];
    [deviceOutput setDelegate:delegate];

    // store config
    NSDictionary* pixAttr = [deviceOutput pixelBufferAttributes];
    const int frameWidth = [[pixAttr valueForKey:(id)kCVPixelBufferWidthKey] floatValue];
    const int frameHeight = [[pixAttr valueForKey:(id)kCVPixelBufferHeightKey] floatValue];
    const int bytesPerLine = frameWidth * 2;
    videoFormat.setFrameSize(QSize(frameWidth, frameHeight));
    videoFormat.setPixelFormat(QXmppVideoFrame::Format_YUYV);
    delegate->currentFrame = QXmppVideoFrame(bytesPerLine * frameHeight, videoFormat.frameSize(), bytesPerLine, videoFormat.pixelFormat());
    return true;
}

QVideoGrabber::QVideoGrabber(const QXmppVideoFormat &format)
{
    AutoReleasePool pool;

    d = new QVideoGrabberPrivate(this);
    d->delegate = [[QVideoGrabberDelegate alloc] init];
    d->delegate->q = this;
    d->videoFormat = format;
}

QVideoGrabber::~QVideoGrabber()
{
    // stop acquisition
    stop();

    // close device
    d->close();

    delete d;
}

QXmppVideoFormat QVideoGrabber::format() const
{
    return d->videoFormat;
}

void QVideoGrabber::onFrameCaptured()
{
}

bool QVideoGrabber::start()
{
    AutoReleasePool pool;

    if (!d->session && !d->open())
        return false;

    [d->session startRunning];
    return true;
}

void QVideoGrabber::stop()
{
    AutoReleasePool pool;

    if (!d->session)
        return;

    [d->session stopRunning];
}

QList<QVideoGrabberInfo> QVideoGrabberInfo::availableGrabbers()
{
    AutoReleasePool pool;
    QList<QVideoGrabberInfo> grabbers;

    NSArray* devices = [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
    for (QTCaptureDevice *device in devices) {
        QVideoGrabberInfo grabber;
        grabber.d->deviceName = nsstringToQString([device uniqueID]);
        grabber.d->supportedPixelFormats << QXmppVideoFrame::Format_YUYV;

        //for (QTFormatDescription* fmtDesc in [device formatDescriptions])
        //    NSLog(@"Format Description - %@", [fmtDesc formatDescriptionAttributes]);

        grabbers << grabber;
    }

    return grabbers;
}

