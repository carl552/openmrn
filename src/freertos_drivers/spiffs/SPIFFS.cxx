/** @copyright
 * Copyright (c) 2018, Stuart W Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file SPIFFS.cxx
 * This file implements the generic SPIFFS driver.
 *
 * @author Stuart W. Baker
 * @date 1 January 2018
 */

#include "SPIFFS.hxx"

#include <fcntl.h>

#include "spiffs_nucleus.h"

#ifndef _FDIRECT
#define _FDIRECT 0x80000
#endif

#ifndef O_DIRECT
#define O_DIRECT _FDIRECT
#endif

extern "C"
{
/// Porovide mutex lock.
/// @param fs reference to the file system instance
void extern_spiffs_lock(struct spiffs_t *fs)
{
    SPIFFS::extern_lock(fs);
}

/// Porovide mutex unlock.
/// @param fs reference to the file system instance
void extern_spiffs_unlock(struct spiffs_t *fs)
{
    SPIFFS::extern_unlock(fs);
}
} // extern "C"

//
// SPIFFS::SPIFFS()
//
SPIFFS::SPIFFS(size_t physical_address, size_t size_on_disk,
               size_t erase_block_size, size_t logical_block_size,
               size_t logical_page_size, size_t max_num_open_descriptors,
               size_t cache_pages,
               std::function<void()> post_format_hook)
    : FileSystem()
    , config_({.hal_read_f       = flash_read,
               .hal_write_f      = flash_write,
               .hal_erase_f      = flash_erase,
               .phys_size        = size_on_disk,
               .phys_addr        = physical_address,
               .phys_erase_block = erase_block_size,
               .log_block_size   = logical_block_size,
               .log_page_size    = logical_page_size})
    , postFormatHook_(post_format_hook)
    , lock_()
    , workBuffer_(new uint8_t[logical_page_size * 2])
    , fdSpaceSize_(max_num_open_descriptors * sizeof(spiffs_fd))
    , fdSpace_(new uint8_t[fdSpaceSize_])
    , cacheSize_(sizeof(spiffs_cache) +
                 cache_pages * (sizeof(spiffs_cache_page) + logical_page_size))
    , cache_(new uint8_t[cacheSize_])
    , formatted_(false)
{
    fs_.user_data = this;
}

//
// SPIFFS::open()
//
int SPIFFS::open(File *file, const char *path, int flags, int mode)
{
    spiffs_flags ffs_flags = 0;
    if (flags & O_APPEND)
    {
        ffs_flags |= SPIFFS_O_APPEND;
    }
    if (flags & O_TRUNC)
    {
        ffs_flags |= SPIFFS_O_TRUNC;
    }
    if (flags & O_CREAT)
    {
        ffs_flags |= SPIFFS_O_CREAT;
    }
    if (flags & O_RDONLY)
    {
        ffs_flags |= SPIFFS_O_RDONLY;
    }
    if (flags & O_WRONLY)
    {
        ffs_flags |= SPIFFS_O_WRONLY;
    }
    if (flags & O_RDWR)
    {
        ffs_flags |= SPIFFS_O_RDWR;
    }
    if (flags & O_DIRECT)
    {
        ffs_flags |= SPIFFS_O_DIRECT;
    }
    if (flags & O_EXCL)
    {
        ffs_flags |= SPIFFS_O_EXCL;
    }

    spiffs_file fd = ::SPIFFS_open(&fs_, path, ffs_flags, 0);

    if (fd < 0)
    {
        errno = errno_translate(fd);
        return -1;
    }
    else
    {
        /* no error occured */
        file->priv = (void*)static_cast<intptr_t>(fd);
        return 0;
    }
}

//
// SPIFFS::close()
//
int SPIFFS::close(File *file)
{
    spiffs_file fd = (spiffs_file)(intptr_t)(file->priv);

    int result = SPIFFS_close(&fs_, fd);

    if (result != SPIFFS_OK)
    {
        errno = errno_translate(result);
        return -1;
    }

    return 0;
}

//
// SPIFFS::read()
//
ssize_t SPIFFS::read(File *file, void *buf, size_t count)
{
    spiffs_file fd = (spiffs_file)(intptr_t)(file->priv);

    ssize_t result = SPIFFS_read(&fs_, fd, buf, count);

    if (result < 0)
    {
        errno = errno_translate(result);
        return -1;
    }

    return result;
}

