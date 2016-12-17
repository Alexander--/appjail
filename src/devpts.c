#include "devpts.h"
#include "cap.h"
#include "mounts.h"
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEV_MAJ_TTY 5
#define DEV_MIN_PTMX 2

void setup_devpts() {
  unmount_directory("/dev/pts");
  if( cap_mount("devpts", "/dev/pts", "devpts", 0, "newinstance,gid=5,mode=620,ptmxmode=0666") == -1)
    errExit("mount devpts");

  if ( access("/dev/pts/ptmx", W_OK) == 0 ) {
    if( cap_mount("/dev/pts/ptmx", "/dev/ptmx", NULL, MS_BIND, NULL) == -1 ) {
      errExit("mount --bind /dev/ptmx");
    }
  } else {
    fprintf(stderr, "Creating /dev/ptmx via mknod: this may allow sandbox escape via TTY on older kernels.\n");

    if ( access("/dev/ptmx", F_OK) != 0 ) {
      dev_t dev = makedev(DEV_MAJ_TTY, DEV_MIN_PTMX);
      if ( cap_mknod("/dev/ptmx", S_IFCHR | 0666, dev) < 0 ) {
        errExit("making device /dev/ptmx");
      } else {
        fprintf(stderr, "Created /dev/ptmx via mknod.\n");
      }
    }
  }
}
