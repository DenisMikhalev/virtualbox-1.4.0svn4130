/* $Id: ParallelPortImpl.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * VirtualBox COM class implementation
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

#include "ParallelPortImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (ParallelPort)

HRESULT ParallelPort::FinalConstruct()
{
    return S_OK;
}

void ParallelPort::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the Parallel Port object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT ParallelPort::init (Machine *aParent, ULONG aSlot)
{
    LogFlowThisFunc (("aParent=%p, aSlot=%d\n", aParent, aSlot));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();

    /* initialize data */
    mData->mSlot = aSlot;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the Parallel Port object given another serial port object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT ParallelPort::init (Machine *aParent, ParallelPort *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    unconst (mPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
    mData.share (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT ParallelPort::initCopy (Machine *aParent, ParallelPort *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
    mData.attachCopy (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void ParallelPort::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 *  @note Locks this object for writing.
 */
bool ParallelPort::rollback()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoLock alock (this);

    bool changed = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = mData.hasActualChanges();
        mData.rollback();
    }

    return changed;
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void ParallelPort::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* lock both for writing since we modify both */
    AutoMultiLock <2> alock (this->wlock(), AutoLock::maybeWlock (mPeer));

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void ParallelPort::copyFrom (ParallelPort *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading */
    AutoMultiLock <2> alock (this->wlock(), aThat->rlock());

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}

HRESULT ParallelPort::loadSettings (CFGNODE aNode, ULONG aSlot)
{
    LogFlowThisFunc (("aMachine=%p\n", aNode));

    AssertReturn (aNode, E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    CFGNODE portNode = NULL;
    CFGLDRGetChildNode (aNode, "Port", aSlot, &portNode);

    /* slot number (required) */
    /* slot unicity is guaranteed by XML Schema */
    uint32_t uSlot = 0;
    CFGLDRQueryUInt32 (portNode, "slot", &uSlot);
    /* enabled (required) */
    bool fEnabled = false;
    CFGLDRQueryBool (portNode, "enabled", &fEnabled);
    /* I/O base (required) */
    uint32_t uIOBase;
    CFGLDRQueryUInt32 (portNode, "IOBase", &uIOBase);
    /* IRQ (required) */
    uint32_t uIRQ;
    CFGLDRQueryUInt32 (portNode, "IRQ", &uIRQ);
    /* device path (required) */
    Bstr DevicePath;
    CFGLDRQueryBSTR   (portNode, "DevicePath", DevicePath.asOutParam());

    mData->mEnabled = fEnabled;
    mData->mSlot    = uSlot;
    mData->mIOBase  = uIOBase;
    mData->mIRQ     = uIRQ;
    mData->mDevicePath = DevicePath;

    return S_OK;
}

HRESULT ParallelPort::saveSettings (CFGNODE aNode)
{
    AssertReturn (aNode, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    CFGNODE portNode = 0;
    int vrc = CFGLDRAppendChildNode (aNode, "Port", &portNode);
    ComAssertRCRet (vrc, E_FAIL);

    CFGLDRSetUInt32 (portNode, "slot",    mData->mSlot);
    CFGLDRSetBool   (portNode, "enabled", !!mData->mEnabled);
    CFGLDRSetUInt32 (portNode, "IOBase",  mData->mIOBase);
    CFGLDRSetUInt32 (portNode, "IRQ",     mData->mIRQ);
    CFGLDRSetBSTR   (portNode, "DevicePath", mData->mDevicePath);

    return S_OK;
}

// IParallelPort properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP ParallelPort::COMGETTER(Enabled) (BOOL *aEnabled)
{
    if (!aEnabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aEnabled = mData->mEnabled;

    return S_OK;
}

STDMETHODIMP ParallelPort::COMSETTER(Enabled) (BOOL aEnabled)
{
    LogFlowThisFunc (("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onParallelPortChange (this);
    }

    return S_OK;
}

STDMETHODIMP ParallelPort::COMGETTER(Slot) (ULONG *aSlot)
{
    if (!aSlot)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aSlot = mData->mSlot;

    return S_OK;
}

STDMETHODIMP ParallelPort::COMGETTER(IRQ) (ULONG *aIRQ)
{
    if (!aIRQ)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aIRQ = mData->mIRQ;

    return S_OK;
}

STDMETHODIMP ParallelPort::COMSETTER(IRQ)(ULONG aIRQ)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;
    bool emitChangeEvent = false;

    if (mData->mIRQ != aIRQ)
    {
        mData.backup();
        mData->mIRQ = aIRQ;
        emitChangeEvent = true;
    }

    if (emitChangeEvent)
    {
        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onParallelPortChange (this);
    }

    return rc;
}

STDMETHODIMP ParallelPort::COMGETTER(IOBase) (ULONG *aIOBase)
{
    if (!aIOBase)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aIOBase = mData->mIOBase;

    return S_OK;
}

STDMETHODIMP ParallelPort::COMSETTER(IOBase)(ULONG aIOBase)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;
    bool emitChangeEvent = false;

    if (mData->mIOBase != aIOBase)
    {
        mData.backup();
        mData->mIOBase = aIOBase;
        emitChangeEvent = true;
    }

    if (emitChangeEvent)
    {
        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onParallelPortChange (this);
    }

    return rc;
}

STDMETHODIMP ParallelPort::COMGETTER(DevicePath) (BSTR *aDevicePath)
{
    if (!aDevicePath)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mDevicePath.cloneTo (aDevicePath);

    return S_OK;
}

STDMETHODIMP ParallelPort::COMSETTER(DevicePath) (INPTR BSTR aDevicePath)
{
    if (!aDevicePath || *aDevicePath == 0)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mDevicePath != aDevicePath)
    {
        mData.backup();
        mData->mDevicePath = aDevicePath;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onParallelPortChange (this);
    }

    return S_OK;
}

