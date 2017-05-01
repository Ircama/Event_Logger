/*
 * $Id: hashtable.h,v 2.30 1996/10/15 20:16:35 hzoli Exp $
 *
 * hashtable.h - header file for hash table handling code
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

/* Nodes for reserved word hash table */

#define BUSYBOX

#ifdef GLOBALS
struct reswd reswds[] =
{
    {NULL, "!", 0, BANG},
    {NULL, "[[", 0, DINBRACK},
    {NULL, "{", 0, INBRACE},
    {NULL, "}", 0, OUTBRACE},
    {NULL, "case", 0, CASE},
    {NULL, "coproc", 0, COPROC},
    {NULL, "do", 0, DO},
    {NULL, "done", 0, DONE},
    {NULL, "elif", 0, ELIF},
    {NULL, "else", 0, ELSE},
    {NULL, "end", 0, ZEND},
    {NULL, "esac", 0, ESAC},
    {NULL, "fi", 0, FI},
    {NULL, "for", 0, FOR},
    {NULL, "foreach", 0, FOREACH},
    {NULL, "function", 0, FUNC},
    {NULL, "if", 0, IF},
    {NULL, "nocorrect", 0, NOCORRECT},
    {NULL, "repeat", 0, REPEAT},
    {NULL, "select", 0, SELECT},
    {NULL, "then", 0, THEN},
    {NULL, "time", 0, TIME},
    {NULL, "until", 0, UNTIL},
    {NULL, "while", 0, WHILE},
    {NULL, NULL}
};
#else
extern struct reswd reswds[];
#endif


/* Nodes for special parameters for parameter hash table */

