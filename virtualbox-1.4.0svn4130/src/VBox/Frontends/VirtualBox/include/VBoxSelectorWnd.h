/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSelectorWnd class declaration
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

#ifndef __VBoxSelectorWnd_h__
#define __VBoxSelectorWnd_h__

#include "COMDefs.h"

#include "VBoxGlobal.h"

#include <qapplication.h>
#include <qmainwindow.h>
#include <qgroupbox.h>
#include <qaction.h>

#include <qvaluelist.h>

class VBoxVMListBox;
class VBoxSnapshotsWgt;
class VBoxVMDetailsView;
class VBoxVMDescriptionPage;
class VBoxVMLogViewer;

class QLabel;
class QTextBrowser;
class QTabWidget;
struct QUuid;

class VBoxSelectorWnd : public QMainWindow
{
    Q_OBJECT

public:

    VBoxSelectorWnd (VBoxSelectorWnd **aSelf,
                     QWidget* aParent = 0, const char* aName = 0,
                     WFlags aFlags = WType_TopLevel);
    virtual ~VBoxSelectorWnd();

    bool startMachine (const QUuid &id);

public slots:

    void fileDiskMgr();
    void fileSettings();
    void fileExit();

    void vmNew();
    void vmSettings (const QString &aCategory = QString::null,
                     const QString &aControl = QString::null);
    void vmDelete();
    void vmStart();
    void vmDiscard();
    void vmRefresh();
    void vmShowLogs();

    void refreshVMList();
    void refreshVMItem (const QUuid &aID, bool aDetails,
                                          bool aSnapshots,
                                          bool aDescription);

    void showHelpContents();

protected:

    /* events */
    bool event (QEvent *e);

protected slots:

private:

    void languageChange();

private slots:

    void vmListBoxCurrentChanged (bool aRefreshDetails = true,
                                  bool aRefreshSnapshots = true,
                                  bool aRefreshDescription = true);

    void mediaEnumStarted();
    void mediaEnumFinished (const VBoxMediaList &);

    /* VirtualBox callback events we're interested in */

    void machineStateChanged (const VBoxMachineStateChangeEvent &e);
    void machineDataChanged (const VBoxMachineDataChangeEvent &e);
    void machineRegistered (const VBoxMachineRegisteredEvent &e);
    void sessionStateChanged (const VBoxSessionStateChangeEvent &e);
    void snapshotChanged (const VBoxSnapshotEvent &e);

private:

    /* actions */
    QAction *fileDiskMgrAction;
    QAction *fileSettingsAction;
    QAction *fileExitAction;
    QAction *vmNewAction;
    QAction *vmConfigAction;
    QAction *vmDeleteAction;
    QAction *vmStartAction;
    QAction *vmDiscardAction;
    QAction *vmRefreshAction;
    QAction *vmShowLogsAction;
    QAction *helpContentsAction;
    QAction *helpWebAction;
    QAction *helpAboutAction;
    QAction *helpResetMessagesAction;

    /* widgets */
    VBoxVMListBox *vmListBox;
    QTabWidget *vmTabWidget;
    VBoxVMDetailsView *vmDetailsView;
    VBoxSnapshotsWgt *vmSnapshotsWgt;
    VBoxVMDescriptionPage *vmDescriptionPage;

    QValueList <CSession> sessions;

    QPoint normal_pos;
    QSize normal_size;

    bool doneInaccessibleWarningOnce : 1;
};

#endif // __VBoxSelectorWnd_h__
