/** signals.h                                 **/
/** architecture-customized signals.h for zsh **/

#define SIGCOUNT	32

#ifdef GLOBALS

char *sigmsg[SIGCOUNT+2] = {
	"done",
	"hangup",
	"interrupt",
	"quit",
	"illegal hardware instruction",
	"trace trap",
	"abort",
	"bus error",
	"floating point exception",
	"killed",
	"user-defined signal 1",
	"segmentation fault",
	"user-defined signal 2",
	"broken pipe",
	"alarm",
	"terminated",
	"SIGSTKFLT",
	"death of child",
	"continued",
#ifdef USE_SUSPENDED
	"suspended (signal)",
#else
	"stopped (signal)",
#endif
#ifdef USE_SUSPENDED
	"suspended",
#else
	"stopped",
#endif
#ifdef USE_SUSPENDED
	"suspended (tty input)",
#else
	"stopped (tty input)",
#endif
#ifdef USE_SUSPENDED
	"suspended (tty output)",
#else
	"stopped (tty output)",
#endif
	"urgent condition",
	"cpu limit exceeded",
	"file size limit exceeded",
	"virtual time alarm",
	"profile signal",
	"window size changed",
	"i/o ready",
	"power fail",
	"invalid system call",
	"SIGRTMIN",
	NULL
};

char *sigs[SIGCOUNT+4] = {
	"EXIT",
	"HUP",
	"INT",
	"QUIT",
	"ILL",
	"TRAP",
	"ABRT",
	"BUS",
	"FPE",
	"KILL",
	"USR1",
	"SEGV",
	"USR2",
	"PIPE",
	"ALRM",
	"TERM",
	"STKFLT",
	"CHLD",
	"CONT",
	"STOP",
	"TSTP",
	"TTIN",
	"TTOU",
	"URG",
	"XCPU",
	"XFSZ",
	"VTALRM",
	"PROF",
	"WINCH",
	"IO",
	"PWR",
	"SYS",
	"RTMIN",
	"ZERR",
	"DEBUG",
	NULL
};

#else
extern char *sigs[SIGCOUNT+4],*sigmsg[SIGCOUNT+2];
#endif
