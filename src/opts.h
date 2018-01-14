#pragma once

#include "common.h"
#include "configfile.h"
#include "list.h"

typedef struct {
  uid_t uid;
  uid_t switch_to_uid;

  char *user;

  char *x11_cookie;

  bool allow_new_privs;
  bool keep_shm;
  bool keep_ipc_namespace;
  const char *homedir;
  run_mode_t run_mode;
  bool bind_run_media;
  bool keep_system_bus;

  char **argv;

  strlist *keep_mounts, *keep_mounts_full;
  strlist *shared_mounts;
  strlist *special_mounts;
  strlist *mask_directories;
  bool keep_x11;
  bool x11_trusted;
  unsigned int x11_timeout;
  bool unshare_network;

  bool daemonize;
  bool keep_output;
  bool initstub;
  intlist *keepfds;
  strlist *keepenv;
  strlist *setenv;
  bool cleanenv;
  bool readonly;

  bool has_tmpfs_size;
  unsigned long long int tmpfs_size;

  /* Internal options */
  bool setup_tty;
  int pipefd;
} appjail_options;

appjail_options *parse_options(int argc, char *argv[], const appjail_config *config);
void free_options(appjail_options *opts);

bool string_to_run_mode(run_mode_t *result, const char *s);
bool string_to_size(unsigned long long int *size, const char *s);
