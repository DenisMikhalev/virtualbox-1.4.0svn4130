/** @file
 * innotek Portable Runtime - Filesystem.
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

#ifndef ___iprt_fs_h
#define ___iprt_fs_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/time.h>


__BEGIN_DECLS

/** @defgroup grp_rt_fs    RTFs - Filesystem and Volume
 * @ingroup grp_rt
 * @{
 */


/** @name Filesystem Object Mode Flags.
 *
 * There are two sets of flags: the unix mode flags and the dos
 * attributes.
 *
 * APIs returning mode flags will provide both sets.
 *
 * When specifying mode flags to any API at least one of
 * them must be given. If one set is missing the API will
 * synthesize it from the one given if it requires it.
 *
 * Both sets match their x86 ABIs, the DOS/NT one is simply shifted
 * up 16 bits. The DOS/NT range is bits 16 to 31 inclusivly. The
 * Unix range is bits 0 to 15 (inclusivly).
 *
 * @{
 */

/** Set user id on execution (S_ISUID). */
#define RTFS_UNIX_ISUID             0004000U
/** Set group id on execution (S_ISGID). */
#define RTFS_UNIX_ISGID             0002000U
/** Sticky bit (S_ISVTX / S_ISTXT). */
#define RTFS_UNIX_ISTXT             0001000U

/** Owner RWX mask (S_IRWXU). */
#define RTFS_UNIX_IRWXU             0000700U
/** Owner readable (S_IRUSR). */
#define RTFS_UNIX_IRUSR             0000400U
/** Owner writable (S_IWUSR). */
#define RTFS_UNIX_IWUSR             0000200U
/** Owner executable (S_IXUSR). */
#define RTFS_UNIX_IXUSR             0000100U

/** Group RWX mask (S_IRWXG). */
#define RTFS_UNIX_IRWXG             0000070U
/** Group readable (S_IRGRP). */
#define RTFS_UNIX_IRGRP             0000040U
/** Group writable (S_IWGRP). */
#define RTFS_UNIX_IWGRP             0000020U
/** Group executable (S_IXGRP). */
#define RTFS_UNIX_IXGRP             0000010U

/** Other RWX mask (S_IRWXO). */
#define RTFS_UNIX_IRWXO             0000007U
/** Other readable (S_IROTH). */
#define RTFS_UNIX_IROTH             0000004U
/** Other writable (S_IWOTH). */
#define RTFS_UNIX_IWOTH             0000002U
/** Other executable (S_IXOTH). */
#define RTFS_UNIX_IXOTH             0000001U

/** Named pipe (fifo) (S_IFIFO). */
#define RTFS_TYPE_FIFO              0010000U
/** Character device (S_IFCHR). */
#define RTFS_TYPE_DEV_CHAR          0020000U
/** Directory (S_IFDIR). */
#define RTFS_TYPE_DIRECTORY         0040000U
/** Block device (S_IFBLK). */
#define RTFS_TYPE_DEV_BLOCK         0060000U
/** Regular file (S_IFREG). */
#define RTFS_TYPE_FILE              0100000U
/** Symbolic link (S_IFLNK). */
#define RTFS_TYPE_SYMLINK           0120000U
/** Socket (S_IFSOCK). */
#define RTFS_TYPE_SOCKET            0140000U
/** Whiteout (S_IFWHT). */
#define RTFS_TYPE_WHITEOUT          0160000U
/** Type mask (S_IFMT). */
#define RTFS_TYPE_MASK              0170000U

/** Unix attribute mask. */
#define RTFS_UNIX_MASK              0xffffU
/** The mask of all the NT, OS/2 and DOS attributes. */
#define RTFS_DOS_MASK               (0x7fffU << RTFS_DOS_SHIFT)

/** The shift value. */
#define RTFS_DOS_SHIFT              16
/** The mask of the OS/2 and DOS attributes. */
#define RTFS_DOS_MASK_OS2           (0x003fU << RTFS_DOS_SHIFT)
/** The mask of the NT attributes. */
#define RTFS_DOS_MASK_NT            (0x7fffU << RTFS_DOS_SHIFT)

/** Readonly object. */
#define RTFS_DOS_READONLY           (0x0001U << RTFS_DOS_SHIFT)
/** Hidden object. */
#define RTFS_DOS_HIDDEN             (0x0002U << RTFS_DOS_SHIFT)
/** System object. */
#define RTFS_DOS_SYSTEM             (0x0004U << RTFS_DOS_SHIFT)
/** Directory. */
#define RTFS_DOS_DIRECTORY          (0x0010U << RTFS_DOS_SHIFT)
/** Archived object.
 * This bit is set by the filesystem after each modification of a file. */