#ifdef GLOBALS
# ifdef HAVE_UNION_INIT
#  define BR(X) {X}
struct param
# else
#  define BR(X) X
struct iparam {
    struct hashnode *next;
    char *nam;			/* hash data                             */
    int flags;			/* PM_* flags (defined in zsh.h)         */
    void *value;
    void (*func1) _((void));	/* set func                              */
    char *(*func2) _((void));	/* get func                              */
    int ct;			/* output base or field width            */
    void *data;			/* used by getfns                        */
    char *env;			/* location in environment, if exported  */
    char *ename;		/* name of corresponding environment var */
    Param old;			/* old struct for use with local         */
    int level;			/* if (old != NULL), level of localness  */
}
# endif
special_params[] ={
#define SFN(X) BR(((void (*)_((Param, char *)))(X)))
#define GFN(X) BR(((char *(*)_((Param)))(X)))
#define IPDEF1(A,B,C,D) {NULL,A,PM_INTEGER|PM_SPECIAL|D,BR(NULL),SFN(C),GFN(B),10,NULL,NULL,NULL,NULL,0}
IPDEF1("#", poundgetfn, nullsetfn, PM_READONLY|PM_SETN),
IPDEF1("ERRNO", errnogetfn, nullsetfn, PM_READONLY|PM_SETN),
IPDEF1("GID", gidgetfn, gidsetfn, PM_DONTIMPORT|PM_SETN),
IPDEF1("EGID", egidgetfn, egidsetfn, PM_DONTIMPORT|PM_SETN),
IPDEF1("RANDOM", randomgetfn, randomsetfn, PM_SETN),
// amacri: start of bsh integrations
IPDEF1("ZFORK", zforkgetfn, zforksetfn, 0),
IPDEF1("FB_X", fbxgetfn, fbxsetfn, 0),
IPDEF1("FB_Y", fbygetfn, fbysetfn, 0),
IPDEF1("FB_XR", fbxrgetfn, fbxrsetfn, 0),
IPDEF1("FB_YLR", fbylrgetfn, fbylrsetfn, 0),
IPDEF1("FB_YHR", fbyhrgetfn, fbyhrsetfn, 0),
IPDEF1("FB_XS", fbxSgetfn, fbxSsetfn, 0),
IPDEF1("FB_YS", fbySgetfn, fbySsetfn, 0),
IPDEF1("FB_NUMINK", fbnuminkgetfn, fbnuminksetfn, 0),
IPDEF1("FB_BGCOL", fbBgColgetfn, fbBgColsetfn, 0),
IPDEF1("FB_BOLD", fbBoldgetfn, fbBoldsetfn, 0),
IPDEF1("FB_COL", fbcolgetfn, fbcolsetfn, 0),
IPDEF1("FB_CENTER", fbcentergetfn, fbcentersetfn, 0),
IPDEF1("FB_FONT_X", fbfontxgetfn, fbfontxsetfn, 0),
IPDEF1("FB_FONT_Y", fbfontygetfn, fbfontysetfn, 0),
IPDEF1("FB_FONT", fbfontgetfn, fbfontsetfn, 0),
IPDEF1("NICE", nicegetfn, nicesetfn, 0),
IPDEF1("PRECISION", precisiongetfn, precisionsetfn, 0),
// amacri: end of bsh integrations
IPDEF1("SECONDS", secondsgetfn, secondssetfn, PM_SETN),
IPDEF1("UID", uidgetfn, uidsetfn, PM_DONTIMPORT|PM_SETN),
IPDEF1("EUID", euidgetfn, euidsetfn, PM_DONTIMPORT|PM_SETN),
IPDEF1("TTYIDLE", ttyidlegetfn, nullsetfn, PM_READONLY|PM_SETN),

#define IPDEF2(A,B,C,D) {NULL,A,PM_SCALAR|PM_SPECIAL|D,BR(NULL),BR(C),BR(B),0,NULL,NULL,NULL,NULL,0}
// amacri: start of bsh integrations
IPDEF2("E_RADIUS", EarthRadius_getfn, EarthRadius_setfn, 0),
IPDEF2("HSECONDS", hsecondsgetfn, hsecondssetfn, PM_SETN),
// amacri: end of bsh integrations
IPDEF2("USERNAME", usernamegetfn, usernamesetfn, PM_DONTIMPORT|PM_SETN),
IPDEF2("-", dashgetfn, nullsetfn, PM_READONLY|PM_SETN),
IPDEF2("histchars", histcharsgetfn, histcharssetfn, PM_DONTIMPORT|PM_SETN),
IPDEF2("HOME", homegetfn, homesetfn, 0),
IPDEF2("TERM", termgetfn, termsetfn, 0),
IPDEF2("WORDCHARS", wordcharsgetfn, wordcharssetfn, 0),
IPDEF2("IFS", ifsgetfn, ifssetfn, PM_DONTIMPORT),
IPDEF2("_", underscoregetfn, nullsetfn, PM_READONLY|PM_SETN),

#ifdef LC_ALL
#define LCIPDEF(A,B) {NULL,A,PM_SCALAR|PM_SPECIAL|PM_UNSET,BR(NULL),BR(lcsetfn),BR(strgetfn),0,(void *)B,NULL,NULL,NULL,0}
IPDEF2("LANG", strgetfn, langsetfn, PM_UNSET),
IPDEF2("LC_ALL", strgetfn, lc_allsetfn, PM_UNSET),
#ifdef LC_COLLATE
LCIPDEF("LC_COLLATE", LC_COLLATE),
#endif
#ifdef LC_CTYPE
LCIPDEF("LC_CTYPE", LC_CTYPE),
#endif
#ifdef LC_MESSAGES
LCIPDEF("LC_MESSAGES", LC_MESSAGES),
#endif
#ifdef LC_TIME
LCIPDEF("LC_TIME", LC_TIME),
#endif
#endif

#define IPDEF4(A,B) {NULL,A,PM_INTEGER|PM_READONLY|PM_SETN|PM_SPECIAL,BR(NULL),BR(nullsetfn),GFN(intvargetfn),10,(void *)B,NULL,NULL,NULL,0}
IPDEF4("!", &lastpid),
IPDEF4("$", &mypid),
IPDEF4("?", &lastval),
IPDEF4("LINENO", &lineno),
IPDEF4("PPID", &ppid),

#define IPDEF5(A,B,F) {NULL,A,PM_INTEGER|PM_SETN|PM_SPECIAL,BR(NULL),SFN(F),GFN(intvargetfn),10,(void *)B,NULL,NULL,NULL,0}
IPDEF5("OPTIND", &zoptind, intvarsetfn),
IPDEF5("SHLVL", &shlvl, intvarsetfn),

#define IPDEF6(A,B) {NULL,A,PM_SCALAR|PM_READONLY|PM_SETN|PM_SPECIAL,BR(NULL),SFN(nullsetfn),BR(strvargetfn),0,(void *)B,NULL,NULL,NULL,0}
IPDEF6("PWD", &pwd),

#define IPDEF7(A,B, C) {NULL,A,PM_SCALAR|PM_SPECIAL|C,BR(NULL),BR(strvarsetfn),BR(strvargetfn),0,(void *)B,NULL,NULL,NULL,0}
IPDEF7("OLDPWD", &oldpwd, PM_SETN),
IPDEF7("OPTARG", &zoptarg, PM_SETN),
IPDEF7("NULLCMD", &nullcmd, PM_SETN),
IPDEF7("POSTEDIT", &postedit, PM_SETN),
IPDEF7("READNULLCMD", &readnullcmd, PM_SETN),
IPDEF7("RPROMPT", &rprompt, 0),
IPDEF7("PS1", &prompt, 0),
IPDEF7("PS2", &prompt2, 0),
IPDEF7("PS3", &prompt3, 0),
IPDEF7("PS4", &prompt4, 0),
IPDEF7("RPS1", &rprompt, 0),
IPDEF7("SPROMPT", &sprompt, 0),
IPDEF7("0", &argzero, PM_SETN),

#define IPDEF8(A,B,C) {NULL,A,PM_SCALAR|PM_SETN|PM_SPECIAL,BR(NULL),SFN(colonarrsetfn),GFN(colonarrgetfn),0,(void *)B,NULL,C,NULL,0}
IPDEF8("CDPATH", &cdpath, "cdpath"),
IPDEF8("FIGNORE", &fignore, "fignore"),
IPDEF8("FPATH", &fpath, "fpath"),
IPDEF8("MAILPATH", &mailpath, "mailpath"),
IPDEF8("WATCH", &watch, "watch"),
IPDEF8("PATH", &path, "path"),
IPDEF8("PSVAR", &psvar, "psvar"),

#define IPDEF9(A,B,C) {NULL,A,PM_ARRAY|PM_SETN|PM_SPECIAL|PM_DONTIMPORT,BR(NULL),SFN(arrvarsetfn),GFN(arrvargetfn),0,(void *)B,NULL,C,NULL,0}
IPDEF9("*", &pparams, NULL),
IPDEF9("@", &pparams, NULL),
#define IPDEFA(A,B,C) {NULL,A,PM_ARRAY|PM_SETN|PM_SPECIAL|PM_DONTIMPORT|PM_READONLY,BR(NULL),SFN(arrvarsetfn),GFN(arrvargetfn),0,(void *)B,NULL,C,NULL,0}
IPDEFA("FB_SCRDATA", &screendata, NULL),
{NULL, NULL},

/* The following parameters are not avaible in sh/ksh compatibility *
 * mode. All of these has sh compatible equivalents.                */
IPDEF1("ARGC", poundgetfn, nullsetfn, PM_READONLY|PM_SETN),
IPDEF2("HISTCHARS", histcharsgetfn, histcharssetfn, PM_DONTIMPORT|PM_SETN),
IPDEF4("status", &lastval),
IPDEF7("prompt", &prompt, 0),
IPDEF7("PROMPT", &prompt, 0),
IPDEF7("PROMPT2", &prompt2, 0),
IPDEF7("PROMPT3", &prompt3, 0),
IPDEF7("PROMPT4", &prompt4, 0),
IPDEF8("MANPATH", &manpath, "manpath"),
IPDEF9("argv", &pparams, NULL),
IPDEF9("fignore", &fignore, "FIGNORE"),
IPDEF9("cdpath", &cdpath, "CDPATH"),
IPDEF9("fpath", &fpath, "FPATH"),
IPDEF9("mailpath", &mailpath, "MAILPATH"),
IPDEF9("manpath", &manpath, "MANPATH"),
IPDEF9("watch", &watch, "WATCH"),
IPDEF9("path", &path, "PATH"),
IPDEF9("psvar", &psvar, "PSVAR"),

{NULL, NULL}
};
# undef BR
#else
extern struct param special_params[];
#endif


