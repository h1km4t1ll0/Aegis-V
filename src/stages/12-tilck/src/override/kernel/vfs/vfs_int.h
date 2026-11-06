/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <tilck/common/basic_defs.h>
#include <tilck/common/string_util.h>

#include <tilck/kernel/fs/vfs.h>
#include <tilck/kernel/fs/flock.h>
#include <tilck/kernel/kmalloc.h>
#include <tilck/kernel/errno.h>
#include <tilck/kernel/process.h>
#include <tilck/kernel/process_mm.h>
#include <tilck/kernel/user.h>
#include <tilck/kernel/debug_utils.h>

#include <dirent.h> // system header

/* Aegis-V: Kronos paths have no "..": the file is /src/tilck/kernel/fs/fs_int.h */
#include "/src/tilck/kernel/fs/fs_int.h"

/* internal funcs */
struct mountpoint *mp_get_retained_mp_of(struct mnt_fs *target_fs);