#define RTFS_DOS_ARCHIVED           (0x0020U << RTFS_DOS_SHIFT)
/** Undocumented / Reserved, used to be the FAT volume label. */
#define RTFS_DOS_NT_DEVICE          (0x0040U << RTFS_DOS_SHIFT)
/** Normal object, no other attribute set (NT). */
#define RTFS_DOS_NT_NORMAL          (0x0080U << RTFS_DOS_SHIFT)
/** Temporary object (NT). */
#define RTFS_DOS_NT_TEMPORARY       (0x0100U << RTFS_DOS_SHIFT)
/** Sparse file (NT). */
#define RTFS_DOS_NT_SPARSE_FILE     (0x0200U << RTFS_DOS_SHIFT)
/** Reparse point (NT). */
#define RTFS_DOS_NT_REPARSE_POINT   (0x0400U << RTFS_DOS_SHIFT)
/** Compressed object (NT).
 * For a directory, compression is the default for new files. */
#define RTFS_DOS_NT_COMPRESSED      (0x0800U << RTFS_DOS_SHIFT)
/** Physically offline data (NT).
 * MSDN say, don't mess with this one. */
#define RTFS_DOS_NT_OFFLINE         (0x1000U << RTFS_DOS_SHIFT)
/** Not content indexed by the content indexing service (NT). */
#define RTFS_DOS_NT_NOT_CONTENT_INDEXED (0x2000U << RTFS_DOS_SHIFT)
/** Encryped object (NT).
 * For a directory, encrypted is the default for new files. */
#define RTFS_DOS_NT_ENCRYPTED       (0x4000U << RTFS_DOS_SHIFT)

/** @} */


/** @name Filesystem Object Type Predicates.
 * @{ */
/** Checks the mode flags indicate a named pipe (fifo) (S_ISFIFO). */
#define RTFS_IS_FIFO(fMode)         ( ((fMode) & RTFS_TYPE_MASK) == RTFS_TYPE_FIFO )
/** Checks the mode flags indicate a character device (S_ISCHR). */
#define RTFS_IS_DEV_CHAR(fMode)     ( ((fMode) & RTFS_TYPE_MASK) == RTFS_TYPE_DEV_CHAR )
/** Checks the mode flags indicate a directory (S_ISDIR). */
#define RTFS_IS_DIRECTORY(fMode)    ( ((fMode) & RTFS_TYPE_MASK) == RTFS_TYPE_DIRECTORY )
/** Checks the mode flags indicate a block device (S_ISBLK). */
#define RTFS_IS_DEV_BLOCK(fMode)    ( ((fMode) & RTFS_TYPE_MASK) == RTFS_TYPE_DEV_BLOCK )
/** Checks the mode flags indicate a regular file (S_ISREG). */
#define RTFS_IS_FILE(fMode)         ( ((fMode) & RTFS_TYPE_MASK) == RTFS_TYPE_FILE )
/** Checks the mode flags indicate a symbolic link (S_ISLNK). */
#define RTFS_IS_SYMLINK(fMode)      ( ((fMode) & RTFS_TYPE_MASK) == RTFS_TYPE_SYMLINK )
/** Checks the mode flags indicate a socket (S_ISSOCK). */
#define RTFS_IS_SOCKET(fMode)       ( ((fMode) & RTFS_TYPE_MASK) == RTFS_TYPE_SOCKET )
/** Checks the mode flags indicate a whiteout (S_ISWHT). */
#define RTFS_IS_WHITEOUT(fMode)     ( ((fMode) & RTFS_TYPE_MASK) == RTFS_TYPE_WHITEOUT )
/** @} */


/**
 * The available additional information in a RTFSOBJATTR object.
 */
typedef enum RTFSOBJATTRADD
{
    /** No additional information is available / requested. */
    RTFSOBJATTRADD_NOTHING = 1,
    /** The additional unix attributes (RTFSOBJATTR::u::Unix) are available / requested. */
    RTFSOBJATTRADD_UNIX,
    /** The additional extended attribute size (RTFSOBJATTR::u::EASize) is available / requested. */
    RTFSOBJATTRADD_EASIZE,
    /** The last valid item (inclusive).
     * The valid range is RTFSOBJATTRADD_NOTHING thru RTFSOBJATTRADD_LAST.  */
    RTFSOBJATTRADD_LAST = RTFSOBJATTRADD_EASIZE,

    /** The usual 32-bit hack. */
    RTFSOBJATTRADD_32BIT_SIZE_HACK = 0x7fffffff
} RTFSOBJATTRADD;


