#include "cap.h"
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>

static cap_t emptycaps = NULL;

void init_caps() {
  cap_t prog;
  cap_flag_value_t val;
  cap_value_t c;

  if(emptycaps == NULL) {
    prog = cap_get_proc();
    emptycaps = cap_init();

    c = CAP_NET_ADMIN;
    if( cap_get_flag(prog, c, CAP_PERMITTED, &val) == 0 && val == CAP_SET )
      cap_set_flag(emptycaps, CAP_PERMITTED, 1, &c, CAP_SET);
    c = CAP_CHOWN;
    if( cap_get_flag(prog, c, CAP_PERMITTED, &val) == 0 && val == CAP_SET )
      cap_set_flag(emptycaps, CAP_PERMITTED, 1, &c, CAP_SET);
    c = CAP_MKNOD;
    if( cap_get_flag(prog, c, CAP_PERMITTED, &val) == 0 && val == CAP_SET )
      cap_set_flag(emptycaps, CAP_PERMITTED, 1, &c, CAP_SET);
    c = CAP_SETGID;
    if( cap_get_flag(prog, c, CAP_PERMITTED, &val) == 0 && val == CAP_SET )
      cap_set_flag(emptycaps, CAP_PERMITTED, 1, &c, CAP_SET);
    c = CAP_SETUID;
    if( cap_get_flag(prog, c, CAP_PERMITTED, &val) == 0 && val == CAP_SET )
      cap_set_flag(emptycaps, CAP_PERMITTED, 1, &c, CAP_SET);
    c = CAP_SYS_ADMIN;
    if( cap_get_flag(prog, c, CAP_PERMITTED, &val) != 0 || val != CAP_SET )
      errExitNoErrno("The process is lacking the CAP_SYS_ADMIN capability. Exiting.");
    cap_set_flag(emptycaps, CAP_PERMITTED, 1, &c, CAP_SET);

    cap_free(prog);
  }

  if(cap_set_proc(emptycaps) == -1)
    errExit("cap_set_proc");
}

bool want_cap(cap_value_t c) {
  bool ret;
  cap_t caps;

  if(emptycaps == NULL)
    errExitNoErrno("Internal error: want_cap called, but emptycaps are not initialized.");

  caps = cap_dup(emptycaps);
  cap_set_flag(caps, CAP_EFFECTIVE, 1, &c, CAP_SET);
  ret = cap_set_proc(caps) != -1;
  cap_free(caps);

  return ret;
}

void need_cap(cap_value_t c) {
  if(!want_cap(c))
    errExit("cap_set_proc");
}

void drop_caps() {
  if(emptycaps == NULL)
    errExitNoErrno("Internal error: drops_caps called, but emptycaps are not initialized.");

  if(cap_set_proc(emptycaps) == -1)
    errExit("cap_set_proc");
}

void drop_caps_forever() {
  cap_t caps;
  caps = cap_init();
  if(cap_set_proc(caps) == -1)
    errExit("cap_set_proc");
  cap_free(caps);

  if(emptycaps != NULL) {
    cap_free(emptycaps);
    emptycaps = NULL;
  }
}

int cap_mount(const char *source, const char *target,
              const char *filesystemtype, unsigned long mountflags,
              const void *data) {
  int r;

  need_cap(CAP_SYS_ADMIN);
  r = mount(source, target, filesystemtype, mountflags, data);
  drop_caps();

  return r;
}

int cap_umount2(const char *target, int flags) {
  int r;

  need_cap(CAP_SYS_ADMIN);
  r = umount2(target, flags);
  drop_caps();

  return r;
}

int cap_mknod(const char *path, mode_t mode, dev_t dev) {
  static bool warned_mknod = false;

  int r;

  if (want_cap(CAP_MKNOD)) {
    r = mknod(path, mode, dev);
    drop_caps();
  } else {
    r = -1;
    if(!warned_mknod) {
      fprintf(stderr, "The process is missing CAP_MKNOD capability.\n"
                      "Some crucial systems may not be initialized.\n");
      warned_mknod = true;
    }
  }

  return r;
}

int cap_chown(const char *path, uid_t owner, gid_t group) {
  static bool warned_chown = false;
  int r;

  if(want_cap(CAP_CHOWN)) {
    r = chown(path, owner, group);
    drop_caps();
  }
  else {
    r = -1;
    if(!warned_chown) {
      fprintf(stderr, "The process is missing CAP_CHOWN capability.\n"
                      "Some directories will be owned by the user although they should be owned by root.\n");
      warned_chown = true;
    }
  }

  return r;
}

int cap_setregid(uid_t new_gid) {
  static bool warned_setgid = false;
  int r = -1;

  if(want_cap(CAP_SETGID)) {
    struct passwd *pw = getpwuid(new_gid);

    if (pw != NULL) {
      initgroups(pw->pw_name, new_gid);

      r = setregid(new_gid, new_gid);
    }

    drop_caps();
  } else {
    r = -1;
    if (!warned_setgid) {
      fprintf(stderr, "The process is missing CAP_SETGID capability.\n"
                      "Some processes may run with priviledges of current user.\n");
      warned_setgid = true;
    }
  }

  return r;
}

int cap_setreuid(uid_t new_uid) {
  static bool warned_setuid = false;
  int r = -1;

  if(want_cap(CAP_SETUID)) {
    r = setreuid(new_uid, new_uid);

    drop_caps();
  } else {
    r = -1;
    if (!warned_setuid) {
      fprintf(stderr, "The process is missing CAP_SETUID capability.\n"
                      "Some processes may run with priviledges of current user.\n");
      warned_setuid = true;
    }
  }

  return r;
}