/* Builtin function numbers; used by handler functions that handle more *
 * than one builtin.  Note that builtins such as compctl, that are not  *
 * overloaded, don't get a number.                                      */

#define BIN_TYPESET   0
#define BIN_BG        1
#define BIN_FG        2
#define BIN_JOBS      3
#define BIN_WAIT      4
#define BIN_DISOWN    5
#define BIN_BREAK     6
#define BIN_CONTINUE  7
#define BIN_EXIT      8
#define BIN_RETURN    9
#define BIN_CD       10
#define BIN_POPD     11
#define BIN_PUSHD    12
#define BIN_PRINT    13
#define BIN_EVAL     14
#define BIN_SCHED    15
#define BIN_FC       16
#define BIN_PUSHLINE 17
#define BIN_LOGOUT   18
#define BIN_TEST     19
#define BIN_BRACKET  20
#define BIN_EXPORT   21
#define BIN_ECHO     22
#define BIN_DISABLE  23
#define BIN_ENABLE   24

/* These currently depend on being 0 and 1. */
#define BIN_SETOPT    0
#define BIN_UNSETOPT  1

#define NULLBINCMD ((int (*) _((char *,char **,char *,int))) 0)
#define PREFIX(X,Y) {NULL,X,Y | BINF_PREFIX, NULLBINCMD, 0, 0, 0, NULL, NULL},

