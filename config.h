/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/***** begin user configuration section *****/

/* Define this to be the location of your password file */
#define PASSWD_FILE "/etc/passwd"

/* Define this to be the name of your NIS/YP password *
 * map (if applicable)                                */
#define PASSWD_MAP "passwd.byname"

/* Define to 1 if you want user names to be cached */
#define CACHE_USERNAMES 1

/* Define to 1 if system supports job control */
#define JOB_CONTROL 1

/* Define this if you use "suspended" instead of "stopped" */
#define USE_SUSPENDED 1
 
/* The default history buffer size in lines */
#define DEFAULT_HISTSIZE 30

/* The default editor for the fc builtin */
#define DEFAULT_FCEDIT "vi"

/* The default prefix for temporary files */
#define DEFAULT_TMPPREFIX "/tmp/zsh"


/***** end of user configuration section            *****/
/***** shouldn't have to change anything below here *****/

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if the `getpgrp' function takes no argument.  */
#define GETPGRP_VOID 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you have the strcoll function and it is properly defined.  */
#define HAVE_STRCOLL 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* The global file to source absolutely first whenever zsh is run; *
 * if undefined, don't source anything                             */
#define GLOBAL_ZSHENV "/etc/zshenv"

/* The global file to source whenever zsh is run; *
 * if undefined, don't source anything            */
#define GLOBAL_ZSHRC "/etc/zshrc"

/* The global file to source whenever zsh is run as a login shell; *
 * if undefined, don't source anything                             */
#define GLOBAL_ZLOGIN "/etc/zlogin"

/* The global file to source whenever zsh is run as a login shell, *
 * before zshrc is read; if undefined, don't source anything       */
#define GLOBAL_ZPROFILE "/etc/zprofile"

/* The global file to source whenever zsh was run as a login shell.  *
 * This is sourced right before exiting.  If undefined, don't source *
 * anything                                                          */
#define GLOBAL_ZLOGOUT "/etc/zlogout"

/* Define to 1 if compiler could initialise a union */
#define HAVE_UNION_INIT 1

/* Define to 1 if compiler incorrectly cast signed to unsigned */
/* #undef BROKEN_SIGNED_TO_UNSIGNED_CASTING */

/* Define if your system defines TIOCGWINSZ in sys/ioctl.h.  */
/* #undef GWINSZ_IN_SYS_IOCTL */

/* Define to 1 if you have NIS */
/* #undef HAVE_NIS */

/* Define to 1 if you have NISPLUS */
/* #undef HAVE_NIS_PLUS */

/* Define to 1 if you have RFS superroot directory. */
/* #undef HAVE_SUPERROOT */

/* Define to the path of the /dev/fd filesystem */
#define PATH_DEV_FD "/proc/self/fd"

/* Define if sys/time.h and sys/select.h cannot be both included */
/* #undef TIME_H_SELECT_H_CONFLICTS */

/* Define if your system's struct utmp has a member named ut_host.  */
#define HAVE_UT_HOST 1

/* Define if you have the <utmpx.h> header file.  */
/* #undef HAVE_UTMPX_H */

/* Define to be the machine type (microprocessor class or machine model) */
#define MACHTYPE "i386"

/* Define to be the name of the operating system */
#define OSTYPE "cygwin32"

/* Define to 1 if ANSI function prototypes are usable.  */
#define PROTOTYPES 1

/* Define to be location of utmp file.  This value is only used if UTMP_FILE, *
 * UTMPX_FILE, or _PATH_UTMP are not defined in an include file.              */
#define UTMP_FILE_CONFIG "/var/run/utmp"

/* Define to be a string corresponding the vendor of the machine */
#define VENDOR "pc"

/* Define if your system defines `struct winsize' in sys/ptem.h.  */
/* #undef WINSIZE_IN_PTEM */

/* Define  to be location of wtmp file.  This value is only use if WTMP_FILE, *
 * WTMPX_FILE, or _PATH_WTMP are not defined in an include file.              */
#define WTMP_FILE_CONFIG "/var/log/wtmp"

/* Define to 1 if you want to debug zsh */
/* #undef DEBUG */

/* Define to 1 if you want to use zsh's own memory allocation routines */
/* #undef ZSH_MEM */

/* Define to 1 if you want to debug zsh memory allocation routines */
/* #undef ZSH_MEM_DEBUG */

/* Define to 1 if you want to turn on warnings of memory allocation errors */
/* #undef ZSH_MEM_WARNING */

/* Define to 1 if you want to turn on memory checking for free() */
/* #undef ZSH_SECURE_FREE */

/* Define to 1 if you want to get debugging information on internal *
 * hash tables.  This turns on the `hashinfo' builtin.              */