//
// SPIFFS::write()
//
ssize_t SPIFFS::write(File *file, const void *buf, size_t count)
{
    spiffs_file fd = (spiffs_file)(intptr_t)(file->priv);

    ssize_t result = SPIFFS_write(&fs_, fd, (void*)buf, count);

    if (result < 0)
    {
        errno = errno_translate(result);
        return -1;
    }

    return result;
}

//
// SPIFFS::errno_translate()
//
int SPIFFS::errno_translate(int spiffs_error)
{
    switch (spiffs_error)
    {
        default:
            // unknown error
            HASSERT(0);
            break;
        case SPIFFS_OK:
            return 0;
        case SPIFFS_ERR_NOT_MOUNTED:
            // should never get here
            HASSERT(0);
            break;
        case SPIFFS_ERR_FULL:
            return ENOSPC;
        case SPIFFS_ERR_NOT_FOUND:
            return ENOENT;
        case SPIFFS_ERR_END_OF_OBJECT:
            return EOVERFLOW;
        case SPIFFS_ERR_DELETED:
            return EFAULT;
        case SPIFFS_ERR_NOT_FINALIZED:
            return EBUSY;
        case SPIFFS_ERR_NOT_INDEX:
            return EINVAL;
        case SPIFFS_ERR_OUT_OF_FILE_DESCS:
            return EMFILE;
        case SPIFFS_ERR_FILE_CLOSED:
            return EBADF;
        case SPIFFS_ERR_FILE_DELETED:
            return EFAULT;
        case SPIFFS_ERR_BAD_DESCRIPTOR:
            return EBADF;
        case SPIFFS_ERR_IS_INDEX:
            break;
        case SPIFFS_ERR_IS_FREE:
            return EBUSY;
        case SPIFFS_ERR_INDEX_SPAN_MISMATCH:
            break;
        case SPIFFS_ERR_DATA_SPAN_MISMATCH:
            break;
        case SPIFFS_ERR_INDEX_REF_FREE:
            break;
        case SPIFFS_ERR_INDEX_REF_LU:
            break;
        case SPIFFS_ERR_INDEX_REF_INVALID:
            break;
        case SPIFFS_ERR_INDEX_FREE:
            break;
        case SPIFFS_ERR_INDEX_LU:
            break;
        case SPIFFS_ERR_INDEX_INVALID:
            return EINVAL;
        case SPIFFS_ERR_NOT_WRITABLE:
            return EACCES;
        case SPIFFS_ERR_NOT_READABLE:
            return EACCES;
        case SPIFFS_ERR_CONFLICTING_NAME:
            break;
        case SPIFFS_ERR_NOT_CONFIGURED:
            break;

        case SPIFFS_ERR_NOT_A_FS:
            break;
        case SPIFFS_ERR_MOUNTED:
            break;
        case SPIFFS_ERR_ERASE_FAIL:
            break;
        case SPIFFS_ERR_MAGIC_NOT_POSSIBLE:
            break;

        case SPIFFS_ERR_NO_DELETED_BLOCKS:
            break;

        case SPIFFS_ERR_FILE_EXISTS:
            return EEXIST;

        case SPIFFS_ERR_NOT_A_FILE:
            return ENOENT;
        case SPIFFS_ERR_RO_NOT_IMPL:
            break;
        case SPIFFS_ERR_RO_ABORTED_OPERATION:
            return EAGAIN;
        case SPIFFS_ERR_PROBE_TOO_FEW_BLOCKS:
            return ENOSPC;
        case SPIFFS_ERR_PROBE_NOT_A_FS:
            break;
        case SPIFFS_ERR_NAME_TOO_LONG:
            return ENAMETOOLONG;

        case SPIFFS_ERR_IX_MAP_UNMAPPED:
            break;
        case SPIFFS_ERR_IX_MAP_MAPPED:
            break;
        case SPIFFS_ERR_IX_MAP_BAD_RANGE:
            break;
        case SPIFFS_ERR_SEEK_BOUNDS:
            return EINVAL;

        case SPIFFS_ERR_INTERNAL:
            break;

        case SPIFFS_ERR_TEST:
            break;
    }

    return EINVAL;
}