/* Nodes for builtin function hash table */

#ifdef GLOBALS
struct builtin builtins[] =
{
    PREFIX("-", BINF_DASH)
    PREFIX("builtin", BINF_BUILTIN)
    PREFIX("command", BINF_COMMAND)
    PREFIX("exec", BINF_EXEC)
    PREFIX("noglob", BINF_NOGLOB)
    {NULL, "[", 0, bin_test, 0, -1, BIN_BRACKET, NULL, NULL},
    {NULL, ".", BINF_PSPECIAL, bin_dot, 1, -1, 0, NULL, NULL},
    {NULL, ":", BINF_PSPECIAL, bin_true, 0, -1, 0, NULL, NULL},
    {NULL, "alias", BINF_MAGICEQUALS, bin_alias, 0, -1, 0, "Lgmr", NULL},
    {NULL, "autoload", BINF_TYPEOPTS, bin_functions, 0, -1, 0, "t", "u"},
    {NULL, "bg", 0, bin_fg, 0, -1, BIN_BG, NULL, NULL},
    {NULL, "break", BINF_PSPECIAL, bin_break, 0, 1, BIN_BREAK, NULL, NULL},
    {NULL, "bye", 0, bin_break, 0, 1, BIN_EXIT, NULL, NULL},
    {NULL, "cd", 0, bin_cd, 0, 2, BIN_CD, NULL, NULL},
    {NULL, "chdir", 0, bin_cd, 0, 2, BIN_CD, NULL, NULL},
    {NULL, "chr", 0, bin_chr, 1, 2, 0, "S:", NULL},
#ifdef COMPCTL
    {NULL, "compctl", 0, bin_compctl, 0, -1, 0, NULL, NULL},
#endif
    {NULL, "continue", BINF_PSPECIAL, bin_break, 0, 1, BIN_CONTINUE, NULL, NULL},
    {NULL, "declare", BINF_TYPEOPTS | BINF_MAGICEQUALS | BINF_PSPECIAL, bin_typeset, 0, -1, 0, "LRUZfilrtux", NULL},
    {NULL, "dirs", 0, bin_dirs, 0, -1, 0, "v", NULL},
    {NULL, "disable", 0, bin_enable, 0, -1, BIN_DISABLE, "afmr", NULL},
    {NULL, "disown", 0, bin_fg, 0, -1, BIN_DISOWN, NULL, NULL},
    {NULL, "echo", BINF_PRINTOPTS | BINF_ECHOPTS, bin_print, 0, -1, BIN_ECHO, "neEzZFf", "-"},
    {NULL, "emulate", 0, bin_emulate, 1, 1, 0, "R", NULL},
    {NULL, "enable", 0, bin_enable, 0, -1, BIN_ENABLE, "afmr", NULL},
    {NULL, "eval", BINF_PSPECIAL, bin_eval, 0, -1, BIN_EVAL, NULL, NULL},
    {NULL, "exit", BINF_PSPECIAL, bin_break, 0, 1, BIN_EXIT, NULL, NULL},
    {NULL, "export", BINF_TYPEOPTS | BINF_MAGICEQUALS | BINF_PSPECIAL, bin_typeset, 0, -1, BIN_EXPORT, "LRUZfilrtu", "x"},
    {NULL, "false", 0, bin_false, 0, -1, 0, NULL, NULL},
    {NULL, "fg", 0, bin_fg, 0, -1, BIN_FG, NULL, NULL},
    {NULL, "functions", BINF_TYPEOPTS, bin_functions, 0, -1, 0, "mtu", NULL},
    {NULL, "getln", 0, bin_read, 0, -1, 0, "ecntAlEzZN", "zr"},
    {NULL, "getopts", 0, bin_getopts, 2, -1, 0, NULL, NULL},
    {NULL, "getpid", 0, bin_getpid, 0, 1, 0, NULL, NULL},
    {NULL, "hash", BINF_MAGICEQUALS, bin_hash, 0, -1, 0, "dfmr", NULL},

#ifdef ZSH_HASH_DEBUG
    {NULL, "hashinfo", 0, bin_hashinfo, 0, 0, 0, NULL, NULL},
#endif