/* #undef ZSH_HASH_DEBUG */

/* Define to 1 if your termcap library has the ospeed variable */
#define HAVE_OSPEED 1
/* Define to 1 if you have ospeed, but it is not defined in termcap.h */
/* #undef MUST_DEFINE_OSPEED */

/* Define to 1 if tgetent() accepts NULL as a buffer */
/* #undef TGETENT_ACCEPTS_NULL */

/* Define to 1 if you use POSIX style signal handling */
#define POSIX_SIGNALS 1

/* Define to 1 if you use BSD style signal handling (and can block signals) */
/* #undef BSD_SIGNALS */

/* Define to 1 if you use SYS style signal handling (and can block signals) */
/* #undef SYSV_SIGNALS */

/* Define to 1 if you have no signal blocking at all (bummer) */
/* #undef NO_SIGNAL_BLOCKING */

/* Define to `unsigned int' if <sys/types.h> or <signal.h> doesn't define */
/* #undef sigset_t */

/* Define to 1 if struct timezone is defined by a system header */
#define HAVE_STRUCT_TIMEZONE 1

/* Define if your system's typeahead disappears from the shell editor. */
/* #undef CLOBBERS_TYPEAHEAD */

/* Define to 1 if there is a prototype defined for brk() on your system */
/* #undef HAVE_BRK_PROTO */

/* Define to 1 if there is a prototype defined for sbrk() on your system */
#define HAVE_SBRK_PROTO 1

/* Define to 1 if there is a prototype defined for ioctl() on your system */
#define HAVE_IOCTL_PROTO 1

/* Define to 1 if system has working FIFO's */
/* #undef HAVE_FIFOS */

/* Define to 1 if struct rlimit use quad_t */
/* #undef RLIM_T_IS_QUAD_T */

/* Define to 1 if rlimit use unsigned */
#define RLIM_T_IS_UNSIGNED 1

/* Define to the type used in struct rlimit */
/* #undef rlim_t */

/* Define to 1 if /bin/sh does not interpret \ escape sequences */
#define SH_USE_BSD_ECHO 1

/* The number of bytes in a long.  */
#define SIZEOF_LONG 4

/* Define if you have the difftime function.  */
#define HAVE_DIFFTIME 1

/* Define if you have the getdomainname function.  */
#define HAVE_GETDOMAINNAME 1

/* Define if you have the gethostname function.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have the getlogin function.  */
#define HAVE_GETLOGIN 1

/* Define if you have the getrlimit function.  */
#define HAVE_GETRLIMIT 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the initgroups function.  */
#define HAVE_INITGROUPS 1

/* Define if you have the killpg function.  */
#define HAVE_KILLPG 1

/* Define if you have the lstat function.  */
#define HAVE_LSTAT 1

/* Define if you have the mkfifo function.  */
#define HAVE_MKFIFO 1

/* Define if you have the nis_list function.  */
/* #undef HAVE_NIS_LIST */

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the seteuid function.  */
#define HAVE_SETEUID 1

/* Define if you have the setpgid function.  */
#define HAVE_SETPGID 1

/* Define if you have the setresuid function.  */
/* #undef HAVE_SETRESUID */

/* Define if you have the setreuid function.  */
#define HAVE_SETREUID 1

/* Define if you have the setuid function.  */
#define HAVE_SETUID 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the sigblock function.  */
/* #undef HAVE_SIGBLOCK */

/* Define if you have the sighold function.  */
#define HAVE_SIGHOLD 1

/* Define if you have the sigprocmask function.  */
#define HAVE_SIGPROCMASK 1

/* Define if you have the sigrelse function.  */
#define HAVE_SIGRELSE 1

/* Define if you have the sigsetmask function.  */
/* #undef HAVE_SIGSETMASK */

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the tcgetattr function.  */
#define HAVE_TCGETATTR 1

/* Define if you have the tcsetpgrp function.  */
#define HAVE_TCSETPGRP 1

/* Define if you have the wait3 function.  */
#define HAVE_WAIT3 1

/* Define if you have the waitpid function.  */
#define HAVE_WAITPID 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <libc.h> header file.  */
/* #undef HAVE_LIBC_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/filio.h> header file.  */
/* #undef HAVE_SYS_FILIO_H */

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/times.h> header file.  */
#define HAVE_SYS_TIMES_H 1

/* Define if you have the <sys/utsname.h> header file.  */
#define HAVE_SYS_UTSNAME_H 1

/* Define if you have the <termcap.h> header file.  */
#define HAVE_TERMCAP_H 1

/* Define if you have the <termio.h> header file.  */
#define HAVE_TERMIO_H 1

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */
