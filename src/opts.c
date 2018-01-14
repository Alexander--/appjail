#include "common.h"
#include "opts.h"
#include <getopt.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#define NUM_ENTRIES 10

#define STR_HELPER(s) #s
#define TO_STR(s) STR_HELPER(s)

static void version() {
  printf("appjail " APPJAIL_VERSION " by Thomas Bächler <thomas@archlinux.org>\n"
         "\n"
         "Available at https://github.com/brain0/appjail\n"
         "Licensed under the conditions of the WTFPL (http://www.wtfpl.net/)\n"
         "\n");
}

static void usage() {
  printf("Usage: appjail [OPTIONS] [COMMAND]\n"
         "\n"
         "Start COMMAND in an isolated environment. If COMMAND is not given, it is set to '/bin/sh -i'.\n"
         "\n"
         "Options:\n"
         "  -h, --help               Print command help and exit.\n"
         "  -v, --version            Print version information and exit.\n"
         "  -d, --daemonize          Run the jailed process in the background.\n"
         "  --keep-output            Do not close stdout/stderr in --daemonize mode.\n"
         "  -i, --initstub           Run a stub init process inside the jail.\n"
         "  -p, --allow-new-privs    Don't prevent setuid binaries from raising privileges.\n"
         "  --keep-shm               Keep the host's /dev/shm directory.\n"
         "  --keep-ipc-namespace     Stay in the host's IPC namespace. This is necessary for\n"
         "                           Xorg's MIT-SHM extension.\n"
         "  -H, --homedir <DIR>      Use DIR as home directory instead of a temporary one.\n"
         "  -K, --keep <DIR>         Do not unmount DIR inside the jail.\n"
         "                           This option also affects all mounts that are parents of DIR.\n"
         "  --keep-full <DIR>        Like --keep, but also affects all submounts of DIR.\n"
         "  -S, --shared <DIR>       Do not force private mount propagation on submounts of DIR.\n"
         "  -M, --mask <DIR>         Make DIR and all its subdirectories inaccessible in the jail.\n"
         "  --read-only              Make the file system read-only.\n"
         "  -X, --x11                Allow X11 access.\n"
         "  --x11-trusted            Generate a trusted X11 cookie (an untrusted cookie is used by default).\n"
         "  --x11-timeout <N>        If no X11 client is connected for N seconds, the cookie is revoked.\n"
         "  --x11-cookie <STRING>    Use a manually supplied X11 security cookie.\n"
         "  -N, --private-network    Isolate from the host network.\n"
         "  -n, --no-private-network Do not isolate from the host network.\n"
         "  -R, --run <MODE>         Determine how to handle the /run directory.\n"
         "                            host:    Keep the host's /run directory\n"
         "                            user:    Only keep /run/user/UID\n"
         "                            private: Use a private /run directory\n"
         "  --(no-)run-media         Determine whether /run/media/USER will be available in the jail.\n"
         "                           This option has no effect with --run=host.\n"
         "  --system-bus             Determine whether the host's DBUS socket directory (/run/dbus)\n"
         "                           should be available in the jail.\n"
         "                           This option has no effect with --run=host.\n"
         "  --no-clean-env           Do not clean the environment.\n"
         "  --keep-env VAR           Keep the environment variable VAR.\n"
         "                           This option has no effect with --no-clean-env.\n"
         "  --set-env VAR=VAL        Set the environment variable VAR to VAL.\n"
         "  --keep-fd FD             Do not close the file descriptor FD.\n"
         "  --tmpfs-size SZ          Limit the size of the tmpfs instance used for the jail's temporary\n"
         "                           directory to SZ. The suffixes K, M or G are allowed.\n"
         "  --setuid UID             Run jailed process under specified user ID,\n"
         "                           which must be between " TO_STR(MIN_SAFE_UID) " and " TO_STR(MAX_SAFE_UID) ".\n"
         "\n");
}

char *remove_trailing_slash(const char *p) {
  char *r = strdup(p);
  size_t len = strlen(r);

  while(len > 0 && r[len-1] == '/') {
    r[--len] = '\0';
  }
  return r;
}

#define OPT_KEEP_SHM 256
#define OPT_KEEP_FULL 257
#define OPT_RUN_MEDIA 258
#define OPT_NO_RUN_MEDIA 259
#define OPT_KEEP_IPC_NAMESPACE 260
#define OPT_X11_TRUSTED 261
#define OPT_X11_TIMEOUT 262
#define OPT_KEEP_SYSTEM_BUS 263
#define OPT_KEEP_FD 264
#define OPT_NO_CLEAN_ENV 265
#define OPT_KEEP_ENV 266
#define OPT_READ_ONLY 267
#define OPT_SET_ENV 268
#define OPT_TMPFS_SIZE 269
#define OPT_KEEP_OUTPUT 270
#define OPT_X11_COOKIE 271
#define OPT_SETUID 272

appjail_options *parse_options(int argc, char *argv[], const appjail_config *config) {
  int opt, i;
  unsigned long long int size;
  appjail_options *opts;
  struct passwd *pw;
  static struct option long_options[] = {
    { "version",            no_argument,       0,  'V'                    },
    { "help",               no_argument,       0,  'h'                    },
    { "allow-new-privs",    no_argument,       0,  'p'                    },
    { "homedir",            required_argument, 0,  'H'                    },
    { "keep-shm",           no_argument,       0,  OPT_KEEP_SHM           },
    { "keep-ipc-namespace", no_argument,       0,  OPT_KEEP_IPC_NAMESPACE },
    { "keep",               required_argument, 0,  'K'                    },
    { "keep-full",          required_argument, 0,  OPT_KEEP_FULL          },
    { "shared",             required_argument, 0,  'S'                    },
    { "x11",                no_argument,       0,  'X'                    },
    { "x11-trusted",        no_argument,       0,  OPT_X11_TRUSTED        },
    { "x11-timeout",        required_argument, 0,  OPT_X11_TIMEOUT        },
    { "x11-cookie",         required_argument, 0,  OPT_X11_COOKIE         },
    { "private-network",    no_argument,       0,  'N'                    },
    { "no-private-network", no_argument,       0,  'n'                    },
    { "run",                required_argument, 0,  'R'                    },
    { "run-media",          no_argument,       0,  OPT_RUN_MEDIA          },
    { "no-run-media",       no_argument,       0,  OPT_NO_RUN_MEDIA       },
    { "system-bus",         no_argument,       0,  OPT_KEEP_SYSTEM_BUS    },
    { "mask",               required_argument, 0,  'M'                    },
    { "daemonize",          no_argument,       0,  'd'                    },
    { "keep-output",        no_argument,       0,  OPT_KEEP_OUTPUT        },
    { "initstub",           no_argument,       0,  'i'                    },
    { "keep-fd",            required_argument, 0,  OPT_KEEP_FD            },
    { "no-clean-env",       no_argument,       0,  OPT_NO_CLEAN_ENV       },
    { "keep-env",           required_argument, 0,  OPT_KEEP_ENV           },
    { "set-env",            required_argument, 0,  OPT_SET_ENV            },
    { "read-only",          no_argument,       0,  OPT_READ_ONLY          },
    { "setuid",             required_argument, 0,  OPT_SETUID             },
    { "tmpfs-size",         required_argument, 0,  OPT_TMPFS_SIZE         },
    { 0,                    0,                 0,  0                      }
  };

  if((opts = malloc(sizeof(appjail_options))) == NULL)
    errExit("malloc");

  /* special options */
  opts->uid = getuid();
  opts->switch_to_uid = 0;
  errno = 0;
  if((pw = getpwuid(opts->uid)) == NULL)
    errExit("getpwuid");
  opts->user = strdup(pw->pw_name);
  /* defaults */
  opts->allow_new_privs = false;
  opts->keep_shm = false;
  opts->keep_ipc_namespace = false;
  opts->homedir = NULL;
  opts->keep_x11 = false;
  opts->x11_trusted = false;
  opts->x11_cookie = NULL;
  opts->x11_timeout = 60;
  opts->unshare_network = config->default_private_network;
  opts->run_mode = config->default_run_mode;
  opts->bind_run_media = config->default_bind_run_media;
  opts->keep_system_bus = false;
  opts->daemonize = false;
  opts->keep_output = false;
  opts->initstub = false;
  opts->cleanenv = true;
  opts->readonly = false;
  opts->has_tmpfs_size = config->has_max_tmpfs_size;
  opts->tmpfs_size = config->max_tmpfs_size;
  /* initialize directory lists */
  opts->keep_mounts = strlist_new();
  opts->keep_mounts_full = strlist_new();
  opts->shared_mounts = strlist_new();
  opts->mask_directories = strlist_new();
  opts->keepfds = intlist_new();
  opts->keepenv = strlist_new();
  opts->setenv = strlist_new();

  /* Special environment variables */
  strlist_append_copy_unique(opts->keepenv, "PATH");
  strlist_append_copy_unique(opts->keepenv, "USER");
  strlist_append_copy_unique(opts->keepenv, "HOME");
  strlist_append_copy_unique(opts->keepenv, "LANG");
  strlist_append_copy_unique(opts->keepenv, "LC_CTYPE");
  strlist_append_copy_unique(opts->keepenv, "LC_NUMERIC");
  strlist_append_copy_unique(opts->keepenv, "LC_TIME");
  strlist_append_copy_unique(opts->keepenv, "LC_COLLATE");
  strlist_append_copy_unique(opts->keepenv, "LC_MONETARY");
  strlist_append_copy_unique(opts->keepenv, "LC_MESSAGES");
  strlist_append_copy_unique(opts->keepenv, "LC_PAPER");
  strlist_append_copy_unique(opts->keepenv, "LC_NAME");
  strlist_append_copy_unique(opts->keepenv, "LC_ADDRESS");
  strlist_append_copy_unique(opts->keepenv, "LC_TELEPHONE");
  strlist_append_copy_unique(opts->keepenv, "LC_MEASUREMENT");
  strlist_append_copy_unique(opts->keepenv, "LC_IDENTIFICATION");
  strlist_append_copy_unique(opts->keepenv, "LC_ALL");

  while((opt = getopt_long(argc, argv, "+:hVpH:K:S:XNnR:M:di", long_options, NULL)) != -1) {
    switch(opt) {
      case 'V':
        version();
        exit(EXIT_SUCCESS);
        break;
      case 'h':
        version();
        usage();
        exit(EXIT_SUCCESS);
        break;
      case 'p':
        if(!config->allow_new_privs_permitted)
          errExitNoErrno("Using the --allow-new-privs option is not permitted.");
        opts->allow_new_privs = true;
        break;
      case 'H':
        opts->homedir = optarg;
        break;
      case OPT_KEEP_SHM:
        opts->keep_shm = true;
        break;
      case OPT_KEEP_IPC_NAMESPACE:
        opts->keep_ipc_namespace = true;
        break;
      case 'K':
        strlist_append(opts->keep_mounts, remove_trailing_slash(optarg));
        break;
      case OPT_KEEP_FULL:
        strlist_append(opts->keep_mounts_full, remove_trailing_slash(optarg));
        break;
      case 'S':
        strlist_append(opts->shared_mounts, remove_trailing_slash(optarg));
        break;
      case 'X':
        opts->keep_x11 = true;
        strlist_append_copy_unique(opts->keepenv, "DISPLAY");
        break;
      case OPT_X11_TRUSTED:
        opts->x11_trusted = true;
        break;
      case OPT_X11_TIMEOUT:
        if(!string_to_unsigned_integer(&(opts->x11_timeout), optarg))
          errExitNoErrno("Invalid argument to --x11-timeout.");
        break;
      case OPT_X11_COOKIE:
        opts->x11_cookie = strdup(optarg);
	break;
      case 'N':
        opts->unshare_network = true;
        break;
      case 'n':
        opts->unshare_network = false;
        break;
      case 'R':
        if(!string_to_run_mode(&(opts->run_mode), optarg))
          errExitNoErrno("Invalid argument to -R/--run.");
        break;
      case OPT_RUN_MEDIA:
        opts->bind_run_media = true;
        break;
      case OPT_NO_RUN_MEDIA:
        opts->bind_run_media = false;
        break;
      case OPT_KEEP_SYSTEM_BUS:
        opts->keep_system_bus = true;
        break;
      case 'M':
        strlist_append(opts->mask_directories, remove_trailing_slash(optarg));
        break;
      case 'd':
        opts->daemonize = true;
        break;
      case OPT_KEEP_OUTPUT:
        opts->keep_output = true;
        break;
      case 'i':
        opts->initstub = true;
        break;
      case OPT_KEEP_FD:
        if(!string_to_integer(&i, optarg))
          errExitNoErrno("Invalid argument to --keep-fd.");
        intlist_append(opts->keepfds, i);
        break;
      case OPT_NO_CLEAN_ENV:
        opts->cleanenv = false;
        break;
      case OPT_KEEP_ENV:
        strlist_append_copy_unique(opts->keepenv, optarg);
        break;
      case OPT_SET_ENV:
        strlist_append_copy(opts->setenv, optarg);
        break;
      case OPT_READ_ONLY:
        opts->readonly = true;
        break;
      case OPT_SETUID:
        if(!string_to_unsigned_integer(&(opts->switch_to_uid), optarg))
          errExitNoErrno("Invalid argument to --setuid.");

        if (opts->switch_to_uid < MIN_SAFE_UID || opts->switch_to_uid > MAX_SAFE_UID)
          errExitNoErrno("--setuid argument is outside the allowed range.");
        break;
      case OPT_TMPFS_SIZE:
        if(!string_to_size(&size, optarg))
          errExitNoErrno("Invalid argument to --tmpfs-size");
        if(!opts->has_tmpfs_size || size < opts->tmpfs_size) {
          opts->has_tmpfs_size = true;
          opts->tmpfs_size = size;
        }
        break;
      case ':':
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        exit(EXIT_FAILURE);
      case '?':
        if(optopt == '\0')
          fprintf(stderr, "Invalid option: %s\n", argv[optind-1]);
        else
          fprintf(stderr, "Invalid option: -%c\n", optopt);
        exit(EXIT_FAILURE);
      default:
        errExitNoErrno("Unknown error while parsing options.");
        exit(EXIT_FAILURE);
    }
  }
  opts->argv = &(argv[optind]);

  opts->special_mounts = strlist_new();
  strlist_append_copy(opts->special_mounts, "/dev");
  strlist_append_copy(opts->special_mounts, "/proc");
  strlist_append_copy(opts->special_mounts, "/run");
  strlist_append_copy(opts->special_mounts, APPJAIL_SWAPDIR);

  return opts;
}