    {NULL, "integer", BINF_TYPEOPTS | BINF_MAGICEQUALS | BINF_PSPECIAL, bin_typeset, 0, -1, 0, "lrtux", "i"},
    {NULL, "jobs", 0, bin_fg, 0, -1, BIN_JOBS, "lpZrs", NULL},
    {NULL, "kill", 0, bin_kill, 0, -1, 0, NULL, NULL},
    {NULL, "let", 0, bin_let, 1, -1, 0, NULL, NULL},
    {NULL, "limit", 0, bin_limit, 0, -1, 0, "sh", NULL},
    {NULL, "local", BINF_TYPEOPTS | BINF_MAGICEQUALS | BINF_PSPECIAL, bin_typeset, 0, -1, 0, "LRUZilrtu", NULL},
    {NULL, "log", 0, bin_log, 0, 0, 0, NULL, NULL},
    {NULL, "logout", 0, bin_break, 0, 1, BIN_LOGOUT, NULL, NULL},

#if defined(ZSH_MEM) & defined(ZSH_MEM_DEBUG)
    {NULL, "mem", 0, bin_mem, 0, 0, 0, "v", NULL},
#endif

    {NULL, "popd", 0, bin_cd, 0, 2, BIN_POPD, NULL, NULL},
    {NULL, "print", BINF_PRINTOPTS, bin_print, 0, -1, BIN_PRINT, "RDPnrsFflzZNu0123456789pioOcm-", NULL},
    {NULL, "pushd", 0, bin_cd, 0, 2, BIN_PUSHD, NULL, NULL},
    {NULL, "pushln", BINF_PRINTOPTS, bin_print, 0, -1, BIN_PRINT, "zZ", "-nz"},
    {NULL, "pwd", 0, bin_pwd, 0, 0, 0, "r", NULL},
    {NULL, "read", 0, bin_read, 0, -1, 0, "rzZu0123456789pkqecntAlEN", NULL},
    {NULL, "readonly", BINF_TYPEOPTS | BINF_MAGICEQUALS | BINF_PSPECIAL, bin_typeset, 0, -1, 0, "LRUZfiltux", "r"},
    {NULL, "rehash", 0, bin_hash, 0, 0, 0, "df", "r"},
    {NULL, "return", BINF_PSPECIAL, bin_break, 0, 1, BIN_RETURN, NULL, NULL},
    {NULL, "sched", 0, bin_sched, 0, -1, 0, NULL, NULL},
    {NULL, "set", BINF_PSPECIAL, bin_set, 0, -1, 0, NULL, NULL},
    {NULL, "setopt", 0, bin_setopt, 0, -1, BIN_SETOPT, NULL, NULL},
    {NULL, "shift", BINF_PSPECIAL, bin_shift, 0, -1, 0, NULL, NULL},
    {NULL, "sleep", 0, bin_sleep, 1, 1, 0, NULL, NULL},
    {NULL, "source", BINF_PSPECIAL, bin_dot, 1, -1, 0, NULL, NULL},
    {NULL, "suspend", 0, bin_suspend, 0, 0, 0, "f", NULL},
    {NULL, "test", 0, bin_test, 0, -1, BIN_TEST, NULL, NULL},
    {NULL, "ttyctl", 0, bin_ttyctl, 0, 0, 0, "fu", NULL},
    {NULL, "times", BINF_PSPECIAL, bin_times, 0, 0, 0, NULL, NULL},
    {NULL, "trap", BINF_PSPECIAL, bin_trap, 0, -1, 0, NULL, NULL},
    {NULL, "true", 0, bin_true, 0, -1, 0, NULL, NULL},
    {NULL, "type", 0, bin_whence, 0, -1, 0, "ampf", "v"},
    {NULL, "typeset", BINF_TYPEOPTS | BINF_MAGICEQUALS | BINF_PSPECIAL, bin_typeset, 0, -1, 0, "LRUZfilrtuxm", NULL},
    {NULL, "ulimit", 0, bin_ulimit, 0, -1, 0, NULL, NULL},
    {NULL, "umask", 0, bin_umask, 0, 1, 0, "S", NULL},
    {NULL, "unalias", 0, bin_unhash, 1, -1, 0, "m", "a"},
    {NULL, "unfunction", 0, bin_unhash, 1, -1, 0, "m", "f"},
    {NULL, "unhash", 0, bin_unhash, 1, -1, 0, "adfm", NULL},
    {NULL, "unlimit", 0, bin_unlimit, 0, -1, 0, "hs", NULL},
    {NULL, "unset", BINF_PSPECIAL, bin_unset, 1, -1, 0, "fm", NULL},
    {NULL, "unsetopt", 0, bin_setopt, 0, -1, BIN_UNSETOPT, NULL, NULL},
    {NULL, "wait", 0, bin_fg, 0, -1, BIN_WAIT, NULL, NULL},
    {NULL, "whence", 0, bin_whence, 0, -1, 0, "acmpvf", NULL},
    {NULL, "where", 0, bin_whence, 0, -1, 0, "pm", "ca"},
    {NULL, "which", 0, bin_whence, 0, -1, 0, "amp", "c"},
// amacri: start of bsh integrations:
#ifdef BUSYBOX
    {NULL, "zcp", 0, bin_zcp, 0, -1, 0, "pdRfiar", NULL},
    {NULL, "bcat", 0, bin_bcat, 0, -1, 0, NULL, NULL},
    {NULL, "zmd", 0, bin_zmd, 0, -1, 0, "mp", NULL},
    {NULL, "zrd", 0, bin_zrd, 0, -1, 0, "p", NULL},
    {NULL, "zmv", 0, bin_zmv, 0, -1, 0, "fi", NULL},
    {NULL, "zln", 0, bin_zln, 0, -1, 0, "sfnbS", NULL},
    {NULL, "zrm", 0, bin_zrm, 0, -1, 0, "fiRr", NULL},
    {NULL, "ztouch", 0, bin_ztouch, 0, -1, 0, "cD", NULL},
#endif
    {NULL, "nice", 0, bin_nice, 0, 1, 0, NULL, NULL},
    {NULL, "distance", 0, bin_distance, 4, 5, 0, "S:", NULL},
    {NULL, "geoid", 0, bin_geoid, 2, 3, 0, "S:", NULL},
    {NULL, "zdate", 0, bin_zdate, 0, 3, 0, "S:", NULL},
    {NULL, "gpsdecoder", 0, bin_GpsDecoder, 1, 2, 0, "cmfFpVvsS:", NULL},
    {NULL, "dumps", 0, bin_dumps, 1, 2, 0, "mrpS:", NULL},
    {NULL, "SirfEnvelope", 0, bin_SirfEnvelope, 1, 2, 0, "rS:", NULL},
    {NULL, "zstat", 0, bin_zstat, 2, 3, 0, "S:", NULL},
    {NULL, "fb_dump", 0, bin_fb_dump, 0, 1, 0, "v", NULL},
    {NULL, "fb_init", 0, bin_fb_init, 0, 0, 0, NULL, NULL},
    {NULL, "fb_done", 0, bin_fb_done, 0, 0, 0, NULL, NULL},
    {NULL, "fb_rect", 0, bin_fb_rect, 5, 5, 0, NULL, NULL},
    {NULL, "fb_printxy", 0, bin_fb_printxy, 3, -1, 0, NULL, NULL},
    {NULL, "fb_print", 0, bin_fb_print, 0, -1, 0, NULL, NULL},
    {NULL, "ts_press", 0, bin_ts_press, 2, 2, 0, NULL, NULL},
    {NULL, "suna", 0, bin_suna, 5, 5, 0, "adl", NULL},
    {NULL, "NmeaChkSum", 0, bin_NmeaChkSum, 1, 2, 0, "dgS:", NULL},
    {NULL, "baud", 0, bin_baud, 2, 2, 0, NULL, NULL},
    {NULL, "alarm", 0, bin_alarm, 0, 2, 0, "k", NULL},
    {NULL, "watchf", 0, bin_watchf, 4, 4, 0, "kp", NULL},
    {NULL, "lease", 0, bin_lease, 2, 2, 0, "wrc", NULL},
    {NULL, "watchd", 0, bin_wd, 2, 2, 0, "ckp", NULL},
    {NULL, "tsbg", 0, bin_tsbg, 0, 5, 0, "ckpd", NULL},
    {NULL, "setn", 0, bin_setn, 0, -1, 0, "lntSsuacf", NULL},
    {NULL, "fc", BINF_FCOPTS, bin_fc, 0, -1, BIN_FC, "nlrdDfEikzZeN", NULL},
    {NULL, "boolean", 0, bin_boolean, 1, -1, 0, "ienNAd", NULL},
#ifdef CONFIG_NOTIFY
    {NULL, "dnotify", 0, bin_dnotify, 2, 2, 0, "AMCDRBacm", NULL},
    {NULL, "inotify", 0, bin_inotify, 2, 2, 0, "AMSDOWNFTBCMac", NULL},
#endif /* CONFIG_NOTIFY */
// amacri: end of bsh integrations
    {NULL, NULL}
};
#else
extern struct builtin builtins[];
#endif
