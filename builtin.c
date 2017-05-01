 /*
 * $Id: builtin.c,v 2.95 1996/10/16 22:47:53 hzoli Exp $
 *
 * builtin.c - builtin commands
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

/*
This special version of builtin.c is part of Event_Logger,
author Amacri - amacri@tiscali.it

Permission is hereby granted, without written agreement and without
license or royalty fees, to use, copy, modify, and distribute this
version for non commercial purpose, provided that the above notice
and the following four paragraphs appear in all copies.

In no event shall the Author Amacri be liable to any party for direct,
indirect, special, incidental, or consequential damages arising out of
the use of this software and its documentation,  even if Amacri has been
advised of the possibility of such damage.

The Author Amacri specifically disclaims any warranties, including, but
not limited to, the implied warranties of merchantability and fitness for
a particular purpose.  The software provided hereunder is on an "as is"
basis, and Amacri has no obligation to provide maintenance, support, updates,
enhancements, or modifications.

Event_Logger must not be distributed in ANY form that requires the recipient
to make a payment of any kind, except for donations to amacri in relation
to the implemented addenda. This restriction is not limited to monetary
payment - the restriction includes any form of payment. No permission
is implicitly provided to incorporate this software on CD/DVD or other
media for sale. E-commerce is explicitly disallowed.

Donations for addenda made by amacri in relation to Event_Logger development
are more than happily accepted! Event_Logger took a lot of development time,
as well as large number of testing and user support; if you really like this
software, consider sending donations; email to amacri@tiscali.it with subject
Donation to know how to perform this.
*/

/* Parameter management:
    int ret, val=0;
    char * inbuf=NULL;
    char **s;

ret=getiparam("A");
printf("A=%d\n",ret);
inbuf = getsparam("B");
printf("B=%s\n",inbuf);
unsetparam("C");
setiparam("D", 1);
setsparam("E", ztrdup("ciao"));
s = getaparam("F");
printf("F[1]=%s\n",*(s++));
printf("F[2]=%s\n",*(s++));
printf("F[3]=%s\n",*(s++));
printf("F[4]=%s\n",*(s++));

$ F=(uno due tre quattro cinque)
$ boolean
A=0
B=(null)
F[1]=uno
F[2]=due
F[3]=tre
F[4]=quattro
*/

#define _GNU_SOURCE     /* needed to get the defines */
#include "zsh.h"
#include <utime.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

static void printoptions _((int set));
int bin_watchf(char *name, char **argv, char *ops, int func);
void *thr_wd(void *threadarg);
void *thr_tsbg(void *threadarg);
void *thr_watchf(void *threadarg);
void hex_dump(char ** buf, int * bufsize, void *data, int size, int PrintAll);
double gpstime_to_unix(long week, double tow);
double wgs84_separation(double lat, double lon);
static double bilinear(double x1, double y1, double x2, double y2, double x, double y, double z11, double z12, double z21, double z22);

static char *auxdata;
static int auxlen;
extern long ForkCounter;
extern int Precision;

/* execute a builtin handler function after parsing the arguments */

#define MAX_OPS 128

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#define Degree_to_Radian	(M_PI / 180)

/**/
int
execbuiltin(LinkList args, Builtin bn)
{
    LinkNode n;
    char ops[MAX_OPS], *arg, *pp, *name, **argv, **oargv, *optstr;
    char *oxarg, *xarg = NULL;
    int flags, sense, argc = 0, execop;

    /* initialise some static variables */
    auxdata = NULL;
    auxlen = 0;

    /* initialize some local variables */
    memset(ops, 0, MAX_OPS);
    name = (char *) ugetnode(args);

    arg = (char *) ugetnode(args);

    /* get some information about the command */
    flags = bn->flags;
    optstr = bn->optstr;

    /* Sort out the options. */
    if ((flags & BINF_ECHOPTS) && isset(BSDECHO))
	ops['E'] = 1;
    if (optstr)
	/* while arguments look like options ... */
	while (arg &&
	       ((sense = (*arg == '-')) ||
		 ((flags & BINF_PLUSOPTS) && *arg == '+')) &&
	       ((flags & BINF_PLUSOPTS) || !atoi(arg))) {
	    /* unrecognised options to echo etc. are not really options */
	    if (flags & BINF_ECHOPTS) {
		char *p = arg;
		while (*++p && strchr(optstr, (int) *p));
		if (*p)
		    break;
	    }
	    /* save the options in xarg, for execution tracing */
	    if (xarg) {
		oxarg = tricat(xarg, " ", arg);
		zsfree(xarg);
		xarg = oxarg;
	    } else
		xarg = ztrdup(arg);
	    /* handle -- or - (ops['-']), and + (ops['-'] and ops['+']) */
	    if (arg[1] == '-')
		arg++;
	    if (!arg[1]) {
		ops['-'] = 1;
		if (!sense)
		    ops['+'] = 1;
	    }
	    /* save options in ops, as long as they are in bn->optstr */
	    execop = -1;
	    while (*++arg)
		if (strchr(optstr, execop = (int)*arg))
		    ops[(int)*arg] = (sense) ? 1 : 2;
		else
		    break;
	    /* "typeset" may take a numeric argument *
	     * at the tail of the options            */
	    if (idigit(*arg) && (flags & BINF_TYPEOPT) &&
		(arg[-1] == 'L' || arg[-1] == 'R' ||
		 arg[-1] == 'Z' || arg[-1] == 'i'))
		auxlen = (int)zstrtol(arg, &arg, 10);
	    /* The above loop may have exited on an invalid option.  (We  *
	     * assume that any option requiring metafication is invalid.) */
	    if (*arg) {
		if(*arg == Meta)
		    *++arg ^= 32;
		zerr("bad option: -%c", NULL, *arg);
		zsfree(xarg);
		return 1;
	    }
	    arg = (char *) ugetnode(args);
	    /* for the "print" builtin, the options after -R are treated as
	    options to "echo" */
	    if ((flags & BINF_PRINTOPTS) && ops['R']) {
		optstr = "ne";
		flags |= BINF_ECHOPTS;
	    }
	    /* the option -- indicates the end of the options */
	    if (ops['-'])
		break;
	    /* for "fc", -e takes an extra argument */
	    if ((flags & BINF_FCOPTS) && execop == 'e') {
		auxdata = arg;
		arg = (char *) ugetnode(args);
	    }
	    /* for "typeset", -L, -R, -Z and -i take a numeric extra argument */
	    if ((flags & BINF_TYPEOPT) && (execop == 'L' || execop == 'R' ||
		execop == 'Z' || execop == 'i') && arg && idigit(*arg)) {
		auxlen = atoi(arg);
		arg = (char *) ugetnode(args);
	    }
	}
    if (flags & BINF_R)
	auxdata = "-";
    /* handle built-in options, for overloaded handler functions */
    if ((pp = bn->defopts))
	while (*pp)
	    ops[(int)*pp++] = 1;

    /* Set up the argument list. */
    if (arg) {
	/* count the arguments */
	argc = 1;
	n = firstnode(args);
	while (n)
	    argc++, incnode(n);
    }
    /* Get the actual arguments, into argv.  Oargv saves the *
     * beginning of the array for later reference.           */
    oargv = argv = (char **)ncalloc(sizeof(char **) * (argc + 1));
    if ((*argv++ = arg))
	while ((*argv++ = (char *)ugetnode(args)));
    argv = oargv;
    if (errflag) {
	zsfree(xarg);
	errflag = 0;
	return 1;
    }

    /* check that the argument count lies within the specified bounds */
    if (argc < bn->minargs || (argc > bn->maxargs && bn->maxargs != -1)) {
	zwarnnam(name, (argc < bn->minargs)
		? "not enough arguments" : "too many arguments", NULL, 0);
	zsfree(xarg);
	return 1;
    }

    /* display execution trace information, if required */
    if ((isset(XTRACE)) && isset(TRACE))
       if (!( (name) && (*oargv) && (!strcmp(name,"setopt")) && (strstr(*oargv,"trace")) )) {
#ifdef ORIGPROMPT
	fprintf(stderr, "%s%s", (prompt4) ? prompt4 : "", name);
#else
//export PS4=" %k:%M:%S+ "
if (prompt4)
{
struct tm *tm;
char bp[80];
time_t timet = time(NULL);
tm = localtime(&timet);
ztrftime(bp, 79, prompt4, tm);
fprintf(stderr, "%s%ld, %ld, %ld, %s: %s", bp, lineno, (long)lastval, ForkCounter, isset(SHINSTDIN) ? "zsh" : scriptname ? scriptname : argzero, name);
}
#endif
	if (xarg)
	    fprintf(stderr, " %s", xarg);
	while (*oargv)
	    fprintf(stderr, " %s", *oargv++);
	fputc('\n', stderr);
	fflush(stderr);
    }
if ( (name) && (!strcmp(name,"Setopt")) )
   name[0]='s';

    zsfree(xarg);
    /* call the handler function, and return its return value */
    return (*(bn->handlerfunc)) (name, argv, ops, bn->funcid);
}

/* Enable/disable an element in one of the internal hash tables.  *
 * With no arguments, it lists all the currently enabled/disabled *
 * elements in that particular hash table.                        */

/**/
int
bin_enable(char *name, char **argv, char *ops, int func)
{
    HashTable ht;
    HashNode hn;
    ScanFunc scanfunc;
    Comp com;
    int flags1 = 0, flags2 = 0;
    int match = 0, returnval = 0;

    /* Find out which hash table we are working with. */
    if (ops['f'])
	ht = shfunctab;
    else if (ops['r'])
	ht = reswdtab;
    else if (ops['a'])
	ht = aliastab;
    else
	ht = builtintab;

    /* Do we want to enable or disable? */
    if (func == BIN_ENABLE) {
	flags2 = DISABLED;
	scanfunc = ht->enablenode;
    } else {
	flags1 = DISABLED;
	scanfunc = ht->disablenode;
    }

    /* Given no arguments, print the names of the enabled/disabled elements  *
     * in this hash table.  If func == BIN_ENABLE, then scanhashtable will   *
     * print nodes NOT containing the DISABLED flag, else scanhashtable will *
     * print nodes containing the DISABLED flag.                             */
    if (!*argv) {
	scanhashtable(ht, 1, flags1, flags2, ht->printnode, 0);
	return 0;
    }

    /* With -m option, treat arguments as glob patterns. */
    if (ops['m']) {
	for (; *argv; argv++) {
	    /* parse pattern */
	    tokenize(*argv);
	    if ((com = parsereg(*argv)))
		match += scanmatchtable(ht, com, 0, 0, scanfunc, 0);
	    else {
		untokenize(*argv);
		zwarnnam(name, "bad pattern : %s", *argv, 0);
		returnval = 1;
	    }
	}
	/* If we didn't match anything, we return 1. */
	if (!match)
	    returnval = 1;
	return returnval;
    }

    /* Take arguments literally -- do not glob */
    for (; *argv; argv++) {
	    if ((hn = ht->getnode2(ht, *argv))) {
		scanfunc(hn, 0);
	    } else {
		zwarnnam(name, "no such hash table element: %s", *argv, 0);
		returnval = 1;
	    }
	}
    return returnval;
}

/* set: either set the shell options, or set the shell arguments, *
 * or declare an array, or show various things                    */

/**/
int
bin_set(char *nam, char **args, char *ops, int func)
{
    int action, optno, array = 0, hadopt = 0,
	hadplus = 0, hadend = 0, sort = 0;
    char **x;

    /* Obsolecent sh compatibility: set - is the same as set +xv *
     * and set - args is the same as set +xv -- args             */
    if (*args && **args == '-' && !args[0][1]) {
	dosetopt(VERBOSE, 0, 0);
	dosetopt(XTRACE, 0, 0);
	if (!args[1])
	    return 0;
    }

    /* loop through command line options (begins with "-" or "+") */
    while (*args && (**args == '-' || **args == '+')) {
	action = (**args == '-');
	hadplus |= !action;
	if(!args[0][1])
	    *args = "--";
	while (*++*args) {
	    if(**args == Meta)
		*++*args ^= 32;
	    if(**args != '-' || action)
		hadopt = 1;
	    /* The pseudo-option `--' signifies the end of options. */
	    if (**args == '-') {
		hadend = 1;
		args++;
		goto doneoptions;
	    } else if (**args == 'o') {
		if (!*++*args)
		    args++;
		if (!*args) {
		    zwarnnam(nam, "string expected after -o", NULL, 0);
		    inittyptab();
		    return 1;
		}
		if(!(optno = optlookup(*args)))
		    zwarnnam(nam, "no such option: %s", *args, 0);
		else if(dosetopt(optno, action, 0))
		    zwarnnam(nam, "can't change option: %s", *args, 0);
		break;
	    } else if(**args == 'A') {
		if(!*++*args)
		    args++;
		array = action ? 1 : -1;
		goto doneoptions;
	    } else if (**args == 's')
		sort = action ? 1 : -1;
	    else {
	    	if (!(optno = optlookupc(**args)))
		    zwarnnam(nam, "bad option: -%c", NULL, **args);
		else if(dosetopt(optno, action, 0))
		    zwarnnam(nam, "can't change option: -%c", NULL, **args);
	    }
	}
	args++;
    }
    doneoptions:
    inittyptab();

    /* Show the parameters, possibly with values */
    if (!hadopt && !*args)
	scanhashtable(paramtab, 1, 0, 0, paramtab->printnode,
	    hadplus ? PRINT_NAMEONLY : 0);

    if (array && !*args) {
	/* display arrays */
	scanhashtable(paramtab, 1, PM_ARRAY, 0, paramtab->printnode,
	    hadplus ? PRINT_NAMEONLY : 0);
    }
    if (!*args && !hadend)
	return 0;
    if (array)
	args++;
    if (sort)
	qsort(args, arrlen(args), sizeof(char *),
	      sort > 0 ? strpcmp : invstrpcmp);
    if (array) {
	/* create an array with the specified elements */
	char **a = NULL, **y, *name = args[-1];
	int len = arrlen(args);

	if (array < 0 && (a = getaparam(name))) {
	    int al = arrlen(a);

	    if (al > len)
		len = al;
	}
	for (x = y = zalloc((len + 1) * sizeof(char *)); len--; a++) {
	    if (!*args)
		args = a;
	    *y++ = ztrdup(*args++);
	}
	*y++ = NULL;
	setaparam(name, x);
    } else {
	/* set shell arguments */
	freearray(pparams);
	PERMALLOC {
	    pparams = arrdup(args);
	} LASTALLOC;
    }
    return 0;
}

/* setopt, unsetopt */

/**/
int
bin_setopt(char *nam, char **args, char *ops, int isun)
{
    int action, optno, match = 0;

    /* With no arguments or options, display options.  The output *
     * format is determined by the printoptions function (below). */
    if (!*args) {
	printoptions(!isun);
	return 0;
    }

    /* loop through command line options (begins with "-" or "+") */
    while (*args && (**args == '-' || **args == '+')) {
	action = (**args == '-') ^ isun;
	if(!args[0][1])
	    *args = "--";
	while (*++*args) {
	    if(**args == Meta)
		*++*args ^= 32;
	    /* The pseudo-option `--' signifies the end of options. */
	    if (**args == '-') {
		args++;
		goto doneoptions;
	    } else if (**args == 'o') {
		if (!*++*args)
		    args++;
		if (!*args) {
		    zwarnnam(nam, "string expected after -o", NULL, 0);
		    inittyptab();
		    return 1;
		}
		if(!(optno = optlookup(*args)))
		    zwarnnam(nam, "no such option: %s", *args, 0);
		else if(dosetopt(optno, action, 0))
		    zwarnnam(nam, "can't change option: %s", *args, 0);
		break;
	    } else if(**args == 'm') {
		match = 1;
	    } else {
	    	if (!(optno = optlookupc(**args)))
		    zwarnnam(nam, "bad option: -%c", NULL, **args);
		else if(dosetopt(optno, action, 0))
		    zwarnnam(nam, "can't change option: -%c", NULL, **args);
	    }
	}
	args++;
    }
    doneoptions:

    if (!match) {
	/* Not globbing the arguments -- arguments are simply option names. */
	while (*args) {
	    if(!(optno = optlookup(*args++)))
		zwarnnam(nam, "no such option: %s", args[-1], 0);
	    else if(dosetopt(optno, !isun, 0))
		zwarnnam(nam, "can't change option: %s", args[-1], 0);
	}
    } else {
	/* Globbing option (-m) set. */
	while (*args) {
	    Comp com;

	    /* Expand the current arg. */
	    tokenize(*args);
	    if (!(com = parsereg(*args))) {
		untokenize(*args);
		zwarnnam(nam, "bad pattern: %s", *args, 0);
		continue;
	    }
	    /* Loop over expansions. */
	    for(optno = OPT_SIZE; --optno; )
		if (optno != PRIVILEGED && domatch(optns[optno].name, com, 0))
	    	    dosetopt(optno, !isun, 0);
	    args++;
	}
    }
    inittyptab();
    return 0;
}

/* print options */

static void
printoptions(int set)
{
    int optno;

    if (isset(KSHOPTIONPRINT)) {
	/* ksh-style option display -- list all options, *
	 * with an indication of current status          */
	printf("Current option settings\n");
	for(optno = 1; optno < OPT_SIZE; optno++) {
	    if (defset(optno))
		printf("no%-20s%s\n", optns[optno].name, isset(optno) ? "off" : "on");
	    else
		printf("%-22s%s\n", optns[optno].name, isset(optno) ? "on" : "off");
	}
    } else {
	/* list all options that are on, or all that are off */
	for(optno = 1; optno < OPT_SIZE; optno++) {
	    if (set == (isset(optno) ^ defset(optno))) {
		if (set ^ isset(optno))
		    fputs("no", stdout);
		puts(optns[optno].name);
	    }
	}
    }
}

/**** job control builtins ****/

/* Make sure we have a suitable current and previous job set. */

static void
setcurjob(void)
{
    if (curjob == thisjob ||
	(curjob != -1 && !(jobtab[curjob].stat & STAT_INUSE))) {
	curjob = prevjob;
	setprevjob();
	if (curjob == thisjob ||
	    (curjob != -1 && !((jobtab[curjob].stat & STAT_INUSE) &&
			       curjob != thisjob))) {
	    curjob = prevjob;
	    setprevjob();
	}
    }
}

/* bg, disown, fg, jobs, wait: most of the job control commands are     *
 * here.  They all take the same type of argument.  Exception: wait can *
 * take a pid or a job specifier, whereas the others only work on jobs. */

/**/
int
bin_fg(char *name, char **argv, char *ops, int func)
{
    int job, lng, firstjob = -1, retval = 0;

    if (ops['Z']) {
	if (*argv)
	    strcpy(hackzero, *argv);
	return 0;
    }

    lng = (ops['l']) ? 1 : (ops['p']) ? 2 : 0;
    if ((func == BIN_FG || func == BIN_BG) && !jobbing) {
	/* oops... maybe bg and fg should have been disabled? */
	zwarnnam(name, "no job control in this shell.", NULL, 0);
	return 1;
    }

    /* If necessary, update job table. */
    if (unset(NOTIFY))
	scanjobs();

    setcurjob();

    if (func == BIN_JOBS)
        /* If you immediately type "exit" after "jobs", this      *
         * will prevent zexit from complaining about stopped jobs */
	stopmsg = 2;
    if (!*argv) {
	/* This block handles all of the default cases (no arguments).  bg,
	fg and disown act on the current job, and jobs and wait act on all the
	jobs. */
 	if (func == BIN_FG || func == BIN_BG || func == BIN_DISOWN) {
	    /* W.r.t. the above comment, we'd better have a current job at this
	    point or else. */
	    if (curjob == -1 || (jobtab[curjob].stat & STAT_NOPRINT)) {
		zwarnnam(name, "no current job", NULL, 0);
		return 1;
	    }
	    firstjob = curjob;
	} else if (func == BIN_JOBS) {
	    /* List jobs. */
	    for (job = 0; job != MAXJOB; job++)
		if (job != thisjob && jobtab[job].stat) {
		    if ((!ops['r'] && !ops['s']) ||
			(ops['r'] && ops['s']) ||
			(ops['r'] && !(jobtab[job].stat & STAT_STOPPED)) ||
			(ops['s'] && jobtab[job].stat & STAT_STOPPED))
			printjob(job + jobtab, lng, 2);
		}
	    return 0;
	} else {   /* Must be BIN_WAIT, so wait for all jobs */
	    for (job = 0; job != MAXJOB; job++)
		if (job != thisjob && jobtab[job].stat)
		    waitjob(job, SIGINT);
	    return 0;
	}
    }

    /* Defaults have been handled.  We now have an argument or two, or three...
    In the default case for bg, fg and disown, the argument will be provided by
    the above routine.  We now loop over the arguments. */
    for (; (firstjob != -1) || *argv; (void)(*argv && argv++)) {
	int stopped, ocj = thisjob;

	if (func == BIN_WAIT && isanum(*argv)) {
	    /* wait can take a pid; the others can't. */
	    waitforpid((long)atoi(*argv));
	    retval = lastval2;
	    thisjob = ocj;
	    continue;
	}
	/* The only type of argument allowed now is a job spec.  Check it. */
	job = (*argv) ? getjob(*argv, name) : firstjob;
	firstjob = -1;
	if (job == -1) {
	    retval = 1;
	    break;
	}
	if (!(jobtab[job].stat & STAT_INUSE) ||
	    (jobtab[job].stat & STAT_NOPRINT)) {
	    zwarnnam(name, "no such job: %d", 0, job);
	    return 1;
	}
	/* We have a job number.  Now decide what to do with it. */
	switch (func) {
	case BIN_FG:
	case BIN_BG:
	case BIN_WAIT:
	    if ((stopped = (jobtab[job].stat & STAT_STOPPED)))
		makerunning(jobtab + job);
	    else if (func == BIN_BG) {
		/* Silly to bg a job already running. */
		zwarnnam(name, "job already in background", NULL, 0);
		thisjob = ocj;
		return 1;
	    }
	    /* It's time to shuffle the jobs around!  Reset the current job,
	    and pick a sensible secondary job. */
	    if (curjob == job) {
		curjob = prevjob;
		prevjob = (func == BIN_BG) ? -1 : job;
	    }
	    if (prevjob == job || prevjob == -1)
		setprevjob();
	    if (curjob == -1) {
		curjob = prevjob;
		setprevjob();
	    }
	    if (func != BIN_WAIT)
		/* for bg and fg -- show the job we are operating on */
		printjob(jobtab + job, (stopped) ? -1 : 0, 1);
	    if (func == BIN_BG)
		jobtab[job].stat |= STAT_NOSTTY;
	    else {				/* fg or wait */
		if (strcmp(jobtab[job].pwd, pwd)) {
		    fprintf(shout, "(pwd : ");
		    fprintdir(jobtab[job].pwd, shout);
		    fprintf(shout, ")\n");
		}
		fflush(shout);
		if (func != BIN_WAIT) {		/* fg */
		    thisjob = job;
		    attachtty(jobtab[job].gleader);
		}
	    }
	    if (stopped) {
		if (func != BIN_BG && jobtab[job].ty)
		    settyinfo(jobtab[job].ty);
		killjb(jobtab + job, SIGCONT);
	    }
	    if (func == BIN_WAIT)
	        waitjob(job, SIGINT);
	    if (func != BIN_BG) {
		waitjobs();
		retval = lastval2;
	    }
	    break;
	case BIN_JOBS:
	    printjob(job + jobtab, lng, 2);
	    break;
	case BIN_DISOWN:
	    {
		static struct job zero;

		jobtab[job] = zero;
		break;
	    }
	}
	thisjob = ocj;
    }
    return retval;
}

/* kill: send a signal to a process.  The process(es) may be specified *
 * by job specifier (see above) or pid.  A signal, defaulting to       *
 * SIGTERM, may be specified by name or number, preceded by a dash.    */

/**/
int
bin_kill(char *nam, char **argv, char *ops, int func)
{
    int sig = SIGTERM;
    int returnval = 0;

    /* check for, and interpret, a signal specifier */
    if (*argv && **argv == '-') {
	if (idigit((*argv)[1]))
	    /* signal specified by number */
	    sig = atoi(*argv + 1);
	else if ((*argv)[1] != '-' || (*argv)[2]) {
	    char *signame;

	    /* with argument "-l" display the list of signal names */
	    if ((*argv)[1] == 'l' && (*argv)[2] == '\0') {
		if (argv[1]) {
		    while (*++argv) {
			sig = zstrtol(*argv, &signame, 10);
			if (signame == *argv) {
			    for (sig = 1; sig <= SIGCOUNT; sig++)
				if (!cstrpcmp(sigs + sig, &signame))
				    break;
			    if (sig > SIGCOUNT) {
				zwarnnam(nam, "unknown signal: SIG%s",
					 signame, 0);
				returnval++;
			    } else
				printf("%d\n", sig);
			} else {
			    if (*signame) {
				zwarnnam(nam, "unknown signal: SIG%s",
					 signame, 0);
				returnval++;
			    } else {
				if (WIFSIGNALED(sig))
				    sig = WTERMSIG(sig);
				else if (WIFSTOPPED(sig))
				    sig = WSTOPSIG(sig);
				if (1 <= sig && sig <= SIGCOUNT)
				    printf("%s\n", sigs[sig]);
				else
				    printf("%d\n", sig);
			    }
			}
		    }
		    return returnval;
		}
		printf("%s", sigs[1]);
		for (sig = 2; sig <= SIGCOUNT; sig++)
		    printf(" %s", sigs[sig]);
		putchar('\n');
		return 0;
	    }
	    if ((*argv)[1] == 's' && (*argv)[2] == '\0')
		signame = *++argv;
	    else
		signame = *argv + 1;

	    /* check for signal matching specified name */
	    for (sig = 1; sig <= SIGCOUNT; sig++)
		if (!cstrpcmp(sigs + sig, &signame))
		    break;
	    if (*signame == '0' && !signame[1])
		sig = 0;
	    if (sig > SIGCOUNT) {
		zwarnnam(nam, "unknown signal: SIG%s", signame, 0);
		zwarnnam(nam, "type kill -l for a List of signals", NULL, 0);
		return 1;
	    }
	}
	argv++;
    }

    setcurjob();

    /* Remaining arguments specify processes.  Loop over them, and send the
    signal (number sig) to each process. */
    for (; *argv; argv++) {
	if (**argv == '%') {
	    /* job specifier introduced by '%' */
	    int p;

	    if ((p = getjob(*argv, nam)) == -1) {
		returnval++;
		continue;
	    }
	    if (killjb(jobtab + p, sig) == -1) {
		zwarnnam("kill", "kill %s failed: %e", *argv, errno);
		returnval++;
		continue;
	    }
	    /* automatically update the job table if sending a SIGCONT to a
	    job, and send the job a SIGCONT if sending it a non-stopping
	    signal. */
	    if (jobtab[p].stat & STAT_STOPPED) {
		if (sig == SIGCONT)
		    jobtab[p].stat &= ~STAT_STOPPED;
		if (sig != SIGKILL && sig != SIGCONT && sig != SIGTSTP
		    && sig != SIGTTOU && sig != SIGTTIN && sig != SIGSTOP)
		    killjb(jobtab + p, SIGCONT);
	    }
	} else if (!isanum(*argv)) {
	    zwarnnam("kill", "illegal pid: %s", *argv, 0);
	    returnval++;
	} else if (kill(atoi(*argv), sig) == -1) {
	    zwarnnam("kill", "kill %s failed: %e", *argv, errno);
	    returnval++;
	}
    }
    return returnval < 126 ? returnval : 1;
}

/* Suspend this shell */

/**/
int
bin_suspend(char *name, char **argv, char *ops, int func)
{
    /* won't suspend a login shell, unless forced */
    if (islogin && !ops['f']) {
	zwarnnam(name, "can't suspend login shell", NULL, 0);
	return 1;
    }
    if (jobbing) {
	/* stop ignoring signals */
	signal_default(SIGTTIN);
	signal_default(SIGTSTP);
	signal_default(SIGTTOU);
    }
    /* suspend ourselves with a SIGTSTP */
    kill(0, SIGTSTP);
    if (jobbing) {
	/* stay suspended */
	while (gettygrp() != mypgrp) {
	    sleep(1);
	    if (gettygrp() != mypgrp)
		kill(0, SIGTTIN);
	}
	/* restore signal handling */
	signal_ignore(SIGTTOU);
	signal_ignore(SIGTSTP);
	signal_ignore(SIGTTIN);
    }
    return 0;
}

/* find a job named s */

/**/
int
findjobnam(char *s)
{
    int jobnum;

    for (jobnum = MAXJOB - 1; jobnum >= 0; jobnum--)
	if (!(jobtab[jobnum].stat & (STAT_SUBJOB | STAT_NOPRINT)) &&
	    jobtab[jobnum].stat && jobtab[jobnum].procs && jobnum != thisjob &&
	    jobtab[jobnum].procs->text)
	    return jobnum;
    return -1;
}

/* Convert a job specifier ("%%", "%1", "%foo", "%?bar?", etc.) *
 * to a job number.                                             */

/**/
int
getjob(char *s, char *prog)
{
    int jobnum, returnval;

    /* if there is no %, treat as a name */
    if (*s != '%')
	goto jump;
    s++;
    /* "%%", "%+" and "%" all represent the current job */
    if (*s == '%' || *s == '+' || !*s) {
	if (curjob == -1) {
	    zwarnnam(prog, "no current job", NULL, 0);
	    returnval = -1;
	    goto done;
	}
	returnval = curjob;
	goto done;
    }
    /* "%-" represents the previous job */
    if (*s == '-') {
	if (prevjob == -1) {
	    zwarnnam(prog, "no previous job", NULL, 0);
	    returnval = -1;
	    goto done;
	}
	returnval = prevjob;
	goto done;
    }
    /* a digit here means we have a job number */
    if (idigit(*s)) {
	jobnum = atoi(s);
	if (jobnum && jobnum < MAXJOB && jobtab[jobnum].stat &&
	    !(jobtab[jobnum].stat & STAT_SUBJOB) && jobnum != thisjob) {
	    returnval = jobnum;
	    goto done;
	}
	zwarnnam(prog, "%%%s: no such job", s, 0);
	returnval = -1;
	goto done;
    }
    /* "%?" introduces a search string */
    if (*s == '?') {
	struct process *pn;

	for (jobnum = MAXJOB - 1; jobnum >= 0; jobnum--)
	    if (jobtab[jobnum].stat && !(jobtab[jobnum].stat & STAT_SUBJOB) &&
		jobnum != thisjob)
		for (pn = jobtab[jobnum].procs; pn; pn = pn->next)
		    if (strstr(pn->text, s + 1)) {
			returnval = jobnum;
			goto done;
		    }
	zwarnnam(prog, "job not found: %s", s, 0);
	returnval = -1;
	goto done;
    }
  jump:
    /* anything else is a job name, specified as a string that begins the
    job's command */
    if ((jobnum = findjobnam(s)) != -1) {
	returnval = jobnum;
	goto done;
    }
    /* if we get here, it is because none of the above succeeded and went
    to done */
    zwarnnam(prog, "job not found: %s", s, 0);
    returnval = -1;
  done:
    return returnval;
}

/* This simple function indicates whether or not s may represent      *
 * a number.  It returns true iff s consists purely of digits and     *
 * minuses.  Note that minus may appear more than once, and the empty *
 * string will produce a `true' response.                             */

/**/
int
isanum(char *s)
{
    while (*s == '-' || idigit(*s))
	s++;
    return *s == '\0';
}

/**** directory-handling builtins ****/

int doprintdir = 0;		/* set in exec.c (for autocd) */

/* pwd: display the name of the current directory */

/**/
int
bin_pwd(char *name, char **argv, char *ops, int func)
{
    if (ops['r'] || isset(CHASELINKS))
	printf("%s\n", zgetcwd());
    else {
	zputs(pwd, stdout);
	putchar('\n');
    }
    return 0;
}

/* dirs: list the directory stack, or replace it with a provided list */

/**/
int
bin_dirs(char *name, char **argv, char *ops, int func)
{
    LinkList l;

    /* with the -v option, provide a numbered list of directories, starting at
    zero */
    if (ops['v']) {
	LinkNode node;
	int pos = 1;

	printf("0\t");
	fprintdir(pwd, stdout);
	for (node = firstnode(dirstack); node; incnode(node)) {
	    printf("\n%d\t", pos++);
	    fprintdir(getdata(node), stdout);
	}
	putchar('\n');
	return 0;
    }
    /* given no arguments, list the stack normally */
    if (!*argv) {
	printdirstack();
	return 0;
    }
    /* replace the stack with the specified directories */
    PERMALLOC {
	l = newlinklist();
	if (*argv) {
	    while (*argv)
		addlinknode(l, ztrdup(*argv++));
	    freelinklist(dirstack, freestr);
	    dirstack = l;
	}
    } LASTALLOC;
    return 0;
}

/* cd, chdir, pushd, popd */

/* The main pwd changing function.  The real work is done by other     *
 * functions.  cd_get_dest() does the initial argument processing;     *
 * cd_do_chdir() actually changes directory, if possible; cd_new_pwd() *
 * does the ancilliary processing associated with actually changing    *
 * directory.                                                          */

/**/
int
bin_cd(char *nam, char **argv, char *ops, int func)
{
    LinkNode dir;
    struct stat st1, st2;

    doprintdir = (doprintdir == -1);

    PERMALLOC {
	pushnode(dirstack, ztrdup(pwd));
	if (!(dir = cd_get_dest(nam, argv, ops, func))) {
	    zsfree(getlinknode(dirstack));
	    LASTALLOC_RETURN 1;
	}
    } LASTALLOC;
    cd_new_pwd(func, dir);

    if (stat(unmeta(pwd), &st1) < 0) {
	zsfree(pwd);
	pwd = metafy(zgetcwd(), -1, META_REALLOC);
    } else if (stat(".", &st2) < 0)
	chdir(unmeta(pwd));
    else if (st1.st_ino != st2.st_ino || st1.st_dev != st2.st_dev) {
	if (isset(CHASELINKS)) {
	    zsfree(pwd);
	    pwd = metafy(zgetcwd(), -1, META_REALLOC);
	} else {
	    chdir(unmeta(pwd));
	}
    }
    return 0;
}

/* Get directory to chdir to */

/**/
LinkNode
cd_get_dest(char *nam, char **argv, char *ops, int func)
{
    LinkNode dir = NULL;
    LinkNode target;
    char *dest;

    if (!argv[0]) {
	if (func == BIN_POPD && !nextnode(firstnode(dirstack))) {
	    zwarnnam(nam, "directory stack empty", NULL, 0);
	    return NULL;
	}
	if (func == BIN_PUSHD && unset(PUSHDTOHOME))
	    dir = nextnode(firstnode(dirstack));
	if (dir)
	    insertlinknode(dirstack, dir, getlinknode(dirstack));
	else if (func != BIN_POPD)
	    pushnode(dirstack, ztrdup(home));
    } else if (!argv[1]) {
	int dd;
	char *end;

	doprintdir++;
	if (argv[0][1] && (argv[0][0] == '+' || argv[0][0] == '-')) {
	    dd = zstrtol(argv[0] + 1, &end, 10); 
	    if (*end == '\0') {
		if ((argv[0][0] == '+') ^ isset(PUSHDMINUS))
		    for (dir = firstnode(dirstack); dir && dd; dd--, incnode(dir));
		else
		    for (dir = lastnode(dirstack); dir != (LinkNode) dirstack && dd;
			 dd--, dir = prevnode(dir)); 
		if (!dir || dir == (LinkNode) dirstack) {
		    zwarnnam(nam, "no such entry in dir stack", NULL, 0);
		    return NULL;
		}
	    }
	}
	if (!dir)
	    pushnode(dirstack, ztrdup(strcmp(argv[0], "-")
				      ? (doprintdir--, argv[0]) : oldpwd));
    } else {
	char *u, *d;
	int len1, len2, len3;

	if (!(u = strstr(pwd, argv[0]))) {
	    zwarnnam(nam, "string not in pwd: %s", argv[0], 0);
	    return NULL;
	}
	len1 = strlen(argv[0]);
	len2 = strlen(argv[1]);
	len3 = u - pwd;
	d = (char *)zalloc(len3 + len2 + strlen(u + len1) + 1);
	strncpy(d, pwd, len3);
	strcpy(d + len3, argv[1]);
	strcat(d, u + len1);
	pushnode(dirstack, d);
	doprintdir++;
    }

    target = dir;
    if (func == BIN_POPD) {
	if (!dir) {
	    target = dir = firstnode(dirstack);
	} else if (dir != firstnode(dirstack)) {
	    return dir;
	}
	dir = nextnode(dir);
    }
    if (!dir) {
	dir = firstnode(dirstack);
    }
    if (!(dest = cd_do_chdir(nam, getdata(dir)))) {
	if (!target)
	    zsfree(getlinknode(dirstack));
	if (func == BIN_POPD)
	    zsfree(remnode(dirstack, dir));
	return NULL;
    }
    if (dest != getdata(dir)) {
	zsfree(getdata(dir));
	setdata(dir, dest);
    }
    return target ? target : dir;
}

/* Change to given directory, if possible.  This function works out  *
 * exactly how the directory should be interpreted, including cdpath *
 * and CDABLEVARS.  For each possible interpretation of the given    *
 * path, this calls cd_try_chdir(), which attempts to chdir to that  *
 * particular path.                                                  */

/**/
char *
cd_do_chdir(char *cnam, char *dest)
{
    char **pp, *ret;
    int hasdot = 0, eno = ENOENT;
    /* nocdpath indicates that cdpath should not be used.  This is the case iff
    dest is a relative path whose first segment is . or .., but if the path is
    absolute then cdpath won't be used anyway. */
    int nocdpath = dest[0] == '.' &&
    (dest[1] == '/' || !dest[1] || (dest[1] == '.' &&
				    (dest[2] == '/' || !dest[2])));

    /* if we have an absolute path, use it as-is only */
    if (*dest == '/') {
	if ((ret = cd_try_chdir(NULL, dest)))
	    return ret;
	zwarnnam(cnam, "%e: %s", dest, errno);
	return NULL;
    }

    /* if cdpath is being used, check it for . */
    if (!nocdpath)
	for (pp = cdpath; *pp; pp++)
	    if (!(*pp)[0] || ((*pp)[0] == '.' && (*pp)[1] == '\0'))
		hasdot = 1;
    /* if there is no . in cdpath (or it is not being used), try the directory
    as-is (i.e. from .) */
    if (!hasdot) {
	if ((ret = cd_try_chdir(NULL, dest)))
	    return ret;
	if (errno != ENOENT)
	    eno = errno;
    }
    /* if cdpath is being used, try given directory relative to each element in
    cdpath in turn */
    if (!nocdpath)
	for (pp = cdpath; *pp; pp++) {
	    if ((ret = cd_try_chdir(*pp, dest))) {
		if (strcmp(*pp, ".")) {
		    doprintdir++;
		}
		return ret;
	    }
	    if (errno != ENOENT)
		eno = errno;
	}

    /* handle the CDABLEVARS option */
    if ((ret = cd_able_vars(dest))) {
	if ((ret = cd_try_chdir(NULL, ret))) {
	    doprintdir++;
	    return ret;
	}
	if (errno != ENOENT)
	    eno = errno;
    }

    /* If we got here, it means that we couldn't chdir to any of the
    multitudinous possible paths allowed by zsh.  We've run out of options!
    Add more here! */
    zwarnnam(cnam, "%e: %s", dest, eno);
    return NULL;
}

/* If the CDABLEVARS option is set, return the new *
 * interpretation of the given path.               */

/**/
char *
cd_able_vars(char *s)
{
    char *rest, save;

    if (isset(CDABLEVARS)) {
	for (rest = s; *rest && *rest != '/'; rest++);
	save = *rest;
	*rest = 0;
	s = getnameddir(s);
	*rest = save;

	if (s && *rest)
	    s = dyncat(s, rest);

	return s;
    }
    return NULL;
}

/* Attempt to change to a single given directory.  The directory,    *
 * for the convenience of the calling function, may be provided in   *
 * two parts, which must be concatenated before attempting to chdir. *
 * Returns NULL if the chdir fails.  If the directory change is      *
 * possible, it is performed, and a pointer to the new full pathname *
 * is returned.                                                      */

/**/
char *
cd_try_chdir(char *pfix, char *dest)
{
    char buf[PATH_MAX], buf2[PATH_MAX];
    char *s;
    int dotsct;

    /* handle directory prefix */
    if (pfix && *pfix) {
	if (strlen(dest) + strlen(pfix) + 1 >= PATH_MAX)
	    return NULL;
	sprintf(buf, "%s/%s", (!strcmp("/", pfix)) ? "" : pfix, dest);
    } else {
	if (strlen(dest) >= PATH_MAX)
	    return NULL;
	strcpy(buf, dest);
    }
    /* Normalise path.  See the definition of fixdir() for what this means. */
    dotsct = fixdir(buf2, buf);

    /* if the path is absolute, the test and return value are (relatively)
    simple */
    if (buf2[0] == '/')
	return (chdir(unmeta(buf)) == -1) ? NULL : ztrdup(buf2);
    /* If the path is a simple `downward' relative path, the test is again
    fairly simple.  The relative path must be added to the end of the current
    directory. */
    if (!dotsct) {
	if (chdir(unmeta(buf)) == -1)
	    return NULL;
	if (*buf2) {
	    if (strlen(pwd) + strlen(buf2) + 1 >= PATH_MAX)
		return NULL;
	    sprintf(buf, "%s/%s", (!strcmp("/", pwd)) ? "" : pwd, buf2);
	} else
	    strcpy(buf, pwd);
	return ztrdup(buf);
    }
    /* There are one or more .. segments at the beginning of the relative path.
    A corresponding number of segments must be removed from the end of the
    current directory before the downward relative path is appended. */
    strcpy(buf, pwd);
    s = buf + strlen(buf) - 1;
    while (dotsct--)
	while (s != buf)
	    if (*--s == '/')
		break;
    if (s == buf || *buf2)
	s++;
    strcpy(s, buf2);
    /* For some reason, this chdir must be attempted with both the newly
    created path and the original non-normalised version. */
    if (chdir(unmeta(buf)) != -1 || chdir(unmeta(dest)) != -1)
	return ztrdup(buf);
    return NULL;
}

/* do the extra processing associated with changing directory */

/**/
void
cd_new_pwd(int func, LinkNode dir)
{
    Param pm;
    List l;
    char *new_pwd, *s;
    int dirstacksize;

    if (func == BIN_PUSHD)
	rolllist(dirstack, dir);
    new_pwd = remnode(dirstack, dir);

    if (func == BIN_POPD && firstnode(dirstack)) {
	zsfree(new_pwd);
	new_pwd = getlinknode(dirstack);
    } else if (func == BIN_CD && unset(AUTOPUSHD))
	zsfree(getlinknode(dirstack));

    if (isset(CHASELINKS)) {
	s = new_pwd;
	new_pwd = findpwd(s);
	zsfree(s);
    }
    if (isset(PUSHDIGNOREDUPS)) {
	LinkNode n; 
	for (n = firstnode(dirstack); n; incnode(n)) {
	    if (!strcmp(new_pwd, getdata(n))) {
		zsfree(remnode(dirstack, n));
		break;
	    }
	}
    }

    /* shift around the pwd variables, to make oldpwd and pwd relate to the
    current (i.e. new) pwd */
    zsfree(oldpwd);
    oldpwd = pwd;
    pwd = new_pwd;
    /* update the PWD and OLDPWD shell parameters */
    if ((pm = (Param) paramtab->getnode(paramtab, "PWD")) &&
	(pm->flags & PM_EXPORTED) && pm->env)
	pm->env = replenv(pm->env, pwd);
    if ((pm = (Param) paramtab->getnode(paramtab, "OLDPWD")) &&
	(pm->flags & PM_EXPORTED) && pm->env)
	pm->env = replenv(pm->env, oldpwd);
    if (unset(PUSHDSILENT) && func != BIN_CD && isset(INTERACTIVE))
	printdirstack();
    else if (doprintdir) {
	fprintdir(pwd, stdout);
        putchar('\n');
    }

    /* execute the chpwd function */
    if ((l = getshfunc("chpwd"))) {
	fflush(stdout);
	fflush(stderr);
	doshfunc(l, NULL, 0, 1);
    }

    dirstacksize = getiparam("DIRSTACKSIZE");
    /* handle directory stack sizes out of range */
    if (dirstacksize > 0) {
	int remove = countlinknodes(dirstack) -
		     (dirstacksize < 2 ? 2 : dirstacksize);
	while (remove-- >= 0)
	    zsfree(remnode(dirstack, lastnode(dirstack)));
    }
}

/* Print the directory stack */

/**/
void
printdirstack(void)
{
    LinkNode node;

    fprintdir(pwd, stdout);
    for (node = firstnode(dirstack); node; incnode(node)) {
	putchar(' ');
	fprintdir(getdata(node), stdout);
    }
    putchar('\n');
}

/* Normalise a path.  Segments consisting of ., and foo/.. combinations, *
 * are removed.  The number of .. segments at the beginning of the       *
 * path is returned.  The normalised path, minus leading ..s, is copied  *
 * to dest.                                                              */

/**/
int
fixdir(char *dest, char *src)
{
    int ct = 0;
    char *d0 = dest;

/*** if have RFS superroot directory ***/
#ifdef HAVE_SUPERROOT
    /* allow /.. segments to remain */
    while (*src == '/' && src[1] == '.' && src[2] == '.' &&
      (!src[3] || src[3] == '/')) {
	*dest++ = '/';
	*dest++ = '.';
	*dest++ = '.';
	src += 3;
    }
#endif

    for (;;) {
	/* compress multiple /es into single */
	if (*src == '/') {
	    *dest++ = *src++;
	    while (*src == '/')
		src++;
	}
	/* if we are at the end of the input path, remove a trailing / (if it
	exists), and return ct */
	if (!*src) {
	    while (dest > d0 + 1 && dest[-1] == '/')
		dest--;
	    *dest = '\0';
	    return ct;
	}
	if (src[0] == '.' && src[1] == '.' &&
	  (src[2] == '\0' || src[2] == '/')) {
	    /* remove a foo/.. combination, or increment ct, as appropriate */
	    if (dest > d0 + 1) {
		for (dest--; dest > d0 + 1 && dest[-1] != '/'; dest--);
		if (dest[-1] != '/')
		    dest--;
	    } else
		ct++;
	    src++;
	    while (*++src == '/');
	} else if (src[0] == '.' && (src[1] == '/' || src[1] == '\0')) {
	    /* skip a . section */
	    while (*++src == '/');
	} else {
	    /* copy a normal segment into the output */
	    while (*src != '/' && *src != '\0')
		*dest++ = *src++;
	}
    }
}

#ifdef COMPCTL

/**** compctl builtin ****/

#define COMP_LIST	(1<<0)	/* -L */
#define COMP_COMMAND	(1<<1)	/* -C */
#define COMP_DEFAULT	(1<<2)	/* -D */
#define COMP_FIRST	(1<<3)	/* -T */
#define COMP_REMOVE	(1<<4)

#define COMP_SPECIAL (COMP_COMMAND|COMP_DEFAULT|COMP_FIRST)

/* Flag for listing, command, default, or first completion */
static int cclist;

/* Mask for determining what to print */
static unsigned long showmask = 0;

/* Main entry point for the `compctl' builtin */

/**/
int
bin_compctl(char *name, char **argv, char *ops, int func)
{
    Compctl cc = NULL;
    int ret = 0;

    /* clear static flags */
    cclist = 0;
    showmask = 0;

    /* Parse all the arguments */
    if (*argv) {
	cc = (Compctl) zcalloc(sizeof(*cc));
	if (get_compctl(name, &argv, cc, 1, 0)) {
	    freecompctl(cc);
	    return 1;
	}

	/* remember flags for printing */
	showmask = cc->mask;
	if ((showmask & CC_EXCMDS) && !(showmask & CC_DISCMDS))
	    showmask &= ~CC_EXCMDS;

	/* if no command arguments or just listing, we don't want cc */
	if (!*argv || (cclist & COMP_LIST))
	    freecompctl(cc);
    }

    /* If no commands and no -C, -T, or -D, print all the compctl's *
     * If some flags (other than -C, -T, or -D) were given, then    *
     * only print compctl containing those flags.                   */
    if (!*argv && !(cclist & COMP_SPECIAL)) {
	scanhashtable(compctltab, 1, 0, 0, compctltab->printnode, 0);
	printcompctl((cclist & COMP_LIST) ? "" : "COMMAND", &cc_compos, 0);
	printcompctl((cclist & COMP_LIST) ? "" : "DEFAULT", &cc_default, 0);
 	printcompctl((cclist & COMP_LIST) ? "" : "FIRST", &cc_first, 0);
	return ret;
    }

    /* If we're listing and we've made it to here, then there are arguments *
     * or a COMP_SPECIAL flag (-D, -C, -T), so print only those.            */
    if (cclist & COMP_LIST) {
	HashNode hn;
	char **ptr;

	showmask = 0;
	for (ptr = argv; *ptr; ptr++) {
	    if ((hn = compctltab->getnode(compctltab, *ptr))) {
		compctltab->printnode(hn, 0);
	    } else {
		zwarnnam(name, "no compctl defined for %s", *ptr, 0);
		ret = 1;
	    }
	}
	if (cclist & COMP_COMMAND)
	    printcompctl("", &cc_compos, 0);
	if (cclist & COMP_DEFAULT)
	    printcompctl("", &cc_default, 0);
	if (cclist & COMP_FIRST)
	    printcompctl("", &cc_first, 0);
	return ret;
    }

    /* Assign the compctl to the commands given */
    if (*argv) {
	if(cclist & COMP_SPECIAL)
	    /* Ideally we'd handle this properly, setting both the *
	     * special and normal completions.  For the moment,    *
	     * this is better than silently failing.               */
	    zwarnnam(name, "extraneous commands ignored", NULL, 0);
	else
	    compctl_process_cc(argv, cc);
    }

    return ret;
}

/* Parse the basic flags for `compctl' */

/**/
int
get_compctl(char *name, char ***av, Compctl cc, int first, int isdef)
{
    /* Parse the basic flags for completion:
     * first is a flag that we are not in extended completion,
     * while hx indicates or (+) completion (need to know for
     * default and command completion as the initial compctl is special). 
     * cct is a temporary just to hold flags; it never needs freeing.
     */
    struct compctl cct;
    char **argv = *av;
    int ready = 0, hx = 0;

    /* Handle `compctl + foo ...' specially:  turn it into
     * a default compctl by removing it from the hash table.
     */
    if (first && argv[0][0] == '+' && !argv[0][1] &&
	!(argv[1] && argv[1][0] == '-' && argv[1][1])) {
	argv++;
	if(argv[0] && argv[0][0] == '-')
	    argv++;
	*av = argv;
	freecompctl(cc);
 	cclist = COMP_REMOVE;
	return 0;
    }

    memset((void *)&cct, 0, sizeof(cct));

    /* Loop through the flags until we have no more:        *
     * those with arguments are not properly allocated yet, *
     * we just hang on to the argument that was passed.     */
    for (; !ready && argv[0] && argv[0][0] == '-' && (argv[0][1] || !first);) {
	if (!argv[0][1])
	    *argv = "-+";
	while (!ready && *++(*argv)) {
	    if(**argv == Meta)
		*++*argv ^= 32;
	    switch (**argv) {
	    case 'f':
		cct.mask |= CC_FILES;
		break;
	    case 'c':
		cct.mask |= CC_COMMPATH;
		break;
	    case 'm':
		cct.mask |= CC_EXTCMDS;
		break;
	    case 'w':
		cct.mask |= CC_RESWDS;
		break;
	    case 'o':
		cct.mask |= CC_OPTIONS;
		break;
	    case 'v':
		cct.mask |= CC_VARS;
		break;
	    case 'b':
		cct.mask |= CC_BINDINGS;
		break;
	    case 'A':
		cct.mask |= CC_ARRAYS;
		break;
	    case 'I':
		cct.mask |= CC_INTVARS;
		break;
	    case 'F':
		cct.mask |= CC_SHFUNCS;
		break;
	    case 'p':
		cct.mask |= CC_PARAMS;
		break;
	    case 'E':
		cct.mask |= CC_ENVVARS;
		break;
	    case 'j':
		cct.mask |= CC_JOBS;
		break;
	    case 'r':
		cct.mask |= CC_RUNNING;
		break;
	    case 'z':
		cct.mask |= CC_STOPPED;
		break;
	    case 'B':
		cct.mask |= CC_BUILTINS;
		break;
	    case 'a':
		cct.mask |= CC_ALREG | CC_ALGLOB;
		break;
	    case 'R':
		cct.mask |= CC_ALREG;
		break;
	    case 'G':
		cct.mask |= CC_ALGLOB;
		break;
	    case 'u':
		cct.mask |= CC_USERS;
		break;
	    case 'd':
		cct.mask |= CC_DISCMDS;
		break;
	    case 'e':
		cct.mask |= CC_EXCMDS;
		break;
	    case 'N':
		cct.mask |= CC_SCALARS;
		break;
	    case 'O':
		cct.mask |= CC_READONLYS;
		break;
	    case 'Z':
		cct.mask |= CC_SPECIALS;
		break;
	    case 'q':
		cct.mask |= CC_REMOVE;
		break;
	    case 'U':
		cct.mask |= CC_DELETE;
		break;
	    case 'n':
		cct.mask |= CC_NAMED;
		break;
	    case 'Q':
		cct.mask |= CC_QUOTEFLAG;
		break;
	    case 'k':
		if ((*argv)[1]) {
		    cct.keyvar = (*argv) + 1;
		    *argv = "" - 1;
		} else if (!argv[1]) {
		    zwarnnam(name, "variable name expected after -%c", NULL,
			    **argv);
		    return 1;
		} else {
		    cct.keyvar = *++argv;
		    *argv = "" - 1;
		}
		break;
	    case 'K':
		if ((*argv)[1]) {
		    cct.func = (*argv) + 1;
		    *argv = "" - 1;
		} else if (!argv[1]) {
		    zwarnnam(name, "function name expected after -%c", NULL,
			    **argv);
		    return 1;
		} else {
		    cct.func = *++argv;
		    *argv = "" - 1;
		}
		break;
	    case 'X':
		if ((*argv)[1]) {
		    cct.explain = (*argv) + 1;
		    *argv = "" - 1;
		} else if (!argv[1]) {
		    zwarnnam(name, "string expected after -%c", NULL, **argv);
		    return 1;
		} else {
		    cct.explain = *++argv;
		    *argv = "" - 1;
		}
		break;
	    case 'P':
		if ((*argv)[1]) {
		    cct.prefix = (*argv) + 1;
		    *argv = "" - 1;
		} else if (!argv[1]) {
		    zwarnnam(name, "string expected after -%c", NULL, **argv);
		    return 1;
		} else {
		    cct.prefix = *++argv;
		    *argv = "" - 1;
		}
		break;
	    case 'S':
		if ((*argv)[1]) {
		    cct.suffix = (*argv) + 1;
		    *argv = "" - 1;
		} else if (!argv[1]) {
		    zwarnnam(name, "string expected after -%c", NULL, **argv);
		    return 1;
		} else {
		    cct.suffix = *++argv;
		    *argv = "" - 1;
		}
		break;
	    case 'g':
		if ((*argv)[1]) {
		    cct.glob = (*argv) + 1;
		    *argv = "" - 1;
		} else if (!argv[1]) {
		    zwarnnam(name, "glob pattern expected after -%c", NULL,
			    **argv);
		    return 1;
		} else {
		    cct.glob = *++argv;
		    *argv = "" - 1;
		}
		break;
	    case 's':
		if ((*argv)[1]) {
		    cct.str = (*argv) + 1;
		    *argv = "" - 1;
		} else if (!argv[1]) {
		    zwarnnam(name, "command string expected after -%c", NULL,
			    **argv);
		    return 1;
		} else {
		    cct.str = *++argv;
		    *argv = "" - 1;
		}
		break;
	    case 'l':
		if ((*argv)[1]) {
		    cct.subcmd = (*argv) + 1;
		    *argv = "" - 1;
		} else if (!argv[1]) {
		    zwarnnam(name, "command name expected after -%c", NULL,
			    **argv);
		    return 1;
		} else {
		    cct.subcmd = *++argv;
		    *argv = "" - 1;
		}
		break;
	    case 'H':
		if ((*argv)[1])
		    cct.hnum = atoi((*argv) + 1);
		else if (argv[1])
		    cct.hnum = atoi(*++argv);
		else {
		    zwarnnam(name, "number expected after -%c", NULL,
			    **argv);
		    return 1;
		}
		if (!argv[1]) {
		    zwarnnam(name, "missing pattern after -%c", NULL,
			    **argv);
		    return 1;
		}
		cct.hpat = *++argv;
		if (cct.hnum < 1)
		    cct.hnum = 0;
		if (*cct.hpat == '*' && !cct.hpat[1])
		    cct.hpat = "";
		*argv = "" - 1;
		break;
	    case 'C':
		if (first && !hx) {
		    cclist |= COMP_COMMAND;
		} else {
		    zwarnnam(name, "misplaced command completion (-C) flag",
			    NULL, 0);
		    return 1;
		}
		break;
	    case 'D':
		if (first && !hx) {
		    isdef = 1;
		    cclist |= COMP_DEFAULT;
		} else {
		    zwarnnam(name, "misplaced default completion (-D) flag",
			    NULL, 0);
		    return 1;
		}
		break;
 	    case 'T':
              if (first && !hx) {
 		    cclist |= COMP_FIRST;
 		} else {
 		    zwarnnam(name, "misplaced first completion (-T) flag",
 			    NULL, 0);
 		    return 1;
 		}
 		break;
	    case 'L':
		if (!first || hx) {
		    zwarnnam(name, "illegal use of -L flag", NULL, 0);
		    return 1;
		}
		cclist |= COMP_LIST;
		break;
	    case 'x':
		if (!argv[1]) {
		    zwarnnam(name, "condition expected after -%c", NULL,
			    **argv);
		    return 1;
		}
		if (first) {
		    argv++;
		    if (get_xcompctl(name, &argv, &cct, isdef)) {
			if (cct.ext)
			    freecompctl(cct.ext);
			return 1;
		    }
		    ready = 2;
		} else {
		    zwarnnam(name, "recursive extended completion not allowed",
			    NULL, 0);
		    return 1;
		}
		break;
	    default:
		if (!first && (**argv == '-' || **argv == '+'))
		    (*argv)--, argv--, ready = 1;
		else {
		    zwarnnam(name, "bad option: -%c", NULL, **argv);
		    return 1;
		}
	    }
	}

	if (*++argv && (!ready || ready == 2) &&
	    **argv == '+' && !argv[0][1]) {
	    /* There's an alternative (+) completion:  assign
	     * what we have so far before moving on to that.
	     */
	    if (cc_assign(name, &cc, &cct, first && !hx))
		return 1;

	    hx = 1;
	    ready = 0;

	    if (!*++argv || **argv != '-' ||
		(**argv == '-' && (!argv[0][1] ||
				   (argv[0][1] == '-' && !argv[0][2])))) {
		/* No argument to +, which means do default completion */
		if (isdef)
		    zwarnnam(name,
			    "recursive xor'd default completions not allowed",
			    NULL, 0);
		else
		    cc->xor = &cc_default;
	    } else {
		/* more flags follow:  prepare to loop again */
		cc->xor = (Compctl) zcalloc(sizeof(*cc));
		cc = cc->xor;
		memset((void *)&cct, 0, sizeof(cct));
	    }
	}
    }
    if (!ready && *argv && **argv == '-')
	argv++;

    if (! (cct.mask & (CC_EXCMDS | CC_DISCMDS)))
	cct.mask |= CC_EXCMDS;

    /* assign the last set of flags we parsed */
    if (cc_assign(name, &cc, &cct, first && !hx))
	return 1;

    *av = argv;

    return 0;
}

/* Handle the -x ... -- part of compctl. */

/**/
int
get_xcompctl(char *name, char ***av, Compctl cc, int isdef)
{
    char **argv = *av, *t, *tt, sav;
    int n, l = 0, ready = 0;
    Compcond m, c, o;
    Compctl *next = &(cc->ext);

    while (!ready) {
	/* o keeps track of or's, m remembers the starting condition,
	 * c is the current condition being parsed
	 */
	o = m = c = (Compcond) zcalloc(sizeof(*c));
	/* Loop over each condition:  something like 's[...][...], p[...]' */
	for (t = *argv; *t;) {
	    while (*t == ' ')
		t++;
	    /* First get the condition code */
	    switch (*t) {
	    case 's':
		c->type = CCT_CURSUF;
		break;
	    case 'S':
		c->type = CCT_CURPRE;
		break;
	    case 'p':
		c->type = CCT_POS;
		break;
	    case 'c':
		c->type = CCT_CURSTR;
		break;
	    case 'C':
		c->type = CCT_CURPAT;
		break;
	    case 'w':
		c->type = CCT_WORDSTR;
		break;
	    case 'W':
		c->type = CCT_WORDPAT;
		break;
	    case 'n':
		c->type = CCT_CURSUB;
		break;
	    case 'N':
		c->type = CCT_CURSUBC;
		break;
	    case 'm':
		c->type = CCT_NUMWORDS;
		break;
	    case 'r':
		c->type = CCT_RANGESTR;
		break;
	    case 'R':
		c->type = CCT_RANGEPAT;
		break;
	    default:
		t[1] = '\0';
		zwarnnam(name, "unknown condition code: %s", t, 0);
		zfree(m, sizeof(struct compcond));

		return 1;
	    }
	    /* Now get the arguments in square brackets */
	    if (t[1] != '[') {
		t[1] = '\0';
		zwarnnam(name, "expected condition after condition code: %s", t, 0);
		zfree(m, sizeof(struct compcond));

		return 1;
	    }
	    t++;
	    /* First count how many or'd arguments there are,
	     * marking the active ]'s and ,'s with unprintable characters.
	     */
	    for (n = 0, tt = t; *tt == '['; n++) {
		for (l = 1, tt++; *tt && l; tt++)
		    if (*tt == '\\' && tt[1])
			tt++;
		    else if (*tt == '[')
			l++;
		    else if (*tt == ']')
			l--;
		    else if (l == 1 && *tt == ',')
			*tt = '\201';
		if (tt[-1] == ']')
		    tt[-1] = '\200';
	    }

	    if (l) {
		t[1] = '\0';
		zwarnnam(name, "error after condition code: %s", t, 0);
		zfree(m, sizeof(struct compcond));

		return 1;
	    }
	    c->n = n;

	    /* Allocate space for all the arguments of the conditions */
	    if (c->type == CCT_POS ||
		c->type == CCT_NUMWORDS) {
		c->u.r.a = (int *)zcalloc(n * sizeof(int));
		c->u.r.b = (int *)zcalloc(n * sizeof(int));
	    } else if (c->type == CCT_CURSUF ||
		       c->type == CCT_CURPRE)
		c->u.s.s = (char **)zcalloc(n * sizeof(char *));

	    else if (c->type == CCT_RANGESTR ||
		     c->type == CCT_RANGEPAT) {
		c->u.l.a = (char **)zcalloc(n * sizeof(char *));
		c->u.l.b = (char **)zcalloc(n * sizeof(char *));
	    } else {
		c->u.s.p = (int *)zcalloc(n * sizeof(int));
		c->u.s.s = (char **)zcalloc(n * sizeof(char *));
	    }
	    /* Now loop over the actual arguments */
	    for (l = 0; *t == '['; l++, t++) {
		for (t++; *t && *t == ' '; t++);
		tt = t;
		if (c->type == CCT_POS ||
		    c->type == CCT_NUMWORDS) {
		    /* p[...] or m[...]:  one or two numbers expected */
		    for (; *t && *t != '\201' && *t != '\200'; t++);
		    if (!(sav = *t)) {
			zwarnnam(name, "error in condition", NULL, 0);
			freecompcond(m);
			return 1;
		    }
		    *t = '\0';
		    c->u.r.a[l] = atoi(tt);
		    /* Second argument is optional:  see if it's there */
		    if (sav == '\200')
			/* no:  copy first argument */
			c->u.r.b[l] = c->u.r.a[l];
		    else {
			tt = ++t;
			for (; *t && *t != '\200'; t++);
			if (!*t) {
			    zwarnnam(name, "error in condition", NULL, 0);
			    freecompcond(m);
			    return 1;
			}
			*t = '\0';
			c->u.r.b[l] = atoi(tt);
		    }
		} else if (c->type == CCT_CURSUF ||
			   c->type == CCT_CURPRE) {
		    /* -s[..] or -S[..]:  single string expected */
		    for (; *t && *t != '\200'; t++)
			if (*t == '\201')
			    *t = ',';
		    if (!*t) {
			zwarnnam(name, "error in condition", NULL, 0);
			freecompcond(m);
			return 1;
		    }
		    *t = '\0';
		    c->u.s.s[l] = ztrdup(tt);
		} else if (c->type == CCT_RANGESTR ||
			   c->type == CCT_RANGEPAT) {
		    /* -r[..,..] or -R[..,..]:  two strings expected */
		    for (; *t && *t != '\201'; t++);
		    if (!*t) {
			zwarnnam(name, "error in condition", NULL, 0);
			freecompcond(m);
			return 1;
		    }
		    *t = '\0';
		    c->u.l.a[l] = ztrdup(tt);
		    tt = ++t;
		    /* any more commas are text, not active */
		    for (; *t && *t != '\200'; t++)
			if (*t == '\201')
			    *t = ',';
		    if (!*t) {
			zwarnnam(name, "error in condition", NULL, 0);
			freecompcond(m);
			return 1;
		    }
		    *t = '\0';
		    c->u.l.b[l] = ztrdup(tt);
		} else {
		    /* remaining patterns are number followed by string */
		    for (; *t && *t != '\200' && *t != '\201'; t++);
		    if (!*t || *t == '\200') {
			zwarnnam(name, "error in condition", NULL, 0);
			freecompcond(m);
			return 1;
		    }
		    *t = '\0';
		    c->u.s.p[l] = atoi(tt);
		    tt = ++t;
		    for (; *t && *t != '\200'; t++)
			if (*t == '\201')
			    *t = ',';
		    if (!*t) {
			zwarnnam(name, "error in condition", NULL, 0);
			freecompcond(m);
			return 1;
		    }
		    *t = '\0';
		    c->u.s.s[l] = ztrdup(tt);
		}
	    }
	    while (*t == ' ')
		t++;
	    if (*t == ',') {
		/* Another condition to `or' */
		o->or = c = (Compcond) zcalloc(sizeof(*c));
		o = c;
		t++;
	    } else if (*t) {
		/* Another condition to `and' */
		c->and = (Compcond) zcalloc(sizeof(*c));
		c = c->and;
	    }
	}
	/* Assign condition to current compctl */
	*next = (Compctl) zcalloc(sizeof(*cc));
	(*next)->cond = m;
	argv++;
	/* End of the condition; get the flags that go with it. */
	if (get_compctl(name, &argv, *next, 0, isdef))
	    return 1;
 	if ((!argv || !*argv) && (cclist & COMP_SPECIAL))
 	    /* default, first, or command completion finished */
	    ready = 1;
	else {
	    /* see if we are looking for more conditions or are
	     * ready to return (ready = 1)
	     */
	    if (!argv || !*argv || **argv != '-' ||
		((!argv[0][1] || argv[0][1] == '+') && !argv[1])) {
		zwarnnam(name, "missing command names", NULL, 0);
		return 1;
	    }
	    if (!strcmp(*argv, "--"))
		ready = 1;
	    else if (!strcmp(*argv, "-+") && argv[1] &&
		     !strcmp(argv[1], "--")) {
		ready = 1;
		argv++;
	    }
	    argv++;
	    /* prepare to put the next lot of conditions on the end */
	    next = &((*next)->next);
	}
    }
    /* save position at end of parsing */
    *av = argv - 1;
    return 0;
}

/**/
int
cc_assign(char *name, Compctl *ccptr, Compctl cct, int reass)
{
    /* Copy over the details from the values in cct to those in *ccptr */
    Compctl cc;

    if (cct->subcmd && (cct->keyvar || cct->glob || cct->str ||
			cct->func || cct->explain || cct->prefix)) {
	zwarnnam(name, "illegal combination of options", NULL, 0);
	return 1;
    }

    /* Handle assignment of new default or command completion */
    if (reass && !(cclist & COMP_LIST)) {
	/* if not listing */
	if (cclist == (COMP_COMMAND|COMP_DEFAULT)
	    || cclist == (COMP_COMMAND|COMP_FIRST)
	    || cclist == (COMP_DEFAULT|COMP_FIRST)
	    || cclist == COMP_SPECIAL) {
 	    zwarnnam(name, "can't set -D, -T, and -C simultaneously", NULL, 0);
	    /* ... because the following code wouldn't work. */
	    return 1;
	}
	if (cclist & COMP_COMMAND) {
	    /* command */
	    *ccptr = &cc_compos;
	    cc_reassign(*ccptr);
	} else if (cclist & COMP_DEFAULT) {
	    /* default */
	    *ccptr = &cc_default;
	    cc_reassign(*ccptr);
 	} else if (cclist & COMP_FIRST) {
 	    /* first */
 	    *ccptr = &cc_first;
 	    cc_reassign(*ccptr);
	}
    }

    /* Free the old compctl */
    cc = *ccptr;
    zsfree(cc->keyvar);
    zsfree(cc->glob);
    zsfree(cc->str);
    zsfree(cc->func);
    zsfree(cc->explain);
    zsfree(cc->prefix);
    zsfree(cc->suffix);
    zsfree(cc->subcmd);
    zsfree(cc->hpat);
    
    /* and copy over the new stuff, (permanently) allocating
     * space for strings.
     */
    cc->mask = cct->mask;
    cc->keyvar = ztrdup(cct->keyvar);
    cc->glob = ztrdup(cct->glob);
    cc->str = ztrdup(cct->str);
    cc->func = ztrdup(cct->func);
    cc->explain = ztrdup(cct->explain);
    cc->prefix = ztrdup(cct->prefix);
    cc->suffix = ztrdup(cct->suffix);
    cc->subcmd = ztrdup(cct->subcmd);
    cc->hpat = ztrdup(cct->hpat);
    cc->hnum = cct->hnum;

    /* careful with extended completion:  it's already allocated */
    cc->ext = cct->ext;

    return 0;
}

/**/
void
cc_reassign(Compctl cc)
{
    /* Free up a new default or command completion:
     * this is a hack to free up the parts which should be deleted,
     * without removing the basic variable which is statically allocated
     */
    Compctl c2;

    c2 = (Compctl) zcalloc(sizeof *cc);
    c2->xor = cc->xor;
    c2->ext = cc->ext;
    c2->refc = 1;

    freecompctl(c2);

    cc->ext = cc->xor = NULL;
}

/**/
void
compctl_process(char **s, int mask, char *uk, char *gl, char *st, char *fu, char *ex, char *pr, char *su, char *sc, char *hp, int hn)
{
    /* Command called internally to initialise some basic compctls */
    Compctl cc;

    cc = (Compctl) zcalloc(sizeof *cc);
    cc->mask = mask;
    cc->keyvar = ztrdup(uk);
    cc->glob = ztrdup(gl);
    cc->str = ztrdup(st);
    cc->func = ztrdup(fu);
    cc->explain = ztrdup(ex);
    cc->prefix = ztrdup(pr);
    cc->suffix = ztrdup(su);
    cc->subcmd = ztrdup(sc);
    cc->hpat = ztrdup(hp);
    cc->hnum = hn;

    cclist = 0;
    compctl_process_cc(s, cc);
}

/**/
void
compctl_process_cc(char **s, Compctl cc)
{
    Compctlp ccp;

    if (cclist & COMP_REMOVE) {
	/* Delete entries for the commands listed */
	for (; *s; s++) {
	    if ((ccp = (Compctlp) compctltab->removenode(compctltab, *s)))
		compctltab->freenode((HashNode) ccp);
	}
    } else {
	/* Add the compctl just read to the hash table */

	cc->refc = 0;
	for (; *s; s++) {
	    cc->refc++;
	    ccp = (Compctlp) zalloc(sizeof *ccp);
	    ccp->cc = cc;
	    compctltab->addnode(compctltab, ztrdup(*s), ccp);
	}
    }
}

/* Print a `compctl' */

/**/
void
printcompctl(char *s, Compctl cc, int printflags)
{
    Compctl cc2;
    char *css = "fcqovbAIFpEjrzBRGudeNOZUnQmw";
    char *mss = " pcCwWsSnNmrR";
    unsigned long t = 0x7fffffff;
    unsigned long flags = cc->mask;
    unsigned long oldshowmask;

    if ((flags & CC_EXCMDS) && !(flags & CC_DISCMDS))
	flags &= ~CC_EXCMDS;

    /* If showmask is non-zero, then print only those *
     * commands with that flag set.                   */
    if (showmask && !(flags & showmask))
	return;

    /* Temporarily clear showmask in case we make *
     * recursive calls to printcompctl.           */
    oldshowmask = showmask;
    showmask = 0;

    /* print either command name or start of compctl command itself */
    if (s) {
	if (cclist & COMP_LIST) {
	    printf("compctl");
	    if (cc == &cc_compos)
		printf(" -C");
	    if (cc == &cc_default)
		printf(" -D");
	    if (cc == &cc_first)
		printf(" -T");
	} else
	    quotedzputs(s, stdout);
    }

    /* loop through flags w/o args that are set, printing them if so */
    if (flags & t) {
	printf(" -");
	if ((flags & (CC_ALREG | CC_ALGLOB)) == (CC_ALREG | CC_ALGLOB))
	    putchar('a'), flags &= ~(CC_ALREG | CC_ALGLOB);
	while (*css) {
	    if (flags & t & 1)
		putchar(*css);
	    css++;
	    flags >>= 1;
	    t >>= 1;
	}
    }

    /* now flags with arguments */
    flags = cc->mask;
    printif(cc->keyvar, 'k');
    printif(cc->func, 'K');
    printif(cc->explain, 'X');
    printif(cc->prefix, 'P');
    printif(cc->suffix, 'S');
    printif(cc->glob, 'g');
    printif(cc->str, 's');
    printif(cc->subcmd, 'l');
    if (cc->hpat) {
	printf(" -H %d ", cc->hnum);
	quotedzputs(cc->hpat, stdout);
    }

    /* now the -x ... -- extended completion part */
    if (cc->ext) {
	Compcond c, o;
	int i;

	cc2 = cc->ext;
	printf(" -x");

	while (cc2) {
	    /* loop over conditions */
	    c = cc2->cond;

	    printf(" '");
	    for (c = cc2->cond; c;) {
		/* loop over or's */
		o = c->or;
		while (c) {
		    /* loop over and's */
		    putchar(mss[c->type]);

		    for (i = 0; i < c->n; i++) {
			/* for all [...]'s of a given condition */
			putchar('[');
			switch (c->type) {
			case CCT_POS:
			case CCT_NUMWORDS:
			    printf("%d,%d", c->u.r.a[i], c->u.r.b[i]);
			    break;
			case CCT_CURSUF:
			case CCT_CURPRE:
			    printqt(c->u.s.s[i]);
			    break;
			case CCT_RANGESTR:
			case CCT_RANGEPAT:
			    printqt(c->u.l.a[i]);
			    putchar(',');
			    printqt(c->u.l.b[i]);
			    break;
			default:
			    printf("%d,", c->u.s.p[i]);
			    printqt(c->u.s.s[i]);
			}
			putchar(']');
		    }
		    if ((c = c->and))
			putchar(' ');
		}
		if ((c = o))
		    printf(" , ");
	    }
	    putchar('\'');
	    c = cc2->cond;
	    cc2->cond = NULL;
	    /* now print the flags for the current condition */
	    printcompctl(NULL, cc2, 0);
	    cc2->cond = c;
	    if ((cc2 = (Compctl) (cc2->next)))
		printf(" -");
	}
	if (cclist & COMP_LIST)
	    printf(" --");
    }
    if (cc && cc->xor) {
	/* print xor'd (+) completions */
	printf(" +");
	if (cc->xor != &cc_default)
	    printcompctl(NULL, cc->xor, 0);
    }
    if (s) {
	if ((cclist & COMP_LIST) && (cc != &cc_compos)
	    && (cc != &cc_default) && (cc != &cc_first)) {
	    if(s[0] == '-' || s[0] == '+')
		printf(" -");
	    putchar(' ');
	    quotedzputs(s, stdout);
	}
	putchar('\n');
    }

    showmask = oldshowmask;
}

/**/
void
printcompctlp(HashNode hn, int printflags)
{
    Compctlp ccp = (Compctlp) hn;

    /* Function needed for use by scanhashtable() */
    printcompctl(ccp->nam, ccp->cc, printflags);
}

/**/
void
printqt(char *str)
{
    /* Print str, but turn any single quote into '\'' or ''. */
    for (; *str; str++)
	if (*str == '\'')
	    printf(isset(RCQUOTES) ? "''" : "'\\''");
	else
	    putchar(*str);
}

/**/
void
printif(char *str, int c)
{
    /* If flag c has an argument, print that */
    if (str) {
	printf(" -%c ", c);
	quotedzputs(str, stdout);
    }
}

#endif

/**** parameter builtins ****/

/* declare, export, integer, local, readonly, typeset */

/**/
int
bin_typeset(char *name, char **argv, char *ops, int func)
{
    Param pm;
    Asgment asg;
    Comp com;
    char *optstr = "iLRZlurtxU";
    int on = 0, off = 0, roff, bit = PM_INTEGER;
    int initon, initoff, of, i;
    int returnval = 0, printflags = 0;

    /* hash -f is really the builtin `functions' */
    if (ops['f'])
	return bin_functions(name, argv, ops, func);

    /* Translate the options into PM_* flags.   *
     * Unfortunately, this depends on the order *
     * these flags are defined in zsh.h         */
    for (; *optstr; optstr++, bit <<= 1)
	if (ops[(int) *optstr] == 1)
	    on |= bit;
	else if (ops[(int) *optstr] == 2)
	    off |= bit;
    roff = off;

    /* Sanity checks on the options.  Remove conficting options. */
    if ((on | off) & PM_EXPORTED)
	func = BIN_EXPORT;
    if (on & PM_INTEGER)
	off |= PM_RIGHT_B | PM_LEFT | PM_RIGHT_Z | PM_UPPER | PM_ARRAY;
    if (on & PM_LEFT)
	off |= PM_RIGHT_B | PM_INTEGER;
    if (on & PM_RIGHT_B)
	off |= PM_LEFT | PM_INTEGER;
    if (on & PM_RIGHT_Z)
	off |= PM_INTEGER;
    if (on & PM_UPPER)
	off |= PM_LOWER;
    if (on & PM_LOWER)
	off |= PM_UPPER;
    on &= ~off;

    /* Given no arguments, list whatever the options specify. */
    if (!*argv) {
	if (!(on|roff))
	    printflags |= PRINT_TYPE;
	if (roff || ops['+'])
	    printflags |= PRINT_NAMEONLY;
	scanhashtable(paramtab, 1, on|roff, 0, paramtab->printnode, printflags);
	return 0;
    }

    /* With the -m option, treat arguments as glob patterns */
    if (ops['m']) {
	while ((asg = getasg(*argv++))) {
	    tokenize(asg->name);   /* expand argument */
	    if (!(com = parsereg(asg->name))) {
		untokenize(asg->name);
		zwarnnam(name, "bad pattern : %s", argv[-1], 0);
		returnval = 1;
		continue;
	    }
	    /* If no options or values are given, display all *
	     * parameters matching the glob pattern.          */
	    if (!(on || roff || asg->value)) {
		scanmatchtable(paramtab, com, 0, 0, paramtab->printnode, 0);
		continue;
	    }
	    /* Since either options or values are given, we search   *
	     * through the parameter table and change all parameters *
	     * matching the glob pattern to have these flags and/or  *
	     * value.                                                */
	    for (i = 0; i < paramtab->hsize; i++) {
		for (pm = (Param) paramtab->nodes[i]; pm; pm = (Param) pm->next) {
		    if (domatch(pm->nam, com, 0)) {
			/* set up flags if we have any */
			if (on || roff) {
			    if (PM_TYPE(pm->flags) == PM_ARRAY && (on & PM_UNIQUE) &&
				!(pm->flags & PM_READONLY & ~off))
				uniqarray((*pm->gets.afn) (pm));
			    pm->flags = (pm->flags | on) & ~off;
			    if (PM_TYPE(pm->flags) != PM_ARRAY) {
				if ((on & (PM_LEFT | PM_RIGHT_B | PM_RIGHT_Z | PM_INTEGER)) && auxlen)
				    pm->ct = auxlen;
				/* did we just export this? */
				if ((pm->flags & PM_EXPORTED) && !pm->env) {
				    pm->env = addenv(pm->nam, (asg->value) ? asg->value : getsparam(pm->nam));
				} else if (!(pm->flags & PM_EXPORTED) && pm->env) {
				/* did we just unexport this? */
				    delenv(pm->env);
				    zsfree(pm->env);
				    pm->env = NULL;
				}
			    }
			}
			/* set up a new value if given */
			if (asg->value) {
			    setsparam(pm->nam, ztrdup(asg->value));
			}
		    }
		}
	    }
	}
	return returnval;
    }

    /* Save the values of on, off, and func */
    initon = on;
    initoff = off;
    of = func;

    /* Take arguments literally.  Don't glob */
    while ((asg = getasg(*argv++))) {
	/* restore the original values of on, off, and func */
	on = initon;
	off = initoff;
	func = of;
	on &= ~PM_ARRAY;

	/* check if argument is a valid identifier */
	if (!isident(asg->name)) {
	    zerr("not an identifier: %s", asg->name, 0);
	    returnval = 1;
	    continue;
	}
	if ((pm = (Param) paramtab->getnode(paramtab, asg->name))) {
	    if (pm->flags & PM_SPECIAL) {
		func = 0;
		on = (PM_TYPE(pm->flags) == PM_INTEGER) ?
		    (on &= ~(PM_LEFT | PM_RIGHT_B | PM_RIGHT_Z | PM_UPPER)) :
		    (on & ~PM_INTEGER);
		off &= ~PM_INTEGER;
	    }
	    if (pm->level) {
		if ((on & PM_EXPORTED) && !(on &= ~PM_EXPORTED) && !off)
		    return 1;
	    }
	}
	bit = 0;    /* flag for switching int<->not-int */
	if (pm && !(pm->flags & PM_UNSET) && ((((locallevel == pm->level) || func == BIN_EXPORT)
		&& !(bit = ((off & pm->flags) | (on & ~pm->flags)) & PM_INTEGER)) || (pm->flags & PM_SPECIAL))) {
	    /* if no flags or values are given, just print this parameter */
	    if (!on && !roff && !asg->value) {
		paramtab->printnode((HashNode) pm, 0);
		continue;
	    }
	    if (PM_TYPE(pm->flags) == PM_ARRAY && (on & PM_UNIQUE) &&
		!(pm->flags & PM_READONLY & ~off))
		uniqarray((*pm->gets.afn) (pm));
	    pm->flags = (pm->flags | on) & ~off;
	    if ((on & (PM_LEFT | PM_RIGHT_B | PM_RIGHT_Z | PM_INTEGER)) && auxlen)
		pm->ct = auxlen;
	    if (PM_TYPE(pm->flags) != PM_ARRAY) {
		if (pm->flags & PM_EXPORTED) {
		    if (!pm->env)
			pm->env = addenv(asg->name, (asg->value) ? asg->value : getsparam(asg->name));
		} else if (pm->env) {
		    delenv(pm->env);
		    zsfree(pm->env);
		    pm->env = NULL;
		}
		if (asg->value)
		    setsparam(asg->name, ztrdup(asg->value));
	    }
	} else {
	    if (bit) {
		if (pm->flags & PM_READONLY) {
		    on |= ~off & PM_READONLY;
		    pm->flags &= ~PM_READONLY;
		}
		if (!asg->value)
		    asg->value = dupstring(getsparam(asg->name));
		unsetparam(asg->name);
	    } else if (locallist && func != BIN_EXPORT) {
		PERMALLOC {
		    addlinknode(locallist, ztrdup(asg->name));
		} LASTALLOC;
	    }
	    /* create a new node for a parameter with the *
	     * flags in `on' minus the readonly flag      */
	    pm = createparam(ztrdup(asg->name), on & ~PM_READONLY);
	    DPUTS(!pm, "BUG: parameter not created");
	    pm->ct = auxlen;
	    if (func != BIN_EXPORT)
		pm->level = locallevel;
	    if (asg->value)
		setsparam(asg->name, ztrdup(asg->value));
	    pm->flags |= (on & PM_READONLY);
	}
    }
    return returnval;
}

/* Display or change the attributes of shell functions.   *
 * If called as autoload, it will define a new autoloaded *
 * (undefined) shell function.                            */

/**/
int
bin_functions(char *name, char **argv, char *ops, int func)
{
    Comp com;
    Shfunc shf;
    int i, returnval = 0;
    int on = 0, off = 0;

    /* Do we have any flags defined? */
    if (ops['u'] || ops['t']) {
	if (ops['u'] == 1)
	    on |= PM_UNDEFINED;
	else if (ops['u'] == 2)
	    off |= PM_UNDEFINED;

	if (ops['t'] == 1)
	    on |= PM_TAGGED;
	else if (ops['t'] == 2)
	    off |= PM_TAGGED;
    }

    if (off & PM_UNDEFINED) {
	zwarnnam(name, "invalid option(s)", NULL, 0);
	return 1;
    }

    /* If no arguments given, we will print functions.  If flags *
     * are given, we will print only functions containing these  *
     * flags, else we'll print them all.                         */
    if (!*argv) {
	scanhashtable(shfunctab, 1, on|off, DISABLED, shfunctab->printnode, 0);
	return 0;
    }

    /* With the -m option, treat arguments as glob patterns */
    if (ops['m']) {
	on &= ~PM_UNDEFINED;
	for (; *argv; argv++) {
	    /* expand argument */
	    tokenize(*argv);
	    if ((com = parsereg(*argv))) {
		/* with no options, just print all functions matching the glob pattern */
		if (!(on|off)) {
		    scanmatchtable(shfunctab, com, 0, DISABLED, shfunctab->printnode, 0);
		} else {
		/* apply the options to all functions matching the glob pattern */
		    for (i = 0; i < shfunctab->hsize; i++) {
			for (shf = (Shfunc) shfunctab->nodes[i]; shf; shf = (Shfunc) shf->next)
			    if (domatch(shf->nam, com, 0) && !(shf->flags & DISABLED))
				shf->flags = (shf->flags | on) & (~off);
		    }
		}
	    } else {
		untokenize(*argv);
		zwarnnam(name, "bad pattern : %s", *argv, 0);
		returnval = 1;
	    }
	}
	return returnval;
    }

    /* Take the arguments literally -- do not glob */
    for (; *argv; argv++) {
	if ((shf = (Shfunc) shfunctab->getnode(shfunctab, *argv))) {
	    /* if any flag was given */
	    if (on|off)
		/* turn on/off the given flags */
		shf->flags = (shf->flags | (on & ~PM_UNDEFINED)) & ~off;
	    else
		/* no flags, so just print */
		shfunctab->printnode((HashNode) shf, 0);
	} else if (on & PM_UNDEFINED) {
	    /* Add a new undefined (autoloaded) function to the *
	     * hash table with the corresponding flags set.     */
	    shf = (Shfunc) zcalloc(sizeof *shf);
	    shf->flags = on;
	    shfunctab->addnode(shfunctab, ztrdup(*argv), shf);
	} else
	    returnval = 1;
    }
    return returnval;
}

/* unset: unset parameters */

/**/
int
bin_unset(char *name, char **argv, char *ops, int func)
{
    Param pm, next;
    Comp com;
    char *s;
    int match = 0, returnval = 0;
    int i;

    /* unset -f is the same as unfunction */
    if (ops['f'])
	return bin_unhash(name, argv, ops, func);

    /* with -m option, treat arguments as glob patterns */
    if (ops['m']) {
	while ((s = *argv++)) {
	    /* expand */
	    tokenize(s);
	    if ((com = parsereg(s))) {
		/* Go through the parameter table, and unset any matches */
		for (i = 0; i < paramtab->hsize; i++) {
		    for (pm = (Param) paramtab->nodes[i]; pm; pm = next) {
			/* record pointer to next, since we may free this one */
			next = (Param) pm->next;
			if (domatch(pm->nam, com, 0)) {
			    unsetparam(pm->nam);
			    match++;
			}
		    }
		}
	    } else {
		untokenize(s);
		zwarnnam(name, "bad pattern : %s", s, 0);
		returnval = 1;
	    }
	}
	/* If we didn't match anything, we return 1. */
	if (!match)
	    returnval = 1;
	return returnval;
    }

    /* do not glob -- unset the given parameter */
    while ((s = *argv++)) {
	if (paramtab->getnode(paramtab, s)) {
	    unsetparam(s);
	} else {
	    returnval = 1;
	}
    }
    return returnval;
}


/* type, whence, which */

/**/
int
bin_whence(char *nam, char **argv, char *ops, int func)
{
    HashNode hn;
    Comp com;
    int returnval = 0;
    int printflags = 0;
    int csh, all, v;
    int informed;
    char *cnam;

    /* Check some option information */
    csh = ops['c'];
    v   = ops['v'];
    all = ops['a'];

    if (ops['c'])
	printflags |= PRINT_WHENCE_CSH;
    else if (ops['v'])
	printflags |= PRINT_WHENCE_VERBOSE;
    else
	printflags |= PRINT_WHENCE_SIMPLE;
    if(ops['f'])
	printflags |= PRINT_WHENCE_FUNCDEF;

    /* With -m option -- treat arguments as a glob patterns */
    if (ops['m']) {
	for (; *argv; argv++) {
	    /* parse the pattern */
	    tokenize(*argv);
	    if (!(com = parsereg(*argv))) {
		untokenize(*argv);
		zwarnnam(nam, "bad pattern : %s", *argv, 0);
		returnval = 1;
		continue;
	    }
	    if (!ops['p']) {
		/* -p option is for path search only.    *
		 * We're not using it, so search for ... */

		/* aliases ... */
		scanmatchtable(aliastab, com, 0, DISABLED, aliastab->printnode, printflags);

		/* and reserved words ... */
		scanmatchtable(reswdtab, com, 0, DISABLED, reswdtab->printnode, printflags);

		/* and shell functions... */
		scanmatchtable(shfunctab, com, 0, DISABLED, shfunctab->printnode, printflags);

		/* and builtins. */
		scanmatchtable(builtintab, com, 0, DISABLED, builtintab->printnode, printflags);
	    }
	    /* Done search for `internal' commands, if the -p option *
	     * was not used.  Now search the path.                   */
	    cmdnamtab->filltable(cmdnamtab);
	    scanmatchtable(cmdnamtab, com, 0, 0, cmdnamtab->printnode, printflags);

	}
    return returnval;
    }

    /* Take arguments literally -- do not glob */
    for (; *argv; argv++) {
	informed = 0;

	if (!ops['p']) {
	    /* Look for alias */
	    if ((hn = aliastab->getnode(aliastab, *argv))) {
		aliastab->printnode(hn, printflags);
		if (!all)
		    continue;
		informed = 1;
	    }
	    /* Look for reserved word */
	    if ((hn = reswdtab->getnode(reswdtab, *argv))) {
		reswdtab->printnode(hn, printflags);
		if (!all)
		    continue;
		informed = 1;
	    }
	    /* Look for shell function */
	    if ((hn = shfunctab->getnode(shfunctab, *argv))) {
		shfunctab->printnode(hn, printflags);
		if (!all)
		    continue;
		informed = 1;
	    }
	    /* Look for builtin command */
	    if ((hn = builtintab->getnode(builtintab, *argv))) {
		builtintab->printnode(hn, printflags);
		if (!all)
		    continue;
		informed = 1;
	    }
	    /* Look for commands that have been added to the *
	     * cmdnamtab with the builtin `hash foo=bar'.    */
	    if ((hn = cmdnamtab->getnode(cmdnamtab, *argv)) && (hn->flags & HASHED)) {
		cmdnamtab->printnode(hn, printflags);
		if (!all)
		    continue;
		informed = 1;
	    }
	}

	/* Option -a is to search the entire path, *
	 * rather than just looking for one match. */
	if (all) {
	    char **pp, buf[PATH_MAX], *z;

	    for (pp = path; *pp; pp++) {
		z = buf;
		if (**pp) {
		    strucpy(&z, *pp);
		    *z++ = '/';
		}
		if ((z - buf) + strlen(*argv) >= PATH_MAX)
		    continue;
		strcpy(z, *argv);
		if (iscom(buf)) {
		    if (v && !csh)
			printf("%s is %s\n", *argv, buf);
		    else
			puts(buf);
		    informed = 1;
		}
	    }
	    if (!informed && (v || csh)) {
		printf("%s not found\n", *argv);
		returnval = 1;
	    }
	} else if ((cnam = findcmd(*argv))) {
	    /* Found external command. */
	    if (v && !csh)
		printf("%s is %s\n", *argv, cnam);
	    else
		puts(cnam);
	    zsfree(cnam);
	} else {
	    /* Not found at all. */
	    if (v || csh)
		printf("%s not found\n", *argv);
	    returnval = 1;
	}
    }
    return returnval;
}

/**** command & named directory hash table builtins ****/

/*****************************************************************
 * hash -- explicitly hash a command.                            *
 * 1) Given no arguments, list the hash table.                   *
 * 2) The -m option prints out commands in the hash table that   *
 *    match a given glob pattern.                                *
 * 3) The -f option causes the entire path to be added to the    *
 *    hash table (cannot be combined with any arguments).        *
 * 4) The -r option causes the entire hash table to be discarded *
 *    (cannot be combined with any arguments).                   *
 * 5) Given argument of the form foo=bar, add element to command *
 *    hash table, so that when `foo' is entered, then `bar' is   *
 *    executed.                                                  *
 * 6) Given arguments not of the previous form, add it to the    *
 *    command hash table as if it were being executed.           *
 * 7) The -d option causes analogous things to be done using     *
 *    the named directory hash table.                            *
 *****************************************************************/

/**/
int
bin_hash(char *name, char **argv, char *ops, int func)
{
    HashTable ht;
    Comp com;
    Asgment asg;
    int returnval = 0;

    if (ops['d'])
	ht = nameddirtab;
    else
	ht = cmdnamtab;

    if (ops['r'] || ops['f']) {
	/* -f and -r can't be used with any arguments */
	if (*argv) {
	    zwarnnam("hash", "too many arguments", NULL, 0);
	    return 1;
	}

	/* empty the hash table */
	if (ops['r'])
	    ht->emptytable(ht);

	/* fill the hash table in a standard way */
	if (ops['f'])
	    ht->filltable(ht);

	return 0;
    }

    /* Given no arguments, display current hash table. */
    if (!*argv) {
	scanhashtable(ht, 1, 0, 0, ht->printnode, 0);
	return 0;
    }

    while ((asg = getasg(*argv))) {
	if (asg->value) {
	    /* The argument is of the form foo=bar, *
	     * so define an entry for the table.    */
	    if(ops['d']) {
		Nameddir nd = (Nameddir) zcalloc(sizeof *nd);
		nd->flags = 0;
		nd->dir = ztrdup(asg->value);
		ht->addnode(ht, ztrdup(asg->name), nd);
	    } else {
		Cmdnam cn = (Cmdnam) zcalloc(sizeof *cn);
		cn->flags = HASHED;
		cn->u.cmd = ztrdup(asg->value);
		ht->addnode(ht, ztrdup(asg->name), cn);
	    }
	} else if (ops['m']) {
	    /* with the -m option, treat the argument as a glob pattern */
	    tokenize(*argv);  /* expand */
	    if ((com = parsereg(*argv))) {
		/* display matching hash table elements */
		scanmatchtable(ht, com, 0, 0, ht->printnode, 0);
	    } else {
		untokenize(*argv);
		zwarnnam(name, "bad pattern : %s", *argv, 0);
		returnval = 1;
	    }
	} else if (!ht->getnode2(ht, asg->name)) {
	    /* With no `=value' part to the argument, *
	     * work out what it ought to be.          */
	    if(ops['d'] && !getnameddir(asg->name)) {
		zwarnnam(name, "no such directory name: %s", asg->name, 0);
		returnval = 1;
	    } else if (!hashcmd(asg->name, path)) {
		zwarnnam(name, "no such command: %s", asg->name, 0);
		returnval = 1;
	    }
	}
	argv++;
    }
    return returnval;
}

/* unhash: remove specified elements from a hash table */

/**/
int
bin_unhash(char *name, char **argv, char *ops, int func)
{
    HashTable ht;
    HashNode hn, nhn;
    Comp com;
    int match = 0, returnval = 0;
    int i;

    /* Check which hash table we are working with. */
    if (ops['d'])
	ht = nameddirtab;	/* named directories */
    else if (ops['f'])
	ht = shfunctab;		/* shell functions   */
    else if (ops['a'])
	ht = aliastab;		/* aliases           */
    else
	ht = cmdnamtab;		/* external commands */

    /* With -m option, treat arguments as glob patterns. *
     * "unhash -m '*'" is legal, but not recommended.    */
    if (ops['m']) {
	for (; *argv; argv++) {
	    /* expand argument */
	    tokenize(*argv);
	    if ((com = parsereg(*argv))) {
		/* remove all nodes matching glob pattern */
		for (i = 0; i < ht->hsize; i++) {
		    for (hn = ht->nodes[i]; hn; hn = nhn) {
			/* record pointer to next, since we may free this one */
			nhn = hn->next;
			if (domatch(hn->nam, com, 0)) {
			    ht->freenode(ht->removenode(ht, hn->nam));
			    match++;
			}
		    }
		}
	    } else {
		untokenize(*argv);
		zwarnnam(name, "bad pattern : %s", *argv, 0);
		returnval = 1;
	    }
	}
	/* If we didn't match anything, we return 1. */
	if (!match)
	    returnval = 1;
	return returnval;
    }

    /* Take arguments literally -- do not glob */
    for (; *argv; argv++) {
	if ((hn = ht->removenode(ht, *argv))) {
	    ht->freenode(hn);
	} else {
	    zwarnnam(name, "no such hash table element: %s", *argv, 0);
	    returnval = 1;
	}
    }
    return returnval;
}

/**** alias builtins ****/

/* alias: display or create aliases. */

/**/
int
bin_alias(char *name, char **argv, char *ops, int func)
{
    Alias a;
    Comp com;
    Asgment asg;
    int haveflags = 0, returnval = 0;
    int flags1 = 0, flags2 = DISABLED;
    int printflags = 0;

    /* Did we specify the type of alias? */
    if (ops['r'] || ops['g']) {
	if (ops['r'] && ops['g']) {
	    zwarnnam(name, "illegal combination of options", NULL, 0);
	    return 1;
	}
	haveflags = 1;
	if (ops['g'])
	    flags1 |= ALIAS_GLOBAL;
	else
	    flags2 |= ALIAS_GLOBAL;
    }

    if (ops['L'])
	printflags |= PRINT_LIST;

    /* In the absence of arguments, list all aliases.  If a command *
     * line flag is specified, list only those of that type.        */
    if (!*argv) {
	scanhashtable(aliastab, 1, flags1, flags2, aliastab->printnode, printflags);
	return 0;
    }

    /* With the -m option, treat the arguments as *
     * glob patterns of aliases to display.       */
    if (ops['m']) {
	for (; *argv; argv++) {
	    tokenize(*argv);  /* expand argument */
	    if ((com = parsereg(*argv))) {
		/* display the matching aliases */
		scanmatchtable(aliastab, com, flags1, flags2, aliastab->printnode, printflags);
	    } else {
		untokenize(*argv);
		zwarnnam(name, "bad pattern : %s", *argv, 0);
		returnval = 1;
	    }
	}
	return returnval;
    }

    /* Take arguments literally.  Don't glob */
    while ((asg = getasg(*argv++))) {
	if (asg->value && !ops['L']) {
	    /* The argument is of the form foo=bar and we are not *
	     * forcing a listing with -L, so define an alias      */
	    aliastab->addnode(aliastab, ztrdup(asg->name),
		createaliasnode(ztrdup(asg->value), flags1));
	} else if ((a = (Alias) aliastab->getnode(aliastab, asg->name))) {
	    /* display alias if appropriate */
	    if (!haveflags ||
		(ops['r'] && !(a->flags & ALIAS_GLOBAL)) ||
		(ops['g'] &&  (a->flags & ALIAS_GLOBAL)))
		aliastab->printnode((HashNode) a, printflags);
	} else
	    returnval = 1;
    }
    return returnval;
}


/**** resource limit builtins ****/

#ifdef HAVE_GETRLIMIT

#if defined(RLIM_T_IS_QUAD_T) || defined(RLIM_T_IS_UNSIGNED)
# define ZSTRTORLIMT(a, b, c)	zstrtorlimit((a), (b), (c))
#else
# define ZSTRTORLIMT(a, b, c)	zstrtol((a), (b), (c))
#endif

/* Generated rec array containing limits required for the limit builtin.
 * They must appear in this array in numerical order of the RLIMIT_* macros.
 */

#include "rlimits.h"

#endif /* HAVE_GETRLIMIT */

/* limit: set or show resource limits.  The variable hard indicates *
 * whether `hard' or `soft' resource limits are being set/shown.    */

/**/
int
bin_limit(char *nam, char **argv, char *ops, int func)
{
#ifndef HAVE_GETRLIMIT
    /* limit builtin not appropriate to this system */
    zwarnnam(nam, "not available on this system", NULL, 0);
    return 1;
#else
    char *s;
    int hard, limnum, lim;
    rlim_t val;
    int ret = 0;

    hard = ops['h'];
    if (ops['s'] && !*argv)
	return setlimits(NULL);
    /* without arguments, display limits */
    if (!*argv) {
	showlimits(hard, -1);
	return 0;
    }
    while ((s = *argv++)) {
	/* Search for the appropriate resource name.  When a name matches (i.e. *
	 * starts with) the argument, the lim variable changes from -1 to the   *
	 * number of the resource.  If another match is found, lim goes to -2.  */
	for (lim = -1, limnum = 0; limnum < ZSH_NLIMITS; limnum++)
	    if (!strncmp(recs[limnum], s, strlen(s))) {
		if (lim != -1)
		    lim = -2;
		else
		    lim = limnum;
	    }
	/* lim==-1 indicates that no matches were found.       *
	 * lim==-2 indicates that multiple matches were found. */
	if (lim < 0) {
	    zwarnnam("limit",
		     (lim == -2) ? "ambiguous resource specification: %s"
		     : "no such resource: %s", s, 0);
	    return 1;
	}
	/* without value for limit, display the current limit */
	if (!(s = *argv++)) {
	    showlimits(hard, lim);
	    return 0;
	}
	if (lim==RLIMIT_CPU) {
	    /* time-type resource -- may be specified as seconds, or minutes or *
	     * hours with the `m' and `h' modifiers, and `:' may be used to add *
	     * together more than one of these.  It's easier to understand from *
	     * the code:                                                        */
	    val = ZSTRTORLIMT(s, &s, 10);
	    if (*s) {
		if ((*s == 'h' || *s == 'H') && !s[1])
		    val *= 3600L;
		else if ((*s == 'm' || *s == 'M') && !s[1])
		    val *= 60L;
		else if (*s == ':')
		    val = val * 60 + ZSTRTORLIMT(s + 1, &s, 10);
		else {
		    zwarnnam("limit", "unknown scaling factor: %s", s, 0);
		    return 1;
		}
            }
	}
# ifdef RLIMIT_NPROC
	else if (lim == RLIMIT_NPROC)
	    /* pure numeric resource -- only a straight decimal number is
	    permitted. */
	    val = ZSTRTORLIMT(s, &s, 10);
# endif /* RLIMIT_NPROC */
# ifdef RLIMIT_NOFILE
	else if (lim == RLIMIT_NOFILE)
	    /* pure numeric resource -- only a straight decimal number is
	    permitted. */
	    val = ZSTRTORLIMT(s, &s, 10);
# endif /* RLIMIT_NOFILE */
	else {
	    /* memory-type resource -- `k' and `M' modifiers are permitted,
	    meaning (respectively) 2^10 and 2^20. */
	    val = ZSTRTORLIMT(s, &s, 10);
	    if (!*s || ((*s == 'k' || *s == 'K') && !s[1]))
		val *= 1024L;
	    else if ((*s == 'M' || *s == 'm') && !s[1])
		val *= 1024L * 1024;
	    else {
		zwarnnam("limit", "unknown scaling factor: %s", s, 0);
		return 1;
	    }
	}
	/* new limit is valid and has been interpreted; apply it to the
	specified resource */
	if (hard) {
	    /* can only raise hard limits if running as root */
	    if (val > current_limits[lim].rlim_max && geteuid()) {
		zwarnnam("limit", "can't raise hard limits", NULL, 0);
		return 1;
	    } else {
		limits[lim].rlim_max = val;
		if (val < limits[lim].rlim_cur)
		    limits[lim].rlim_cur = val;
	    }
	} else if (val > limits[lim].rlim_max) {
	    zwarnnam("limit", "limit exceeds hard limit", NULL, 0);
	    return 1;
	} else
	    limits[lim].rlim_cur = val;
	if (ops['s'] && zsetlimit(lim, "limit"))
	    ret++;
    }
    return ret;
#endif /* HAVE_GETRLIMIT */
}

/* unlimit: remove resource limits.  Much of this code is the same as *
 * that in bin_limit().                                               */

/**/
int
bin_unlimit(char *nam, char **argv, char *ops, int func)
{
#ifndef HAVE_GETRLIMIT
    /* unlimit builtin not appropriate to this system */
    zwarnnam(nam, "not available on this system", NULL, 0);
    return 1;
#else
    int hard, limnum, lim;
    int ret = 0;
    uid_t euid = geteuid();

    hard = ops['h'];
    /* Without arguments, remove all limits. */
    if (!*argv) {
	for (limnum = 0; limnum != RLIM_NLIMITS; limnum++) {
	    if (hard)
		if (euid && current_limits[limnum].rlim_max != RLIM_INFINITY)
		    ret++;
		else
		    limits[limnum].rlim_max = RLIM_INFINITY;
	    else
		limits[limnum].rlim_cur = limits[limnum].rlim_max;
	}
	if (ops['s'])
	    ret += setlimits(nam);
	if (ret)
	    zwarnnam(nam, "can't remove hard limits", NULL, 0);
    } else {
	for (; *argv; argv++) {
	    /* Search for the appropriate resource name.  When a name     *
	     * matches (i.e. starts with) the argument, the lim variable  *
	     * changes from -1 to the number of the resource.  If another *
	     * match is found, lim goes to -2.                            */
	    for (lim = -1, limnum = 0; limnum < ZSH_NLIMITS; limnum++)
		if (!strncmp(recs[limnum], *argv, strlen(*argv))) {
		    if (lim != -1)
			lim = -2;
		    else
			lim = limnum;
		}
	    /* lim==-1 indicates that no matches were found.       *
	     * lim==-2 indicates that multiple matches were found. */
	    if (lim < 0) {
		zwarnnam(nam,
			 (lim == -2) ? "ambiguous resource specification: %s"
			 : "no such resource: %s", *argv, 0);
		return 1;
	    }
	    /* remove specified limit */
	    if (hard)
		if (euid && current_limits[lim].rlim_max != RLIM_INFINITY) {
		    zwarnnam(nam, "can't remove hard limits", NULL, 0);
		    ret++;
		} else
		    limits[lim].rlim_max = RLIM_INFINITY;
	    else
		limits[lim].rlim_cur = limits[lim].rlim_max;
	    if (ops['s'] && zsetlimit(lim, nam))
		ret++;
	}
    }
    return ret;
#endif /* HAVE_GETRLIMIT */
}

/* ulimit: set or display resource limits */

/**/
int
bin_ulimit(char *name, char **argv, char *ops, int func)
{
#ifndef HAVE_GETRLIMIT
    /* builtin not appropriate */
    zwarnnam(name, "not available on this system", NULL, 0);
    return 1;
#else
    int res, resmask = 0, hard = 0, soft = 0, nres = 0;
    char *options;

    do {
	options = *argv;
	if (options && *options == '-' && !options[1]) {
	    zwarnnam(name, "missing option letter", NULL, 0);
	    return 1;
	}
	res = -1;
	if (options && *options == '-') {
	    argv++;
	    while (*++options) {
		if(*options == Meta)
		    *++options ^= 32;
		res = -1;
		switch (*options) {
		case 'H':
		    hard = 1;
		    continue;
		case 'S':
		    soft = 1;
		    continue;
		case 'a':
		    if (*argv || options[1] || resmask) {
			zwarnnam(name, "no arguments required after -a",
				 NULL, 0);
			return 1;
		    }
		    resmask = (1 << RLIM_NLIMITS) - 1;
		    nres = RLIM_NLIMITS;
		    continue;
		case 't':
		    res = RLIMIT_CPU;
		    break;
		case 'f':
		    res = RLIMIT_FSIZE;
		    break;
		case 'd':
		    res = RLIMIT_DATA;
		    break;
		case 's':
		    res = RLIMIT_STACK;
		    break;
		case 'c':
		    res = RLIMIT_CORE;
		    break;
# ifdef RLIMIT_RSS
		case 'm':
		    res = RLIMIT_RSS;
		    break;
# endif /* RLIMIT_RSS */
# ifdef RLIMIT_MEMLOCK
		case 'l':
		    res = RLIMIT_MEMLOCK;
		    break;
# endif /* RLIMIT_MEMLOCK */
# ifdef RLIMIT_NOFILE
		case 'n':
		    res = RLIMIT_NOFILE;
		    break;
# endif /* RLIMIT_NOFILE */
# ifdef RLIMIT_NPROC
		case 'u':
		    res = RLIMIT_NPROC;
		    break;
# endif /* RLIMIT_NPROC */
# ifdef RLIMIT_VMEM
		case 'v':
		    res = RLIMIT_VMEM;
		    break;
# endif /* RLIMIT_VMEM */
		default:
		    /* unrecognised limit */
		    zwarnnam(name, "bad option: -%c", NULL, *options);
		    return 1;
		}
		if (options[1]) {
		    resmask |= 1 << res;
		    nres++;
		}
	    }
	}
	if (!*argv || **argv == '-') {
	    if (res < 0) {
		if (*argv || nres)
		    continue;
		else
		    res = RLIMIT_FSIZE;
                }
	    resmask |= 1 << res;
	    nres++;
	    continue;
	}
	if (res < 0)
	    res = RLIMIT_FSIZE;
	if (strcmp(*argv, "unlimited")) {
	    /* set limit to specified value */
	    rlim_t limit;

	    limit = ZSTRTORLIMT(*argv, NULL, 10);
	    /* scale appropriately */
	    switch (res) {
	    case RLIMIT_FSIZE:
	    case RLIMIT_CORE:
		limit *= 512;
		break;
	    case RLIMIT_DATA:
	    case RLIMIT_STACK:
# ifdef RLIMIT_RSS
	    case RLIMIT_RSS:
# endif /* RLIMIT_RSS */
# ifdef RLIMIT_MEMLOCK
	    case RLIMIT_MEMLOCK:
# endif /* RLIMIT_MEMLOCK */
# ifdef RLIMIT_VMEM
	    case RLIMIT_VMEM:
# endif /* RLIMIT_VMEM */
		limit *= 1024;
		break;
	    }
	    if (hard) {
		/* can't raise hard limit unless running as root */
		if (limit > current_limits[res].rlim_max && geteuid()) {
		    zwarnnam(name, "can't raise hard limits", NULL, 0);
		    return 1;
		}
		limits[res].rlim_max = limit;
		if (limit < limits[res].rlim_cur)
		    limits[res].rlim_cur = limit;
	    }
	    if (!hard || soft) {
		/* can't raise soft limit above hard limit */
		if (limit > limits[res].rlim_max) {
		    if (limit > current_limits[res].rlim_max && geteuid()) {
			zwarnnam(name, "value exceeds hard limit", NULL, 0);
			return 1;
		    }
		    limits[res].rlim_max = limits[res].rlim_cur = limit;
		} else
		    limits[res].rlim_cur = limit;
	    }
	} else {
	    /* remove specified limit */
	    if (hard) {
		/* can't remove hard limit unless running as root */
		if (current_limits[res].rlim_max != RLIM_INFINITY && geteuid()) {
		    zwarnnam(name, "can't remove hard limits", NULL, 0);
		    return 1;
		}
		limits[res].rlim_max = RLIM_INFINITY;
	    }
	    if (!hard || soft)
		/* `removal' of soft limit means setting it equal to the
		   corresponding hard limit */
		limits[res].rlim_cur = limits[res].rlim_max;
	}
	if (zsetlimit(res, name))
	    return 1;
	argv++;
    } while (*argv);
    for (res = 0; res < RLIM_NLIMITS; res++, resmask >>= 1)
	if (resmask & 1)
	    printulimit(res, hard, nres > 1);
    return 0;
#endif /* HAVE_GETRLIMIT */
}

/* Display resource limits.  hard indicates whether `hard' or `soft'  *
 * limits should be displayed.  lim specifies the limit, or may be -1 *
 * to show all.                                                       */

#ifdef HAVE_GETRLIMIT
/**/
void
showlimits(int hard, int lim)
{
    int rt;
    rlim_t val;

    /* main loop over resource types */
    for (rt = 0; rt != ZSH_NLIMITS; rt++)
	if (rt == lim || lim == -1) {
	    /* display limit for resource number rt */
	    printf("%-16s", recs[rt]);
	    val = (hard) ? limits[rt].rlim_max : limits[rt].rlim_cur;
	    if (val == RLIM_INFINITY)
		printf("unlimited\n");
	    else if (rt==RLIMIT_CPU)
		/* time-type resource -- display as hours, minutes and
		seconds. */
		printf("%d:%02d:%02d\n", (int)(val / 3600),
		       (int)(val / 60) % 60, (int)(val % 60));
# ifdef RLIMIT_NPROC
	    else if (rt == RLIMIT_NPROC)
		/* pure numeric resource */
		printf("%d\n", (int)val);
# endif /* RLIMIT_NPROC */
# ifdef RLIMIT_NOFILE
	    else if (rt == RLIMIT_NOFILE)
		/* pure numeric resource */
		printf("%d\n", (int)val);
# endif /* RLIMIT_NOFILE */
	    else if (val >= 1024L * 1024L)
		/* memory resource -- display with `K' or `M' modifier */
# ifdef RLIM_T_IS_QUAD_T
		printf("%qdMB\n", val / (1024L * 1024L));
	    else
		printf("%qdkB\n", val / 1024L);
# else
		printf("%ldMB\n", val / (1024L * 1024L));
            else
		printf("%ldkB\n", val / 1024L);
# endif /* RLIM_T_IS_QUAD_T */
	}
}
#endif  /* HAVE_GETRLIMIT */

/* Display a resource limit, in ulimit style.  lim specifies which   *
 * limit should be displayed, and hard indicates whether the hard or *
 * soft limit should be displayed.                                   */

#ifdef HAVE_GETRLIMIT
/**/
void
printulimit(int lim, int hard, int head)
{
    rlim_t limit;

    /* get the limit in question */
    limit = (hard) ? limits[lim].rlim_max : limits[lim].rlim_cur;
    /* display the appropriate heading */
    switch (lim) {
    case RLIMIT_CPU:
	if (head)
	    printf("cpu time (seconds)         ");
	break;
    case RLIMIT_FSIZE:
	if (head)
	    printf("file size (blocks)         ");
	if (limit != RLIM_INFINITY)
	    limit /= 512;
	break;
    case RLIMIT_DATA:
	if (head)
	    printf("data seg size (kbytes)     ");
	if (limit != RLIM_INFINITY)
	    limit /= 1024;
	break;
    case RLIMIT_STACK:
	if (head)
	    printf("stack size (kbytes)        ");
	if (limit != RLIM_INFINITY)
	    limit /= 1024;
	break;
    case RLIMIT_CORE:
	if (head)
	    printf("core file size (blocks)    ");
	if (limit != RLIM_INFINITY)
	    limit /= 512;
	break;
# ifdef RLIMIT_RSS
    case RLIMIT_RSS:
	if (head)
	    printf("resident set size (kbytes) ");
	if (limit != RLIM_INFINITY)
	    limit /= 1024;
	break;
# endif /* RLIMIT_RSS */
# ifdef RLIMIT_MEMLOCK
    case RLIMIT_MEMLOCK:
	if (head)
	    printf("locked-in-memory size (kb) ");
	if (limit != RLIM_INFINITY)
	    limit /= 1024;
	break;
# endif /* RLIMIT_MEMLOCK */
# ifdef RLIMIT_NPROC
    case RLIMIT_NPROC:
	if (head)
	    printf("processes                  ");
	break;
# endif /* RLIMIT_NPROC */
# ifdef RLIMIT_NOFILE
    case RLIMIT_NOFILE:
	if (head)
	    printf("file descriptors           ");
	break;
# endif /* RLIMIT_NOFILE */
# ifdef RLIMIT_VMEM
    case RLIMIT_VMEM:
	if (head)
	    printf("virtual memory size (kb)   ");
	if (limit != RLIM_INFINITY)
	    limit /= 1024;
	break;
# endif /* RLIMIT_VMEM */
# if defined RLIMIT_AS && RLIMIT_AS != RLIMIT_VMEM
    case RLIMIT_AS:
	if (head)
	    printf("address space (kb)         ");
	if (limit != RLIM_INFINITY)
	    limit /= 1024;
	break;
# endif /* RLIMIT_AS */
# ifdef RLIMIT_TCACHE
    case RLIMIT_TCACHE:
	if (head)
	    printf("cached threads             ");
	break;
# endif /* RLIMIT_TCACHE */
    }
    /* display the limit */
    if (limit == RLIM_INFINITY)
	printf("unlimited\n");
    else
	printf("%ld\n", (long)limit);
}
#endif /* HAVE_GETRLIMIT */

/**** miscellaneous builtins ****/

/* true, : (colon) */

/**/
int
bin_true(char *name, char **argv, char *ops, int func)
{
    return 0;
}

/* false builtin */

/**/
int
bin_false(char *name, char **argv, char *ops, int func)
{
    return 1;
}

static char * writeout = NULL;
size_t writeoutS = 0;

/**/
void *
pushWO(size_t size)
{
size_t OwriteoutS = writeoutS;
if (writeoutS) {
	 if (*(writeout+writeoutS-1)=='\0')
			writeoutS+=size-1;
	 else
			writeoutS+=size;
	 }
else
	writeoutS+=size;
if(writeout=zrealloc(writeout, writeoutS))
	return(writeout+writeoutS-size);
writeoutS=OwriteoutS;
return NULL;
}

/**/
void *
flushWO(FILE *fout)
{
	fwrite(writeout, 1, writeoutS-1, fout);
	zfree(writeout, writeoutS);
	writeout = NULL;
	writeoutS = 0;
}

/* echo, print, pushln */

/**/
int
bin_print(char *name, char **args, char *ops, int func)
{
    int nnl = 0, fd, argc, n;
    int *len;
    Histent ent;
    FILE *fout = stdout;
		void *(*Oncalloc) _((size_t));

    /* -m option -- treat the first argument as a pattern and remove
     * arguments not matching */
    if (ops['m']) {
	Comp com;
	char **t, **p;

	tokenize(*args);
	if (!(com = parsereg(*args))) {
	    untokenize(*args);
	    zwarnnam(name, "bad pattern : %s", *args, 0);
	    return 1;
	}
	for (p = ++args; *p; p++)
	    if (!domatch(*p, com, 0))
		for (t = p--; (*t = t[1]); t++);
    }
    /* compute lengths, and interpret according to -P, -D, -e, etc. */
    argc = arrlen(args);
    len = (int *)ncalloc(argc * sizeof(int));
    for(n = 0; n < argc; n++) {
	/* first \ sequences */
	if (!ops['e'] && (ops['R'] || ops['r'] || ops['E']))
	    unmetafy(args[n], &len[n]);
	else
	    args[n] = getkeystring(args[n], &len[n],
				    func != BIN_ECHO && !ops['e'], &nnl);
	/* -P option -- interpret as a prompt sequence */
	if(ops['P']) {
	    fwrite(metafy(args[n], len[n], META_NOALLOC), strlen(metafy(args[n], len[n], META_NOALLOC)), 1, stderr);
	    fflush(stderr);
//	    char *arg = putprompt(metafy(args[n], len[n], META_NOALLOC),
//				  &len[n], NULL, 0);
//	    args[n] = (char *)alloc(len[n] + 1);
//	    memcpy(args[n], arg, len[n]);
//	    args[n][len[n]] = 0;
//	    free(arg);
	}
	/* -D option -- interpret as a directory, and use ~ */
	if(ops['D']) {
	    Nameddir d = finddir(args[n]);
	    if(d) {
		char *arg = alloc(strlen(args[n]) + 1);
		sprintf(arg, "~%s%s", d->nam,
			args[n] + strlen(d->dir));
		args[n] = arg;
		len[n] = strlen(args[n]);
	    }
	}
    }

    /* -Z option -- push the arguments onto the bottom of the editing buffer stack */
    if (ops['Z']) {
    	if (! *args)
    		return 1;
	PERMALLOC {
	    insertFirstLinkNode(bufstack, sepjoin(args, NULL, ops['n'] || nnl, ops['N'] ? '\0' : '\n'));
	} LASTALLOC;
	return 0;
    }
    /* -z option -- push the arguments onto the editing buffer stack */
    if (ops['z']) {
    	if (! *args)
    		return 1;
	PERMALLOC {
	    pushnode(bufstack, sepjoin(args, NULL, ops['n'] || nnl, ops['N'] ? '\0' : '\n'));
	} LASTALLOC;
	return 0;
    }
    /* -f option -- push the arguments onto the writeout stack */
    if (ops['f']) {
    	if (! *args) {
    		char * w;
    		if(w=pushWO(2)) {
    			*w++='\n';
    			*w++='\0';
    		}
    		else {
			    if ( (ops['F']) && writeout && writeoutS)
    			   flushWO(fout);
    		 	return 1;
    		}
			}
			else {
			Oncalloc=ncalloc;
			ncalloc = pushWO;
			sepjoin(args, NULL, ops['n'] || nnl, ops['N'] ? '\0' : '\n');
			ncalloc=Oncalloc;
			}
	    if ( (ops['F']) && writeout && writeoutS)
	    	flushWO(fout);
	    return 0;
    }
    /* -F option -- flush the writeout stack */
    if (ops['F']) {
    	if (writeout && writeoutS)
    		flushWO(fout);
    	return 0;
    }
    /* -s option -- add the arguments to the history list */
    if (ops['s']) {
	int nwords = 0, nlen, iwords;
	char **pargs = args;

	PERMALLOC {
	    ent = gethistent(++curhist);
	    zsfree(ent->text);
	    if (ent->nwords)
		zfree(ent->words, ent->nwords*2*sizeof(short));
	    while (*pargs++)
		nwords++;
	    if ((ent->nwords = nwords)) {
		ent->words = (short *)zalloc(nwords*2*sizeof(short));
		nlen = iwords = 0;
		for (pargs = args; *pargs; pargs++) {
		    ent->words[iwords++] = nlen;
		    nlen += strlen(*pargs);
		    ent->words[iwords++] = nlen;
		    nlen++;
		}
	    } else
		ent->words = (short *)NULL;
	    ent->text = zjoin(args, ' ');
	    ent->stim = ent->ftim = time(NULL);
	    ent->flags = 0;
	} LASTALLOC;
	return 0;
    }
    /* -u and -p -- output to other than standard output */
    if (ops['u'] || ops['p']) {
	if (ops['u']) {
	    for (fd = 0; fd < 10; fd++)
		if (ops[fd + '0'])
		    break;
	    if (fd == 10)
		fd = 0;
	} else
	    fd = coprocout;
	if ((fd = dup(fd)) < 0) {
	    zwarnnam(name, "bad file number", NULL, 0);
	    return 1;
	}
	if ((fout = fdopen(fd, "w")) == 0) {
	    zwarnnam(name, "bad mode on fd", NULL, 0);
	    return 1;
	}
    }

    /* -o and -O -- sort the arguments */
    if (ops['o']) {
	if (ops['i'])
	    qsort(args, arrlen(args), sizeof(char *), cstrpcmp);

	else
	    qsort(args, arrlen(args), sizeof(char *), strpcmp);
    } else if (ops['O']) {
	if (ops['i'])
	    qsort(args, arrlen(args), sizeof(char *), invcstrpcmp);

	else
	    qsort(args, arrlen(args), sizeof(char *), invstrpcmp);
    }
    /* after sorting arguments, recalculate lengths */
    if(ops['o'] || ops['O'])
	for(n = 0; n < argc; n++)
	    len[n] = strlen(args[n]);

    /* -c -- output in columns */
    if (ops['c']) {
	int l, nc, nr, sc, n, t, i;
	char **ap;

	for (n = l = 0, ap = args; *ap; ap++, n++)
	    if (l < (t = strlen(*ap)))
		l = t;

	sc = l + 2;
	nc = (columns + 1) / sc;
	if (!nc)
	    nc = 1;
	nr = (n + nc - 1) / nc;

	for (i = 0; i < nr; i++) {
	    ap = args + i;
	    do {
		l = strlen(*ap);
		fprintf(fout, "%s", *ap);
		for (t = nr; t && *ap; t--, ap++);
		if(*ap)
		    for (; l < sc; l++)
			fputc(' ', fout);
	    } while (*ap);
	    fputc(ops['N'] ? '\0' : '\n', fout);
	}
	if (fout != stdout)
	    fclose(fout);
	return 0;
    }
    /* normal output */
    for (; *args; args++, len++) {
	fwrite(*args, *len, 1, fout);
	if (args[1])
	    fputc(ops['l'] ? '\n' : ops['N'] ? '\0' : ' ', fout);
    }
    if (!(ops['n'] || nnl))
	fputc(ops['N'] ? '\0' : '\n', fout);
    if (fout != stdout)
	fclose(fout);
    return 0;
}

/* shift builtin */

/**/
int
bin_shift(char *name, char **argv, char *ops, int func)
{
    int num = 1, l, ret = 0;
    char **s;
 
    /* optional argument can be either numeric or an array */
    if (*argv && !getaparam(*argv))
        num = matheval(*argv++);
 
    if (num < 0) {
        zwarnnam(name, "argument to shift must be non-negative", NULL, 0);
        return 1;
    }

    if (*argv) {
        for (; *argv; argv++)
            if ((s = getaparam(*argv))) {
                if (num > arrlen(s)) {
		    zwarnnam(name, "shift count must be <= $#", NULL, 0);
		    ret++;
		    continue;
		}
                PERMALLOC {
		    s = arrdup(s + num);
                } LASTALLOC;
                setaparam(*argv, s);
            }
    } else {
        if (num > (l = arrlen(pparams))) {
	    zwarnnam(name, "shift count must be <= $#", NULL, 0);
	    ret = 1;
	} else {
	    s = zalloc((l - num + 1) * sizeof(char *));
	    memcpy(s, pparams + num, (l - num + 1) * sizeof(char *));
	    while (num--)
		zsfree(pparams[num]);
	    zfree(pparams, (l + 1) * sizeof(char *));
	    pparams = s;
	}
    }
    return ret;
}

/* chr builtin */

// amacri, 2006
/**/
int
bin_zstat(char *nam, char **argv, char *ops, int func)
{
    char buffer[300], *pbuf, *StringName=NULL;
    time_t secs;
    struct stat *st;
 
    /* with the -S option, the first argument is taken *
     * as a string to save the chr result */
    if (*argv && ops['S']) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 1;
	}
    }

    pbuf=argv[0];
    if ((*pbuf++ != '%' ) || (argv[1]==NULL)) {
        zwarnnam(nam, "invalid argument", NULL, 0);
        return 1;
    }

    if (!( st=getstat(argv[1]) )) {
	if (isset(SHOWFSERR))
           zwarnnam(nam, "invalid file name", NULL, 0);
        return 1;
    }

    switch (*pbuf) {
//    case 'd': /* Device ID of device containing file */
//	secs=st->st_dev;
//	break;
//    case 'a': /* Mode of file/Access rights */
//	secs=st->st_mode;
//	break;
    case 'i': /* File serial/inode number */
	secs=st->st_ino;
	break;
    case 'h': /* Number of hard links to the file */
	secs=st->st_nlink;
	break;
    case 'u': /* User ID of file */
	secs=st->st_uid;
	break;
    case 'g': /* Group ID of file */
	secs=st->st_gid;
	break;
    case 'X': /* Time of last access as seconds since Epoch */
	secs=st->st_atime;
	break;
    case 'Y': /* Time of last modification as seconds since Epoch */
	secs=st->st_mtime;
	break;
    case 'z': /* Time of last status change as seconds since Epoch */
	secs=st->st_ctime;
	break;
    case 's': /* %s     Total size, in bytes */
	secs=st->st_size;
	break;
    case 'B': /* The size in bytes of each block reported by %b */
	secs=st->st_blksize;
	break;
    case 'b': /* Number of blocks allocated (see %B) */
	secs=st->st_blocks;
	break;
    default:
	zwarnnam(nam, "invalid option", NULL, 0);
	return 1;
    }
  
    sprintf(buffer,"%d",(int)secs);

    if (StringName != NULL) {
	setsparam(StringName, ztrdup(buffer));
    } else {
        printf("%s\n",buffer);
    }
    return 0;
}

// amacri 2006
/**/
int
bin_chr(char *nam, char **argv, char *ops, int func)
{
    int num=-1;
    char * buffer, *pbuf, *StringName=NULL;
 
    /* with the -S option, the first argument is taken *
     * as a string to save the chr result */
    if (*argv && ops['S']) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 1;
	}
    }

    if (*argv)
        num = matheval(*argv++);
 
    if ((num < 0) || (num>255)) {
        zwarnnam(nam, "argument must be existing, non-negative and less than 256", NULL, 0);
        return 1;
    }

    if (StringName != NULL) {

    /* allocate buffer space for result */
    pbuf = buffer = (char *)zalloc(3);

    *pbuf =(char) num;
    if (num==0)
       {
       *pbuf= -125;
       * ++pbuf= 32;
       }
    * ++pbuf ='\0';

	setsparam(StringName, buffer);
    } else {
	putchar(num);
//	putchar('\n');
    }
    return 0;
}

/* getpid builtin */

/**/
int
bin_getpid(char *nam, char **argv, char *ops, int func)
{
    int mypid=-1;
    char buffer[100], *StringName=NULL;
 
    /* the first argument is the string to save the getpid result */
    if (*argv) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 1;
	}
    }
    mypid = getpid();
    sprintf(buffer,"%d",mypid);

    if (StringName != NULL) {
        setsparam(StringName, ztrdup(buffer));
    } else {
	printf("%s\n", buffer);
    }
    return 0;
}

/* getopts: automagical option handling for shell scripts */

/**/
int
bin_getopts(char *name, char **argv, char *ops, int func)
{
    int lenstr, lenoptstr, i;
    char *optstr = unmetafy(*argv++, &lenoptstr), *var = *argv++;
    char **args = (*argv) ? argv : pparams;
    static int optcind = 1, quiet;
    char *str, optbuf[2], *opch = optbuf + 1;

    /* zoptind keeps count of the current argument number */
    if (zoptind < 1)
	/* first call */
	zoptind = 1;
    if (zoptind == 1)
	quiet = 0;
    optbuf[0] = '+';
    zsfree(zoptarg);
    zoptarg = ztrdup("");
    setsparam(var, ztrdup(""));
    if (*optstr == ':') {
	quiet = 1;
	optstr++;
	lenoptstr--;
    }
    if (zoptind > arrlen(args))
	return 1;
    str = unmetafy(args[zoptind - 1], &lenstr);
    if ((*str != '+' && *str != '-') || optcind >= lenstr ||
	(lenstr == 2 && str[0] == '-' && str[1] == '-')) {
	/* current argument doesn't contain options, or optcind is impossibly
	large */
	if (*str == '+' || *str == '-')
	    zoptind++;
	optcind = 0;
	return 1;
    }
    /* Get the option character.  optcind records the current position within
    the argument. */
    if (!optcind)
	optcind = 1;
    *opch = str[optcind++];
    if (optcind == lenstr) {
	if(args[zoptind++])
	    str = unmetafy(args[zoptind - 1], &lenstr);
	optcind = 0;
    }
    /* look for option in the provided optstr */
    for (i = 0; i != lenoptstr; i++)
	if (*opch == optstr[i])
	    break;
    if (i == lenoptstr || *opch == ':') {
	/* not a valid option */
	setsparam(var, ztrdup("?"));
	if (quiet) {
	    zsfree(zoptarg);
	    zoptarg = metafy(opch, 1, META_DUP);
	    return 0;
	}
	zerr("bad option: -%c", NULL, *opch);
	errflag = 0;
	return 0;
    }
    /* copy option into specified parameter, with + if required */
    setsparam(var, metafy(opch - (*str == '+'), 1 + (*str == '+'), META_DUP));
    /* handle case of an expected extra argument */
    if (optstr[i + 1] == ':') {
	if (!args[zoptind - 1]) {
	    /* no extra argument was provided */
	    if (quiet) {
		zsfree(zoptarg);
		zoptarg = metafy(opch, 1, META_DUP);
		setsparam(var, ztrdup(":"));
		return 0;
	    }
	    setsparam(var, ztrdup("?"));
	    zerr("argument expected after -%c option", NULL, *opch);
	    errflag = 0;
	    return 0;
	}
	/* skip over the extra argument */
	zsfree(zoptarg);
	zoptarg = metafy(str + optcind, lenstr - optcind, META_DUP);
	zoptind++;
	optcind = 0;
    }
    return 0;
}

/* break, bye, continue, exit, logout, return -- most of these take   *
 * one numeric argument, and the other (logout) is related to return. *
 * (return is treated as a logout when in a login shell.)             */

/**/
int
bin_break(char *name, char **argv, char *ops, int func)
{
    int num = lastval, nump = 0;

    /* handle one optional numeric argument */
    if (*argv) {
	num = matheval(*argv++);
	nump = 1;
    }

    switch (func) {
    case BIN_CONTINUE:
	if (!loops) {   /* continue is only permitted in loops */
	    zerrnam(name, "not in while, until, select, or repeat loop", NULL, 0);
	    return 1;
	}
	contflag = 1;   /* ARE WE SUPPOSED TO FALL THROUGH HERE? */
    case BIN_BREAK:
	if (!loops) {   /* break is only permitted in loops */
	    zerrnam(name, "not in while, until, select, or repeat loop", NULL, 0);
	    return 1;
	}
	breaks = nump ? minimum(num,loops) : 1;
	break;
    case BIN_RETURN:
	if (isset(INTERACTIVE) || locallevel || sourcelevel) {
	    retflag = 1;
	    breaks = loops;
	    lastval = num;
	    if (trapreturn == -2)
		trapreturn = lastval;
	    return lastval;
	}
	zexit(num, 0);	/* else treat return as logout/exit */
	break;
    case BIN_LOGOUT:
	if (unset(LOGINSHELL)) {
	    zerrnam(name, "not login shell", NULL, 0);
	    return 1;
	}
	zexit(num, 0);
	break;
    case BIN_EXIT:
	zexit(num, 0);
	break;
    }
    return 0;
}

/**/
int
bin_sleep(char *nam, char **argv, char * ops, int func)
{
      double secs=0;

    if (*argv)
        secs = matheval(*argv++);
    if (secs<0)
        return -2;

    return(usleep((int) (secs*1000000) ));
}

extern double EarthRadius;

/**/
int
bin_distance(char *nam, char **argv, char * ops, int func)
{
    char buffer[100], *StringName=NULL;
    double lat1, lon1, lat2, lon2, cl, Distance;

    /* with the -S option, the first argument is taken *
     * as a string to save the distance  */
    if (*argv && ops['S']) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 1;
	}
    }

lat1 = matheval(*argv++);

if (! *argv) {
     zwarnnam(nam, "invalid starting longitude", NULL, 0);
     return 1;
}
lon1 = matheval(*argv++);

if (! *argv) {
     zwarnnam(nam, "invalid ending latitude", NULL, 0);
     return 1;
}
lat2 = matheval(*argv++);

if (! *argv) {
     zwarnnam(nam, "invalid ending longitude", NULL, 0);
     return 1;
}
lon2 = matheval(*argv++);

if ((lat1 < -90) || (lat1 > 90)) {
     zwarnnam(nam, "invalid starting latitude", NULL, 0);
     return 1;
}
if ((lat2 < -90) || (lat2 > 90)) {
     zwarnnam(nam, "invalid ending latitude", NULL, 0);
     return 1;
}
if ((lon1 < -180) || (lon1 > 180)) {
     zwarnnam(nam, "invalid starting longitude", NULL, 0);
     return 1;
}
if ((lon2 < -180) || (lon2 > 180)) {
     zwarnnam(nam, "invalid ending longitude", NULL, 0);
     return 1;
}

// Great Circle Distance Formula
lat1=lat1 * Degree_to_Radian;
lat2=lat2 * Degree_to_Radian;
cl=lon2 * Degree_to_Radian - lon1 * Degree_to_Radian;
Distance=EarthRadius * acos(sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(cl));
//printf("lat1=%f, lon1=%f, lat2=%f, lon2=%f, Earth Radius=%f Distance=%f\n",lat1,lon1,lat2,lon2,EarthRadius,Distance);

sprintf((char *) buffer,"%.*f", Precision, Distance);
if (StringName != NULL) {
        setsparam(StringName, ztrdup(buffer));
    } else {
        printf("%s\n", buffer);
   }
return 0;
}

/**/
int
bin_zdate(char *nam, char **argv, char * ops, int func)
{
    int bufsize = 200;
    char buffer[300], *StringName=NULL;
    time_t secs;
    struct tm *t;

    /* with the -S option, the first argument is taken *
     * as a string to save the date  */
    if (*argv && ops['S']) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 1;
	}
    }

    if ((argv[0]) && (argv[1]))
    secs = get_date (argv[1], (time_t *) NULL);
    else secs = time(NULL);
/*    secs = (time_t)strtoul(argv[1], &endptr, 10); */

    t = localtime(&secs);

    if (argv[0])
    strftime(buffer, bufsize, argv[0], t);
    else strftime(buffer, bufsize, "%c", t);

    if (buffer[0]=='\0') snprintf((char *) buffer,300,"%d",(int) secs);

    if (StringName != NULL) {
	setsparam(StringName, ztrdup(buffer));
    } else {
	printf("%s\n", buffer);
    }
    return 0;
}

/* exit the shell.  val is the return value of the shell.  *
 * from_signal should be non-zero if zexit is being called *
 * because of a signal.                                    */

/**/
void
zexit(int val, int from_signal)
{
    static int in_exit;

    HEAPALLOC {
	if (isset(MONITOR) && !stopmsg && !from_signal) {
	    scanjobs();    /* check if jobs need printing           */
	    checkjobs();   /* check if any jobs are running/stopped */
	    if (stopmsg) {
		stopmsg = 2;
		LASTALLOC_RETURN;
	    }
	}
	if (in_exit++ && from_signal) {
	    LASTALLOC_RETURN;
        }
	if (isset(MONITOR))
	    /* send SIGHUP to any jobs left running  */
	    killrunjobs(from_signal);
	if (isset(RCS) && interact) {
	    if (islogin && !subsh) {
		sourcehome(".zlogout");
#ifdef GLOBAL_ZLOGOUT
		source(GLOBAL_ZLOGOUT);
#endif
	    }
	}
	if (sigtrapped[SIGEXIT])
	    dotrap(SIGEXIT);
	if (mypid != getpid())
	    _exit(val);
	else
	    exit(val);
    } LASTALLOC;
}

/* . (dot), source */

/**/
int
bin_dot(char *name, char **argv, char *ops, int func)
{
    char **old, *old0 = NULL;
    int ret, diddot = 0, dotdot = 0;
    char buf[PATH_MAX];
    char *s, **t, *enam, *arg0;
    struct stat st;

    if (!*argv || strlen(*argv) >= PATH_MAX)
	return 0;
    old = pparams;
    /* get arguments for the script */
    if (argv[1]) {
	PERMALLOC {
	    pparams = arrdup(argv + 1);
	} LASTALLOC;
    }
    enam = arg0 = ztrdup(*argv);
    if (isset(FUNCTIONARGZERO)) {
	old0 = argzero;
	argzero = arg0;
    if (isset(FUNCTIONPHARGZ))
	argzero = ztrdup3(arg0, " of ", old0);
      else
	argzero = ztrdup(arg0);

    }
    s = unmeta(enam);
    errno = ENOENT;
    ret = 1;
    /* for source only, check in current directory first */
    if (*name != '.' && access(s, F_OK) == 0
	&& stat(s, &st) >= 0 && !S_ISDIR(st.st_mode)) {
	diddot = 1;
	ret = source(enam);
    }
    if (ret) {
	/* use a path with / in it */
	for (s = arg0; *s; s++)
	    if (*s == '/') {
		if (*arg0 == '.') {
		    if (arg0 + 1 == s)
			++diddot;
		    else if (arg0[1] == '.' && arg0 + 2 == s)
			++dotdot;
		}
		ret = source(arg0);
		break;
	    }
	if (!*s || (ret && isset(PATHDIRS) && diddot < 2 && dotdot == 0)) {
	    /* search path for script */
	    for (t = path; *t; t++) {
		if (!(*t)[0] || ((*t)[0] == '.' && !(*t)[1])) {
		    if (diddot)
			continue;
		    diddot = 1;
		    strcpy(buf, arg0);
		} else {
		    if (strlen(*t) + strlen(arg0) + 1 >= PATH_MAX)
			continue;
		    sprintf(buf, "%s/%s", *t, arg0);
		}
		s = unmeta(buf);
		if (access(s, F_OK) == 0 && stat(s, &st) >= 0
		    && !S_ISDIR(st.st_mode)) {
		    ret = source(enam = buf);
		    break;
		}
	    }
	}
    }
    /* clean up and return */
    if (argv[1]) {
	freearray(pparams);
	pparams = old;
    }
    if (ret)
	zwarnnam(name, "%e: %s", enam, errno);
    zsfree(arg0);
    if (old0) {
	zsfree(argzero);
	argzero = old0;
    }
    return ret ? ret : lastval;
}

/**/
int
bin_emulate(char *nam, char **argv, char *ops, int func)
{
    emulate(*argv, ops['R']);
    return 0;
}

/* eval: simple evaluation */

/**/
int
bin_eval(char *nam, char **argv, char *ops, int func)
{
    List list;

    inpush(zjoin(argv, ' '), 0, NULL);
    strinbeg();
    stophist = 2;
    list = parse_list();
    strinend();
    inpop();
    if (!list) {
	errflag = 0;
	return 1;
    }
    execlist(list, 1, 0);
    if (errflag) {
	lastval = errflag;
	errflag = 0;
    }
    return lastval;
}

static char *zbuf;
static int readfd;

/* Read a character from readfd, or from the buffer zbuf.  Return EOF on end of
file/buffer. */

extern int cs;

/* read: get a line of input, or (for compctl functions) return some *
 * useful data about the state of the editing line.  The -E and -e   *
 * options mean that the result should be sent to stdout.  -e means, *
 * in addition, that the result should not actually be assigned to   *
 * the specified parameters.                                         */

/**/
int
bin_read(char *name, char **args, char *ops, int func)
{
    char *reply, *readpmpt;
    int bsiz, c = 0, gotnl = 0, al = 0, first, nchars = 1, bslash;
    int haso = 0;	/* true if /dev/tty has been opened specially */
    int isem = !strcmp(term, "emacs");
    char *buf, *bptr, *firstarg, *zbuforig;
    LinkList readll = newlinklist();
  	double timeout=-1;

  if (*args && ops['t'])
    if ((timeout = matheval(*args++)) <= 0)
		    timeout = 0;

    if ((ops['k'] || ops['b']) && *args && idigit(**args)) {
	if (!(nchars = atoi(*args)))
	    nchars = 1;
	args++;
    }

    firstarg = *args;
    if (*args && **args == '?')
	args++;
    /* default result parameter */
    reply = *args ? *args++ : ops['A'] ? "reply" : "REPLY";
    if (ops['A'] && *args) {
	zwarnnam(name, "only one array argument allowed", NULL, 0);
	return 1;
    }

    if ((ops['k'] && !ops['u'] && !ops['p']) || ops['q']) {
	if (SHTTY == -1) {
	    /* need to open /dev/tty specially */
	    SHTTY = open("/dev/tty", O_RDWR);
	    haso = 1;
	}
	/* We should have a SHTTY opened by now. */
	if (SHTTY == -1) {
	    /* Unfortunately, we didn't. */
	    fprintf(stderr, "not interactive and can't open terminal\n");
	    fflush(stderr);
	    return 1;
	}
	if (unset(INTERACTIVE))
	    gettyinfo(&shttyinfo);
	/* attach to the tty */
	attachtty(mypgrp);
	if (!isem && ops['k'])
	    setcbreak();
	readfd = SHTTY;
    } else if (ops['u'] && !ops['p']) {
	/* -u means take input from the specified file descriptor. *
	 * -up means take input from the coprocess.                */
	for (readfd = 9; readfd && !ops[readfd + '0']; --readfd);
    } else if (ops['p'])
	readfd = coprocin;
    else
	readfd = 0;

    /* handle prompt */
    if (firstarg) {
	for (readpmpt = firstarg;
	     *readpmpt && *readpmpt != '?'; readpmpt++);
	if (*readpmpt++) {
	    if (isatty(0)) {
		zputs(readpmpt, stderr);
		fflush(stderr);
	    }
	    readpmpt[-1] = '\0';
	}
    }

    /* option -k means read only a given number of characters (default 1) */
    if (ops['k']) {
	int val=-1, delta;
	char d;
  struct timeval tv, t, ot;
  struct timezone timez;
	fd_set readfds;
	
	if (timeout > 0) {
			  if (timeout - (int) timeout == 0)
	    	tv.tv_usec = 0;
	  else
	  		tv.tv_usec = (timeout - (int) timeout)*1000000;
	  tv.tv_sec = (int) timeout;

	gettimeofday(&t, &timez);
	ot.tv_usec = t.tv_usec + tv.tv_usec;
	delta = (int) (ot.tv_usec/1000000);
	ot.tv_sec = t.tv_sec + tv.tv_sec + delta;
	ot.tv_usec = ot.tv_usec - delta * 1000000;
	}

	/* allocate buffer space for result */
	bptr = buf = (char *)zalloc(nchars+1);

	errno=0;
	do {
	    /* If read returns 0, is end of file */

		  if (timeout > 0) {
					FD_ZERO (&readfds);
				  FD_SET(readfd, &readfds);
					select (readfd+1, &readfds, NULL, NULL, &tv);
			}
			val=0;
			if ( (timeout <= 0) || (FD_ISSET (readfd, &readfds)) )
				if ((val = read(readfd, bptr, nchars)) <= 0)
						break;
	
	    /* decrement number of characters read from number required */
	    nchars -= val;

	    /* increment pointer past read characters */
	    bptr += val;

	  if (timeout > 0) {
	  	gettimeofday(&t, &timez);
		  if ( (ot.tv_sec < t.tv_sec) || ( (ot.tv_sec==t.tv_sec)  && (ot.tv_usec<=t.tv_usec) ) )
		  	break;
	  }

	} while (nchars > 0);
	
	if (!ops['u'] && !ops['p']) {
	    /* dispose of result appropriately, etc. */
	    if (isem)
		while (val > 0 && read(SHTTY, &d, 1) == 1 && d != '\n');
	    else
		settyinfo(&shttyinfo);
	    if (haso) {
		close(SHTTY);
		SHTTY = -1;
	    }
	}

	if (ops['e'] || ops['E'])
	    fwrite(buf, bptr - buf, 1, stdout);
	if (!ops['e'])
	    setsparam(reply, metafy(buf, bptr - buf, META_REALLOC));
	else
	    zfree(buf, bptr - buf + 1);
	// Amacri, 9/19/2007, added return codes 2, 3, 4
	if ( (bptr == buf) && (val == -1) && (errno == EINTR) )
		return 2; // interrupt received without characters
	if ( (val == -1) && (errno == EINTR) )
		return 3; // interrupt received with characters
	if ( (val <= 0) && (bptr > buf) )
		return 4; // error code with characters
	if ( (val == 0) && (bptr == buf) )
		return 1; // no characters
	if ( (val < 0) && (bptr == buf) )
		return 5; // error code without characters
	return val <= 0; // 0 (ok, with characters) or 1 (no characters)
    }

    /* option -q means get one character, and interpret it as a Y or N */
    if (ops['q']) {
	char readbuf[2];

	/* set up the buffer */
	readbuf[1] = '\0';

	/* get, and store, reply */
	readbuf[0] = ((char)getquery(NULL)) == 'y' ? 'y' : 'n';

	/* dispose of result appropriately, etc. */
	if (haso) {
	    close(SHTTY);
	    SHTTY = -1;
	}

	if (ops['e'] || ops['E'])
	    printf("%s\n", readbuf);
	if (!ops['e'])
	    setsparam(reply, ztrdup(readbuf));

	return readbuf[0] == 'n';
    }

    /* All possible special types of input have been exhausted.  Take one line,
    and assign words to the parameters until they run out.  Leftover words go
    onto the last parameter.  If an array is specified, all the words become
    separate elements of the array. */

//    zbuforig = zbuf = (!(ops['z']||ops['Z'])) ? NULL :
//	(nonempty(bufstack)) ? ( ops['z']? (char *) getlinknode(bufstack) : (char *) getFirstLinknode(bufstack) ) : ztrdup("");
		if (ops['z']||ops['Z'])
			if (nonempty(bufstack))
				zbuforig = zbuf = ops['Z']? (char *) getFirstLinknode(bufstack) : (char *) getlinknode(bufstack);
			else
				return 1;
		else
			zbuforig = zbuf = NULL;

    first = 1;
    bslash = 0;
    while (*args || (ops['A'] && !gotnl)) {
	buf = bptr = (char *)zalloc(bsiz = 64);
	/* get input, a character at a time */
	while (!gotnl) {
	    c = zread();
            if ((c==0) && !ops['r']) continue; /* amacri skip 0 chars */
	    /* \ at the end of a line indicates a continuation *
	     * line, except in raw mode (-r option)            */
	    if (bslash && c == '\n') {
		bslash = 0;
		continue;
	    }
	    if (c == EOF || c == '\n')
		break;
	    if (!bslash && isep(c)) {
		if (bptr != buf || (!iwsep(c) && first)) {
		    first |= !iwsep(c);
		    break;
		}
		first |= !iwsep(c);
		continue;
	    }
	    bslash = c == '\\' && !bslash && !ops['r'];
	    if (bslash)
		continue;
	    first = 0;
	    if (imeta(c)) {
		*bptr++ = Meta;
		*bptr++ = c ^ 32;
	    } else
		*bptr++ = c;
	    /* increase the buffer size, if necessary */
	    if (bptr >= buf + bsiz - 1) {
		int blen = bptr - buf;

		buf = realloc(buf, bsiz *= 2);
		bptr = buf + blen;
	    }
	}
	if (c == '\n' || c == EOF)
	    gotnl = 1;
	*bptr = '\0';
	/* dispose of word appropriately */
	if (ops['e'] || ops['E']) {
	    zputs(buf, stdout);
	    putchar('\n');
	}
	if (!ops['e']) {
	    if (ops['A']) {
		addlinknode(readll, buf);
		al++;
	    } else
		setsparam(reply, buf);
	} else
	    free(buf);
	if (!ops['A'])
	    reply = *args++;
    }
    /* handle EOF */
    if (c == EOF) {
	if (readfd == coprocin) {
	    close(coprocin);
	    close(coprocout);
	    coprocin = coprocout = -1;
	}
    }
    /* final assignment (and display) of array parameter */
    if (ops['A']) {
	char **pp, **p = NULL;
	LinkNode n;

	p = (ops['e'] ? (char **)NULL
	     : (char **)zalloc((al + 1) * sizeof(char *)));

	for (pp = p, n = firstnode(readll); n; incnode(n)) {
	    if (ops['e'] || ops['E']) {
		zputs((char *) getdata(n), stdout);
		putchar('\n');
	    }
	    if (p)
		*pp++ = (char *)getdata(n);
	    else
		zsfree(getdata(n));
	}
	if (p) {
	    *pp++ = NULL;
	    setaparam(reply, p);
	}
	return c == EOF;
    }
    buf = bptr = (char *)zalloc(bsiz = 64);
    /* any remaining part of the line goes into one parameter */
    bslash = 0;
    if (!gotnl)
	for (;;) {
	    c = zread();
	    /* \ at the end of a line introduces a continuation line, except in
	    raw mode (-r option) */
	    if (bslash && c == '\n') {
		bslash = 0;
		continue;
	    }
	    if (c == EOF || (c == '\n' && !zbuf))
		break;
	    if (!bslash && isep(c) && bptr == buf) {
		if (iwsep(c))
		    continue;
		else if (!first) {
		    first = 1;
		    continue;
		}
            }
	    bslash = c == '\\' && !bslash && !ops['r'];
	    if (bslash)
		continue;
	    if (imeta(c)) {
		*bptr++ = Meta;
		*bptr++ = c ^ 32;
	    } else
		*bptr++ = c;
	    /* increase the buffer size, if necessary */
	    if (bptr >= buf + bsiz - 1) {
		int blen = bptr - buf;

		buf = realloc(buf, bsiz *= 2);
		bptr = buf + blen;
	    }
	}
    if (!(ops['z']||ops['Z']))
    	while (bptr > buf && iwsep(bptr[-1]))
				bptr--;
    *bptr = '\0';
    /* final assignment of reply, etc. */
    if (ops['e'] || ops['E']) {
	zputs(buf, stdout);
	if (!ops['N'])
		putchar('\n');
    }
    if (!ops['e'])
	setsparam(reply, buf);
    else
	zsfree(buf);
    if (zbuforig) {
	char first = *zbuforig;

	zsfree(zbuforig);
	if (!first)
	    return 1;
    } else if (c == EOF) {
	if (readfd == coprocin) {
	    close(coprocin);
	    close(coprocout);
	    coprocin = coprocout = -1;
	}
	return 1;
    }
    return 0;
}

/**/
int
zread(void)
{
    char cc, retry = 0;

    /* use zbuf if possible */
    if (zbuf) {
	/* If zbuf points to anything, it points to the next character in the
	buffer.  This may be a null byte to indicate EOF.  If reading from the
	buffer, move on the buffer pointer. */
	if (*zbuf == Meta)
	    return zbuf++, STOUC(*zbuf++ ^ 32);
	else
	    return (*zbuf) ? STOUC(*zbuf++) : EOF;
    }
    for (;;) {
	/* read a character from readfd */
	switch (read(readfd, &cc, 1)) {
	case 1:
	    /* return the character read */
	    return STOUC(cc);
	case -1:
	    if (!retry && errno == EWOULDBLOCK &&
		readfd == 0 && setblock_stdin()) {
		retry = 1;
		continue;
	    }
	    break;
	}
	return EOF;
    }
}

/* sched: execute commands at scheduled times */

/**/
int
bin_sched(char *nam, char **argv, char *ops, int func)
{
    char *s = *argv++;
    time_t t;
    long h, m;
    struct tm *tm;
    struct schedcmd *sch, *sch2, *schl;
    int sn;

    /* If the argument begins with a -, remove the specified item from the
    schedule. */
    if (s && *s == '-') {
	sn = atoi(s + 1);

	if (!sn) {
	    zwarnnam("sched", "usage for delete: sched -<item#>.", NULL, 0);
	    return 1;
	}
	for (schl = (struct schedcmd *)&schedcmds, sch = schedcmds, sn--;
	     sch && sn; sch = (schl = sch)->next, sn--);
	if (!sch) {
	    zwarnnam("sched", "not that many entries", NULL, 0);
	    return 1;
	}
	schl->next = sch->next;
	zsfree(sch->cmd);
	zfree(sch, sizeof(struct schedcmd));

	return 0;
    }

    /* given no arguments, display the schedule list */
    if (!s) {
	char tbuf[40];

	for (sn = 1, sch = schedcmds; sch; sch = sch->next, sn++) {
	    t = sch->time;
	    tm = localtime(&t);
	    ztrftime(tbuf, 20, "%a %b %e %k:%M:%S", tm);
	    printf("%3d %s %s\n", sn, tbuf, sch->cmd);
	}
	return 0;
    } else if (!*argv) {
	/* other than the two cases above, sched *
	 *requires at least two arguments        */
	zwarnnam("sched", "not enough arguments", NULL, 0);
	return 1;
    }

    /* The first argument specifies the time to schedule the command for.  The
    remaining arguments form the command. */
    if (*s == '+') {
	/* + introduces a relative time.  The rest of the argument is an
	hour:minute offset from the current time.  Once the hour and minute
	numbers have been extracted, and the format verified, the resulting
	offset is simply added to the current time. */
	h = zstrtol(s + 1, &s, 10);
	if (*s != ':') {
	    zwarnnam("sched", "bad time specifier", NULL, 0);
	    return 1;
	}
	m = zstrtol(s + 1, &s, 10);
	if (*s) {
	    zwarnnam("sched", "bad time specifier", NULL, 0);
	    return 1;
	}
	t = time(NULL) + h * 3600 + m * 60;
    } else {
	/* If there is no +, an absolute time of day must have been given.
	This is in hour:minute format, optionally followed by a string starting
	with `a' or `p' (for a.m. or p.m.).  Characters after the `a' or `p'
	are ignored. */
	h = zstrtol(s, &s, 10);
	if (*s != ':') {
	    zwarnnam("sched", "bad time specifier", NULL, 0);
	    return 1;
	}
	m = zstrtol(s + 1, &s, 10);
	if (*s && *s != 'a' && *s != 'A' && *s != 'p' && *s != 'P') {
	    zwarnnam("sched", "bad time specifier", NULL, 0);
	    return 1;
	}
	t = time(NULL);
	tm = localtime(&t);
	t -= tm->tm_sec + tm->tm_min * 60 + tm->tm_hour * 3600;
	if (*s == 'p' || *s == 'P')
	    h += 12;
	t += h * 3600 + m * 60;
	/* If the specified time is before the current time, it must refer to
	tomorrow. */
	if (t < time(NULL))
	    t += 3600 * 24;
    }
    /* The time has been calculated; now add the new entry to the linked list
    of scheduled commands. */
    sch = (struct schedcmd *) zcalloc(sizeof *sch);
    sch->time = t;
    PERMALLOC {
	sch->cmd = zjoin(argv, ' ');
    } LASTALLOC;
    sch->next = NULL;
    for (sch2 = (struct schedcmd *)&schedcmds; sch2->next; sch2 = sch2->next);
    sch2->next = sch;
    return 0;
}


/* holds lexer for par_cond():  normally yylex(), testlex() for bin_test() */
extern void (*condlex) _((void));

/* holds arguments for testlex() */
char **testargs;

/* test, [: the old-style general purpose logical expression builtin */

/**/
void
testlex(void)
{
    if (tok == LEXERR)
	return;

    tokstr = *testargs;
    if (!*testargs) {
	/* if tok is already zero, reading past the end:  error */
	tok = tok ? NULLTOK : LEXERR;
	return;
    } else if (!strcmp(*testargs, "-o"))
	tok = DBAR;
    else if (!strcmp(*testargs, "-a"))
	tok = DAMPER;
    else if (!strcmp(*testargs, "!"))
	tok = BANG;
    else if (!strcmp(*testargs, "("))
	tok = INPAR;
    else if (!strcmp(*testargs, ")"))
	tok = OUTPAR;
    else
	tok = STRING;
    testargs++;
}

/**/
int
bin_test(char *name, char **argv, char *ops, int func)
{
    char **s;
    Cond c;

    /* if "test" was invoked as "[", it needs a matching "]" *
     * which is subsequently ignored                         */
    if (func == BIN_BRACKET) {
	for (s = argv; *s; s++);
	if (s == argv || strcmp(s[-1], "]")) {
	    zwarnnam(name, "']' expected", NULL, 0);
	    return 1;
	}
	s[-1] = NULL;
    }
    /* an empty argument list evaluates to false (1) */
    if (!*argv)
	return 1;

    testargs = argv;
    tok = NULLTOK;
    condlex = testlex;
    testlex();
    c = par_cond();
    condlex = yylex;

    if (errflag) {
	errflag = 0;
	return 1;
    }

    if (!c || tok == LEXERR) {
	zwarnnam(name, tokstr ? "parse error" : "argument expected", NULL, 0);
	return 1;
    }

    /* syntax is OK, so evaluate */
    return !evalcond(c);
}

/* display a time, provided in units of 1/60s, as minutes and seconds */
#define pttime(X) printf("%ldm%ld.%02lds",((long) (X))/3600,\
			 ((long) (X))/60%60,((long) (X))*100/60%100)

/* times: display, in a two-line format, the times provided by times(3) */

/**/
int
bin_times(char *name, char **argv, char *ops, int func)
{
    struct tms buf;

    /* get time accounting information */
    if (times(&buf) == -1)
	return 1;
    pttime(buf.tms_utime);	/* user time */
    putchar(' ');
    pttime(buf.tms_stime);	/* system time */
    putchar('\n');
    pttime(buf.tms_cutime);	/* user time, children */
    putchar(' ');
    pttime(buf.tms_cstime);	/* system time, children */
    putchar('\n');
    return 0;
}

/* trap: set/unset signal traps */

/**/
int
bin_trap(char *name, char **argv, char *ops, int func)
{
    List l;
    char *arg, *s;
    int sig;

    if (*argv && !strcmp(*argv, "--"))
	argv++;

    /* If given no arguments, list all currently-set traps */
    if (!*argv) {
	for (sig = 0; sig < VSIGCOUNT; sig++) {
	    if (sigtrapped[sig] & ZSIG_FUNC) {
		char fname[20];
		HashNode hn;

		sprintf(fname, "TRAP%s", sigs[sig]);
		if ((hn = shfunctab->getnode(shfunctab, fname)))
		    shfunctab->printnode(hn, 0);
		DPUTS(!hn, "BUG: I did not find any trap functions!");
	    } else if (sigtrapped[sig]) {
		if (!sigfuncs[sig])
		    printf("trap -- '' %s\n", sigs[sig]);
		else {
		    s = getpermtext((void *) dupstruct((void *) sigfuncs[sig]));
		    printf("trap -- ");
		    quotedzputs(s, stdout);
		    printf(" %s\n", sigs[sig]);
		    zsfree(s);
		}
	    }
	}
	return 0;
    }

    /* If we have a signal number, unset the specified *
     * signals.  With only -, remove all traps.        */
    if ((getsignum(*argv) != -1) || (!strcmp(*argv, "-") && argv++)) {
	if (!*argv)
	    for (sig = 0; sig < VSIGCOUNT; sig++)
		unsettrap(sig);
	else
	    while (*argv)
		unsettrap(getsignum(*argv++));
	return 0;
    }

    /* Sort out the command to execute on trap */
    arg = *argv++;
    if (!*arg)
	l = NULL;
    else if (!(l = parse_string(arg))) {
	zwarnnam(name, "couldn't parse trap command", NULL, 0);
	return 1;
    }

    /* set traps */
    for (; *argv; argv++) {
	List t;

	sig = getsignum(*argv);
	if (sig == -1) {
	    zwarnnam(name, "undefined signal: %s", *argv, 0);
	    break;
	}
	PERMALLOC {
	    t = (List) dupstruct(l);
	} LASTALLOC;
	if (settrap(sig, t))
	    freestruct(t);
    }
    return *argv != NULL;
}

/**/
int
bin_ttyctl(char *name, char **argv, char *ops, int func)
{
    if (ops['f'])
	ttyfrozen = 1;
    else if (ops['u'])
	ttyfrozen = 0;
    else
	printf("tty is %sfrozen\n", ttyfrozen ? "" : "not ");
    return 0;
}

/* let -- mathematical evaluation */

/**/
int
bin_let(char *name, char **argv, char *ops, int func)
{
    long val = 0;

    while (*argv)
	val = matheval(*argv++);
    /* Errors in math evaluation in let are non-fatal. */
    errflag = 0;
    return !val;
}

/* umask command.  umask may be specified as octal digits, or in the  *
 * symbolic form that chmod(1) uses.  Well, a subset of it.  Remember *
 * that only the bottom nine bits of umask are used, so there's no    *
 * point allowing the set{u,g}id and sticky bits to be specified.     */

/**/
int
bin_umask(char *nam, char **args, char *ops, int func)
{
    mode_t um;
    char *s = *args;

    /* Get the current umask. */
    um = umask(0);
    umask(um);
    /* No arguments means to display the current setting. */
    if (!s) {
	if (ops['S']) {
	    char *who = "ugo";

	    while (*who) {
		char *what = "rwx";
		printf("%c=", *who++);
		while (*what) {
		    if (!(um & 0400))
			putchar(*what);
		    um <<= 1;
		    what++;
		}
		putchar(*who ? ',' : '\n');
	    }
	} else {
	    if (um & 0700)
		putchar('0');
	    printf("%03o\n", (unsigned)um);
	}
	return 0;
    }

    if (idigit(*s)) {
	/* Simple digital umask. */
	um = zstrtol(s, &s, 8);
	if (*s) {
	    zwarnnam(nam, "bad umask", NULL, 0);
	    return 1;
	}
    } else {
	/* Symbolic notation -- slightly complicated. */
	int whomask, umaskop, mask;

	/* More than one symbolic argument may be used at once, each separated
	by commas. */
	for (;;) {
	    /* First part of the argument -- who does this apply to?
	    u=owner, g=group, o=other. */
	    whomask = 0;
	    while (*s == 'u' || *s == 'g' || *s == 'o' || *s == 'a')
		if (*s == 'u')
		    s++, whomask |= 0700;
		else if (*s == 'g')
		    s++, whomask |= 0070;
		else if (*s == 'o')
		    s++, whomask |= 0007;
		else if (*s == 'a')
		    s++, whomask |= 0777;
	    /* Default whomask is everyone. */
	    if (!whomask)
		whomask = 0777;
	    /* Operation may be +, - or =. */
	    umaskop = (int)*s;
	    if (!(umaskop == '+' || umaskop == '-' || umaskop == '=')) {
		if (umaskop)
		    zwarnnam(nam, "bad symbolic mode operator: %c", NULL, umaskop);
		else
		    zwarnnam(nam, "bad umask", NULL, 0);
		return 1;
	    }
	    /* Permissions mask -- r=read, w=write, x=execute. */
	    mask = 0;
	    while (*++s && *s != ',')
		if (*s == 'r')
		    mask |= 0444 & whomask;
		else if (*s == 'w')
		    mask |= 0222 & whomask;
		else if (*s == 'x')
		    mask |= 0111 & whomask;
		else {
		    zwarnnam(nam, "bad symbolic mode permission: %c",
			     NULL, *s);
		    return 1;
		}
	    /* Apply parsed argument to um. */
	    if (umaskop == '+')
		um &= ~mask;
	    else if (umaskop == '-')
		um |= mask;
	    else		/* umaskop == '=' */
		um = (um | (whomask)) & ~mask;
	    if (*s == ',')
		s++;
	    else
		break;
	}
	if (*s) {
	    zwarnnam(nam, "bad character in symbolic mode: %c", NULL, *s);
	    return 1;
	}
    }

    /* Finally, set the new umask. */
    umask(um);
    return 0;
}

/*** debugging functions ***/

#ifdef ZSH_HASH_DEBUG
/**/
int
bin_hashinfo(char *nam, char **args, char *ops, int func)
{
    printf("----------------------------------------------------\n");
    cmdnamtab->printinfo(cmdnamtab);
    printf("----------------------------------------------------\n");
    shfunctab->printinfo(shfunctab);
    printf("----------------------------------------------------\n");
    builtintab->printinfo(builtintab);
    printf("----------------------------------------------------\n");
    paramtab->printinfo(paramtab);
    printf("----------------------------------------------------\n");
    compctltab->printinfo(compctltab);
    printf("----------------------------------------------------\n");
    aliastab->printinfo(aliastab);
    printf("----------------------------------------------------\n");
    reswdtab->printinfo(reswdtab);
    printf("----------------------------------------------------\n");
    emkeybindtab->printinfo(emkeybindtab);
    printf("----------------------------------------------------\n");
    vikeybindtab->printinfo(vikeybindtab);
    printf("----------------------------------------------------\n");
    nameddirtab->printinfo(nameddirtab);
    printf("----------------------------------------------------\n");
    return 0;
}
#endif

/**** utility functions -- should go in utils.c ****/

/* Separate an argument into name=value parts, returning them in an     *
 * asgment structure.  Because the asgment structure used is global,    *
 * only one of these can be active at a time.  The string s gets placed *
 * in this global structure, so it needs to be in permanent memory.     */

/**/
Asgment
getasg(char *s)
{
    static struct asgment asg;

    /* sanity check for valid argument */
    if (!s)
	return NULL;

    /* check if name is empty */
    if (*s == '=') {
	zerr("bad assignment", NULL, 0);
	return NULL;
    }
    asg.name = s;

    /* search for `=' */
    for (; *s && *s != '='; s++);

    /* found `=', so return with a value */
    if (*s) {
	*s = '\0';
	asg.value = s + 1;
    } else {
    /* didn't find `=', so we only have a name */
	asg.value = NULL;
    }
    return &asg;
}

/* Get a signal number from a string */

/**/
int
getsignum(char *s)
{
    int x, i;

    /* check for a signal specified by number */
    x = atoi(s);
    if (idigit(*s) && x >= 0 && x < VSIGCOUNT)
	return x;

    /* search for signal by name */
    for (i = 0; i < VSIGCOUNT; i++)
	if (!strcmp(s, sigs[i]))
	    return i;

    /* no matching signal */
    return -1;
}

// end of original builtin.c code [Copyright (c) 1992-1996 Paul Falstad, with some modifications by amacri]

// Here below there is the code added by amacri (developed or imported from other free software packages)

/* ====================================================================================== */
/* = amacri: Busybox builtin commands: http://busybox.net/downloads/  */
/* ====================================================================================== */

#define FILEUTILS_PRESERVE_STATUS 1
#define FILEUTILS_DEREFERENCE 2
#define FILEUTILS_RECUR 4
#define FILEUTILS_FORCE 8
#define FILEUTILS_INTERACTIVE 16

#define OPT_FILEUTILS_FORCE 1
#define OPT_FILEUTILS_INTERACTIVE 2

#ifdef BUSYBOX
/****************************************************************************************************************************************************/

/* Sources based on BUSYBOX: http://busybox.net/downloads/ (until endif)

Imported functions:

coreutils:
 mv.c cp.c ln.c mkdir.c rm.c
 libcoreutils/cp_mv_stat.c

libbb:
 make_directory.c remove_file.c parse_mode.c copy_file.c copyfd.c full_write.c full_read.c safe_write.c
 safe_read.c concat_subpath_file.c concat_path_file.c ask_confirmation.c last_char_is.c bb_asprintf.c
 xreadlink.c xfuncs.c get_last_path_component.c isdirectory.c

*/

typedef int (*stat_func)(const char *fn, struct stat *ps);

int cp_mv_stat2(const char *fn, struct stat *fn_stat, stat_func sf);
int cp_mv_stat(const char *fn, struct stat *fn_stat);
char *bb_xasprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
char *last_char_is(const char *s, int c);
void *xrealloc(void *old, size_t size);
char *xreadlink(const char *path);
int bb_ask_confirmation(void);
char *concat_path_file(const char *path, const char *filename);
char *concat_subpath_file(const char *path, const char *filename);
ssize_t safe_read(int fd, void *buf, size_t count);
ssize_t bb_full_read(int fd, void *buf, size_t len);
ssize_t safe_write(int fd, const void *buf, size_t count);
ssize_t bb_full_write(int fd, const void *buf, size_t len);
char *bb_get_last_path_component(char *path);
int remove_file(char * name, char *path, int flags);
int copy_file(char * name, char *source, char *dest, int flags);
int is_directory(const char *name, int followLinks, struct stat *statBuf);
int bb_make_directory (char *path, long mode, int flags);
int bb_parse_mode(const char *s, mode_t *current_mode);
FILE *bb_wfopen_input(const char *filename);
FILE *bb_wfopen(const char *path, const char *mode);
static size_t bb_full_fd_action(int src_fd, int dst_fd, const size_t size2);

#define FALSE   ((int) 0)
#define TRUE    ((int) 1)
/*
 * Return TRUE if a fileName is a directory.
 * Nonexistent files return FALSE.
 */
int is_directory(const char *fileName, const int followLinks, struct stat *statBuf)
{
	int status;
	struct stat astatBuf;

	if (statBuf == NULL) {
	    /* set from auto stack buffer */
	    statBuf = &astatBuf;
	}

	if (followLinks)
		status = stat(fileName, statBuf);
	else
		status = lstat(fileName, statBuf);

	if (status < 0 || !(S_ISDIR(statBuf->st_mode))) {
	    status = FALSE;
	}
	else status = TRUE;

	return status;
}

/* Set to 1 if you want basename() behavior for NULL or "". */
/* WARNING!!! Doing so will break basename applet at least! */
#define EMULATE_BASENAME	0

char *bb_get_last_path_component(char *path)
{
#if EMULATE_BASENAME
	static const char null_or_empty[] = ".";
#endif
	char *first = path;
	char *last;

#if EMULATE_BASENAME
	if (!path || !*path) {
		return (char *) null_or_empty;
	}
#endif

	last = path - 1;

	while (*path) {
		if ((*path != '/') && (path > ++last)) {
			last = first = path;
		}
		++path;
	}

	if (*first == '/') {
		last = first;
	}
	last[1] = 0;

	return first;
}

int cp_mv_stat2(const char *fn, struct stat *fn_stat, stat_func sf)
{
	if (sf(fn, fn_stat) < 0) {
		if (errno != ENOENT) {
			if (isset(SHOWFSERR))
			   zerr("unable to stat `%s'", (char *)fn,0);
			return -1;
		}
		return 0;
	} else if (S_ISDIR(fn_stat->st_mode)) {
		return 3;
	}
	return 1;
}

extern int cp_mv_stat(const char *fn, struct stat *fn_stat)
{
	return cp_mv_stat2(fn, fn_stat, stat);
}

void *xrealloc(void *ptr, size_t size)
{
        ptr = realloc(ptr, size);
        if ((ptr == NULL) && (size != 0))
                zerr("%e: memory exhausted",NULL,errno);
        return ptr;
}

char *xreadlink(const char *path)
{   
	static const int GROWBY = 80; /* how large we will grow strings by */

	char *buf = NULL;
	int bufsize = 0, readsize = 0;

	do {
		buf = xrealloc(buf, bufsize += GROWBY);
		readsize = readlink(path, buf, bufsize); /* 1st try */
		if (readsize == -1) {
			if (isset(SHOWFSERR))
			   zerr("%s", (char *)path, 0);
			free(buf);
			return NULL;
		}
	}
	while (bufsize < readsize + 1);

	buf[readsize] = '\0';

	return buf;
}

#include <stdarg.h>
char *bb_xasprintf(const char *format, ...)
{
	va_list p;
	int r;
	char *string_ptr;

	va_start(p, format);
	r = vasprintf(&string_ptr, format, p);
	va_end(p);

	if ( r < 0 ) {
		zerr("bb_xasprintf", NULL, 0);
	}
	return string_ptr;
}

char * last_char_is(const char *s, int c)
{
	char *sret = (char *)s;
	if (sret) {
		sret = strrchr(sret, c);
		if(sret != NULL && *(sret+1) != 0)
			sret = NULL;
	}
	return sret;
}

int bb_ask_confirmation(void)
{
	int retval = 0;
	int first = 1;
	int c;
  fflush(stdout);fflush(stderr);
	while (((c = getchar()) != EOF) && (c != '\n')) {
		/* Make sure we get the actual function call for isspace,
		 * as speed is not critical here. */
		if (first && !(isspace)(c)) {
			--first;
			if ((c == 'y') || (c == 'Y')) {
				++retval;
			}
		}
	}

	return retval;
}

char *concat_path_file(const char *path, const char *filename)
{
	char *lc;

	if (!path)
		path = "";
	lc = last_char_is(path, '/');
	while (*filename == '/')
		filename++;
	return bb_xasprintf("%s%s%s", path, (lc==NULL ? "/" : ""), filename);
}

char *concat_subpath_file(const char *path, const char *f)
{
	if(f && *f == '.' && (!f[1] || (f[1] == '.' && !f[2])))
		return NULL;
	return concat_path_file(path, f);
}

ssize_t safe_read(int fd, void *buf, size_t count)
{
	ssize_t n;

	do {
		n = read(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

ssize_t safe_write(int fd, const void *buf, size_t count)
{
	ssize_t n;

	do {
		n = write(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

ssize_t bb_full_read(int fd, void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len > 0) {
		cc = safe_read(fd, buf, len);

		if (cc < 0)
			return cc;	/* read() returns -1 on failure. */

		if (cc == 0)
			break;

		buf = ((char *)buf) + cc;
		total += cc;
		len -= cc;
	}

	return total;
}

ssize_t bb_full_write(int fd, const void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len > 0) {
		cc = safe_write(fd, buf, len);

		if (cc < 0)
			return cc;		/* write() returns -1 on failure. */

		total += cc;
		buf = ((const char *)buf) + cc;
		len -= cc;
	}

	return total;
}

#if BUFSIZ < 4096
#undef BUFSIZ
#define BUFSIZ 4096
#endif

static size_t bb_full_fd_action(int src_fd, int dst_fd, const size_t size2)
{
	int status;
	size_t xread, wrote, total, size = size2;

	if (src_fd < 0) {
		return -1;
	}

	if (size == 0) {
		/* If size is 0 copy until EOF */
		size = ULONG_MAX;
	}

	{
	  char buffer[BUFSIZ];
		total = 0;
		wrote = 0;
		status = -1;
		while (total < size)
		{
			xread = BUFSIZ;
			if (size < (total + BUFSIZ))
				xread = size - total;
			xread = bb_full_read(src_fd, buffer, xread);
			if (xread > 0) {
				if (dst_fd < 0) {
					/* A -1 dst_fd means we need to fake it... */
					wrote = xread;
				} else {
					wrote = bb_full_write(dst_fd, buffer, xread);
				}
				if (wrote < xread) {
					if (isset(SHOWFSERR))
					   zerr("write error", NULL, 0);
					break;
				}
				total += wrote;
			} else if (xread < 0) {
				if (isset(SHOWFSERR))
				   zerr("read error",NULL,0);
				break;
			} else if (xread == 0) {
				/* All done. */
				status = 0;
				break;
			}
		}
	}

	if (status == 0 || total)
		return total;
	/* Some sortof error occured */
	return -1;
}

int copy_file(char *name, char *source, char *dest, int flags)
{
	struct stat source_stat;
	struct stat dest_stat;
	int dest_exists = 0;
	int status = 0;

	if ((!(flags & FILEUTILS_DEREFERENCE) &&
			lstat(source, &source_stat) < 0) ||
			((flags & FILEUTILS_DEREFERENCE) &&
			 stat(source, &source_stat) < 0)) {
		if (isset(SHOWFSERR))
		   zwarnnam(name, "%e: %s", source, errno);
		return -1;
	}

	if (lstat(dest, &dest_stat) < 0) {
		if (errno != ENOENT) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to stat `%s'", dest, errno);
			return -1;
		}
	} else {
		if (source_stat.st_dev == dest_stat.st_dev &&
			source_stat.st_ino == dest_stat.st_ino)
		{
			char ErrString[120];
			sprintf(ErrString, "`%s' and `%s' are the same file", source, dest);
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%s", ErrString, 0);
			return -1;
		}
		dest_exists = 1;
	}

	if (S_ISDIR(source_stat.st_mode)) {
		DIR *dp;
		struct dirent *d;
		mode_t saved_umask = 0;

		if (!(flags & FILEUTILS_RECUR)) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%s: omitting directory", source, 0);
			return -1;
		}

		/* Create DEST.  */
		if (dest_exists) {
			if (!S_ISDIR(dest_stat.st_mode)) {
				if (isset(SHOWFSERR))
				   zwarnnam(name,"`%s' is not a directory", dest,0);
				return -1;
			}
		} else {
			mode_t mode;
			saved_umask = umask(0);

			mode = source_stat.st_mode;
			if (!(flags & FILEUTILS_PRESERVE_STATUS))
				mode = source_stat.st_mode & ~saved_umask;
			mode |= S_IRWXU;

			if (mkdir(dest, mode) < 0) {
				umask(saved_umask);
				if (isset(SHOWFSERR))
				   zwarnnam(name, "%e: cannot create directory `%s'", dest, errno);
				return -1;
			}

			umask(saved_umask);
		}

		/* Recursively copy files in SOURCE.  */
		if ((dp = opendir(source)) == NULL) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to open directory `%s'", source, errno);
			status = -1;
			goto end;
		}

		while ((d = readdir(dp)) != NULL) {
			char *new_source, *new_dest;

			new_source = concat_subpath_file(source, d->d_name);
			if(new_source == NULL)
				continue;
			new_dest = concat_path_file(dest, d->d_name);
			if (copy_file(name, new_source, new_dest, flags) < 0)
				status = -1;
			free(new_source);
			free(new_dest);
		}
		/* closedir have only EBADF error, but "dp" not changes */
		closedir(dp);

		if (!dest_exists &&
				chmod(dest, source_stat.st_mode & ~saved_umask) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to change permissions of `%s'", dest, errno);
			status = -1;
		}
	} else if (S_ISREG(source_stat.st_mode)) {
		int src_fd;
		int dst_fd;
#ifdef CONFIG_FEATURE_PRESERVE_HARDLINKS
		char *link_name;

		if (!(flags & FILEUTILS_DEREFERENCE) &&
				is_in_ino_dev_hashtable(&source_stat, &link_name)) {
			if (link(link_name, dest) < 0) {
				if (isset(SHOWFSERR))
				   zwarnnam(name, "%e: unable to link `%s'", dest, errno);
				return -1;
			}

			return 0;
		}
#endif
		src_fd = open(source, O_RDONLY);
		if (src_fd == -1) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to open `%s'", source, errno);
			return(-1);
		}

		if (dest_exists) {
			if (flags & FILEUTILS_INTERACTIVE) {
				fprintf(stderr, "%s: overwrite `%s'? ", name, dest);
				if (!bb_ask_confirmation()) {
					close (src_fd);
					return 0;
				}
			}

			dst_fd = open(dest, O_WRONLY|O_TRUNC);
			if (dst_fd == -1) {
				if (!(flags & FILEUTILS_FORCE)) {
					if (isset(SHOWFSERR))
					   zwarnnam(name,"%e: unable to open `%s'", dest, errno);
					close(src_fd);
					return -1;
				}

				if (unlink(dest) < 0) {
					if (isset(SHOWFSERR))
					   zwarnnam(name, "%e unable to remove `%s'", dest, errno);
					close(src_fd);
					return -1;
				}

				dest_exists = 0;
			}
		}

		if (!dest_exists) {
			int O_Creat=0;
			if (isset(CREATE))
				O_Creat=O_CREAT;
			dst_fd = open(dest, O_WRONLY|O_Creat, source_stat.st_mode);
			if (dst_fd == -1) {
				if (isset(SHOWFSERR))
				   zwarnnam(name, "%e unable to open `%s'", dest, errno);
				close(src_fd);
				return(-1);
			}
		}

		if (bb_full_fd_action(src_fd, dst_fd,0) == -1)
			status = -1;

		if (close(dst_fd) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to close `%s'", dest, errno);
			status = -1;
		}

		if (close(src_fd) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to close `%s'", source, errno);
			status = -1;
		}
			}
	else if (S_ISBLK(source_stat.st_mode) || S_ISCHR(source_stat.st_mode) ||
	    S_ISSOCK(source_stat.st_mode) || S_ISFIFO(source_stat.st_mode) ||
	    S_ISLNK(source_stat.st_mode)) {

		if (dest_exists) {
			if((flags & FILEUTILS_FORCE) == 0) {
				if (isset(SHOWFSERR))
				   fprintf(stderr, "`%s' exists\n", dest);
				return -1;
			}
			if(unlink(dest) < 0) {
				if (isset(SHOWFSERR))
				   zwarnnam(name, "%e: unable to remove `%s'", dest, errno);
				return -1;
			}
		}
	} else {
		if (isset(SHOWFSERR))
		   zwarnnam(name, "internal error: unrecognized file type", NULL, 0);
		return -1;
		}
	if (S_ISBLK(source_stat.st_mode) || S_ISCHR(source_stat.st_mode) ||
	    S_ISSOCK(source_stat.st_mode)) {
		if (mknod(dest, source_stat.st_mode, source_stat.st_rdev) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name,"%e: unable to create `%s'", dest, errno);
			return -1;
		}
	} else if (S_ISFIFO(source_stat.st_mode)) {
		if (mkfifo(dest, source_stat.st_mode) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: cannot create fifo `%s'", dest, errno);
			return -1;
		}
	} else if (S_ISLNK(source_stat.st_mode)) {
		char *lpath;

		lpath = xreadlink(source);
		if (symlink(lpath, dest) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: cannot create symlink `%s'", dest, errno);
			return -1;
		}
		free(lpath);

#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
		if (flags & FILEUTILS_PRESERVE_STATUS)
			if (lchown(dest, source_stat.st_uid, source_stat.st_gid) < 0)
				{
				if (isset(SHOWFSERR))
				   zwarnnam(name, "%e: unable to preserve ownership of `%s'", dest, errno);
				}
#endif

#ifdef CONFIG_FEATURE_PRESERVE_HARDLINKS
		add_to_ino_dev_hashtable(&source_stat, dest);
#endif

		return 0;
	}

#ifdef CONFIG_FEATURE_PRESERVE_HARDLINKS
	if (! S_ISDIR(source_stat.st_mode)) {
		add_to_ino_dev_hashtable(&source_stat, dest);
	}
#endif

end:

	if (flags & FILEUTILS_PRESERVE_STATUS) {
		struct utimbuf times;

		times.actime = source_stat.st_atime;
		times.modtime = source_stat.st_mtime;
		if (utime(dest, &times) < 0)
			{
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to preserve times of `%s'", dest, errno);
			}
		if (chown(dest, source_stat.st_uid, source_stat.st_gid) < 0) {
			source_stat.st_mode &= ~(S_ISUID | S_ISGID);
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to preserve ownership of `%s'", dest, errno);
		}
		if (chmod(dest, source_stat.st_mode) < 0)
			{
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to preserve permissions of `%s'", dest, errno);
			}
	}

	return status;
}

#define FILEMODEBITS    (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)

#include <assert.h>
int bb_parse_mode(const char *s, mode_t *current_mode)
{
	static const mode_t who_mask[] = {
		S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO, /* a */
		S_ISUID | S_IRWXU,		/* u */
		S_ISGID | S_IRWXG,		/* g */
		S_IRWXO					/* o */
	};

	static const mode_t perm_mask[] = {
		S_IRUSR | S_IRGRP | S_IROTH, /* r */
		S_IWUSR | S_IWGRP | S_IWOTH, /* w */
		S_IXUSR | S_IXGRP | S_IXOTH, /* x */
		S_IXUSR | S_IXGRP | S_IXOTH, /* X -- special -- see below */
		S_ISUID | S_ISGID,		/* s */
		S_ISVTX					/* t */
	};

	static const char who_chars[] = "augo";
	static const char perm_chars[] = "rwxXst";

	const char *p;

	mode_t wholist;
	mode_t permlist;
	mode_t mask;
	mode_t new_mode;
	char op;

	assert(s);

	if (((unsigned int)(*s - '0')) < 8) {
		unsigned long tmp;
		char *e;

		tmp = strtol(s, &e, 8);
		if (*e || (tmp > 07777U)) { /* Check range and trailing chars. */
			return 0;
		}
		*current_mode = tmp;
		return 1;
	}

	mask = umask(0);
	umask(mask);

	new_mode = *current_mode;

	/* Note: We allow empty clauses, and hence empty modes.
	 * We treat an empty mode as no change to perms. */

	while (*s) {	/* Process clauses. */

		if (*s == ',') {	/* We allow empty clauses. */
			++s;
			continue;
		}

		/* Get a wholist. */
		wholist = 0;

	WHO_LIST:
		p = who_chars;
		do {
			if (*p == *s) {
				wholist |= who_mask[(int)(p-who_chars)];
				if (!*++s) {
					return 0;
				}
				goto WHO_LIST;
			}
		} while (*++p);

		do {	/* Process action list. */
			if ((*s != '+') && (*s != '-')) {
				if (*s != '=') {
					return 0;
				}
				/* Since op is '=', clear all bits corresponding to the
				 * wholist, of all file bits if wholist is empty. */
				permlist = ~FILEMODEBITS;
				if (wholist) {
					permlist = ~wholist;
				}
				new_mode &= permlist;
			}
			op = *s++;

			/* Check for permcopy. */
			p = who_chars + 1;	/* Skip 'a' entry. */
			do {
				if (*p == *s) {
					int i = 0;
					permlist = who_mask[(int)(p-who_chars)]
							 & (S_IRWXU | S_IRWXG | S_IRWXO)
							 & new_mode;
					do {
						if (permlist & perm_mask[i]) {
							permlist |= perm_mask[i];
						}
					} while (++i < 3);
					++s;
					goto GOT_ACTION;
				}
			} while (*++p);

			/* It was not a permcopy, so get a permlist. */
			permlist = 0;

		PERM_LIST:
			p = perm_chars;
			do {
				if (*p == *s) {
					if ((*p != 'X')
						|| (new_mode & (S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH))
					) {
						permlist |= perm_mask[(int)(p-perm_chars)];
					}
					if (!*++s) {
						break;
					}
					goto PERM_LIST;
				}
			} while (*++p);

		GOT_ACTION:
			if (permlist) {	/* The permlist was nonempty. */
				mode_t tmp = ~mask;
				if (wholist) {
					tmp = wholist;
				}
				permlist &= tmp;

				if (op == '-') {
					new_mode &= ~permlist;
				} else {
					new_mode |= permlist;
				}
			}
		} while (*s && (*s != ','));
	}

	*current_mode = new_mode;

	return 1;
}


int remove_file(char *name, char *path, int flags)
{
	struct stat path_stat;
	int path_exists = 1;

	if (lstat(path, &path_stat) < 0) {
		if (errno != ENOENT) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to stat `%s'", path, errno);
			return -1;
		}

		path_exists = 0;
	}

	if (!path_exists) {
		if (!(flags & FILEUTILS_FORCE)) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: cannot remove `%s'", path, errno);
			return -1;
		}
		return 0;
	}

	if (S_ISDIR(path_stat.st_mode)) {
		DIR *dp;
		struct dirent *d;
		int status = 0;

		if (!(flags & FILEUTILS_RECUR)) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%s: is a directory", path, 0);
			return -1;
		}

		if ((!(flags & FILEUTILS_FORCE) && access(path, W_OK) < 0 &&
					isatty(0)) ||
				(flags & FILEUTILS_INTERACTIVE)) {
			if (isset(SHOWFSERR))
			   fprintf(stderr, "%s: descend into directory `%s'? ", name,
					path);
			if (!bb_ask_confirmation())
				return 0;
		}

		if ((dp = opendir(path)) == NULL) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to open `%s'", path, errno);
			return -1;
		}

		while ((d = readdir(dp)) != NULL) {
			char *new_path;

			new_path = concat_subpath_file(path, d->d_name);
			if(new_path == NULL)
				continue;
			if (remove_file(name, new_path, flags) < 0)
				status = -1;
			free(new_path);
		}

		if (closedir(dp) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to close `%s'", path, errno);
			return -1;
		}

		if (flags & FILEUTILS_INTERACTIVE) {
			if (isset(SHOWFSERR))
			   fprintf(stderr, "%s: remove directory `%s'? ", name, path);
			if (!bb_ask_confirmation())
				return status;
		}

		if (rmdir(path) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to remove `%s'", path, errno);
			return -1;
		}

		return status;
	} else {
		if ((!(flags & FILEUTILS_FORCE) && access(path, W_OK) < 0 &&
					!S_ISLNK(path_stat.st_mode) &&
					isatty(0)) ||
				(flags & FILEUTILS_INTERACTIVE)) {
			   fprintf(stderr, "%s: remove `%s'? ", name, path);
			if (!bb_ask_confirmation())
				return 0;
		}

		if (unlink(path) < 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%e: unable to remove `%s'", path, errno);
			return -1;
		}

		return 0;
	}
}

int bb_make_directory (char *path, long mode, int flags)
{
	mode_t mask;
	const char *fail_msg;
	char *s = path;
	char c;
	struct stat st;

	mask = umask(0);
	if (mode == -1) {
		umask(mask);
		mode = (S_IXUSR | S_IXGRP | S_IXOTH |
				S_IWUSR | S_IWGRP | S_IWOTH |
				S_IRUSR | S_IRGRP | S_IROTH) & ~mask;
	} else {
		umask(mask & ~0300);
	}

	do {
		c = 0;

		if (flags & FILEUTILS_RECUR) {	/* Get the parent. */
			/* Bypass leading non-'/'s and then subsequent '/'s. */
			while (*s) {
				if (*s == '/') {
					do {
						++s;
					} while (*s == '/');
					c = *s;		/* Save the current char */
					*s = 0;		/* and replace it with nul. */
					break;
				}
				++s;
			}
		}

		if (mkdir(path, 0777) < 0) {
			/* If we failed for any other reason than the directory
			 * already exists, output a diagnostic and return -1.*/
			if (errno != EEXIST
				|| !(flags & FILEUTILS_RECUR)
				|| (stat(path, &st) < 0 || !S_ISDIR(st.st_mode))) {
				fail_msg = "create";
				umask(mask);
				break;
			}
			/* Since the directory exists, don't attempt to change
			 * permissions if it was the full target.  Note that
			 * this is not an error conditon. */
			if (!c) {
				umask(mask);
				return 0;
			}
		}

		if (!c) {
			/* Done.  If necessary, updated perms on the newly
			 * created directory.  Failure to update here _is_
			 * an error.*/
			umask(mask);
			if ((mode != -1) && (chmod(path, mode) < 0)){
				fail_msg = "set permissions of";
				break;
			}
			return 0;
		}

		/* Remove any inserted nul from the path (recursive mode). */
		*s = c;

	} while (1);

	if (isset(SHOWFSERR))
	   zwarnnam( (char *) fail_msg, "Cannot %s directory `%s'", (char *) path, 0);
	return -1;
}

/**/
int
bin_zrd(char *name, char **argv, char *ops, int func)
{
	int status = EXIT_SUCCESS;
	int flags;
	int do_dot;
	char *path;

	flags=0;
	if (ops['p'])
		flags = FILEUTILS_RECUR;

	if (!*argv) {
          zwarnnam(name, "Usage error: rmdir [-p] directory directory...", NULL, 0);
          return 2;
	}

	do {
		path = *argv;

		/* Record if the first char was a '.' so we can use dirname later. */
		do_dot = (*path == '.');

		do {
			if (rmdir(path) < 0) {
				if (isset(SHOWFSERR))
					   zwarnnam(name, "failed to remove `%s': %e", *argv, errno);
				status = EXIT_FAILURE;
			} else if (flags) {
				/* Note: path was not empty or null since rmdir succeeded. */
				path = (char *) dirname(path);
				/* Path is now just the parent component.  Note that dirname
				 * returns "." if there are no parents.  We must distinguish
				 * this from the case of the original path starting with '.'.
                 */
				if (do_dot || (*path != '.') || path[1]) {
					continue;
				}
			}
			break;
		} while (1);

	} while (*++argv);

	return status;
}


static const char cp_opts[] = "pdRfiarPHL";

/**/
int
bin_zcp(char *name, char **argv, char *ops, int func)
{
	struct stat source_stat;
	struct stat dest_stat;
	const char *last;
	const char *dest;
	int s_flags;
	int d_flags;
	int flags, argc;
	int status = 0;

flags=0;
if (ops['p'])
	flags |= FILEUTILS_PRESERVE_STATUS;
if (ops['d'])
	flags |= FILEUTILS_DEREFERENCE;
if (ops['R'])
	flags |= FILEUTILS_RECUR;
if (ops['f'])
	flags |= FILEUTILS_FORCE;
if (ops['i'])
	flags |= FILEUTILS_INTERACTIVE;
if (ops['a'])
		flags |= (FILEUTILS_PRESERVE_STATUS | FILEUTILS_RECUR | FILEUTILS_DEREFERENCE);
if (ops['r'])
		/* Make -r a synonym for -R,
		 * -r was marked as obsolete in SUSv3, but is included for compatability
		 */
		flags |= FILEUTILS_RECUR;
if (ops['P'])
		/* Make -P a synonym for -d,
		 * -d is the GNU option while -P is the POSIX 2003 option
		 */
		flags |= FILEUTILS_DEREFERENCE;

	flags ^= FILEUTILS_DEREFERENCE;		/* The sense of this flag was reversed. */
  
  argc = arrlen(argv);
	if (argc < 2) {
          zwarnnam(name, "Usage error: zcp [-pdRfiarP] file file ...", NULL, 0);
          return 2;
	}
	last = argv[argc - 1];

	/* If there are only two arguments and...  */
	if (argc == 2) {
		s_flags = cp_mv_stat2(*argv, &source_stat,
		                      (flags & FILEUTILS_DEREFERENCE) ? stat : lstat);
		if ((s_flags < 0) || ((d_flags = cp_mv_stat(last, &dest_stat)) < 0)) {
			return(EXIT_FAILURE);
		}
		/* ...if neither is a directory or...  */
		if ( !((s_flags | d_flags) & 2) ||
			/* ...recursing, the 1st is a directory, and the 2nd doesn't exist... */
			/* ((flags & FILEUTILS_RECUR) && (s_flags & 2) && !d_flags) */
			/* Simplify the above since FILEUTILS_RECUR >> 1 == 2. */
			((((flags & FILEUTILS_RECUR) >> 1) & s_flags) && !d_flags)
		) {
			/* ...do a simple copy.  */
			dest = last;
			goto DO_COPY; /* Note: optind+2==argc implies argv[1]==last below. */
		}
	}

	do {
		dest = concat_path_file(last, bb_get_last_path_component(*argv));
	DO_COPY:
		if (copy_file(name, *argv, (char *)dest, flags) < 0) {
			status = 1;
		}
		if (*++argv == last) {
			break;
		}
		free((void *) dest);
	} while (1);

	return(status);
}

FILE *bb_wfopen(const char *path, const char *mode)
{
        FILE *fp;
        if ((fp = fopen(path, mode)) == NULL) {
					if (isset(SHOWFSERR))
						zwarnnam("bcat", "%s %e", (char *) path, errno);
		      errno = 0;
        }
        return fp;
}

FILE *bb_wfopen_input(const char *filename)
{
        FILE *fp = stdin;

        if ((filename != "standard input")
                && filename[0] && ((filename[0] != '-') || filename[1])
        ) {
#if 0
                /* This check shouldn't be necessary for linux, but is left
                 * here disabled just in case. */
                struct stat stat_buf;
                if (is_directory(filename, 1, &stat_buf)) {
								if (isset(SHOWFSERR))
									zwarnnam("bcat", "%s: Is a directory", filename);
					      return NULL;
                }
#endif
                fp = bb_wfopen(filename, "r");
        }

        return fp;
}

/**/
int
bin_bcat(char *name, char **argv, char *ops, int func)
{
        FILE *f;
        int retval = EXIT_SUCCESS;

        if (!*argv) {
                *--argv = "-";
        }

        do {
                if ((f = bb_wfopen_input(*argv)) != NULL) {
			int r = bb_full_fd_action(fileno(f), STDOUT_FILENO, 0);
			if (f != stdin) {
				fclose(f);
			}
                        if (r >= 0) {
                                continue;
                        }
                }
                retval = EXIT_FAILURE;
        } while (*++argv);

        return retval;
}

/**/
int
bin_ztouch(char *name, char **argv, char *ops, int func)
{
	int fd;
	int flags;
	int status = 0;

  struct utimbuf tb, * t;

	t = NULL;
	if (ops['D']) {
		tb.actime = tb.modtime = matheval(*argv++);
	  t = & tb;
		if (tb.actime <0) {
	          zwarnnam(name, "Invalid option <seconds>; usage: ztouch [-c] [-D <seconds>] file file ...", NULL, 0);
	          return 2;
		}
  }

	flags=0;
	if (ops['c'])
		flags=1;

	if (!*argv) {
          zwarnnam(name, "Usage error: ztouch [-c] [-D <seconds>] file file ...", NULL, 0);
          return 2;
	}

	do {
		if (utime(*argv, t)) {
			if (errno == ENOENT) {	/* no such file*/
				if (flags) {	/* Creation is disabled, so ignore. */
			    status = 1;
					if (isset(SHOWFSERR))
						   zwarnnam(name, "file does not exist: `%s'", *argv,0);
					continue;
				}
				/* Try to create the file. */
				fd = open(*argv, O_RDWR | O_CREAT,
						  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
						  );
				if ((fd >= 0) && !close(fd)) {
						utime(*argv, t);
						continue;
				}
			}
			status = 1;
			if (isset(SHOWFSERR))
				   zwarnnam(name, "cannot ztouch `%s': %e", *argv,errno);
		}
	} while (*++argv);

	return status;
}

/**/
int
bin_zmv(char *name, char **argv, char *ops, int func)
{
	struct stat dest_stat;
	const char *last;
	const char *dest;
	unsigned long flags;
	int dest_exists, argc;
	int status = 0;

flags=0;
if (ops['f'])
	flags |= FILEUTILS_FORCE;
if (ops['i'])
	flags |= FILEUTILS_INTERACTIVE;

  argc = arrlen(argv);
	if (argc < 2) {
          zwarnnam(name, "Usage error: zmv [-fi] file file ...", NULL, 0);
          return 2;
	}
	last = argv[argc - 1];

	if (argc == 2) {
		if ((dest_exists = cp_mv_stat(last, &dest_stat)) < 0) {
			return 1;
		}

		if (!(dest_exists & 2)) {
			dest = last;
			goto DO_MOVE;
		}
	}

	do {
		dest = concat_path_file(last, bb_get_last_path_component(*argv));

		if ((dest_exists = cp_mv_stat(dest, &dest_stat)) < 0) {
			goto RET_1;
		}

DO_MOVE:

		if (dest_exists && !(flags & OPT_FILEUTILS_FORCE) &&
			((access(dest, W_OK) < 0 && isatty(0)) ||
			(flags & OPT_FILEUTILS_INTERACTIVE))) {
			if (fprintf(stderr, "mv: overwrite `%s'? ", dest) < 0) {
				goto RET_1;	/* Ouch! fprintf failed! */
			}
			if (!bb_ask_confirmation()) {
				goto RET_0;
			}
		}
		if (rename(*argv, dest) < 0) {
			struct stat source_stat;
			int source_exists;

			if (errno != EXDEV ||
				(source_exists = cp_mv_stat(*argv, &source_stat)) < 1) {
				if (isset(SHOWFSERR))
				   zwarnnam(name, "unable to rename `%s'", *argv,0);
			} else {
				if (dest_exists) {
					if (dest_exists == 3) {
						if (source_exists != 3) {
							if (isset(SHOWFSERR))
							   zwarnnam(name, "destination exists",NULL,0);
							goto RET_1;
						}
					} else {
						if (source_exists == 3) {
							if (isset(SHOWFSERR))
							   zwarnnam(name, "source exists", NULL,0);
							goto RET_1;
						}
					}
					if (unlink(dest) < 0) {
						if (isset(SHOWFSERR))
						   zwarnnam(name, "cannot remove `%s'", (char *)dest,0);
						goto RET_1;
					}
				}
				if ((copy_file(name, *argv, (char *)dest,
					FILEUTILS_RECUR | FILEUTILS_PRESERVE_STATUS) >= 0) &&
					(remove_file(name, *argv, FILEUTILS_RECUR | FILEUTILS_FORCE) >= 0)) {
					goto RET_0;
				}
			}
RET_1:
			status = 1;
		}
RET_0:
		if (dest != last) {
			free((void *) dest);
		}
	} while (*++argv != last);

	return (status);
}

/**/
int
bin_zrm(char *name, char **argv, char *ops, int func)
{
	int status = 0;
	int flags = 0;	

flags=0;
if (ops['f'])
	flags |= FILEUTILS_FORCE;
if (ops['i'])
	flags |= FILEUTILS_INTERACTIVE;
if (ops['R'])
	flags |= FILEUTILS_RECUR;
if (ops['r'])
	flags |= FILEUTILS_RECUR;

	if (*(argv ) != NULL) {
		do {
			const char *base = bb_get_last_path_component(*argv);

			if ((base[0] == '.') && (!base[1] || ((base[1] == '.') && !base[2]))) {
				if (isset(SHOWFSERR))
				   zwarnnam(name, "cannot remove `.' or `..'", NULL,0);
			} else if (remove_file(name, *argv, flags) >= 0) {
				continue;
			}
			status = 1;
		} while (*++argv);
	} else if (!(flags & FILEUTILS_FORCE)) {
				   zwarnnam(name, "usage error", NULL,0);
	}

	return status;
}

#define LN_SYMLINK          1
#define LN_FORCE            2
#define LN_NODEREFERENCE    4
#define LN_BACKUP           8
#define LN_SUFFIX           16

/**/
int
bin_zln(char *name, char **argv, char *ops, int func)
{
	int status = EXIT_SUCCESS;
	int flag, argc;
	char *last;
	char *src_name;
	char *src;
	char *suffix = "~";
	struct stat statbuf;
	int (*link_func)(const char *, const char *);

flag=0;
if (ops['s'])
	flag |= LN_SYMLINK;
if (ops['f'])
	flag |= LN_FORCE;
if (ops['n'])
	flag |= LN_NODEREFERENCE;
if (ops['b'])
	flag |= LN_BACKUP;
if (ops['S'])
{
	flag |= LN_SUFFIX;
	if (*argv)
	    suffix=*argv++;
}

  argc = arrlen(argv);
	last = argv[argc - 1];

	if (argc < 1) {
          zwarnnam(name, "Usage error: zln [-sfnbS] file file", NULL, 0);
          return 2;
	}
	if (argc == 1) {
		*--argv = last;
		last = bb_get_last_path_component(strdup(last));
	}

	do {
		src_name = NULL;
		src = last;

		if (is_directory(src,
						 (flag & LN_NODEREFERENCE) ^ LN_NODEREFERENCE,
						 NULL)) {
			src_name = strdup(*argv);
			src = concat_path_file(src, bb_get_last_path_component(src_name));
			free(src_name);
			src_name = src;
		}
		if (!(flag & LN_SYMLINK) && stat(*argv, &statbuf)) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%s", *argv,0);
			status = EXIT_FAILURE;
			free(src_name);
			continue;
		}

		if (flag & LN_BACKUP) {
				char *backup;
				backup = bb_xasprintf("%s%s", src, suffix);
				if (rename(src, backup) < 0 && errno != ENOENT) {
						if (isset(SHOWFSERR))
						   zwarnnam(name, "%s", src,0);
						status = EXIT_FAILURE;
						free(backup);
						continue;
				}
				free(backup);
				/*
				 * When the source and dest are both hard links to the same
				 * inode, a rename may succeed even though nothing happened.
				 * Therefore, always unlink().
				 */
				unlink(src);
		} else if (flag & LN_FORCE) {
			unlink(src);
		}

		link_func = link;
		if (flag & LN_SYMLINK) {
			link_func = symlink;
		}

		if (link_func(*argv, src) != 0) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "%s", src,0);
			status = EXIT_FAILURE;
		}

		free(src_name);

	} while ((++argv)[1]);

	return status;
}

/**/
int
bin_zmd(char *name, char **argv, char *ops, int func)
{
	mode_t mode = (mode_t)(-1);
	int status = EXIT_SUCCESS;
	int flags = 0;

	if (! *argv) {
          zwarnnam(name, "Usage error: zmd [-mp] directory", NULL, 0);
          return 2;
	}

flags=0;
if (ops['m'])
{
	  mode = 0777;
		if (!bb_parse_mode (*argv, &mode)) {
			if (isset(SHOWFSERR))
			   zwarnnam(name, "invalid mode `%s'", *argv, 0);
			status = EXIT_FAILURE;
			return status;
			}
	  *argv++;
}
if (ops['p'])
		flags |= FILEUTILS_RECUR;

	do {
		if (bb_make_directory(*argv, mode, flags)) {
			status = EXIT_FAILURE;
		}
	} while (*++argv);

	return status;
}

#endif /* BUSYBOX */

/* ====================================================================================== */
/* = amacri, 2006: Framebuffer and Touchscreen Functions */
/* ====================================================================================== */

#include "fb.h"

extern int fb_FontX, fb_FontY, fb_MaxX, fb_MaxY;
extern int fbfd, tsfd, fb_rotate, fb_x, fb_y, fb_xs, fb_ys, fb_col, fb_font, fb_NumInk, fb_BgCol, ts_x, ts_y, ts_pen;
extern unsigned short* fb;

/**/
int
bin_fb_init(char *name, char **argv, char *ops, int func)
{
	fb_init();
	if(fb == NULL)
		return 1;

		/* clear screen & go to top */
	if (fb_col == -1)
	{
	   fb_rect(0, 0, fb_MaxX-1, fb_MaxY-1, fb_colour(0xff, 0xff, 0xff));
		 fb_col=0;
  }

  if ( (fb_x < 0) || (fb_y < 0) ) 
    fb_x = fb_y = 0;

	if ((TsScreen_Init()) == -1)
	   return 2;
	  // flush pen input
  {
		ts_x=0; ts_y=0; ts_pen=0;
    while (TsScreen_pen(&ts_x,&ts_y,&ts_pen));
  }

	return 0;
}

// amacri, 2006
/**/
int
bin_fb_done(char *name, char **argv, char *ops, int func)
{
	  // flush pen input
  {
		ts_x=0; ts_y=0; ts_pen=0;
    while (TsScreen_pen(&ts_x,&ts_y,&ts_pen));
  }
	if(fb == NULL)
		return 1;
	fb_done();
	fb = NULL;
  if ((TsScreen_Exit())<0)
     return 2;
	return 0;
}

// amacri, 2006
/**/
int
bin_fb_rect(char *name, char **argv, char *ops, int func)
{
int x1=-1, y1=-1, x2=-1, y2=-1, col=-1;

	if(fb == NULL)
			fb_init();
	if(fb == NULL)
		return 1;
	if (fb_col < 0)
	  fb_col=0;

    if (*argv)
        x1 = matheval(*argv++);
    if (*argv)
        y1 = matheval(*argv++);
    if (*argv)
        x2 = matheval(*argv++);
    if (*argv)
        y2 = matheval(*argv++);
    if (*argv)
        col = matheval(*argv++);
 
    if ( (x1 < 0) || (y1 < 0) || (x2 < 0) || (y2 < 0) || (col < 0) ) {
           zwarnnam(name, "argument(s) must be non-negative", NULL, 0);
        return 2;
    }
	
	fb_rect(x1, y1, x2, y2, col);
	return 0;
}

// amacri, 2006
/**/
int
bin_fb_printxy(char *name, char **argv, char *ops, int func)
{
    if (*argv)
        fb_x = matheval(*argv++);
    if (*argv)
        fb_y = matheval(*argv++);
    if (*argv)
        fb_col = matheval(*argv++);

	if (fb_col < 0)
	  fb_col=0;
 
    if ( (fb_x < 0) || (fb_y < 0) || (fb_col < 0) ) {
        zwarnnam(name, "argument(s) must be non-negative", NULL, 0);
        fb_x = fb_y = fb_col = 0;
        return 2;
    }
	
	  return(bin_fb_print(name, argv, ops, func));
}

// amacri, 2006
/**/
int
bin_ts_press(char *name, char **argv, char *ops, int func)
{
    double secs=60;
    int timeout=0;

//		ts_x=0; ts_y=0; ts_pen=0;
//		while (!TsScreen_pen(&ts_x,&ts_y,&ts_pen));

    if (*argv)
       secs = matheval(*argv);
    if (* ++argv)
       timeout = (int) matheval(*argv);
		return (ts_press(secs,&ts_x,&ts_y,&ts_pen,timeout));
}

struct sigaction almact;

/**/
RETSIGTYPE
almhandler(int sig)
{
return;
}

/**/
int
bin_alarm(char *name, char **argv, char *ops, int func)
{
    struct sigaction almact;
    double secs=0, secsi=0;

    if (*argv)
        secs = matheval(*argv++);
    else return 0;
    secsi=secs;
    if (*argv)
        secsi = matheval(*argv++);
    if ((secs<0) || (secsi<0))
        return -2;

    almact.sa_handler = (SIGNAL_HANDTYPE) almhandler;
    sigemptyset(&almact.sa_mask);        /* only block sig while in handler */
    almact.sa_flags = 0;
    almact.sa_flags |= SA_RESTART; /* Restart functions if interrupted by handler */
    if (!ops['k'])
       sigaction(SIGALRM, &almact, (struct sigaction *)NULL);
    return (ualarm( (int) (secs*1000000), (int) (secsi*1000000) ) / 1000);
}

// amacri, 2006
/**/
int
bin_fb_print(char *name, char **argv, char *ops, int func)
{
	char *c;

	while ((c = *argv++ ) != NULL)
	 if (fbprintf("%s", c) == 0)
	    return 1;
	return 0;
}

#ifdef ARM
#include <linux/fb.h>
extern struct fb_var_screeninfo vinfo;
#endif
extern long int screensize;
extern char SystemShortName[];
extern char SystemTFTType[];
extern char SystemModelName[];

/**/
int
bin_fb_dump(char *name, char **argv, char *ops, int func)
{
	char buf[32]="Unknown";
	int i;

	if(fb == NULL)
			fb_init();

if (ops['v'])
{
#ifdef ARM
	printf("ARM Model Name: %s\n", SystemModelName);
	printf("System Short Name: %s\n", SystemShortName);
	printf("TFT Display Type: %s\n", SystemTFTType);
#else
	printf("LINUX System Name: %s\n", SystemShortName);
#endif
	printf("\nFramebuffer:\n");
	printf("- File descriptor: %d\n", fbfd);
	printf("- Memory map pointer: %d\n", (int) fb);
	#ifdef ARM
	printf("- X visible resolution: %d\n", vinfo.xres);
	printf("- Y visible resolution: %d\n", vinfo.yres);
	printf("- bits per pixel: %d\n", vinfo.bits_per_pixel);
	#else
	printf("- X visible resolution: %d\n", -1);
	printf("- Y visible resolution: %d\n", -1);
	printf("- bits per pixel: %d\n", -1);
	#endif
	printf("- Screen size: %d\n", (int) screensize);
	printf("- Screen is rotated: %d\n", fb_rotate);
	printf("- Actual X size of the screen after rotation: %d\n", fb_MaxX);
	printf("- Actual Y size of the screen after rotation: %d\n", fb_MaxY);
	printf("\nFont:\n");
	printf("- X size: %d\n", fb_FontX);
	printf("- Y size: %d\n", fb_FontY);
	printf("\nCursor:\n");
	printf("- X position: %d\n", fb_x);
	printf("- Y position: %d\n", fb_y);
	printf("- X shift: %d\n", fb_xs);
	printf("- Y shift: %d\n", fb_ys);
	printf("- Ink colour: %d\n", fb_col);
	printf("- Ink colour when numeric char: %d\n", fb_NumInk);
	printf("- Background colour: %d\n", fb_BgCol);
	printf("\nTouchscreen:\n");
	printf("- File descriptor: %d\n", tsfd);
	printf("- x: %d\n", ts_x);
	printf("- y: %d\n", ts_y);
	printf("- pen: %d\n", ts_pen);
}

#define MAXSCRD 26
for (i=0;i<MAXSCRD;i++)
   if (screendata[i]==NULL)
      break;
if (++i<MAXSCRD)
   {
    for (i=0;i<MAXSCRD;i++)
      if (screendata[i]>0)
         zfree(screendata[i],0);
      else if (screendata[i]==0)
         break;
    zfree(screendata,0);
    screendata    = (char **) zcalloc(sizeof(*screendata) * MAXSCRD);
    screendata[MAXSCRD-1] = NULL;
   }

i=-1;
#ifdef ARM
if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup("ARM");
#else
if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup("LINUX");
#endif
sprintf(buf, "%d",fb_x); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_y); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_col); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
#ifdef ARM
sprintf(buf, "%d",vinfo.xres); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",vinfo.yres); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",vinfo.bits_per_pixel); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
#else
strcpy(buf,"not supported");
if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
#endif
sprintf(buf, "%d",fb_MaxX); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_MaxY); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_FontX); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_FontY); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%ld",screensize); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(SystemShortName);
sprintf(buf, "%d",fb_rotate); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fbfd); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d", (int) fb); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",tsfd); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);

sprintf(buf, "%d",ts_x); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",ts_y); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",ts_pen); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_BgCol); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_NumInk); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_xs); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
sprintf(buf, "%d",fb_ys); if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(buf);
if (screendata[++i]>0) zfree(screendata[i],0); screendata[i] = ztrdup(SystemModelName);
return ++i;
}

/* ====================================================================================== */
/* = amacri: sun elevation (suna) functions (horizontal coordinates of the sun position) */
/* ====================================================================================== */

/* Sources based on potm.c, found in Internet */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

/**********************************************/
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef ARM
#define _timezone timezone
#define _daylight daylight
#endif
/**********************************************/

void CalStuff(long tim);
void timedata(long tim);
double replace_fmod(double	x, double	m);

/* useful trigonometric costants */

#define f1		(1. - (1./298.25))	/* 1-flattening of Earth     */
#define Sec_per_day		(24 * 60 * 60)
#define Asec_Radian		((2.0 * M_PI)/(360. * 60. * 60.)) // seconds (not degrees) to radians
#define Tsec_to_Radian		((2.0 * M_PI)/( 24. * 60.* 60.))
#define	J1970	/* 24 */40587.5	/* UNIX clock Epoch 1970 Jan 1 (0h UT) */
#define	J1985	/* 24 */46065.5	/* Epoch 1985 Jan 1 (0h UT) */
#define	J2000	/* 24 */51545.0	/* Epoch 2000 Jan 1 (12h UT) */
#define Delta_T		(54.6 + 0.9*(Julian_Day - J1985)/365.)	/* TDT - UT */
#define Round			0.5		/* for rounding to integer */

long	UTC, localdst;
double	Julian_Day, MJD, Tu, Ru, T70, Local, GMST, LST;
double	la, lf, S, C, Sp, cp, tp, Az, alt;
double	Tua, L, G, e, eps, g, alpha, delta, sd, cd, lha, lhr, sh, ch;
//double	Eqt;

/**/
int
bin_suna(char *name, char **argv, char *ops, int func)
{
      int ManualTime=0,ManualTz=0,year,month,day;
      double lon, lat, elev;
      struct tm time_str;

			int Flag_All=0;
			int Flag_Chk=0;
			int Flag_LocalTime=0;
			
				if (ops['d'])
				     Flag_Chk=1;
				
				if (ops['l'])
				     Flag_LocalTime=1;
				
				if (ops['a'])
				     Flag_All=1;

		    if (*argv)
	         ManualTime=strcmp(*argv, "A");

	      if (ManualTime)
	      {
    	     UTC = (int) matheval(*argv++);
	         ManualTime=(UTC!=0);
	         if (Flag_Chk) printf("Manual UTC=%d %s\n",(int)UTC, ctime(&UTC));
	      }
	      else argv++;

	      lat = matheval(*argv++);
	      lon = matheval(*argv++);
	      elev = matheval(*argv++);
		    if (*argv)
			      ManualTz=strcmp(*argv, "A");
	      localdst = matheval(*argv++);
	      if (!ManualTz)
        {
     	    tzset();				/* read _timezone */
			    localdst = (long) _timezone;
	      }

	      if (!ManualTime)
	      {
					// read system date and extract the year
				
					/** First get time **/
				    tzset();				/* read _timezone */
				    time(&UTC);				/* get time */
	         if (Flag_Chk) printf("Time automatically fetched from the system, UTC=%d, %s",(int)UTC,ctime(&UTC));
	      }
  		  if (Flag_Chk) printf("ManualTime=%d ManualTz=%d Flag_LocalTime=%d timezone=%d daylight=%d localdst=%d\n", ManualTime, ManualTz, Flag_LocalTime, (int) _timezone,  _daylight, (int) localdst);
				if (Flag_LocalTime)
				      {
						    if (ManualTime) UTC=UTC-_timezone+localdst;
				      }
				else
				      {
						    if (!ManualTime) UTC=UTC+localdst;
						    if (ManualTime) UTC=UTC-_timezone;
						  }

					/** Next generate time_str **/
		    gmtime_r(&UTC, &time_str);
		    // subtract timezone as mktime calculates a local time including timezone, which should be managed by localdst
  		  if (Flag_Chk) printf("%d-%d-%d %d:%02d:%02d isdst=%d lat=%lf, lon=%lf\n", time_str.tm_year+1900, time_str.tm_mon+1, time_str.tm_mday, time_str.tm_hour, time_str.tm_min, time_str.tm_sec, time_str.tm_isdst, lat, lon);

		  year = time_str.tm_year+1900;
		  month = time_str.tm_mon + 1;
		  day = time_str.tm_mday;
    
    /* correct apparent latitude for shape of Earth */

    lf = atan(f1*f1 * tan(lat * 3600. * Asec_Radian));
    Sp = sin(lf);
    cp = cos(lf);
    tp = Sp/cp;

    Local = lon*3600/15.;		/* Local apparent time correction */

    CalStuff(UTC);			/* start with local time info */

    alt=alt/3600.;

if (Flag_All)
{
	  printf("Sun: Declination=%.3f Degrees, ",delta/3600.);
    printf("Azimuth=%.3f Degrees, ",Az/3600.);
    printf("Elevation=%.3f Degrees.\n",alt);
}

    if (alt > elev)
      return(1);
		return(0);
}

/**/
void
timedata(long tim)
{
  
  /* compute seconds from 2000 Jan 1.5 UT (Ephemeris Epoch) */
  /* the VAX Epoch is     1970 Jan 1.0 UT (Midnight on Jan 1) */
  
  Julian_Day = (tim/Sec_per_day) +
    (double)(tim % Sec_per_day)/Sec_per_day + J1970;
  MJD = Julian_Day -J2000;	/* Julian Days past Epoch */
  Tu  = MJD/36525.;		/* Julian Centuries past Epoch */
  
  /* compute Sidereal time */
  
  Ru = 24110.54841 + Tu * (8640184.812866
			  + Tu * (0.09304 - Tu * 6.2e-6));	/* seconds */
  GMST = (tim % Sec_per_day) + Sec_per_day + replace_fmod(Ru, (double)Sec_per_day);
  LST  = GMST + Local;
//  printf("Ru=%d, Tu=%d, GMST=%d, LST=%d\n",Ru,Tu,GMST,LST);
}

/**/
void
CalStuff(long tim)
{		/* main computation loop */
  
  timedata(tim);
  
  /* where is the Sun (angles are in seconds of arc) */
  /*	Low precision elements from 1985 Almanac   */
  
  L   = 280.460 + 0.9856474 * MJD;	/* Mean Longitde             */
  L   = replace_fmod(L, 360.);			/* corrected for aberration  */
  
  g   = 357.528 + 0.9856003 * MJD;	/* Mean Anomaly              */
  g   = replace_fmod(g, 360.);
  
  eps = 23.439 - 0.0000004 * MJD;	/* Mean Obiquity of Ecliptic */
  
					/* convert to R.A. and DEC   */
  double Lr, gr, epsr, lr, ca, sa;
  double sA, cA;
  
  Lr = L * Degree_to_Radian;
  gr = g * Degree_to_Radian;
  epsr = eps * Degree_to_Radian;
  
  lr = (L + 1.915*sin(gr) + 0.020*sin(2.0*gr)) * Degree_to_Radian;
  
  sd = sin(lr) * sin(epsr);
  cd = sqrt(1.0 - sd*sd);
  sa = sin(lr) * cos(epsr);
  ca = cos(lr);
  
  delta = asin(sd);
  alpha = atan2(sa, ca);
  
  /* equation of time */
//  Eqt= (Lr - alpha) / Tsec_to_Radian;
  
  delta = delta / Asec_Radian;
  alpha = alpha / Tsec_to_Radian;
  
  lhr = (LST - alpha) * Tsec_to_Radian;
  sh =  sin(lhr);
  ch =  cos(lhr);
  lhr= atan2(sh, ch);	/* normalized -M_PI to M_PI */
  lha= lhr / Tsec_to_Radian + Sec_per_day/2;
  
  /* convert to Azimuth and altitude */
  
  alt = asin(sd*Sp + cd*ch*cp);
  ca =  cos(alt);
  sA =  -cd * sh / ca;
  cA =  (sd*cp - cd*ch*Sp) / ca;
  Az = atan2(sA, cA) / Asec_Radian;
  Az = replace_fmod(Az, 1296000. /* 360.*3600. */);
  alt = alt / Asec_Radian;
}

/* double precision modulus, put in range 0 <= result < m */

/**/
double
replace_fmod(double	x, double	m)
{
  long i;
  
  i = fabs(x)/m;		/* compute integer part of x/m */
  if (x < 0)	return( x + (i+1)*m);
  else		return( x - i*m);
}


// amacri, 2006
// Based on nmea_checksum of driver_nmea.c, GPSD project: http://gpsd.berlios.de/
/**/
int
bin_NmeaChkSum(char *name, char **argv, char *ops, int func)
{
    unsigned char sum = '\0';
    char c, *p, chk[3], *StringName=NULL;
    int Dbg=0;
    int Gen=0;

				if (ops['d'])
				   Dbg=1;

				if (ops['g'])
				   Gen=1;

		    if (*argv && ops['S']) {
					tokenize(*argv);
					if (!(StringName=*argv++)) {
					    zwarnnam(name, "invalid string", NULL, 0);
					    return 1;
					}
		    }

		    if ( (! *argv) || (*argv[0]=='\0') )
		      {
		       if (Dbg)
				      zwarnnam(name, "NMEA string is missing", NULL, 0);
		       return(1);
		      }
	      
	      p = *argv;

    if (*p++ != '$')
      {
       if (Dbg)
		      zwarnnam(name, "no heading dollar in NMEA string: %s", *argv, 0);
       return(1);
      }
 /* is the checksum on the specified sentence good? */
 while ( ((c = *p++) != '*') && c != '\0' )
        sum ^= c;

    if (Dbg && (c != '*'))
       zwarnnam(name, "no trailing asterisk in NMEA string: %s", *argv, 0);

    sprintf(chk, "%02X", sum);
    if (Gen) {
			if (StringName != NULL) {
					setsparam(StringName, ztrdup(chk));
			  } else {
	        printf("%02X\n", sum);
			  }
		}
    return (strncmp(chk, p, 2) != 0);
}

static int
getspeed(struct termios * cfg) {
    speed_t code = cfgetospeed(cfg);
    switch (code) {
    case B0:     return(0);
    case B300:   return(300);
    case B1200:  return(1200);
    case B2400:  return(2400);
    case B4800:  return(4800);
    case B9600:  return(9600);
    case B19200: return(19200);
    case B38400: return(38400);
    case B57600: return(57600);
    case B115200: return(115200);
    default: return(-1);
    }
}

// amacri, 2006
/**/
int
bin_baud(char *name, char **argv, char *ops, int func)
{
	int fd;
	int speed=-1, spd;
  unsigned int rate;
	struct termios cfg;

  if (*argv)
      speed = matheval(*argv++);

  if (speed < 0) {
		printf ("invalid speed\n");
		return -1;
	}

	if ((fd = open(*argv++, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
		printf ("could not open tty\n");
		return -1;
	}

  tcgetattr(fd, &cfg);

  if (speed == 0) {
    close(fd);
		spd=getspeed(&cfg);
		if (spd >= 0)
		   setiparam("BAUD", spd);
    return(spd);

  }

	if (speed < 300)
	    rate = 0;
	else if (speed < 1200)
	    rate =  B300;
	else if (speed < 2400)
	    rate =  B1200;
	else if (speed < 4800)
	    rate =  B2400;
	else if (speed < 9600)
	    rate =  B4800;
	else if (speed < 19200)
	    rate =  B9600;
	else if (speed < 38400)
	    rate =  B19200;
	else if (speed < 57600)
	    rate =  B38400;
	else if (speed < 115200)
	    rate =  B57600;
	else
	    rate =  B115200;

		cfsetispeed(&cfg, rate);
    cfsetospeed(&cfg, rate);
    /*
		//bzero(&cfg, sizeof(cfg));
		cfg.c_cflag = rate | CS8 | CLOCAL | CREAD;
		cfg.c_iflag = IGNPAR;
		cfg.c_oflag = 0;
		cfg.c_lflag = 0;
		tcflush(fd, TCIFLUSH);
		tcsetattr(fd, TCSADRAIN, &cfg);
		*/
		int stopbits=1;
		cfg.c_cflag &=~ CSIZE;
		cfg.c_cflag |= (CSIZE & (stopbits==2 ? CS7 : CS8));
		if (tcsetattr(fd, TCSANOW, &cfg) != 0) {
				close(fd);
		    return 1;
		  }
    (void)tcflush(fd, TCIOFLUSH);
		
		close(fd);
		spd=getspeed(&cfg);
		if (spd >= 0)
		   setiparam("BAUD", spd);
    return(spd);
}

// Sources of this SiRF decoder are based on the GPSD project: http://gpsd.berlios.de/
/**/
int
bin_GpsDecoder(char *nam, char **argv, char * ops, int func)
{

    char * inbuf=NULL, * hexbuf, *StringName=NULL;
    int setvars=0, leninbuf, buflen, lenstr, SirfFilter=0, ChkSm=1, Ret, printout=0;
    char SirfDebug=0;

    /* with the -S option, the first argument is taken *
     * as a string to save the date  */
    if (ops['c'])
    	ChkSm=0;
    if (ops['v'])
    	SirfDebug=1;
    if (ops['p'])
    	printout=1;
    if (ops['f'])
    	SirfFilter=1;
    if (ops['F'])
    	SirfFilter=2;
    if (ops['s'])
    	setvars=1;
    if (ops['V'])
    	SirfDebug=2;
    if (*argv && ops['S']) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 2;
	}
    }

	if (argv[1] && argv[1][0]) {
	    zwarnnam(nam, "invalid args", NULL, 0);
	    return 2;
	}

	inbuf = getsparam(argv[0]);
	if (inbuf==NULL) {
	    if (strlen(argv[0]) < 256)
	    	zwarnnam(nam, "invalid string: %s", argv[0], 0);
	    else
	    	zwarnnam(nam, "invalid string in argument", NULL, 0);
	    return 2;
	}
	
	inbuf=ztrdup(inbuf);
	leninbuf=strlen(inbuf);
	unmetafy(inbuf, &lenstr);
// inbuf is the string to parse (possibly including null chars); lenstr is the related length.
// hexbuf is the returned parsed string (use zalloc or zrealloc to create space; zfree ro release it)

	if (ops['m'])
		unmetafy(inbuf, &lenstr); // e.g., when debugging metafyed strings

PERMALLOC {
	ParseMain(&hexbuf, &buflen, lenstr, inbuf, SirfDebug, setvars, printout, SirfFilter, ChkSm);
	Ret=0;
	if (hexbuf==NULL)
		Ret=3;
	else if (hexbuf[0]=='\0')
		Ret=1;
  if ( (StringName != NULL) && (hexbuf>0) && (hexbuf[0]!='\0') ) {
		setsparam(StringName, ztrdup(hexbuf));
  } else {
	  if ( (hexbuf>0) && (hexbuf[0]!='\0') && (printout) )
			printf("%s", hexbuf);
  }
  if (hexbuf!=NULL)
		zfree(hexbuf, 0);
	zfree(inbuf, 0);
} LASTALLOC;
	return Ret;
}

// Source based on GPSD, sirf.c. Project site for gpsd at <http://gpsd.berlios.de/>
/*********************************************************************************************/
/*
 * The packet buffers need to be as long than the longest packet we
 * expect to see in any protocol, because we have to be able to hold
 * an entire packet for checksumming.  Thus, in particular, they need
 * to be as long as a SiRF MID 4 packet, 188 bytes payload plus eight bytes 
 * of header/length/checksum/trailer. 
 */
// #define MAX_PACKET_LENGTH	196	/* 188 + 8 */
#define MAX_PACKET_LENGTH	1024

#define HI(n)		((n) >> 8)
#define LO(n)		((n) & 0xff)

#include "bits.h"

/**/
static int
sirf_bin_parse(char ** retbuf, int * buflen, int * ptr, int len, char buf[], int SirfDebug, int setvars, int SirfFilter, int ChkSm)
{
	int i, checksum, Mark;
	char * ErrorIDD="";
	time_t secs;

	checksum = 0;
	if (SirfDebug==2)
		printf ("\nSIRF parser: ");
	for (i = 0; i<len; i++)
	{
		if (SirfDebug==2)
			printf ("%02X ",(unsigned char) buf[i]);
		checksum += (unsigned char) buf[i];
	}
	checksum &= 0x7FFF;
	if (SirfDebug==2)
		printf ("\nCalculated checksum = %04X. Checksum in frame=(%02X %02X)=%04X\n",checksum, (unsigned char)buf[len], (unsigned char)buf[len+1], ((unsigned char)buf[len] << 8) + (unsigned char)buf[len+1]);
	if (checksum != ((unsigned char)buf[len] << 8) + (unsigned char) buf[len+1])
	{
		if (SirfDebug) {
			printf ("Bad Checksum! (%d+%d=%d != %d)\n", ((unsigned char)buf[len] << 8), (unsigned char)buf[len+1], ((unsigned char)buf[len] << 8) + (unsigned char)buf[len+1], checksum);
			if (SirfDebug<2)
				for (i = 0; i<len; i++)
					printf ("%02X ",(unsigned char) buf[i]);
			if (SirfDebug<2)
					printf ("\n");
		}
		if (SirfFilter<2) {
			*retbuf = zrealloc(*retbuf, *buflen+=35);
			*ptr+=snprintf (*retbuf+ *ptr,34,"$SIRFBINu3,bad checksum,%02X\n", (unsigned char) buf[0]);
		}
		if (ChkSm)
			return(0);
	}
	if (SirfDebug)
		printf ("\n_________________________________________________________________\n");

	switch ( (unsigned char) buf[0])
	{
		case 0x02:		/* Measure Navigation Data Out */
			secs=gpstime_to_unix( (long) getuw(buf, 22), ( (double) getul(buf, 24) ) / 100 );
			if (SirfDebug) {
				printf ("Frame ID 02: Measure Navigation Data Out\n");
				printf ("X-position = %f m\n", (float) getsl(buf, 1));
				printf ("Y-position = %f m\n", (float) getsl(buf, 5));
				printf ("Z-position = %f m\n", (float) getsl(buf, 9));
				printf ("X-velocity = %g m/sec\n", (float) (getsw(buf, 13)/8));
				printf ("Y-velocity = %g m/sec\n", (float) (getsw(buf, 15)/8));
				printf ("Z-velocity = %g m/sec\n", (float) (getsw(buf, 17)/8));
				printf ("Mode 1 = %02X hex\n", (unsigned char) getub(buf, 19) );
				printf ("HDOP = %g\n",(float) (getub(buf, 20)/5) );
				printf ("Mode 2 = %02X hex\n",  (unsigned char) getub(buf, 21) );
				printf ("GPS Week = %ld\n", (long) getuw(buf, 22) );
				printf ("GPS TOW = %.02f seconds\n", ( (double) getul(buf, 24) ) / 100 );
				printf ("UNIX time based on Week and TOW = %.02f seconds = %s", (double) secs, ctime(&secs) );
				printf ("SVs in Fix = %d\n",(unsigned int) getub(buf, 28));
				printf ("CH1  PRN = %d\n",(unsigned int) getub(buf, 29));
				printf ("CH2  PRN = %d\n",(unsigned int) getub(buf, 30));
				printf ("CH3  PRN = %d\n",(unsigned int) getub(buf, 31));
				printf ("CH4  PRN = %d\n",(unsigned int) getub(buf, 32));
				printf ("CH5  PRN = %d\n",(unsigned int) getub(buf, 33));
				printf ("CH1  PRN = %d\n",(unsigned int) getub(buf, 34));
				printf ("CH7  PRN = %d\n",(unsigned int) getub(buf, 35));
				printf ("CH8  PRN = %d\n",(unsigned int) getub(buf, 36));
				printf ("CH9  PRN = %d\n",(unsigned int) getub(buf, 37));
				printf ("CH10 PRN = %d\n",(unsigned int) getub(buf, 38));
				printf ("CH11 PRN = %d\n",(unsigned int) getub(buf, 39));
				printf ("CH12 PRN = %d\n",(unsigned int) getub(buf, 40));
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=100);
				*ptr+=snprintf (*retbuf+*ptr,99,"$SIRFBIN%02X,%g,%d\n",(unsigned char) buf[0], (float) (getub(buf, 20)/5), (unsigned int) getub(buf, 28));
			}
			break;

		case 0x04:		/* Measured tracker data out */
			if (SirfDebug) {
				printf ("Frame ID 04: Measured tracker data out\n");
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;
	
		case 0x05:		/* Raw Tracker Data Out */
			if (SirfDebug) {
				printf ("Frame ID 05: Raw Tracker Data Out\n");
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;

		case 0x06:		/* Software Version String */
			if (SirfDebug) {
				printf ("Frame ID 06: Software Version String\n");
				for (i = 1; i < (int)len; i++)
				    if ((isprint(buf[i])) || (buf[i]=='\0'))
							(void)printf("%c", buf[i]);
				    else
							(void)printf("\\x%02x", (unsigned int)buf[i]);
				(void)printf("\n");
			}
			if (! SirfFilter) {
				int buf2len=len*5;
				char * buf2=zalloc(buf2len*5);
				buf2[0] = '\0';
				for (i = 1; i < (int)len; i++)
				    if ((isprint(buf[i])) || (buf[i]=='\0'))
					(void)snprintf(buf2+strlen(buf2), 
						       buf2len,
						       "%c", buf[i]);
				    else
					(void)snprintf(buf2+strlen(buf2), 
						       buf2len,
						       "\\x%02x", (unsigned int)buf[i]);
				*retbuf = zrealloc(*retbuf, *buflen+=15+strlen(buf2));
				*ptr+=snprintf (*retbuf+*ptr,14+strlen(buf2),"$SIRFBIN%02X,%s\n", (unsigned char) buf[0], buf2);
				zfree(buf2, buf2len);
			}
			break;

		case 0x07:		/* Clock Status Data */
			secs=gpstime_to_unix( (long) getuw(buf, 1), (double) getul(buf, 3) ) / 100;
			if (SirfDebug) {
				printf ("Frame ID 07: Clock Status Data\n");
				printf ("Extended GPS Week = %ld (GPS week number; week 0 started January 6 1980)\n", (long) getuw(buf, 1) );
				printf ("GPS TOW = %.3f sec.\n", ( (double) getul(buf, 3) ) / 100 );
				printf ("UNIX time based on Week and TOW = %.02f seconds = %s", (double) secs, ctime(&secs) );
				printf ("SVs = %d\n", getub(buf, 7) );
				printf ("Clock Drift = %ld Hz\n", (long) getul(buf, 8) );
				printf ("Clock Bias = %ld ns\n", (long) getul(buf, 12) );
				printf ("Estimated GPS Time = %.3f (GPS time of week in seconds)\n", (double) getul(buf, 16)/1000);
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=100);
				*ptr+=snprintf (*retbuf+*ptr,99,"$SIRFBIN%02X,%d,%d\n", (unsigned char) buf[0],getub(buf, 7), (int) getsl(buf, 16));
			}
			break;

		case 0x08:		/* subframe data -- extract leap-second from this */
			if (SirfDebug) {
				printf ("Frame ID 08: 50 BPS Data\n");
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;

		case 0x09:		/* CPU Throughput */
			if (SirfDebug) {
				printf ("Frame ID 09: CPU Throughput\n");
				printf ("SegStatMax = %g ms\n", (double) getuw(buf, 1)/186 );
				printf ("SegStatLat = %g ms\n", (double) getuw(buf, 3)/186 );
				printf ("AveTrkTime = %g ms\n", (double) getuw(buf, 5)/186 );
				printf ("Last Millisecond = %d ms\n",getuw(buf, 7) );
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=100);
				*ptr+=snprintf (*retbuf+*ptr,99,"$SIRFBIN%02X,%d,%d,%d,%d\n", (unsigned char) buf[0],getuw(buf, 1), getuw(buf, 3), getuw(buf, 5), getuw(buf, 7) );
			}
			break;

		case 0x0a:		/* Error ID Data */
			switch ( (unsigned int)getuw(buf, 1) )
			{
				case	2: ErrorIDD="Satellite subframe # failed parity check.";break;
				case	9: ErrorIDD="Failed to obtain a position for acquired satellite ID.";break;
				case	10: ErrorIDD="Conversion of Nav Pseudo Range to Time of Week (TOW) for tracker exceeds limits";break;
				case	11: ErrorIDD="Convert pseudorange rate to Doppler frequency exceeds limit.";break;
				case	12: ErrorIDD="Satellite’s ephemeris age has exceeded 2 hours (7200 s).";break;
				case	13: ErrorIDD="SRAM position is bad during a cold start.";break;
				case	4097: ErrorIDD="VCO lost lock indicator.";break;
				case	4099: ErrorIDD="Nav detect false acquisition; reset receiver by calling NavForceReset routine.";break;
				case	4104: ErrorIDD="Failed SRAM checksum during startup.";break;
				case	4105: ErrorIDD="Failed RTC SRAM checksum during startup.";break;
				case	4106: ErrorIDD="Failed battery-backing position because of ECEF velocity sum was greater than equal to 3600.";break;
				case	4107: ErrorIDD="Failed battery-backing position because current navigation mode is not KFNav and not LSQFix.";break;
				case	8193: ErrorIDD="Buffer allocation error occurred.";break;
				case	8194: ErrorIDD="PROCESS_1SEC task was unable to complete upon entry.";break;
				case	8195: ErrorIDD="Failure of hardware memory test.";break;
				default: ErrorIDD="Unknown error.";break;
			}
			if (SirfDebug) {
				printf ("Frame ID 0A=10: Error ID Data\n");
				printf ("Error number = %d\n",(unsigned int)getuw(buf, 1) );
				printf ("%s\n",ErrorIDD );
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=100+strlen(ErrorIDD));
				*ptr+=snprintf (*retbuf+*ptr,99+strlen(ErrorIDD),"$SIRFBIN%02X,%d,%s\n", (unsigned char) buf[0],(unsigned int)getuw(buf, 1),ErrorIDD );
			}
			break;

		case 0x0b:		/* Command Acknowledgement */
			if (SirfDebug) {
				printf ("Frame ID 0B=11: Command Acknowledgement\n");
				printf ("ACK ID = %d\n",(unsigned char)getub(buf, 1) );
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=30);
				*ptr+=snprintf (*retbuf+*ptr,29,"$SIRFBIN%02X,%d\n", (unsigned char) buf[0],(unsigned char)getub(buf, 1));
			}
			break;

		case 0x0c:		/* Command NAcknowledgement */
			if (SirfDebug) {
				printf ("Frame ID 0C=12: Command NAcknowledgement\n");
				printf ("NACK ID = %d\n",(unsigned char)getub(buf, 1) );
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X,%d\n", (unsigned char) buf[0],(unsigned char)getub(buf, 1));
			}
			break;

		case 0x0d:		/* Visible List */
			if (SirfDebug) {
				printf ("Frame ID 0D=13: Visible List\n");
				printf ("Visible Svs = %d\n",(int) (getub(buf, 2)) );
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=100);
				*ptr+=snprintf (*retbuf+*ptr,99,"$SIRFBIN%02X,%d\n", (unsigned char) buf[0],(int) (getub(buf, 2)) );
			}
			break;

		case 0x12:		/* OK To Send */
			if (SirfDebug) {
				printf ("Frame ID 0x12=18: OK To Send\n");
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;

		case 0x13:		/* Navigation Parameters (Response to Poll) */
			if (SirfDebug) {
				printf ("Frame ID 0x13=19: Navigation Parameters (Response to Poll)\n");
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;

		case 0x1b:		/* DGPS status (undocumented) */
			if (SirfDebug) {
				printf ("Frame ID 1b=27: DGPS status (undocumented)\n");
			}
			Mark=0;
			for (i = 1; i < (int)len; i++)
			    if ( (unsigned char) buf[i] != (unsigned char) '\0')
			    	Mark=(unsigned char) buf[i];
			if (SirfDebug) {
				if (Mark==0)
					printf ("All %d bytes are valued to zero.\n", len);				
				else printf ("First non-zero byte is %d; related value is %02X hex.; total length=%d.\n", i, buf[i], len);
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X,%d\n", (unsigned char) buf[0], Mark);
			}
			break;

		case 0x1c:		/* Navigation Library Measurement Data  */
			if (SirfDebug) {
				printf ("Frame ID 1c=28: Navigation Library Measurement Data\n");
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;

		case 0x29:		/* Geodetic Navigation Information */
			secs=gpstime_to_unix( (long) getuw(buf, 5), ( (double) getul(buf, 7) ) / 1000 );

			if ((setvars) && ((unsigned short)getuw(buf, 1) == 0) ) {
				char str[80], *inbuf;
				sprintf(str, "%02d%02d%02d", (int)getub(buf, 14), (int)getub(buf, 13),(int)getuw(buf, 11) % 100);
				setsparam("DateDmy", ztrdup(str));
				sprintf(str, "%02d%02d%02.f", (int)getub(buf, 15), (int)getub(buf, 16),getuw(buf, 17)*1e-3);
				setsparam("DateHms", ztrdup(str));
				sprintf(str, "%d", lrint(getsl(buf, 23)*1e-2));
				setsparam("Latitude", ztrdup(str));
				sprintf(str, "%d", lrint(getsl(buf, 27)*1e-2));
				setsparam("Longitude", ztrdup(str));
				sprintf(str, "%f", getsl(buf, 23)*1e-7);
				setsparam("Lat", ztrdup(str));
				sprintf(str, "%f", getsl(buf, 27)*1e-7);
				setsparam("Lon", ztrdup(str));
				sprintf(str, "%.1f", getsl(buf, 31)*1e-2);
				setsparam("Altitude", ztrdup(str));
				sprintf(str, "%.1f", getsl(buf, 31)*1e-2 - getsl(buf, 35)*1e-2);
				setsparam("GeoidSeparation", ztrdup(str));
				inbuf = getsparam("MaxSpeedToShowPoint");
				if ( ((inbuf!=NULL) && (*inbuf!='\0')) && getsw(buf, 40)*3.6e-2 > atoi(inbuf) )
					sprintf(str, "%d", (int) (0.5 + getsw(buf, 40)*3.6e-2));
			  else
					sprintf(str, "%.1f", getsw(buf, 40)*3.6e-2);
				setsparam("Speed", ztrdup(str));
				if ( ( getsw(buf, 42)*1e-2 >= 0 ) && ( getsw(buf, 42)*1e-2 <= 360 ) ) { 
					sprintf(str, "%.1f", getsw(buf, 42)*1e-2);
					setsparam("Course", ztrdup(str));
				}
				sprintf(str, "%.1f", getsl(buf, 50)*1e-2);
				setsparam("PDOP", ztrdup(str));
				sprintf(str, "%.1f", getsl(buf, 54)*1e-2);
				setsparam("VDOP", ztrdup(str));
				if ((getsl(buf, 58)*1e-2) == 0)
					setsparam("Ldncd", dupstring(getsparam("LoggerDoNotChangeDate"))); // here date is accurate and can drive a sync
				else
					setsparam("Ldncd", ztrdup("yes")); // here date is not precise enough to drive a sync
				sprintf(str, "%d", (int) (getub(buf, 88)));
				setsparam("SatellitesUsed", ztrdup(str));
				sprintf(str, "%.1f", (float) ((float)getub(buf, 89)/5));
				setsparam("HDOP", ztrdup(str));
				setsparam("Dimension", ztrdup("3"));
			}

			if (SirfDebug) {
				printf ("Frame ID 0x29=41: Geodetic Navigation Information\n");
				printf ("Navvalid = %04X hex\n",(unsigned short)getuw(buf, 1));
				printf ("Navtype = %04X hex\n",(unsigned short)getuw(buf, 3));
				printf ("Extended Week Number = %ld (GPS week number; week 0 started January 6 1980)\n",(long) getuw(buf, 5));
				printf ("TOW = %.3f (GPS time of week in seconds)\n", (double) getul(buf, 7)/1000);
				printf ("UNIX time based on Week and TOW = %.02f seconds = %s", (double) secs, ctime(&secs) );
				printf ("Date = %02d/%02d/%04d %02d:%02d:%s%g\n",(int)getub(buf, 14), (int)getub(buf, 13), (int)getuw(buf, 11), (int)getub(buf, 15), (int)getub(buf, 16),(getuw(buf, 17)<10000?"0":""),getuw(buf, 17)*1e-3);
				printf ("Satellite ID List = %08X\n",getul(buf, 19));
				printf ("Latitude = %.5f degrees (+ = North)\n",getsl(buf, 23)*1e-7);
				printf ("Longitude = %.5f (+ = East)\n",getsl(buf, 27)*1e-7);
				printf ("Altitude from Ellipsoid = %g m\n",getsl(buf, 31)*1e-2);
				printf ("Altitude from MSL = %g m\n",getsl(buf, 35)*1e-2);
				printf ("Map Datum = %d\n", (int) (getub(buf, 39)) );
				printf ("Speed Over Ground (SOG) = %g m/s\n",getsw(buf, 40)*1e-2);
				printf ("Course Over Ground (COG, True) = %g degrees clockwise from true north\n",getsw(buf, 42)*1e-2);
				printf ("Magnetic Variation = %g\n", (float) getsw(buf, 44));
				printf ("Climb Rate = %g m/s\n",getsw(buf, 46)*1e-2);
				printf ("Heading Rate = %g deg/s\n",getsw(buf, 48)*1e-2);
				printf ("Estimated Horizontal Position Error = %g m\n",getsl(buf, 50)*1e-2);
				printf ("Estimated Vertical Position Error = %g m\n",getsl(buf, 54)*1e-2);
				printf ("Estimated Time Error = %g seconds\n",getsl(buf, 58)*1e-2);
				printf ("Estimated Horizontal Velocity Error = %g m/s\n",getsw(buf, 62)*1e-2);
				printf ("Clock Bias = %f m\n",getul(buf, 64)*1e-2);
				printf ("Clock Bias Error = %f m\n",getul(buf, 68)*1e-2);
				printf ("Clock Drift = %f m/s\n",getul(buf, 72)*1e-2);
				printf ("Clock Drift Error = %f m/s\n",getul(buf, 76)*1e-2);
				printf ("Distance = %g m (Distance traveled since reset in meters)\n", (float) getsl(buf, 80));
				printf ("Distance error = %g degrees\n",(float) getsw(buf, 84));
				printf ("Heading Error = %g\n",getsw(buf, 86)*1e-2);
				printf ("Number of SVs in Fix = %d (Count of SVs indicated by SV ID list)\n",(int) (getub(buf, 88)) );
				printf ("HDOP = %g (Horizontal Dilution of Precision)\n",(float) ((float)getub(buf, 89)/5) );
				printf ("AdditionalModeInfo = %02X hex\n",getub(buf, 90) );
			}

			if (SirfFilter<2) {
				*retbuf = zrealloc(*retbuf, *buflen+=300);
				*ptr+=snprintf (*retbuf+*ptr,299,"$SIRFBIN%02X,%04X,\
%02d%02d%02d,%02d%02d%02.f,\
%d,%d,\
%.1f,%.1f,\
%.1f,%.1f,\
%.1f,%.1f,%.1f,%.1f,\
%g,%g,\
%d,%.1f\n",
//%04X,%d,%.3f,
//*%08X,
//%d,
//%g,%g,%g,
//%f,%f,%f,%f,
//%g,
					(unsigned char) buf[0],
					(unsigned short)getuw(buf, 1), // Navvalid
//					(unsigned short)getuw(buf, 3),(unsigned short)getuw(buf, 5),(double) getul(buf, 7)/1000,
					(int)getub(buf, 14), (int)getub(buf, 13),(int)getuw(buf, 11) % 100, // DateDmy
					(int)getub(buf, 15), (int)getub(buf, 16),getuw(buf, 17)*1e-3, // DateHms
//					getul(buf, 19),
					lrint(getsl(buf, 23)*1e-2), // Latitude
					lrint(getsl(buf, 27)*1e-2), // Longitude
					getsl(buf, 31)*1e-2, // Altitude of the Ellipsoid
					getsl(buf, 35)*1e-2, // Altitude SLM
//					(int) (getub(buf, 39)),
					getsw(buf, 40)*3.6e-2, // Speed OG
					getsw(buf, 42)*1e-2, // Course OG
//					(float) getsw(buf, 44),
//					getsw(buf, 46)*1e-2,
//					getsw(buf, 48)*1e-2,
					getsl(buf, 50)*1e-2, // Estimated Horizontal Position Error
					getsl(buf, 54)*1e-2, // Estimated Vertical Position Error
					getsl(buf, 58)*1e-2, // Estimated Time Error
					getsw(buf, 62)*1e-2, // Estimated Horizontal Velocity Error
//					getsl(buf, 64)*1e-2,
//					getsl(buf, 68)*1e-2,
//					getsl(buf, 72)*1e-2,
//					getsl(buf, 76)*1e-2,
					(float) getsl(buf, 80), // Distance traveled since reset (meters, floating point)
					(float) getsw(buf, 84), // (meters, floating point)
//					getsw(buf, 86)*1e-2,
					(int) (getub(buf, 88) ), // Number of Satellites in view (integer)
					(float) ((float)getub(buf, 89)/5) // Horizontal Dilution of Precision (one decimal digit)
				 );
				}
			break;

		case 0x2b:		/* Queue Command Parameters */
			if (SirfDebug) {
				printf ("Frame ID 2B=43: Queue Command Parameters\n");
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;

		case 0x32:		/* SBAS corrections */
			if (SirfDebug) {
				printf ("Frame ID 0x32=50: SBAS corrections\n");
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;

		case 0x34:		/* PPS Time */
			if (SirfDebug) {
				printf ("Frame ID 0x34=52: PPS Time\n");
				printf ("Date = %d/%d/%d %d:%d:%d\n",(int)getub(buf, 4), (int)getub(buf, 5)-1, (int)getuw(buf, 6), (int)getub(buf, 1), (int)getub(buf, 2),getub(buf, 3));
				if (((int)getub(buf, 14) & 0x07) == 0x07)	/* valid UTC time? */
					printf ("Valid UTC time. Status = %d\n", getub(buf, 14));
				else
					printf ("Invalid time. Status = %d\n", getub(buf, 14));
			}
			if (SirfFilter<2) {
				*retbuf = zrealloc(*retbuf, *buflen+=100);
				*ptr+=snprintf (*retbuf+*ptr,99,"$SIRFBIN%02X,%d/%d/%d %d:%d:%d,%d\n", (unsigned char) buf[0],(int)getub(buf, 4), (int)getub(buf, 5)-1, (int)getuw(buf, 6), (int)getub(buf, 1), (int)getub(buf, 2),getub(buf, 3), getub(buf, 14) );
			}
			break;

		case 0x62:		/* uBlox Extended Measured Navigation Data */
			if (SirfDebug) {
				printf ("Frame ID 0x62=98: uBlox Extended Measured Navigation Data\n");
				printf ("Latitude = %g\n",getsl(buf, 1) / Degree_to_Radian * 1e-8);
				printf ("Longitude = %g\n",getsl(buf, 5) / Degree_to_Radian * 1e-8);
				printf ("Altitude from Ellipsoid = %g\n",getsl(buf, 9) * 1e-3);
				printf ("Speed Over Ground (SOG) = %g\n",getsl(buf, 13) * 1e-3);
				printf ("Climb Rate = %g\n",getsl(buf, 17) * 1e-3);
				printf ("Course Over Ground (COG, True) = %g\n",getsl(buf, 21) / Degree_to_Radian * 1e-8);
				printf ("Navtype = %04X hex\n",(unsigned short)getub(buf, 25));
				printf ("Date = %02d/%02d/%04d %02d:%02d:%s%g\n",(int)getub(buf, 29), (int)getub(buf, 28), (int)getuw(buf, 26), (int)getub(buf, 30), (int)getub(buf, 31),(getuw(buf, 32)<10000?"0":""),getuw(buf, 32)*1e-3);
				printf ("GDOP = %g\n",(float) (getub(buf, 34)/5) );
				printf ("PDOP = %g\n",(float) (getub(buf, 35)/5) );
				printf ("HDOP = %g\n",(float) (getub(buf, 36)/5) );
				printf ("VDOP = %g\n",(float) (getub(buf, 37)/5) );
				printf ("TDOP = %g\n",(float) (getub(buf, 38)/5) );
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=15);
				*ptr+=snprintf (*retbuf+*ptr,14,"$SIRFBIN%02X\n", (unsigned char) buf[0]);
			}
			break;

		case 0xff:		/* Debug messages */
			if (SirfDebug) {
				printf ("Frame ID FF=255: Debug messages\n");
				for (i = 1; i < (int)len; i++)
				    if ((isprint(buf[i])) || (buf[i]=='\0'))
							(void)printf("%c", buf[i]);
				    else
							(void)printf("\\x%02x", (unsigned int)buf[i]);
				(void)printf("\n");
			}
			if (! SirfFilter) {
				int buf2len=len*5;
				char * buf2=zalloc(buf2len*5);
				buf2[0] = '\0';
				for (i = 1; i < (int)len; i++)
				    if ((isprint(buf[i])) || (buf[i]=='\0'))
					(void)snprintf(buf2+strlen(buf2), 
						       buf2len,
						       "%c", buf[i]);
				    else
					(void)snprintf(buf2+strlen(buf2), 
						       buf2len,
						       "\\x%02x", (unsigned int)buf[i]);
				*retbuf = zrealloc(*retbuf, *buflen+=15+strlen(buf2));
				*ptr+=snprintf (*retbuf+*ptr,14+strlen(buf2),"$SIRFBIN%02X,%s\n", (unsigned char) buf[0], buf2);
				zfree(buf2, buf2len);
			}
			break;

		default:
			if (SirfDebug) {
				printf ("Unknown SiRF packet id %d=%02X. Length=%d\n", (unsigned char) buf[0], (unsigned char) buf[0], len);
			}
			if (! SirfFilter) {
				*retbuf = zrealloc(*retbuf, *buflen+=30);
				*ptr+=snprintf (*retbuf+*ptr,29,"$SIRFBIN%02X,unknown\n", (unsigned char) buf[0]);
			}
			break;
	}
return(1);
}

// Source slightly modified from GPSD, sirf.c. Project site for gpsd at <http://gpsd.berlios.de/>

/**/
static void
ParseMain(char ** retbuf, int * buflen, int len, char * buf, int SirfDebug, int setvars, int printout, int SirfFilter, int ChkSm)
{
		int i, i_chks=-1, i_start=0, i_end=0, i_head2=0;
		enum {UNKNOWN, SIRF_HEADER1, SIRF_HEADER2, SIRF_END1, SIRF_END2} state;
		int checksum = 0, count = 0, ValidFrames=0, ptr=0, lenp=0;
		char parse_buffer[MAX_PACKET_LENGTH], c, p[202];

		*retbuf = zalloc(*buflen=30);
		*retbuf[0]='\0';
	  state = UNKNOWN;
		if (len > 0)
		{
			for (i = 0; i<=len; i++) // Check NMEA
			{
				if ( (i==len) || ((c =(unsigned char) buf[i]) == '\r') || (c == '\n') || (c == '\0')) {
					if ( (i_start < i_end) && (i_chks >= 0) && (i_chks + 2 == i) && (isalnum(buf[i_chks]) && (isalnum (buf[i_chks+1])) && (16*(buf[i_chks]-(buf[i_chks]>='A'?'A':'0'))+buf[i_chks+1]-(buf[i_chks+1]>='A'?55:'0') ) == checksum) ) {
						lenp=i_end-i_start+1;
						if (lenp>200)
							if (SirfDebug==2)
								printf("NMEA String too long: '%.*s'...\n", 199, buf+i_start);
							else
								continue;
						strncpy(p, buf+i_start, lenp);
						*(p+lenp-1)='\0'; // remove the final asterisk
						if (SirfFilter<2) {
							*retbuf = zrealloc(*retbuf, *buflen+=lenp+1);
							ptr+=snprintf (*retbuf+ ptr,lenp+1, "%s\n", p);
						}
						ProcessNmea(p, lenp, SirfDebug, setvars);
					}
					else
						if ( (i_start < i_end) && (SirfDebug==2) )
								printf("Invalid NMEA checksum for sentence '%.*s'.\nChecksum should be %d (decimal) %02X (hex)\n", i_end-i_start, buf+i_start, checksum, checksum);
					i_chks=-1;i_start=i+1;i_end=i_start;
					continue;
				}

				if ( (c < ' ') || (c > 'Z') )
					break; // Not NMEA; check SiRF (this also implies discarding valid NMEA strings inside a buffer starting with invalid chars)

				switch (i_chks) { // calculate checksum and set i_chks to the first checksum char
					case -1: // start of string; expected dollar (begin NMEA sentence)
						if (c == '$') { // NMEA checksum will be checked after the dollar
							checksum=0; // initialize checksum
							i_chks=-2; // NMEA correctly initialized
						} else
							i_chks=-4; // NMEA sentence must start with a dollar (skip until EOL)
						break;
					case -2: // inside NMEA sentence: count NMEA checksum until dollar
						if (c != '*') // NMEA checksum will be counted until asterisk
		  				checksum ^= c;
		  			else {
							i_end=i;
		  				i_chks=-3;		  				
		  			}
						break;
					case -3: // NMEA checksum
	  				i_chks=i; // set i_chks to the first checksum char
	  				break;
			  }
			}
			for (i = 0; i<len; i++) // Check SiRF
			{
				if (SirfDebug==3)
					printf ("%02X ", (unsigned char) buf[i]);
				switch (state)
				{
					case UNKNOWN:
					case SIRF_END2:
						if ((unsigned char) buf[i] == 0xA0)
							state = SIRF_HEADER1;
						break;

					case SIRF_HEADER1:
						if ((unsigned char) buf[i] == 0xA2)
						{
							if (SirfDebug==2)
								printf("\n====================== SIRF_HEADER2 (A0 A2) =====================\n");
							state = SIRF_HEADER2;
							i_head2=i;
							count = 0;
						}
						else {
							if (SirfDebug==2)
								printf("\nSIRF_HEADER1 (A0) without SIRF_HEADER2 (A2)\n");
							state = UNKNOWN;
						}
						break;

					case SIRF_HEADER2:
						if (count < sizeof(parse_buffer)) {
							parse_buffer[count++] = (unsigned char) buf[i];
							if ((unsigned char) buf[i] == 0xB0)
								state = SIRF_END1;
						} else {
							state = UNKNOWN;
							i=i_head2;
							if (SirfDebug)
								printf("\nSIRF message broken or uncomplete (SIRF_HEADER1 and SIRF_HEADER2 found (A0 A2), but SIRF_END1 (B0) not found).\n");
							if (SirfFilter<2) {
								*retbuf = zrealloc(*retbuf, *buflen+=35);
								ptr+=snprintf (*retbuf+ptr,34,"$SIRFBINu1,missing SIRF_END1\n");
							}
						}
						break;

					case SIRF_END1:
//						printf("\nSIRF_END1 %d %d+%d+5=%d\n",(unsigned char) buf[i], (unsigned char)parse_buffer[0] << 8,(unsigned char)parse_buffer[1] ,count);

						if (((unsigned char) buf[i] == 0xB3) && (((unsigned char)parse_buffer[0] << 8) + (unsigned char)parse_buffer[1] + 5 == count))
						{
							parse_buffer[count] = (unsigned char) buf[i];
							if (SirfDebug==2)
								printf ("\nSIRF_END1 and SIRF_END2 found (B0 B3)\nBlock size=(%02X %02X)=%d+5=%d. Expected: %d\n", parse_buffer[0], parse_buffer[1], ((unsigned char)parse_buffer[0] << 8) + (unsigned char)parse_buffer[1], ((unsigned char)parse_buffer[0] << 8) + (unsigned char)parse_buffer[1] + 5, count);
							state = SIRF_END2;
							if (sirf_bin_parse(retbuf, buflen, &ptr, count-5, &parse_buffer[2], SirfDebug, setvars, SirfFilter, ChkSm))
								ValidFrames+=1;
							else
								i=i_head2;
						}
						else if (count < sizeof(parse_buffer))
						{
							state = SIRF_HEADER2;
							parse_buffer[count++] = (unsigned char) buf[i];
						}
						else {
							state = UNKNOWN;
							i=i_head2;
							if (SirfDebug)
								printf("\nSIRF message invalid or too long (SIRF_HEADER1, SIRF_HEADER2, SIRF_END1 found, but SIRF_END2 not found or invalid checksum).\n");
							if (SirfFilter<2) {
								*retbuf = zrealloc(*retbuf, *buflen+=35);
								ptr+=snprintf (*retbuf+ptr,34,"$SIRFBINu2,missing SIRF_END2\n");
							}
						}
						break;

					default:
						if (SirfDebug==2)
							printf("\nUnknown state (internal error)!\n");
						if (SirfFilter<2) {
							*retbuf = zrealloc(*retbuf, *buflen+=30);
							ptr+=snprintf (*retbuf+ptr,29,"$SIRFBINzz,internal error\n");
						}
						break;
				}
			}
			if ( (state == SIRF_END1) || (state == SIRF_HEADER2) ) {
				if ( (SirfDebug) && (state == SIRF_END1) )
					printf("\nSIRF message broken or uncomplete (SIRF_HEADER1, SIRF_HEADER2 and SIRF_END1 found, but SIRF_END2 not found).\n");
				if ( (SirfDebug) && (state == SIRF_HEADER2) )
					printf("\nSIRF message broken or uncomplete (SIRF_HEADER1 and SIRF_HEADER2 found, but SIRF_END1 and SIRF_END2 not found).\n");
				if (SirfFilter<2) {
					*retbuf = zrealloc(*retbuf, *buflen+=35);
					ptr+=snprintf (*retbuf+ptr,34,"$SIRFBINu4,broken or uncomplete\n");
				}
			}
		}
if ( (ValidFrames>0) && (*retbuf[0]=='\0') && (SirfFilter<2) ) {
	ptr+=snprintf(*retbuf+ptr,29,"$SIRFBINxx,%d\n", ValidFrames);
}
return;
}

/**/
static void
do_lat_lon(char *field[])
/* process a pair of latitude/longitude fields starting at field index BEGIN */
{
    double lat, lon, d, m;
    char str[20], str1[20], *p;
    int count, updated = 0;

    if (*(p = field[0]) != '\0') {
			while (1)
		    if ( (!isdigit(*p)) && (!ispunct(*p)) )
					break;
				else p++;
	    if (*p != '\0')
	    	return;

			(void)sscanf(field[0], "%lf", &lat);
			m = 100.0 * modf(lat / 100.0, &d);
			lat = d + m / 60.0;
			p = field[1];
			if (*p == 'S')
			    lat = -lat;
			sprintf(str1, "%f", lat); // 6 decimal digits
			if (*(str1+strlen(str1)-1) == '0') // remove the last decimal digit (6th) in case it is 0 (some device returns 5 decimal digits)
				*(str1+strlen(str1)-1) = '\0';
	    if (*(p = field[2]) != '\0') {
				while (1)
			    if ( (!isdigit(*p)) && (!ispunct(*p)) )
						break;
					else p++;
		    if (*p != '\0')
		    	return;
				(void)sscanf(field[2], "%lf", &lon);
				m = 100.0 * modf(lon / 100.0, &d);
				lon = d + m / 60.0;
			
				p = field[3];
				if (*p == 'W')
				    lon = -lon;
				sprintf(str, "%f", lon); // 6 decimal digits
				if (*(str+strlen(str)-1) == '0') // remove the last decimal digit (6th) in case it is 0 (some device returns 5 decimal digits)
					*(str+strlen(str)-1) = '\0';
				setsparam("Lon", ztrdup(str));
				setsparam("Lat", ztrdup(str1));
				sprintf(str, "%d", (int)((double) (lat*100000+0.5)));
				sprintf(str1, "%d", (int)((double) (lon*100000+0.5)));
				setsparam("Latitude", ztrdup(str));
				setsparam("Longitude", ztrdup(str1));
	    }
    }
}


#define NMEA_FIELDS 40

/**/
static void
ProcessNmea(char * buf, int lenbuf, int debug, int setvars)
{
int i, count;
char str[200], * p, * t, * field[NMEA_FIELDS], *inbuf, *inbuf1;

    /* split sentence copy on commas, filling the field array */
    count = 0;
    t = buf+lenbuf;  /* end of sentence */
    p = buf; /* beginning of tag, 'G' not '$' */

    /* while there is a search string and we haven't run off the buffer... */
    while((p != NULL) && (p < t)){
			field[count] = p; /* we have a field. record it */
			if ((p = strchr(p, ',')) != NULL){ /* search for the next delimiter */
			    *p = '\0'; /* replace it with a NUL */
			    count++; /* bump the counters and continue */
			    if (count==NMEA_FIELDS)
			    	break;
			    p++;
			}
		}

if (debug)
	for (i=0;i<=count;i++)
		printf ("i=%i ->	%s\n",i, field[i]);

/* Process NMEA sentences */
if (!setvars)
	return;

if (strcmp(field[0],"$GPGGA") == 0) {
				if (*field[6] != '\0') // Position Fix Indicator; -1=missing. 0=Fix not available or invalid. 1(SPS),2(DGPS SPS),3(PPS),4(RTK)=fix valid
					setsparam("PosFixInd", ztrdup(field[6]));
				else
					setsparam("PosFixInd", ztrdup("-1"));
				inbuf = getsparam("Altitude");
				if ( (*field[6] != '\0') && ( atoi(field[6]) > 0 ) && ((inbuf==NULL) || (*inbuf=='\0')) ) {
					inbuf = getsparam("FastGpsFetch");
					inbuf1 = getsparam("DateHms");
					if ( ((inbuf!=NULL) && (*inbuf!='\0')) && (strcmp(inbuf,"yes") == 0) && ((inbuf1==NULL) || (*inbuf1=='\0')) ) {
						    time_t secs;
						    struct tm *t;
						    int bufsize = 200;
						    char *StringName=NULL;

						sprintf(str, "%06d", (int) (0.5 + strtod(field[1], (char **)NULL)));
						setsparam("DateHms", ztrdup(str));
				    strcpy(str, "now ");
 						inbuf1 = getsparam("TzShift");
						if ( ((inbuf1==NULL) || (*inbuf1=='\0')) || (strlen(inbuf1) > 150) )
					    strcat(str, "0");
						else
					    strcat(str, inbuf1);
				    strcat(str, " seconds");
				    secs = get_date (str, (time_t *) NULL);
				    t = localtime(&secs);
				    strftime(str, bufsize, "%d%m%y", t);
						setsparam("DateDmy", ztrdup(str));
						setsparam("Ldncd", ztrdup("yes")); // here date is not precise enough to drive a sync
					}
					do_lat_lon(&field[2]);
					if (*field[7] != '\0') {
						sprintf(str, "%d", atoi(field[7]));
						setsparam("SatellitesUsed", ztrdup(str));
					}
					setsparam("HDOP", ztrdup(field[8]));
					if ( (strcmp(field[10], "M") == 0) && (*field[9] != '\0') )
							setsparam("Altitude", ztrdup(field[9]));
					if ( (strcmp(field[12], "M") == 0) && (*field[11] != '\0') )
							setsparam("GeoidSeparation", ztrdup(field[11]));
				}
} else if (strcmp(field[0],"$GPRMC") == 0) {
				if ( (*field[8] != '\0') && ( atoi(field[8]) >= 0 ) && ( atoi(field[8]) <= 360 ) ) 
					setsparam("Course", ztrdup(field[8]));
				if ( (strcmp(field[2], "A") == 0) && (*field[1] != '\0') && (*field[9] != '\0') ) {
						sprintf(str, "%06d", (int) (0.5 + strtod(field[1], (char **)NULL)));
						setsparam("DateHms", ztrdup(str));
						setsparam("DateDmy", ztrdup(field[9]));
						setsparam("MagneticVariation", ztrdup(field[10]));
						setsparam("Ldncd", dupstring(getsparam("LoggerDoNotChangeDate")));
						inbuf = getsparam("Latitude");
						inbuf1 = getsparam("Longitude");
						if ( ((inbuf==NULL) || (*inbuf=='\0')) || ((inbuf1==NULL) || (*inbuf1=='\0')) )
								do_lat_lon(&field[3]);
						if (*field[7] != '\0') {
							sprintf(str, "%.*f", Precision, (float) strtod(field[7], (char **)NULL)*0.51444444);
							setsparam("SpeedMs", ztrdup(str));
							inbuf = getsparam("MaxSpeedToShowPoint");
							if ( ((inbuf!=NULL) && (*inbuf!='\0')) && (atoi(field[7])*1.852 > atoi(inbuf) ) ) {
								sprintf(str, "%d", (int) (0.5 + strtod(field[7], (char **)NULL)*1.852));
								setsparam("Speed", ztrdup(str));
							}
						  else {
								sprintf(str, "%.1f", (float) strtod(field[7], (char **)NULL)*1.852);
								setsparam("Speed", ztrdup(str));
							}
						}
				}
} else
if (strcmp(field[0],"$GPGSA") == 0) {
				if (*field[2] != '\0')
					setsparam("Dimension", ztrdup(field[2]));
				if ( atoi(field[2]) > 1 ) {
					setsparam("PDOP", ztrdup(field[15]));
					setsparam("VDOP", ztrdup(field[17]));
					inbuf = getsparam("HDOP");
					if ( ((inbuf==NULL) || (*inbuf=='\0')) && (*field[16] != '\0') )
						setsparam("HDOP", ztrdup(field[16]));
				}
} else
if (strcmp(field[0],"$GPVTG") == 0) {
				inbuf = getsparam("Course");
				if ( ((inbuf==NULL) || (*inbuf=='\0')) && (*field[1] != '\0') && ( atoi(field[1]) >= 0 ) && ( atoi(field[1]) <= 360 ) ) 
					setsparam("Course", ztrdup(field[1]));
				if ( (strcmp(field[8], "K") == 0) && (*field[7] != '\0') ) {
					sprintf(str, "%.*f", Precision, (float) strtod(field[7], (char **)NULL)/3.6);
					setsparam("SpeedMs", ztrdup(str));
					inbuf = getsparam("MaxSpeedToShowPoint");
					if ( ((inbuf!=NULL) && (*inbuf!='\0')) && (atoi(field[7]) > atoi(inbuf) ) ) {
						sprintf(str, "%d", (int) (0.5 + strtod(field[7], (char **)NULL)));
						setsparam("Speed", ztrdup(str));
					}
				  else
						setsparam("Speed", ztrdup(field[7]));
				}
} else
if (strcmp(field[0],"$GPGSV") == 0) {
				if (*field[3] != '\0') {
					sprintf(str, "%d", atoi(field[3]));
					setsparam("SatellitesInView", ztrdup(str));
				}
}

}

/*
Some notes on SiRF messages (ref. TomTom GPS devices):
Returned by TomTom / SiRF Star III: 02, 04, 07, 09,                 29
Returned by TomTom / SiRF Star II:  02,     07, 09, 0A, 0B, 0C, 1B,     FF

02: *Measured Navigation Data - Position, velocity, and time (useless)
04: *Measured Tracking Data   - Satellite and C/No information (information of each satellite in view; useless)
07: *Clock Status             - Current clock status (useless)
09: *Throughput               - Navigation complete data (useless)
0A:  Error ID                 - Error coding for message failure
0B:  Command Acknowledgment   - Successful request
0C:  Command NAcknowledgment  - Unsuccessful request
1B:  (27)  unknown
29: *(41)  Geodetic Navigation Data - Geodetic navigation in (essential); not supported by SiRFLoc (SiRF Star II)
FF:  (255) Debug messages
*/


// amacri Aug 8, 2007
// Source slightly modified from GPSD, sirf.c. Project site for gpsd at <http://gpsd.berlios.de/>
/**/
int
bin_SirfEnvelope(char *nam, char **argv, char * ops, int func)
{
    char *inbuf=NULL, *zinbuf=NULL, * buf = NULL, *StringName=NULL;
    int leninbuf, lenstr, zlenstr;
    int nnl = 0;

		unsigned int       crc;
		size_t    i;
		int ok=1;

    /* with the -S option, the first argument is taken *
     * as a string to save the date  */
    if (*argv && ops['S']) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 1;
	}
    }

	if (argv[1] && argv[1][0]) {
	    zwarnnam(nam, "invalid args", NULL, 0);
	    return 1;
	}

	inbuf = getsparam(argv[0]);
	if (inbuf==NULL) {
	    if (strlen(argv[0]) < 256)
	    	zwarnnam(nam, "invalid string: %s", argv[0], 0);
	    else
	    	zwarnnam(nam, "invalid string in argument", NULL, 0);
	    return 1;
	}
	
	inbuf=ztrdup(inbuf);
	leninbuf=strlen(inbuf);
  if (ops['r'])
		lenstr=strlen(inbuf);
	else
		unmetafy(inbuf, &lenstr);
	zinbuf = getkeystring(inbuf, &zlenstr, 2, &nnl);

  buf=zalloc(zlenstr+8);
  buf[0]=0xa0;
  buf[1]=0xa2;
  buf[2]=(zlenstr << 8) & 0xFF;
  buf[3]=zlenstr & 0xFF;
   /* copy the frame and calculate CRC */
  crc = 0;
  for (i = 0; i < zlenstr; i++) {
  	buf[i+4]=zinbuf[i];
		crc += (unsigned char)zinbuf[i];
}
	/* enter CRC after payload */
	crc &= 0x7fff;
	buf[i+4] = (unsigned char)((crc & 0xff00) >> 8);
	buf[i+5] = (unsigned char)( crc & 0x00ff);
  buf[i+6]=0xb0;
  buf[i+7]=0xb3;

	zfree(inbuf, leninbuf);
	zfree(zinbuf, lenstr+1);
  if (StringName != NULL) {
		setsparam(StringName, metafy(buf, zlenstr+8, META_REALLOC));
  } else {
    ok=write(1, buf, zlenstr+8) == (ssize_t)(zlenstr+8);
  	zfree(buf, zlenstr+8);
  }
  return (! ok);
}

/*********************************************************************************************/


// amacri Aug 8, 2007
/**/
int
bin_dumps(char *nam, char **argv, char * ops, int func)
{
    char *inbuf=NULL, * hexbuf = NULL, *StringName=NULL;
    int leninbuf, PrintAll=0, lenstr, bufsize=0, Ret;

    /* with the -S option, the first argument is taken *
     * as a string to save the date  */
    if (ops['p'])
    	PrintAll=1;
    if (*argv && ops['S']) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 1;
	}
    }

	if (argv[1] && argv[1][0]) {
	    zwarnnam(nam, "invalid args", NULL, 0);
	    return 1;
	}

PERMALLOC {
	inbuf = dupstring(getsparam(argv[0]));
	if (inbuf==NULL) {
	    if (strlen(argv[0]) < 256)
	    	zwarnnam(nam, "invalid string: %s", argv[0], 0);
	    else
	    	zwarnnam(nam, "invalid string in argument", NULL, 0);
	    Ret=1;
	    goto enddump;
	}
	
	leninbuf=strlen(inbuf);
  if (ops['r'])
		lenstr=strlen(inbuf);
	else
		unmetafy(inbuf, &lenstr);

	if (ops['m'])
		unmetafy(inbuf, &lenstr); // e.g., when debugging metafyed strings

	hex_dump(&hexbuf, &bufsize, inbuf, lenstr, PrintAll);

	Ret=0;
	if (hexbuf==NULL)
		Ret=3;
	else {
	  if (StringName != NULL) {
			setsparam(StringName, ztrdup(hexbuf));
	  } else {
			printf("%s", hexbuf);
	  }
	  zfree(hexbuf, bufsize);
	}
enddump:
if (inbuf!=NULL)
	zfree(inbuf, leninbuf);
} LASTALLOC;
return Ret;
}


// amacri Aug 8, 2007
// Slightly modified source from what available in Internet
void hex_dump(char ** buf, int * bufsize, void *data, int size, int PrintAll)
{
	*buf = NULL;
	*bufsize = 0;
	static const int GROWBY = 80; /* how large we will grow strings by */

    /* dumps size bytes of *data to stdout. Looks like:
     * [0000] 75 6E 6B 6E 6F 77 6E 20
     *                  30 FF 00 00 00 00 39 00 unknown 0.....9.
     * (in a single line of course)
     */

    unsigned char *p = data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};
    int ptr;

		*buf = zrealloc(*buf, *bufsize += (GROWBY*2) );
		ptr=0;
		ptr+=snprintf(*buf+ptr, GROWBY*2-1, "Number of dumped bytes: %d\n[____]   __ __ __ __ __ __ __ __   __ __ __ __ __ __ __ __   ________ ________\n", size);
			//printf("Number of dumped bytes: %d\n[____]   __ __ __ __ __ __ __ __   __ __ __ __ __ __ __ __   ________ ________\n", size);

    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4x",
               ((unsigned int)p-(unsigned int)data) );
        }
            
        c = *p;
        if (isalnum(c) == 0) {
            c = '.';
        }
        if ((PrintAll>0) && (isprint(c) > 0) )
        	c=*p;
        if ( (c == 10) || (c == 13) || (c == 0) || (c == 9) )
            c = '.';
        if ( (c > 5) && (c < 22) )
            c = '.';
        if ( (c > 144) && (c < 161) )
            c = '.';

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
						*buf = zrealloc(*buf, *bufsize += GROWBY);
            ptr+=snprintf(*buf+ptr, GROWBY, "[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            	//printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if(n%8 == 0) {
            /* half line: add whitespaces */
            strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
            strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
				*buf = zrealloc(*buf, *bufsize += GROWBY);
        ptr+=snprintf(*buf+ptr, GROWBY, "[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
        	//printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
return;
}

// Source taken from GPSD, gpsutils.c. Project site for gpsd at <http://gpsd.berlios.de/>
/*
 * The 'week' part of GPS dates are specified in weeks since 0000 on 06
 * January 1980, with a rollover at 1024.  At time of writing the last
 * rollover happened at 0000 22 August 1999.  Time-of-week is in seconds.
 *
 * This code copes with both conventional GPS weeks and the "extended"
 * 15-or-16-bit version with no wraparound that appears in Zodiac
 * chips and is supposed to appear in the Geodetic Navigation
 * Information (0x29) packet of SiRF chips.  Some SiRF firmware versions
 * (notably 231) actually ship the wrapped 10-bit week, despite what
 * the protocol reference claims.
 *
 * Note: This time will need to be corrected for leap seconds.
 */
#define GPS_EPOCH       315964800               /* GPS epoch in Unix time */
#define SECS_PER_WEEK   (60*60*24*7)            /* seconds per week */
#define GPS_ROLLOVER    (1024*SECS_PER_WEEK)    /* rollover period */

double gpstime_to_unix(long week, double tow)
{
    double fixtime;

    if (week >= 1024)
        fixtime = GPS_EPOCH + (week * SECS_PER_WEEK) + tow;
    else {
        time_t now, last_rollover;
        (void)time(&now);
        last_rollover = GPS_EPOCH+((now-GPS_EPOCH)/GPS_ROLLOVER)*GPS_ROLLOVER;
        /*@i@*/fixtime = last_rollover + (week * SECS_PER_WEEK) + tow;
    }
    return fixtime;
}

// amacri Aug 8, 2007
/**/
int
bin_geoid(char *nam, char **argv, char * ops, int func)
{
    char buffer[100], *StringName=NULL;
    double lat, lon, Distance;

    /* with the -S option, the first argument is taken *
     * as a string to save the distance  */
    if (*argv && ops['S']) {
	tokenize(*argv);
	if (!(StringName=*argv++)) {
	    zwarnnam(nam, "invalid string", NULL, 0);
	    return 1;
	}
    }

if (! *argv) {
     zwarnnam(nam, "invalid latitude", NULL, 0);
     return 1;
}

lat = matheval(*argv++);

if (! *argv) {
     zwarnnam(nam, "invalid longitude", NULL, 0);
     return 1;
}

lon = matheval(*argv++);

if ((lat < -90) || (lat > 90)) {
     zwarnnam(nam, "invalid latitude", NULL, 0);
     return 1;
}
if ((lon < -180) || (lon > 180)) {
     zwarnnam(nam, "invalid longitude", NULL, 0);
     return 1;
}

Distance=wgs84_separation(lat, lon);

sprintf((char *) buffer,"%.*f", Precision, Distance);
if (StringName != NULL) {
        setsparam(StringName, ztrdup(buffer));
    } else {
        printf("%s\n", buffer);
   }
return 0;
}

// Source taken from GPSD, geoid.c. Project site for gpsd at <http://gpsd.berlios.de/>
/* 
 * geoid.c -- ECEF to WGS84 conversions, including ellipsoid-to-MSL height
 *
 * Geoid separation code by Oleg Gusev, from data by Peter Dana.
 * ECEF conversion by Rob Janssen.
 */

static double bilinear(double x1, double y1, double x2, double y2, double x, double y, double z11, double z12, double z21, double z22)
{
 double delta;

 if (y1 == y2 && x1 == x2 ) return (z11);
 if (y1 == y2 && x1 != x2 ) return (z22*(x-x1)+z11*(x2-x))/(x2-x1);
 if (x1 == x2 && y1 != y2 ) return (z22*(y-y1)+z11*(y2-y))/(y2-y1);

 delta=(y2-y1)*(x2-x1);

 return (z22*(y-y1)*(x-x1)+z12*(y2-y)*(x-x1)+z21*(y-y1)*(x2-x)+z11*(y2-y)*(x2-x))/delta;
}


// Source taken from GPSD, geoid.c. Project site for gpsd at <http://gpsd.berlios.de/>
/* return geoid separtion (WGS84 ellipsoid to MSL) in meters, given a lat/lot in degrees
 * Geoid separation code by Oleg Gusev, from data by Peter Dana.
 */
/*@ +charint @*/
double wgs84_separation(double lat, double lon)
{
#define GEOID_ROW	19
#define GEOID_COL	37
    const char geoid_delta[GEOID_COL*GEOID_ROW]={
	/* 90S */ -30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30, -30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,
	/* 80S */ -53,-54,-55,-52,-48,-42,-38,-38,-29,-26,-26,-24,-23,-21,-19,-16,-12, -8, -4, -1,  1,  4,  4,  6,  5,  4,   2, -6,-15,-24,-33,-40,-48,-50,-53,-52,-53,
	/* 70S */ -61,-60,-61,-55,-49,-44,-38,-31,-25,-16, -6,  1,  4,  5,  4,  2,  6, 12, 16, 16, 17, 21, 20, 26, 26, 22,  16, 10, -1,-16,-29,-36,-46,-55,-54,-59,-61,
	/* 60S */ -45,-43,-37,-32,-30,-26,-23,-22,-16,-10, -2, 10, 20, 20, 21, 24, 22, 17, 16, 19, 25, 30, 35, 35, 33, 30,  27, 10, -2,-14,-23,-30,-33,-29,-35,-43,-45,
	/* 50S */ -15,-18,-18,-16,-17,-15,-10,-10, -8, -2,  6, 14, 13,  3,  3, 10, 20, 27, 25, 26, 34, 39, 45, 45, 38, 39,  28, 13, -1,-15,-22,-22,-18,-15,-14,-10,-15,
	/* 40S */  21,  6,  1, -7,-12,-12,-12,-10, -7, -1,  8, 23, 15, -2, -6,  6, 21, 24, 18, 26, 31, 33, 39, 41, 30, 24,  13, -2,-20,-32,-33,-27,-14, -2,  5, 20, 21,
	/* 30S */  46, 22,  5, -2, -8,-13,-10, -7, -4,  1,  9, 32, 16,  4, -8,  4, 12, 15, 22, 27, 34, 29, 14, 15, 15,  7,  -9,-25,-37,-39,-23,-14, 15, 33, 34, 45, 46,
	/* 20S */  51, 27, 10,  0, -9,-11, -5, -2, -3, -1,  9, 35, 20, -5, -6, -5,  0, 13, 17, 23, 21,  8, -9,-10,-11,-20, -40,-47,-45,-25,  5, 23, 45, 58, 57, 63, 51,
	/* 10S */  36, 22, 11,  6, -1, -8,-10, -8,-11, -9,  1, 32,  4,-18,-13, -9,  4, 14, 12, 13, -2,-14,-25,-32,-38,-60, -75,-63,-26,  0, 35, 52, 68, 76, 64, 52, 36,
	/* 00N */  22, 16, 17, 13,  1,-12,-23,-20,-14, -3, 14, 10,-15,-27,-18,  3, 12, 20, 18, 12,-13, -9,-28,-49,-62,-89,-102,-63, -9, 33, 58, 73, 74, 63, 50, 32, 22,
	/* 10N */  13, 12, 11,  2,-11,-28,-38,-29,-10,  3,  1,-11,-41,-42,-16,  3, 17, 33, 22, 23,  2, -3, -7,-36,-59,-90, -95,-63,-24, 12, 53, 60, 58, 46, 36, 26, 13,
	/* 20N */   5, 10,  7, -7,-23,-39,-47,-34, -9,-10,-20,-45,-48,-32, -9, 17, 25, 31, 31, 26, 15,  6,  1,-29,-44,-61, -67,-59,-36,-11, 21, 39, 49, 39, 22, 10,  5,
	/* 30N */  -7, -5, -8,-15,-28,-40,-42,-29,-22,-26,-32,-51,-40,-17, 17, 31, 34, 44, 36, 28, 29, 17, 12,-20,-15,-40, -33,-34,-34,-28,  7, 29, 43, 20,  4, -6, -7,
	/* 40N */ -12,-10,-13,-20,-31,-34,-21,-16,-26,-34,-33,-35,-26,  2, 33, 59, 52, 51, 52, 48, 35, 40, 33, -9,-28,-39, -48,-59,-50,-28,  3, 23, 37, 18, -1,-11,-12,
	/* 50N */  -8,  8,  8,  1,-11,-19,-16,-18,-22,-35,-40,-26,-12, 24, 45, 63, 62, 59, 47, 48, 42, 28, 12,-10,-19,-33, -43,-42,-43,-29, -2, 17, 23, 22,  6,  2, -8,
	/* 60N */   2,  9, 17, 10, 13,  1,-14,-30,-39,-46,-42,-21,  6, 29, 49, 65, 60, 57, 47, 41, 21, 18, 14,  7, -3,-22, -29,-32,-32,-26,-15, -2, 13, 17, 19,  6,  2,
	/* 70N */   2,  2,  1, -1, -3, -7,-14,-24,-27,-25,-19,  3, 24, 37, 47, 60, 61, 58, 51, 43, 29, 20, 12,  5, -2,-10, -14,-12,-10,-14,-12, -6, -2,  3,  6,  4,  2,
	/* 80N */   3,  1, -2, -3, -3, -3, -1,  3,  1,  5,  9, 11, 19, 27, 31, 34, 33, 34, 33, 34, 28, 23, 17, 13,  9,  4,   4,  1, -2, -2,  0,  2,  3,  2,  1,  1,  3,
	/* 90N */  13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,  13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13}; 
/*@ -charint @*/
    int	ilat, ilon;
    int	ilat1, ilat2, ilon1, ilon2;

    ilat=(int)floor(( 90.+lat)/10);
    ilon=(int)floor((180.+lon)/10);
	
    /* sanity checks to prevent segfault on bad data */
    if ( ( ilat > 90 ) || ( ilat < -90 ) ) {
	return 0.0;
    }
    if ( ( ilon > 180 ) || ( ilon < -180 ) ) {
	return 0.0;
    }

    ilat1=ilat;
    ilon1=ilon;
    ilat2=(ilat < GEOID_ROW-1)? ilat+1:ilat;
    ilon2=(ilon < GEOID_COL-1)? ilon+1:ilon;
	
    return bilinear(
	ilon1*10.-180.,ilat1*10.-90.,
	ilon2*10.-180.,ilat2*10.-90.,
	lon,           lat,
	(double)geoid_delta[ilon1+ilat1*GEOID_COL], 
	(double)geoid_delta[ilon2+ilat1*GEOID_COL],
	(double)geoid_delta[ilon1+ilat2*GEOID_COL], 
	(double)geoid_delta[ilon2+ilat2*GEOID_COL]
	);
}

#ifdef CONFIG_NOTIFY

/* size of the event structure, not counting name */
#define EVENT_SIZE  (sizeof (struct inotify_event))

/* reasonable guess as to size of 1024 events */
#define BUF_LEN        (1024 * (EVENT_SIZE + 16)
static int INotifyFd = -2, wd;

// amacri, 2008
/**/
int
bin_inotify(char *name, char **argv, char *ops, int func)
{
  long notifyarg = 0;
  int sig;

  /* get trap signal */
	sig = getsignum(argv[1]);
	if (sig == -1) {
	    zwarnnam(name, "undefined signal: %s", argv[1], 0);
      return -1;
	}

  if (ops['A'])
	    notifyarg |= IN_ACCESS;
  if (ops['M'])
	    notifyarg |= IN_MODIFY;
  if (ops['S'])
	    notifyarg |= IN_DELETE_SELF;
  if (ops['D'])
	    notifyarg |= IN_DELETE;
  if (ops['O'])
	    notifyarg |= IN_OPEN;
  if (ops['W'])
	    notifyarg |= IN_CLOSE_WRITE;
  if (ops['N'])
	    notifyarg |= IN_CLOSE_NOWRITE;
  if (ops['F'])
	    notifyarg |= IN_MOVED_FROM;
  if (ops['T'])
	    notifyarg |= IN_MOVED_TO;
  if (ops['B'])
	    notifyarg |= IN_ATTRIB;
  if (ops['C'])
	    notifyarg |= IN_CLOSE;
  if (ops['M'])
	    notifyarg |= IN_MOVE;
  if (ops['a'])
	    notifyarg |= IN_ALL_EVENTS;

  if ( (argv[0][0] == '\0') || (ops['c']) ) { /* remove watch */
			if ( inotify_rm_watch(INotifyFd, sig) < 0) {
		  		zwarnnam(name, "failed removing inotify: %e", NULL, errno);
          if (close(INotifyFd)==0)
		      	return(0);
		  		zwarnnam(name, "close failed: %e", NULL, errno);
			}
		  return -1;
  }

  if (INotifyFd != -2)
  		INotifyFd = inotify_init ();

  if (INotifyFd < 0) {
	    zwarnnam(name, "cannot initialize inotify: %s", argv[0], 0);
      return -1;
	}

	if (fcntl(INotifyFd, F_SETSIG, sig) < 0) {
		  zwarnnam(name, "%s, F_SETSIG failed for signal %d", strerror(errno), sig);
      close(INotifyFd);
      return -1;
	}

  wd = inotify_add_watch (INotifyFd, argv[0], notifyarg);

	if (wd < 0) {
		  zwarnnam(name, "%s, inotify_add_watch failed for flags %d", strerror(errno), notifyarg);
      return -1;
	}

  return(wd);
}


// amacri, 2008
/**/
int
bin_dnotify(char *name, char **argv, char *ops, int func)
{
  int sig, FNotifyFd;
  long notifyarg = 0;

	FNotifyFd=-2;
	
  /* get trap signal */
	sig = getsignum(argv[1]);
	if (sig == -1) {
	    zwarnnam(name, "undefined signal: %s", argv[1], 0);
      return -1;
	}

#ifdef ARM
  if (ops['A'])
	    notifyarg |= DN_ACCESS;
  if (ops['M'])
	    notifyarg |= DN_MODIFY;
  if (ops['C'])
	    notifyarg |= DN_CREATE;
  if (ops['D'])
	    notifyarg |= DN_DELETE;
  if (ops['R'])
	    notifyarg |= DN_RENAME;
  if (ops['B'])
	    notifyarg |= DN_ATTRIB;
  if (ops['a'])
	    notifyarg |= DN_ACCESS|DN_MODIFY|DN_CREATE|DN_DELETE|DN_RENAME|DN_ATTRIB;

  if ( (argv[0][0] == '\0') || (ops['c']) ) { /* close FNotifyFd */
			if (fcntl(sig, F_NOTIFY, 0) < 0) {
		  		zwarnnam(name, "fcntl; failed removing F_NOTIFY: %e", NULL, errno);
          if (close(sig)==0)
		      	return(0);
		  		zwarnnam(name, "close failed: %e", NULL, errno);
			}
		  return -1;
  }

  /* check directory */
  if (is_directory(argv[0], 1, NULL) == FALSE) {
	if (isset(SHOWFSERR))
		zwarnnam(name, "%s: Is not a directory", argv[0], 0);
    return -1;
  }

  FNotifyFd = open(argv[0], O_RDONLY);
  if (FNotifyFd < 0) {
	    zwarnnam(name, "cannot open directory: %s", argv[0], 0);
      return -1;
	}

  if (notifyarg == 0)
	   notifyarg = DN_CREATE|DN_DELETE;

  if (ops['m'])
	    notifyarg |= DN_MULTISHOT;

	if (fcntl(FNotifyFd, F_SETSIG, sig) < 0) {
		  zwarnnam(name, "fcntl, %s, F_SETSIG failed for signal %d", strerror(errno), sig);
      close(FNotifyFd);
      return -1;
	}

	if (fcntl(FNotifyFd, F_NOTIFY, notifyarg) < 0) {
		  zwarnnam(name, "fcntl, %s, F_NOTIFY failed for flags %d", strerror(errno), notifyarg);
      close(FNotifyFd);
      return -1;
	}
#endif /* ARM */

	if (FNotifyFd== -2) {
		  zwarnnam(name, "fcntl F_NOTIFY unsupported.", NULL, 0);
      return -1;
	}

  return(FNotifyFd);
}

#endif /* CONFIG_NOTIFY */


// amacri, 2008
/**/
int
bin_lease(char *name, char **argv, char *ops, int func)
{
  int sig, LeaseFd;
  long notifyarg = 0;

	LeaseFd=-2;
	
  /* get trap signal */
	sig = getsignum(argv[1]);
	if (sig == -1) {
	    zwarnnam(name, "undefined signal: %s", argv[1], 0);
      return -1;
	}

  if (ops['w'])
	    notifyarg |= F_WRLCK;
  if (ops['r'])
	    notifyarg |= F_RDLCK;

#ifdef ARM
  if ( (argv[0][0] == '\0') || (ops['c']) ) { /* close LeaseFd */
			if (fcntl(sig, F_SETLEASE, F_UNLCK) < 0)
		  		zwarnnam(name, "fcntl; failed removing F_SETLEASE: %e", NULL, errno);
      if (close(sig)==0)
		      	return(0);
		  zwarnnam(name, "close failed: %e", NULL, errno);
		  return -1;
  }

  LeaseFd = open(argv[0], O_RDONLY);
  if (LeaseFd < 0) {
	    zwarnnam(name, "cannot open directory: %s", argv[0], 0);
      return -1;
	}

	if (fcntl(LeaseFd, F_SETSIG, sig) < 0) {
		  zwarnnam(name, "fcntl, %s, F_SETSIG failed for signal %d", strerror(errno), sig);
      close(LeaseFd);
      return -1;
	}

	if (fcntl(LeaseFd, F_SETLEASE, notifyarg) < 0) {
		  zwarnnam(name, "fcntl, %s, F_SETLEASE failed for flag %d", strerror(errno), notifyarg);
      close(LeaseFd);
      return -1;
	}
#endif /* ARM */

	if (LeaseFd== -2) {
		  zwarnnam(name, "fcntl F_SETLEASE unsupported.", NULL, 0);
      return -1;
	}

  return(LeaseFd);
}


#include <pthread.h>
#include <stdio.h>

struct thread_data{
   int  Interval;
   char *Directory;
   char *CommandL;
   char *CommandG;
};

static ThreadActive=0;
static sigset_t watchf_signal_mask;  /* signals to block         */

// amacri, 2008
void *thr_watchf(void *threadarg)
{
	DIR *dirp;
	struct dirent *dp;
	int Size, OldSize, Ret;
	char buf[sizeof(struct dirent)+MAXNAMLEN+1];
	
	char * tid;
	struct thread_data *watchf_data;
	watchf_data = (struct thread_data *) threadarg;

  sigemptyset (&watchf_signal_mask);
  sigaddset (&watchf_signal_mask, SIGINT);
	if (pthread_sigmask (SIG_BLOCK, &watchf_signal_mask, NULL) != 0) {
		  zwarnnam("watchf", "cannot set SIG_BLOCK SIGINT to thread: %e", NULL, errno);
			pthread_exit(NULL);
	    return;
	}

	if ((dirp = opendir(watchf_data->Directory)) == NULL) {
		  zwarnnam("watchf", "could not open directory %s: %e", watchf_data->Directory, errno);
      ThreadActive=0;
  		pthread_exit(NULL);
	    return;
	}

	OldSize=-1;
	for (;;) {
    Size=0;
    while ( (Ret=readdir_r(dirp, (struct dirent *)buf, &dp )) == 0 && dp) {

                /*Skip "." and ".." entries if found*/
                if ((strcmp(dp->d_name, ".") == 0) ||
                    (strcmp(dp->d_name, "..") == 0)) {
                        continue;
                }

								//printf("%s\n", dp->d_name);fflush(stdout);
								Size++;
        }
		rewinddir(dirp);
		if ( (OldSize>=0) && (OldSize < Size) && (*(watchf_data->CommandG)!='\0') ) {
			system(watchf_data->CommandG);
			OldSize=-1;
			continue;
    }
		if ( (OldSize>=0) && (OldSize > Size) && (*(watchf_data->CommandL)!='\0') ) {
			system(watchf_data->CommandL);
			OldSize=-1;
			continue;
    }
    OldSize=Size;
    sleep(watchf_data->Interval);
    if (ThreadActive==0)
    	break;
	}

  if (Ret != 0)
		 zwarnnam("watchf", "error reading directory: %e", NULL, errno);
  (void) closedir(dirp);
  ThreadActive=0;
  pthread_exit(NULL);
}


static struct thread_data watchf_data;

// amacri, 2008
/**/
int
bin_watchf(char *name, char **argv, char *ops, int func)
{
   pthread_t thread;
   int rc, i;

		  if (ops['k']) {
				if (ThreadActive==0) {
		  	  zwarnnam(name, "watchf inactive, cannot be killed", NULL, 0);
		  	  return(1);
		  	}
				if (ThreadActive==1)
		  	  zwarnnam(name, "watchf killed", NULL, 0);
				ThreadActive=0;
				return(0);
			}
		  if (ops['p']) {
				if (ThreadActive==0)
		  	  zwarnnam(name, "watchf inactive", NULL, 0);
				if (ThreadActive==1)
		  	  zwarnnam(name, "watchf active on directory %s with interval %d", watchf_data.Directory, watchf_data.Interval);
				return(ThreadActive+2);
			}

      watchf_data.Interval=matheval(*argv++);
      watchf_data.Directory=ztrdup(*argv++);
      watchf_data.CommandL=ztrdup(*argv++);
      watchf_data.CommandG=ztrdup(*argv++);

			if (ThreadActive==1) {
		  	  zwarnnam(name, "watchf already active on directory %s with interval %d", watchf_data.Directory, watchf_data.Interval);
          return(0);
      }
      ThreadActive=1;
      rc = pthread_create(&thread, NULL, thr_watchf, (void *) &watchf_data);
      if (rc){
		  	 zwarnnam(name, "cannot create thread: %e", NULL, errno);
         return(1);
      }
      return(0);
}


static struct thread_data wd_data;

static WdActive=0;

// amacri, 2008
/**/
int
bin_wd(char *name, char **argv, char *ops, int func)
{
   pthread_t thread;
   int rc, i;

		  if (ops['k']) {
				if (WdActive==0) {
		  	  zwarnnam(name, "watchdog inactive, cannot be killed", NULL, 0);
		  	  return(1);
		  	}
				if (WdActive==1)
			  	zwarnnam(name, "watchdog killed", NULL, 0);
				WdActive=0;
				return(0);
			}

		  if (ops['p']) {
				if (WdActive==0)
		  	  zwarnnam(name, "watchdog inactive", NULL, 0);
				if (WdActive==1)
		  	  zwarnnam(name, "watchdog active on file %s with interval %d", wd_data.CommandL, wd_data.Interval);
				return(WdActive+2);
			}

		  wd_data.CommandG=(char *) 0;
		  if (ops['c'])
		     wd_data.CommandG=(char *) 1;

      wd_data.Interval=matheval(*argv++);
      wd_data.CommandL=ztrdup(*argv++);

			if (WdActive==1) {
		  	  zwarnnam(name, "watchdog already active on file %s with interval %d", wd_data.CommandL, wd_data.Interval);
          return(0);
      }
      WdActive=1;
      rc = pthread_create(&thread, NULL, thr_wd, (void *) &wd_data);
      if (rc){
		  	 zwarnnam(name, "cannot create thread: %e", NULL, errno);
         return(1);
      }
      return(0);
}

#ifdef ARM
#include "watchdog.h"
#endif /* ARM */

static sigset_t wd_signal_mask;  /* signals to block         */

// amacri, 2008
void *thr_wd(void *threadarg)
{
	char * tid;
	struct thread_data *wd_data;
	wd_data = (struct thread_data *) threadarg;

  sigemptyset (&wd_signal_mask);
  sigaddset (&wd_signal_mask, SIGINT);
	if (pthread_sigmask (SIG_BLOCK, &wd_signal_mask, NULL) != 0) {
		  zwarnnam("wd", "cannot set SIG_BLOCK SIGINT to thread: %e", NULL, errno);
			pthread_exit(NULL);
	    return;
	}

	int fd=open(wd_data->CommandL,O_WRONLY);

	if (fd < 0) {
		  zwarnnam("wd", "could not open %s: %e", wd_data->CommandL, errno);
      WdActive=0;
  		pthread_exit(NULL);
	    return;
	}


	for (;;) {
		if(fd==-1) {
				if (wd_data->CommandG == (char *) 0) {
				  zwarnnam("wd", "could not open %s: %e", wd_data->CommandL, errno);
      		WdActive=0;
		  		pthread_exit(NULL);
			    return;
				}
				else {
					close(fd);
					sleep(wd_data->Interval);
					fd=open(wd_data->CommandL,O_WRONLY);
					continue;
				}
			}
		while(1)
			{
#ifdef ARM
				if (ioctl(fd, WDIOC_KEEPALIVE, 0) == -1) {
				  zwarnnam("wd", "could not perform ioctl on %s: %e", wd_data->CommandL, errno);
					close(fd);
					sleep(wd_data->Interval);
					fd=open(wd_data->CommandL,O_WRONLY);
					continue;
				}
				//write(fd,"\0",1);
				//fsync(fd);
#else /* not ARM */
		  	zwarnnam("wd", "WDIOC_KEEPALIVE unsupported", NULL, 0	);
#endif /* ARM */
				sleep(wd_data->Interval);
		    if (WdActive==0)
		    	break;
			}
    if (WdActive==0)
    	break;
		if (wd_data->CommandG == (char *) 0) {
		  zwarnnam("wd", "could not open %s: %e", wd_data->CommandL, errno);
      WdActive=0;
  		pthread_exit(NULL);
	    return;
		}
		sleep(wd_data->Interval);
    if (WdActive==0)
    	break;
	}
	close(fd);
  WdActive=0;
  pthread_exit(NULL);
}


// amacri, 2009
/**/
int
bin_setn(char *nam, char **args, char *ops, int func)
{
    int hadplus = 0, sort = 0;
    Param pm;

    if (ops['l'])
    	hadplus = PRINT_LIST;

    if (ops['n'])
    	hadplus = PRINT_NAMEONLY;

    if (ops['t'])
    	hadplus = PRINT_TYPE;

    if (ops['a'])
    	hadplus = PRINT_ASSIGN;

    if (ops['S'])
    	sort = 1;

    if (!*args) {
				scanhashtable(paramtab, sort, 0, PM_SETN | PM_READONLY, paramtab->printnode, hadplus);
				return 0;
    }

    if (*args && (ops['u'] || ops['s']) ) {
    	do {
		    if (pm = (Param) paramtab->getnode(paramtab, *args)) {
			    if (ops['s'])
			    	pm->flags |= PM_SETN;
			    if (ops['u'])
			    	pm->flags |= PM_SETN;
			    }
			  else {
    			if (ops['c']) {
    				setsparam(*args, ztrdup(""));
				    if (pm = (Param) paramtab->getnode(paramtab, *args)) {
					    if (ops['s'])
					    	pm->flags |= PM_SETN;
					    if (ops['u'])
					    	pm->flags |= PM_SETN;
					    continue;
					  }
					}
    			if (!ops['f']) {
			  		zwarnnam("setn", "invalid arg '%s'", *args, 0);
				  	return 1;
				  }
			  }
		  }
		  while (*++args);
		  return 0;
		}
return 1;
}


/* Print a series of history events to a file.  The file pointer is     *
 * given by f, and the required range of events by first and last.      *
 * subs is an optional list of foo=bar substitutions to perform on the  *
 * history lines before output.  com is an optional comp structure      *
 * that the history lines are required to match.  n, r, D and d are     *
 * options: n indicates that each line should be numbered.  r indicates *
 * that the lines should be output in reverse order (newest first).     *
 * D indicates that the real time taken by each command should be       *
 * output.  d indicates that the time of execution of each command      *
 * should be output; d>1 means that the date should be output too; d>3  *
 * means that mm/dd/yyyy form should be used for the dates, as opposed  *
 * to dd.mm.yyyy form; d>7 means that yyyy-mm-dd form should be used.   */

/**/
int
fclist(FILE *f, int n, int r, int D, int d, int first, int last, struct asgment *subs, Comp com)
{
    int Ret=0, fclistdone = 0;
    char *s, *hs;
    Histent ent;

PERMALLOC {
    /* reverse range if required */
    if (r) {
	r = last;
	last = first;
	first = r;
    }
    /* suppress "no substitution" warning if no substitution is requested */
    if (!subs)
	fclistdone = 1;

    for (;;) {
	hs = quietgetevent(first);
	if (!hs) {
	    zwarnnam("fc", "no such event: %d", NULL, first);
	    Ret=1;
	    goto FcListEnd;
	}
	s = dupstring(hs);
	/* this if does the pattern matching, if required */
	if (!com || domatch(s, com, 0)) {
	    /* perform substitution */
	    //fclistdone |= fcsubs(&s, subs);

	    /* do numbering */
	    if (n)
		fprintf(f, "%5d  ", first);
	    ent = NULL;
	    /* output actual time (and possibly date) of execution of the
	    command, if required */
	    if (d) {
		struct tm *ltm;
		if (!ent)
		    ent = gethistent(first);
		ltm = localtime(&ent->stim);
		if (d >= 2) {
		    if (d >= 8) {
			fprintf(f, "%d-%02d-%02d ",
				ltm->tm_year + 1900,
				ltm->tm_mon + 1, ltm->tm_mday);
		    } else if (d >= 4) {
			fprintf(f, "%d.%d.%d ",
				ltm->tm_mday, ltm->tm_mon + 1,
				ltm->tm_year + 1900);
		    } else {
			fprintf(f, "%d/%d/%d ",
				ltm->tm_mon + 1, ltm->tm_mday,
				ltm->tm_year + 1900);
		    }
		}
		fprintf(f, "%02d:%02d  ", ltm->tm_hour, ltm->tm_min);
	    }
	    /* display the time taken by the command, if required */
	    if (D) {
		long diff;
		if (!ent)
		    ent = gethistent(first);
		diff = (ent->ftim) ? ent->ftim - ent->stim : 0;
		fprintf(f, "%ld:%02ld  ", diff / 60, diff % 60);
	    }

	    /* output the command */
	    if (f == stdout) {
		nicezputs(s, f);
		putc('\n', f);
	    } else
		fprintf(f, "%s\n", s);
	}
	/* move on to the next history line, or quit the loop */
	if (first == last)
	    break;
	else if (first > last)
	    first--;
	else
	    first++;
    }
FcListEnd:
/* final processing */
  if (f != stdout)
		fclose(f);
  if (!fclistdone) {
		zwarnnam("fc", "no substitutions performed", NULL, 0);
		Ret=1;
	}
} LASTALLOC;
	return Ret;
}


/**/
struct histent *
quietgethist(int ev)
{
    static struct histent storehist;

    if (ev < firsthist() || ev > curhist)
        return NULL;
        return gethistent(ev);
}

/**/
char *
quietgetevent(int ev)
{
    Histent ent = quietgethist(ev);

    return ent ? ent->text : NULL;
}


/**/
int
firsthist(void)
{
    int ev;
    Histent ent;

    ev = curhist - histentct + 1;
    if (ev < 1)
        ev = 1;
    do {
        ent = gethistent(ev);
        if (ent->text)
            break;
        ev++;
    }
    while (ev < curhist);
    return ev;
}


// amacri, 2009
/**/
int
bin_fc(char *nam, char **args, char *ops, int func)
{
  int retval;
    struct asgment *asgf = NULL, *asgl = NULL;
    Comp com = NULL;
    LinkNode node, nextnode;

  if ( ops['Z'] ) {
		if (! ((LinkNode)bufstack)->next)
			return 1;
    node = bufstack->first;
    while (node) { // read direction (pop=first to last)
				zputs(node->dat, stdout);
				nextnode=node->next;
				if ( ops['e'] ) {
					zsfree(node->dat);
					zfree(node, sizeof(struct linknode));
				}
				node=nextnode;
			}
		if ( ops['e'] ) {
	    bufstack->first = NULL;
	    bufstack->last = (LinkNode) bufstack;
	  }
  	if ( !ops['N'] )
			putchar('\n');
		return 0;
	}
  if ( ops['z'] ) {
		if (! ((LinkNode)bufstack)->next)
			return 1;
    node = bufstack->last;
    while (node!=(LinkNode)bufstack) { // print direction (push=last to first)
				zputs(node->dat, stdout);
				zsfree(node->dat);
				if ( ops['e'] ) {
	        nextnode=node->last;
					zfree(node, sizeof(struct linknode));
				}
				node=nextnode;
      }
		if ( ops['e'] ) {
	    bufstack->first = NULL;
	    bufstack->last = (LinkNode) bufstack;
	  }
  	if ( !ops['N'] )
			putchar('\n');
		return 0;
	}

	/* list the required part of the history */
	retval = fclist(stdout, !ops['n'], ops['r'], ops['D'],
			ops['d'] + ops['f'] * 2 + ops['E'] * 4 + ops['i'] * 8,
			firsthist(), curhist, asgf, com);
  if ( ops['k'] )
		remhist(); //Does not work!!
  return retval;
}

// amacri, 2010
struct tsbg_thread_d{
   int  pressTimeout;
   int  releaseTimeout;
   int  flag;
   char *pathname;
   char *label;
   char *StringName;
};
static struct tsbg_thread_d tsbg_data;
static pthread_t tsbg_thread;
static tsbgActive=0;

/**/
int
bin_tsbg(char *name, char **argv, char *ops, int func)
{
   int rc, i;

      tsbg_data.flag=0;
		  if (ops['d'])
	      tsbg_data.flag=1;

		  if (ops['k']) {
				if (tsbgActive==0) {
					if (tsbg_data.flag==1)
			  	  zwarnnam(name, "thread inactive, should not be cancelled", NULL, 0);
		  	}
				if (tsbgActive==2) {
					if (tsbg_data.flag==1)
			  	  zwarnnam(name, "thread exited", NULL, 0);
					pthread_join(tsbg_thread, NULL);
					if (tsbg_data.flag==1)
			  	  zwarnnam(name, "pthread_join: %e", NULL, errno);
			  	tsbgActive=0;
		  	}
		  	if (pthread_cancel(tsbg_thread) == 0) {
					pthread_join(tsbg_thread, NULL);
					if (tsbg_data.flag==1)
			  	  zwarnnam(name, "pthread_join: %e", NULL, errno);
					if (tsbg_data.flag==1)
			  		zwarnnam(name, "thread cancelled", NULL, 0);
					tsbgActive=0;
					return(0);
				} else {
					if (tsbg_data.flag==1)
			  		zwarnnam(name, "failed to cancel thread", NULL, 0);
					return(1);
				} 
			}

		  if (ops['p']) {
				if (tsbgActive==0)
		  	  zwarnnam(name, "thread inactive", NULL, 0);
				if (tsbgActive==1)
		  	  zwarnnam(name, "thread active", NULL, 0);
				if (tsbgActive==2)
		  	  zwarnnam(name, "thread exited", NULL, 0);
				return(tsbgActive+2);
			}

		  if (!ops['c']) {
		  	  zwarnnam(name, "invalid option", NULL, 0);
				return(1);
			}

			if (tsbgActive==2) {
					pthread_join(tsbg_thread, NULL);
					if (tsbg_data.flag==1)
			  	  zwarnnam(name, "pthread_join: %e", NULL, errno);
			  	tsbgActive=0;
      }
			if (tsbgActive==1) {
					if (tsbg_data.flag==1)
			  	  zwarnnam(name, "thread already active", NULL, 0);
          return(0);
      }

      tsbg_data.pressTimeout=60;
      tsbg_data.releaseTimeout=60;
      tsbg_data.pathname=0;
      tsbg_data.label=0;
      tsbg_data.StringName=0;
    	if (*argv)
      	tsbg_data.pressTimeout=matheval(*argv);
      else {
			  zwarnnam(name, "missing <press timeout> value", NULL, 0);
				return(1);
			}
			if (* ++argv)
	      tsbg_data.releaseTimeout=matheval(*argv);
      else {
			  zwarnnam(name, "missing <release timeout> value", NULL, 0);
				return(1);
			}
    	if (* ++argv)
	      tsbg_data.pathname=ztrdup(*argv);
      else {
			  zwarnnam(name, "missing pathname", NULL, 0);
				return(1);
			}
    	if (* ++argv)
  	    tsbg_data.label=ztrdup(*argv);
      else {
			  zwarnnam(name, "missing label", NULL, 0);
				return(1);
			}
    	if (* ++argv)
  	    tsbg_data.StringName=ztrdup(*argv);
      else {
			  zwarnnam(name, "missing string name", NULL, 0);
				return(1);
			}
			if (tsbg_data.pathname[0] == '\0') {
			  zwarnnam(name, "missing filename", NULL, 0);
				return(1);
			}

      tsbgActive=1;
      rc = pthread_create(&tsbg_thread, NULL, thr_tsbg, (void *) &tsbg_data);
      if (rc){
		  	 zwarnnam(name, "cannot create thread: %e", NULL, errno);
         return(1);
      }
      return(0);
}

static sigset_t tsbg_signal_mask;  /* signals to block         */

void *thr_tsbg(void *threadarg)
{
	int ret;

	struct tsbg_thread_d *tsbg_data;

  sigemptyset (&tsbg_signal_mask);
  sigaddset (&tsbg_signal_mask, SIGINT);
	ret = pthread_sigmask (SIG_BLOCK, &tsbg_signal_mask, NULL);
	if (ret!=0) {
		if (tsbg_data->flag==1)
		  zwarnnam("tsbg", "cannot set SIG_BLOCK SIGINT to thread: %e", NULL, errno);
		pthread_exit(NULL);
	}
	tsbg_data = (struct tsbg_thread_d *) threadarg;
	ret=ts_press(0.1,&ts_x,&ts_y,&ts_pen,0.1);
	if (tsbg_data->flag==1)
	  zwarnnam("tsbg", "previous ts_press return value: %d", NULL, ret);
	ret=ts_press(0.1,&ts_x,&ts_y,&ts_pen,0.1);
	if (ret==0) {
		if (tsbg_data->flag==1)
		  zwarnnam("tsbg", "key pressed. ts_press return value: %d", NULL, ret);
		tsbgActive=2;
		pthread_exit(NULL);
	}
	if (tsbg_data->flag==1)
	  zwarnnam("tsbg", "previous ts_press return value: %d", NULL, ret);
	ret=ts_press(tsbg_data->pressTimeout,&ts_x,&ts_y,&ts_pen,tsbg_data->releaseTimeout);
	if (ret==0) {
    if ( (tsbg_data->StringName != NULL) && (tsbg_data->StringName[0] != '\0') )
			setsparam(tsbg_data->StringName, ztrdup("no"));
		if ( (tsbg_data->label != (char *) 0) && (tsbg_data->label[0] != '\0') ) {
			fbfontsetfn( (Param) 0, 1);
			fb_rect(0, 0, fb_MaxX-1, fb_MaxY-1, fb_colour(0x00, 0xff, 0xff)); // cyan
			fb_x=2; fb_y=fb_MaxY-1-fb_FontY;
			fb_col=fb_colour(0x00, 0x00, 0x00); // black ink
			fbprintf("%s", tsbg_data->label);
		}
		unlink(tsbg_data->pathname);
		if (tsbg_data->flag==1)
		  zwarnnam("tsbg", "file %s removed with return code \"%e\"", tsbg_data->pathname, errno);
	}
  tsbgActive=2;
  pthread_exit(NULL);
}

/**/
int
bin_nice(char *nam, char **argv, char * ops, int func)
{
    int ret, val=0;

    if (*argv)
			val = matheval(*argv++);
    else {
			errno=0;
			ret=getpriority(PRIO_PROCESS, 0);
	    if ((ret==-1) && (errno!=0)) {
			  zwarnnam("nice", "cannot read priority: return code \"%e\"", NULL, errno);
	    	ret=-100;
	    }
			printf("nice: process priority: %d\n", ret);
	    return(ret);
    }

		errno=0;
    ret=nice((int) val );
    if ((ret==-1) && (errno!=0)) {
		  zwarnnam("nice", "cannot change priority: return code \"%e\"", NULL, errno);
    	ret=-100;
    }
    return(ret);
}

/**/
int
bin_boolean(char *nam, char **argv, char * ops, int func)
{
	char * inbuf=NULL, *s;
	char **words, **yesParms, **pyp, *def[]={ "yes", "true", "1", 0 };
	char isTrue;
	
	yesParms = getaparam("BOOL_YES");
	if (yesParms == (char **) 0) {
		yesParms=def;
		if (ops['e']) {
			zwarnnam("boolean", "Array BOOL_YES is not valued", NULL, 0);
			return 1;
		}
	}
	words=argv;
	if (ops['A']) {
		words = getaparam(*argv);
		if (!words) {
			zwarnnam("boolean", "invalid array \"%s\"", *argv, 0);
			return 1;
		}
		if (*(++argv)) {
			zwarnnam("boolean", "too many arguments", NULL, 0);
			return 1;
		}
	}
	for (; *words; words++) {
		isTrue=0;
		inbuf = getsparam(*words);
		if (ops['d']) {
			printf("%s=%s\n", *words, (inbuf==0)?"<null>":inbuf);
			continue;
		}
		if (inbuf != 0) {
			for(s=inbuf; *s; ++s)
				*s = tolower((unsigned char)*s);
			for (pyp=yesParms; *pyp; pyp++) {
				if (!strcmp(inbuf, *pyp)) {
					isTrue=1;
					break;
				}			
			}
		}
		unsetparam(*words);
		if (ops['i']) {
			if (isTrue)
				setiparam(*words, 1);
			else
				setiparam(*words, 0);
		}
		else {
			if (isTrue)
				setsparam(*words, ztrdup("yes"));
			if ( (!isTrue) && (ops['N']) )
				setsparam(*words, ztrdup(""));
			if ( (!isTrue) && (ops['n']) )
				setsparam(*words, ztrdup("no"));
			}
	}
return 0;
}
