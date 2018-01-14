#pragma once

#include "common.h"
#include <sys/capability.h>
#include <unistd.h>

void init_caps();
bool want_cap(cap_value_t c);
void need_cap(cap_value_t c);
void drop_caps();
void drop_caps_forever();
int cap_mount(const char *source, const char *target,
              const char *filesystemtype, unsigned long mountflags,
              const void *data);
int cap_umount2(const char *target, int flags);
int cap_chown(const char *path, uid_t owner, gid_t group);
int cap_mknod(const char *path, mode_t mode, dev_t dev);
int cap_setreuid(uid_t new_uid);
int cap_setregid(uid_t new_gid);
