#include <unistd.h>

#include "setuid.h"

int switch_ids(uid_t newIdentity) {
  if (cap_setreuid(newIdentity)) {
    return -1;
  }

  if (cap_setregid(newIdentity)) {
    return -1;
  }

  return 0;
}