/**
 * Filesystem object attributes.
 */
#pragma pack(1)
typedef struct RTFSOBJATTR
{
    /** Mode flags (st_mode). RTFS_UNIX_*, RTFS_TYPE_*, and RTFS_DOS_*. */
    RTFMODE         fMode;

    /** The additional attributes available. */
    RTFSOBJATTRADD  enmAdditional;

    /**
     * Additional attributes.
     *
     * Unless explicitly specified to an API, the API can provide additional
     * data as it is provided by the underlying OS.
     */
    union RTFSOBJATTRUNION
    {
        /** Additional Unix Attributes
         * These are available when RTFSOBJATTRADD is set in fUnix.
         */
         struct RTFSOBJATTRUNIX
         {
            /** The user owning the filesystem object (st_uid).
             * This field is ~0U if not supported. */
            RTUID           uid;

            /** The group the filesystem object is assigned (st_gid).
             * This field is ~0U if not supported. */
            RTGID           gid;

            /** Number of hard links to this filesystem object (st_nlink).
             * This field is 1 if the filesystem doesn't support hardlinking or
             * the information isn't available.
             */
            uint32_t        cHardlinks;

            /** The device number of the device which this filesystem object resides on (st_dev).
             * This field is 0 if this information is not available. */
            RTDEV           INodeIdDevice;

            /** The unique identifier (within the filesystem) of this filesystem object (st_ino).
             * Together with INodeIdDevice, this field can be used as a OS wide unique id
             * when both their values are not 0.
             * This field is 0 if the information is not available. */
            RTINODE         INodeId;

            /** User flags (st_flags).
             * This field is 0 if this information is not available. */
            uint32_t        fFlags;

            /** The current generation number (st_gen).
             * This field is 0 if this information is not available. */
            uint32_t        GenerationId;

            /** The device number of a character or block device type object (st_rdev).
             * This field is 0 if the file isn't of a character or block device type and
             * when the OS doesn't subscribe to the major+minor device idenfication scheme. */
            RTDEV           Device;
        } Unix;

        /**
         * Extended attribute size is available when RTFS_DOS_HAVE_EA_SIZE is set.
         */
        struct RTFSOBJATTREASIZE
        {
            /** Size of EAs. */
            RTFOFF          cb;
        } EASize;
    } u;
} RTFSOBJATTR;
#pragma pack()
/** Pointer to a filesystem object attributes structure. */
typedef RTFSOBJATTR *PRTFSOBJATTR;
/** Pointer to a const filesystem object attributes structure. */
typedef const RTFSOBJATTR *PCRTFSOBJATTR;


/**
 * Filesystem object information structure.
 *
 * This is returned by the RTPathQueryInfo(), RTFileQueryInfo() and RTDirRead() APIs.
 */
#pragma pack(1)
typedef struct RTFSOBJINFO
{
   /** Logical size (st_size).
    * For normal files this is the size of the file.
    * For symbolic links, this is the length of the path name contained
    * in the symbolic link.
    * For other objects this fields needs to be specified.
    */
   RTFOFF       cbObject;

   /** Disk allocation size (st_blocks * DEV_BSIZE). */
   RTFOFF       cbAllocated;

   /** Time of last access (st_atime). */
   RTTIMESPEC   AccessTime;

   /** Time of last data modification (st_mtime). */
   RTTIMESPEC   ModificationTime;

   /** Time of last status change (st_ctime).
    * If not available this is set to ModificationTime.
    */
   RTTIMESPEC   ChangeTime;

   /** Time of file birth (st_birthtime).
    * If not available this is set to ChangeTime.
    */
   RTTIMESPEC   BirthTime;

   /** Attributes. */
   RTFSOBJATTR  Attr;

} RTFSOBJINFO;
#pragma pack()
/** Pointer to a filesystem object information structure. */
typedef RTFSOBJINFO *PRTFSOBJINFO;
/** Pointer to a const filesystem object information structure. */
typedef const RTFSOBJINFO *PCRTFSOBJINFO;


#ifdef IN_RING3

/**
 * Query the sizes of a filesystem.
 *
 * @returns iprt status code.
 * @param   pszFsPath       Path within the mounted filesystem.
 * @param   pcbTotal        Where to store the total filesystem space. (Optional)
 * @param   pcbFree         Where to store the remaining free space in the filesystem. (Optional)
 * @param   pcbBlock        Where to store the block size. (Optional)
 * @param   pcbSector       Where to store the sector size. (Optional)
 */