void free_options(appjail_options *opts) {
  strlist_free(opts->keep_mounts);
  strlist_free(opts->keep_mounts_full);
  strlist_free(opts->shared_mounts);
  strlist_free(opts->special_mounts);
  strlist_free(opts->mask_directories);
  intlist_free(opts->keepfds);
  strlist_free(opts->keepenv);
  strlist_free(opts->setenv);
  free(opts->user);
  free(opts->x11_cookie);
  free(opts);
}

bool string_to_run_mode(run_mode_t *result, const char *s) {
  bool ret = true;

  if(!strcmp(s, "host"))
    *result = RUN_HOST;
  else if(!strcmp(s, "user"))
    *result = RUN_USER;
  else if(!strcmp(s, "private"))
    *result = RUN_PRIVATE;
  else
    ret = false;

  return ret;
}

bool string_to_size(unsigned long long int *size, const char *s) {
  unsigned long long int res;
  char *end;

  res = strtoull(s, &end, 10);
  if( end == s || res == ULLONG_MAX || strlen(end) > 1 )
    return false;

  switch(*end) {
    case '\0':
      *size = res;
      break;
    case 'K':
      *size = res * 1024;
      break;
    case 'M':
      *size = res * 1024 * 1024;
      break;
    case 'G':
      *size = res * 1024 * 1024 * 1024;
      break;
    default:
      return false;
  }
  return true;
}
