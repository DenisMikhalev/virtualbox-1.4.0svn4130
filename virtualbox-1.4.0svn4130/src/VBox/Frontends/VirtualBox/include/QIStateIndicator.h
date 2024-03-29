/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIStateIndicator class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QIStateIndicator_h__
#define __QIStateIndicator_h__

#include <qframe.h>
#include <qpixmap.h>

#include <qintdict.h>

class QIStateIndicator : public QFrame
{
    Q_OBJECT

public:

    QIStateIndicator (int aState,
                      QWidget *aParent, const char *aName = 0,
                      WFlags aFlags = 0);

    virtual QSize sizeHint() const;

    int state () const { return mState; }

    QPixmap stateIcon (int aState) const;
    void setStateIcon (int aState, const QPixmap &aPixmap);

public slots:

    void setState (int aState);
    void setState (bool aState) { setState ((int) aState); }

signals:

    void mouseDoubleClicked (QIStateIndicator *aIndicator,
                             QMouseEvent *aEv);
    void contextMenuRequested (QIStateIndicator *aIndicator,
                               QContextMenuEvent *aEv);

protected:

    virtual void drawContents (QPainter *aPainter);

#ifdef Q_WS_MAC
    virtual void mousePressEvent (QMouseEvent *aEv);
#endif 
    virtual void mouseDoubleClickEvent (QMouseEvent *aEv);
    virtual void contextMenuEvent (QContextMenuEvent *aEv);

private:

    int mState;
    QSize mSize;

    struct Icon
    {
        Icon (const QPixmap &aPixmap)
            : pixmap (aPixmap)
            , bgPixmap (NULL) {}

        QPixmap pixmap;
        QPixmap cached;
        QColor bgColor;
        const QPixmap *bgPixmap;
        QPoint bgOff;
    };

    QIntDict <Icon> mStateIcons;
};

#endif // __QIStateIndicator_h__