RTR3DECL(int) RTFsQuerySizes(const char *pszFsPath, PRTFOFF pcbTotal, RTFOFF *pcbFree,
                             uint32_t *pcbBlock, uint32_t *pcbSector);

/**
 * Query the mountpoint of a filesystem.
 *
 * @returns iprt status code.
 * @returns VERR_BUFFER_OVERFLOW if cbMountpoint isn't enough.
 * @param   pszFsPath       Path within the mounted filesystem.
 * @param   pszMountpoint   Where to store the mountpoint path.
 * @param   cbMountpoint    Size of the buffer pointed to by pszMountpoint.
 */
RTR3DECL(int) RTFsQueryMountpoint(const char *pszFsPath, char *pszMountpoint, size_t cbMountpoint);

/**
 * Query the label of a filesystem.
 *
 * @returns iprt status code.
 * @returns VERR_BUFFER_OVERFLOW if cbLabel isn't enough.
 * @param   pszFsPath       Path within the mounted filesystem.
 * @param   pszLabel        Where to store the label.
 * @param   cbLabel         Size of the buffer pointed to by pszLabel.
 */
RTR3DECL(int) RTFsQueryLabel(const char *pszFsPath, char *pszLabel, size_t cbLabel);

/**
 * Query the serial number of a filesystem.
 *
 * @returns iprt status code.
 * @param   pszFsPath       Path within the mounted filesystem.
 * @param   pu32Serial      Where to store the serial number.
 */
RTR3DECL(int) RTFsQuerySerial(const char *pszFsPath, uint32_t *pu32Serial);

/**
 * Query the name of the filesystem driver.
 *
 * @returns iprt status code.
 * @returns VERR_BUFFER_OVERFLOW if cbFsDriver isn't enough.
 * @param   pszFsPath       Path within the mounted filesystem.
 * @param   pszFsDriver     Where to store the filesystem driver name.
 * @param   cbFsDriver      Size of the buffer pointed to by pszFsDriver.
 */
RTR3DECL(int) RTFsQueryDriver(const char *pszFsPath, char *pszFsDriver, size_t cbFsDriver);

#endif /* IN_RING3 */

/**
 * Filesystem properties.
 */
typedef struct RTFSPROPERTIES
{
    /** The maximum size of a filesystem object name.
     * This does not include the '\\0'. */
    uint32_t cbMaxComponent;

    /** True if the filesystem is remote.
     * False if the filesystem is local. */
    bool    fRemote;

    /** True if the filesystem is case sensitive.
     * False if the filesystem is case insensitive. */
    bool    fCaseSensitive;

    /** True if the filesystem is mounted read only.
     * False if the filesystem is mounted read write. */
    bool    fReadOnly;

    /** True if the filesystem can encode unicode object names.
     * False if it can't. */
    bool    fSupportsUnicode;

    /** True if the filesystem is compresses.
     * False if it isn't or we don't know. */
    bool    fCompressed;

    /** True if the filesystem compresses of individual files.
     * False if it doesn't or we don't know. */
    bool    fFileCompression;

    /** @todo more? */
} RTFSPROPERTIES;
/** Pointer to a filesystem properties structure. */
typedef RTFSPROPERTIES *PRTFSPROPERTIES;

#ifdef IN_RING3

/**
 * Query the properties of a mounted filesystem.
 *
 * @returns iprt status code.
 * @param   pszFsPath       Path within the mounted filesystem.
 * @param   pProperties     Where to store the properties.
 */
RTR3DECL(int) RTFsQueryProperties(const char *pszFsPath, PRTFSPROPERTIES pProperties);


/**
 * Mountpoint enumerator callback.
 *
 * @returns iprt status code. Failure terminates the enumeration.
 * @param   pszMountpoint   The mountpoint name.
 * @param   pvUser          The user argument.
 */
typedef DECLCALLBACK(int) FNRTFSMOUNTPOINTENUM(const char *pszMountpoint, void *pvUser);
/** Pointer to a FNRTFSMOUNTPOINTENUM(). */
typedef FNRTFSMOUNTPOINTENUM *PFNRTFSMOUNTPOINTENUM;

/**
 * Enumerate mount points.
 *
 * @returns iprt status code.
 * @param   pfnCallback     The callback function.
 * @param   pvUser          The user argument to the callback.
 */
RTR3DECL(int) RTFsMountpointsEnum(PFNRTFSMOUNTPOINTENUM pfnCallback, void *pvUser);


#endif /* IN_RING3 */

/** @} */

__END_DECLS

#endif /* ___iprt_fs_h */

