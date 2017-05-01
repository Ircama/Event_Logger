/*
 * $Id: hist.c,v 2.27 1996/10/18 01:00:43 hzoli Exp $
 *
 * hist.c - history expansion
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

#include "zsh.h"

/*
 * Note on curhist: with history active, this points to the
 * last line actually added to the history list.  With history inactive,
 * the line does not get added to the list until hend(), if at all.
 * However, curhist is incremented to reflect the current line anyway.
 * Thus if the line is not added to the list, curhist must be
 * decremented in hend().
 */

/* Bits of histactive variable */
#define HA_ACTIVE	(1<<0)	/* History mechanism is active */
#define HA_NOSTORE	(1<<1)	/* Don't store the line when finished */
#define HA_JUNKED	(1<<2)	/* Last history line was already junked */
#define HA_NOINC	(1<<3)	/* Don't store, curhist not incremented */

extern int cs, ll;

/* Array of word beginnings and endings in current history line. */
short *chwords;
/* Max, actual position in chwords.
 * nwords = chwordpos/2 because we record beginning and end of words.
 */
int chwordlen, chwordpos;

/* add a character to the current history word */

/**/
void
hwaddc(int c)
{
    /* Only if history line exists and lexing has not finished. */
    if (chline && !(errflag || lexstop)) {
	/* Quote un-expanded bangs in the history line. */
	if (c == bangchar && stophist < 2 && qbang)
	    /* If qbang is not set, we do not escape this bangchar as it's *
	     * not mecessary (e.g. it's a bang in !=, or it is followed    *
	     * by a space). Roughly speaking, qbang is zero only if the    *
	     * history interpreter has already digested this bang and      *
	     * found that it is not necessary to escape it.                */
	    hwaddc('\\');
	*hptr++ = c;

	/* Resize history line if necessary */
	if (hptr - chline >= hlinesz) {
	    int oldsiz = hlinesz;

	    chline = realloc(chline, hlinesz = oldsiz + 16);
	    hptr = chline + oldsiz;
	}
    }
}

/**/
int
hgetc(void)
{
    int c = ingetc();

    qbang = 0;
    if (!stophist && !(inbufflags & INP_ALIAS)) {
//	/* If necessary, expand history characters. */
//	c = histsubchar(c);
	if (c < 0) {
	    /* bad expansion */
	    errflag = lexstop = 1;
	    return ' ';
	}
    }
    if ((inbufflags & INP_HIST) && !stophist) {
	/* the current character c came from a history expansion          *
	 * (inbufflags && INP_HIST) and history is not disabled           *
	 * (e.g. we are not inside single quotes). In that case, \!       *
	 * should be treated as ! (since this \! came from a previous     *
	 * history line where \ was used to escape the bang). So if       *
	 * c == '\\' we fetch one more character to see if it's a bang,   *
	 * and if it is not, we unget it and reset c back to '\\'         */
	qbang = 0;
	if (c == '\\' && !(qbang = (c = ingetc()) == bangchar))
	    safeinungetc(c), c = '\\';
    } else if (stophist || (inbufflags & INP_ALIAS))
	/* If the result is a bangchar which came from history or alias  *
	 * expansion, we treat it as an escaped bangchar, unless history *
	 * is disabled. If stophist == 1 it only means that history is   *
	 * temporarily disabled by a !" which won't appear in in the     *
	 * history, so we still have an escaped bang. stophist > 1 if    *
	 * history is disabled with NOBANGHIST or by someone else (e.g.  *
	 * when the lexer scans single quoted text).                     */
	qbang = c == bangchar && (stophist < 2);
    hwaddc(c);
//    addtoline(c);

    return c;
}

/**/
void
safeinungetc(int c)
{
    if (lexstop)
	lexstop = 0;
    else
	inungetc(c);
}

/* extract :s/foo/bar/ delimiters and arguments */

/**/
int
getsubsargs(char *subline)
{
    int del;
    char *ptr1, *ptr2;

    del = ingetc();
    ptr1 = hdynread2(del);
    if (!ptr1)
	return 1;
    ptr2 = hdynread2(del);
    if (strlen(ptr1)) {
	zsfree(hsubl);
	hsubl = ptr1;
    }
    zsfree(hsubr);
    hsubr = ptr2;
    if (hsubl && !strstr(subline, hsubl)) {
	zerr("substitution failed", NULL, 0);
	inerrflush();
	return 1;
    }
    return 0;
}

/* Get the maximum no. of words for a history entry. */

/**/
int
getargc(Histent ehist)
{
    return ehist->nwords ? ehist->nwords-1 : 0;
}

/* unget a char and remove it from chline. It can only be used *
 * to unget a character returned by hgetc.                     */

/**/
void
hungetc(int c)
{
    int doit = 1;

    while (!lexstop) {
	if (hptr[-1] != (char) c && stophist < 4 &&
	    hptr > chline + 1 && hptr[-1] == '\n' && hptr[-2] == '\\')
	    hungetc('\n'), hungetc('\\');

	if (expanding) {
	    exlast++;
	}
	DPUTS(hptr <= chline, "BUG: hungetc attempted at buffer start");
	hptr--;
	DPUTS(*hptr != (char) c, "BUG: wrong character in hungetc() ");
	qbang = (c == bangchar && stophist < 2 &&
		 hptr > chline && hptr[-1] == '\\');
	if (doit)
	    inungetc(c);
	if (!qbang)
	    return;
	doit = !stophist && ((inbufflags & INP_HIST) ||
				 !(inbufflags & INP_ALIAS));
	c = '\\';
    }
}

/* begin reading a string */

/**/
void
strinbeg(void)
{
    strin++;
    hbegin();
    lexinit();
}

/* done reading a string */

/**/
void
strinend(void)
{
    hend();
    DPUTS(!strin, "BUG: strinend() called without strinbeg()");
    strin--;
    isfirstch = 1;
    histdone = 0;
}

/* initialize the history mechanism */

/**/
void
hbegin(void)
{
    Histent curhistent;

    isfirstln = isfirstch = 1;
    errflag = histdone = spaceflag = 0;
    stophist = (!interact || unset(BANGHIST) || unset(SHINSTDIN)) << 1;
    chline = hptr = zcalloc(hlinesz = 16);
    chwords = zalloc((chwordlen = 16)*sizeof(short));
    chwordpos = 0;

    curhistent = gethistent(curhist);
    if (!curhistent->ftim)
	curhistent->ftim = time(NULL);
    histactive = HA_ACTIVE;
    if (interact && isset(SHINSTDIN) && !strin) {
	attachtty(mypgrp);
	defev = curhist;
	curhist++;
    } else
	histactive |= HA_NOINC;
}

/* say we're done using the history mechanism */

/**/
int
hend(void)
{
    int flag, save = 1;
    Histent he;

    DPUTS(!chline, "BUG: chline is NULL in hend()");
    if (histactive & (HA_NOSTORE|HA_NOINC)) {
	zfree(chline, hlinesz);
	zfree(chwords, chwordlen*sizeof(short));
	chline = NULL;
	if (!(histactive & HA_NOINC))
	    curhist--;
	histactive = 0;
	return 1;
    }
    flag = histdone;
    histdone = 0;
    if (hptr < chline + 1)
	save = 0;
    else {
	*hptr = '\0';
	if (hptr[-1] == '\n')
	    if (chline[1]) {
		*--hptr = '\0';
	    } else
		save = 0;
	he = gethistent(curhist - 1);
	if (!*chline || !strcmp(chline, "\n") ||
	    (isset(HISTIGNOREDUPS) && he->text && !strcmp(he->text, chline)) ||
	    (isset(HISTIGNORESPACE) && spaceflag))
	    save = 0;
    }
    if (flag & (HISTFLAG_DONE | HISTFLAG_RECALL)) {
	char *ptr;

	ptr = ztrdup(chline);
	if ((flag & (HISTFLAG_DONE | HISTFLAG_RECALL)) == HISTFLAG_DONE) {
	    zputs(ptr, stderr);
	    fputc('\n', stderr);
	    fflush(stderr);
	}
	if (flag & HISTFLAG_RECALL) {
	    PERMALLOC {
		pushnode(bufstack, ptr);
	    } LASTALLOC;
	    save = 0;
	} else
	    zsfree(ptr);
    }
    if (save) {
	Histent curhistent = gethistent(curhist);
	zsfree(curhistent->text);
	if (curhistent->nwords)
	    zfree(curhistent->words, curhistent->nwords*2*sizeof(short));

	curhistent->text = ztrdup(chline);
	curhistent->stim = time(NULL);
	curhistent->ftim = 0L;
	curhistent->flags = 0;
#ifdef DEBUG
	/* debugging only */
	if (chwordpos%2) {
	    hwend();
	    DPUTS(1, "internal:  uncompleted line in history");
	}
#endif
	/* get rid of pesky \n which we've already nulled out */
	if (!chline[chwords[chwordpos-2]])
	    chwordpos -= 2;
	if ((curhistent->nwords = chwordpos/2)) {
	    curhistent->words =
		(short *)zalloc(curhistent->nwords*2*sizeof(short));
	    memcpy(curhistent->words, chwords,
		   curhistent->nwords*2*sizeof(short));
	}
    } else
	curhist--;
    zfree(chline, hlinesz);
    zfree(chwords, chwordlen*sizeof(short));
    chline = NULL;
    histactive = 0;
    return !(flag & HISTFLAG_NOEXEC || errflag);
}

/* remove the current line from the history List */

/**/
void
remhist(void)
{
    if (!(histactive & HA_ACTIVE)) {
	if (!(histactive & HA_JUNKED)) {
	    /* make sure this doesn't show up when we do firsthist() */
	    Histent he = gethistent(curhist);
	    zsfree(he->text);
	    he->text = NULL;
	    histactive |= HA_JUNKED;
	    curhist--;
	}
    } else
	histactive |= HA_NOSTORE;
}

/* Gives current expansion word if not last word before chwordpos. */
int hwgetword = -1;

/* begin a word */

/**/
void
hwbegin(int offset)
{
    if (chwordpos%2)
	chwordpos--;	/* make sure we're on a word start, not end */
    /* If we're expanding an alias, we should overwrite the expansion
     * in the history.
     */
    if ((inbufflags & INP_ALIAS) && !(inbufflags & INP_HIST))
	hwgetword = chwordpos;
    else
	hwgetword = -1;
    chwords[chwordpos++] = hptr - chline + offset;
}

/* add a word to the history List */

/**/
void
hwend(void)
{
    if (chwordpos%2 && chline) {
	/* end of word reached and we've already begun a word */
	if (hptr > chline + chwords[chwordpos-1]) {
	    chwords[chwordpos++] = hptr - chline;
	    if (chwordpos >= chwordlen) {
		chwords = (short *) realloc(chwords,
					    (chwordlen += 16)*sizeof(short));
	    }
	    if (hwgetword > -1) {
		/* We want to reuse the current word position */
		chwordpos = hwgetword;
		/* Start from where previous word ended, if possible */
		hptr = chline + chwords[chwordpos ? chwordpos - 1 : 0];
	    }
	} else {
	    /* scrub that last word, it doesn't exist */
	    chwordpos--;
	}
    }
}

/* Go back to immediately after the last word, skipping space. */

/**/
void
histbackword(void)
{
    if (!(chwordpos%2) && chwordpos)
	hptr = chline + chwords[chwordpos-1];
}

/* Get the start and end point of the current history word */

/**/
void
hwget(char **startptr)
{
    int pos = hwgetword > -1 ? hwgetword : chwordpos - 2;

#ifdef DEBUG
    /* debugging only */
    if (hwgetword == -1 && !chwordpos) {
	/* no words available */
	DPUTS(1, "hwget called with no words.");
	*startptr = "";
	return;
    } 
    else if (hwgetword == -1 && chwordpos%2) {
	DPUTS(1, "hwget called in middle of word.");
	*startptr = "";
	return;
    }
#endif

    *startptr = chline + chwords[pos];
    chline[chwords[++pos]] = '\0';
}

/* Replace the current history word with rep, if different */

/**/
void
hwrep(char *rep)
{
    char *start;
    hwget(&start);

    if (!strcmp(rep, start))
	return;
    
    hptr = start;
    chwordpos = (hwgetword > -1) ? hwgetword : chwordpos - 2;
    hwbegin(0);
    qbang = 1;
    while (*rep)
	hwaddc(*rep++);
    hwend();
}

/* Get the entire current line, deleting it in the history. */

/**/
char *
hgetline(void)
{
    /* Currently only used by pushlineoredit().
     * It's necessary to prevent that from getting too pally with
     * the history code.
     */
    char *ret;

    if (!chline || hptr == chline)
	return NULL;
    *hptr = '\0';
    ret = dupstring(chline);

    /* reset line */
    hptr = chline;
    chwordpos = 0;
    hwgetword = -1;

    return ret;
}

/* get an argument specification */

/**/
int
getargspec(int argc, int marg, int evset)
{
    int c, ret = -1;

    if ((c = ingetc()) == '0')
	return 0;
    if (idigit(c)) {
	ret = 0;
	while (idigit(c)) {
	    ret = ret * 10 + c - '0';
	    c = ingetc();
	}
	inungetc(c);
    } else if (c == '^')
	ret = 1;
    else if (c == '$')
	ret = argc;
    else if (c == '%') {
	if (evset) {
	    inerrflush();
	    zerr("Ambiguous history reference", NULL, 0);
	    return -2;
	}
	if (marg == -1) {
	    inerrflush();
	    zerr("%% with no previous word matched", NULL, 0);
	    return -2;
	}
	ret = marg;
    } else
	inungetc(c);
    return ret;
}

/* do ?foo? search */

/* do !foo search */


/* various utilities for : modifiers */

/**/
int
remtpath(char **junkptr)
{
    char *str = *junkptr, *remcut;

    if ((remcut = strrchr(str, '/'))) {
	if (str != remcut)
	    *remcut = '\0';
	else
	    str[1] = '\0';
	return 1;
    }
    return 0;
}

/**/
int
remtext(char **junkptr)
{
    char *str = *junkptr, *remcut;

    if ((remcut = strrchr(str, '.')) && remcut != str) {
	*remcut = '\0';
	return 1;
    }
    return 0;
}

/**/
int
rembutext(char **junkptr)
{
    char *str = *junkptr, *remcut;

    if ((remcut = strrchr(str, '.')) && remcut != str) {
	*junkptr = dupstring(remcut + 1);	/* .xx or xx? */
	return 1;
    }
    return 0;
}

/**/
int
remlpaths(char **junkptr)
{
    char *str = *junkptr, *remcut;

    if ((remcut = strrchr(str, '/'))) {
	*remcut = '\0';
	*junkptr = dupstring(remcut + 1);
	return 1;
    }
    return 0;
}

/**/
int
makeuppercase(char **junkptr)
{
    char *str = *junkptr;

    for (; *str; str++)
	*str = tuupper(*str);
    return 1;
}

/**/
int
makelowercase(char **junkptr)
{
    char *str = *junkptr;

    for (; *str; str++)
	*str = tulower(*str);
    return 1;
}

/**/
int
makecapitals(char **junkptr)
{
    char *str = *junkptr;

    for (; *str;) {
	for (; *str && !ialnum(*str); str++);
	if (*str)
	    *str = tuupper(*str), str++;
	for (; *str && ialnum(*str); str++)
	    *str = tulower(*str);
    }
    return 1;
}

/**/
void
subst(char **strptr, char *in, char *out, int gbal)
{
    char *str = *strptr, *instr = *strptr, *substcut, *sptr, *oldstr;
    int off, inlen, outlen;

    if (!*in)
	in = str, gbal = 0;
    if (!(substcut = (char *)strstr(str, in)))
	return;
    inlen = strlen(in);
    sptr = convamps(out, in, inlen);
    outlen = strlen(sptr);

    do {
	*substcut = '\0';
	off = substcut - *strptr + outlen;
	substcut += inlen;
	*strptr = tricat(oldstr = *strptr, sptr, substcut);
	if (oldstr != instr)
	    zsfree(oldstr);
	str = (char *)*strptr + off;
    } while (gbal && (substcut = (char *)strstr(str, in)));
}

/**/
char *
convamps(char *out, char *in, int inlen)
{
    char *ptr, *ret, *pp;
    int slen, sdup = 0;

    for (ptr = out, slen = 0; *ptr; ptr++, slen++)
	if (*ptr == '\\')
	    ptr++, sdup = 1;
	else if (*ptr == '&')
	    slen += inlen - 1, sdup = 1;
    if (!sdup)
	return out;
    ret = pp = (char *)alloc(slen + 1);
    for (ptr = out; *ptr; ptr++)
	if (*ptr == '\\')
	    *pp++ = *++ptr;
	else if (*ptr == '&') {
	    strcpy(pp, in);
	    pp += inlen;
	} else
	    *pp++ = *ptr;
    *pp = '\0';
    return ret;
}

/**/
char *
getargs(Histent elist, int arg1, int arg2)
{
    short *words = elist->words;
    int pos1, nwords = elist->nwords;

    if (arg2 < arg1 || arg1 >= nwords || arg2 >= nwords) {
	/* remember, argN is indexed from 0, nwords is total no. of words */
	inerrflush();
	zerr("no such word in event", NULL, 0);
	return NULL;
    }

    pos1 = words[2*arg1];
    return dupstrpfx(elist->text + pos1, words[2*arg2+1] - pos1);
}

/**/
void
upcase(char **x)
{
    char *pp = *(char **)x;

    for (; *pp; pp++)
	*pp = tuupper(*pp);
}

/**/
void
downcase(char **x)
{
    char *pp = *(char **)x;

    for (; *pp; pp++)
	*pp = tulower(*pp);
}

/**/
int
quote(char **tr)
{
    char *ptr, *rptr, **str = (char **)tr;
    int len = 3;
    int inquotes = 0;

    for (ptr = *str; *ptr; ptr++, len++)
	if (*ptr == '\'') {
	    len += 3;
	    if (!inquotes)
		inquotes = 1;
	    else
		inquotes = 0;
	} else if (inblank(*ptr) && !inquotes && ptr[-1] != '\\')
	    len += 2;
    ptr = *str;
    *str = rptr = (char *)alloc(len);
    *rptr++ = '\'';
    for (; *ptr; ptr++)
	if (*ptr == '\'') {
	    if (!inquotes)
		inquotes = 1;
	    else
		inquotes = 0;
	    *rptr++ = '\'';
	    *rptr++ = '\\';
	    *rptr++ = '\'';
	    *rptr++ = '\'';
	} else if (inblank(*ptr) && !inquotes && ptr[-1] != '\\') {
	    *rptr++ = '\'';
	    *rptr++ = *ptr;
	    *rptr++ = '\'';
	} else
	    *rptr++ = *ptr;
    *rptr++ = '\'';
    *rptr++ = 0;
    str[1] = NULL;
    return 0;
}

/**/
int
quotebreak(char **tr)
{
    char *ptr, *rptr, **str = (char **)tr;
    int len = 3;

    for (ptr = *str; *ptr; ptr++, len++)
	if (*ptr == '\'')
	    len += 3;
	else if (inblank(*ptr))
	    len += 2;
    ptr = *str;
    *str = rptr = (char *)alloc(len);
    *rptr++ = '\'';
    for (; *ptr;)
	if (*ptr == '\'') {
	    *rptr++ = '\'';
	    *rptr++ = '\\';
	    *rptr++ = '\'';
	    *rptr++ = '\'';
	    ptr++;
	} else if (inblank(*ptr)) {
	    *rptr++ = '\'';
	    *rptr++ = *ptr++;
	    *rptr++ = '\'';
	} else
	    *rptr++ = *ptr++;
    *rptr++ = '\'';
    *rptr++ = '\0';
    return 0;
}

/* read an arbitrary amount of data into a buffer until stop is found */

/**/
char *
hdynread(int stop)
{
    int bsiz = 256, ct = 0, c;
    char *buf = (char *)zalloc(bsiz), *ptr;

    ptr = buf;
    while ((c = ingetc()) != stop && c != '\n' && !lexstop) {
	if (c == '\\')
	    c = ingetc();
	*ptr++ = c;
	if (++ct == bsiz) {
	    buf = realloc(buf, bsiz *= 2);
	    ptr = buf + ct;
	}
    }
    *ptr = 0;
    if (c == '\n') {
	inungetc('\n');
	zerr("delimiter expected", NULL, 0);
	zfree(buf, bsiz);
	return NULL;
    }
    return buf;
}

/**/
char *
hdynread2(int stop)
{
    int bsiz = 256, ct = 0, c;
    char *buf = (char *)zalloc(bsiz), *ptr;

    ptr = buf;
    while ((c = ingetc()) != stop && c != '\n' && !lexstop) {
	if (c == '\n') {
	    inungetc(c);
	    break;
	}
	if (c == '\\')
	    c = ingetc();
	*ptr++ = c;
	if (++ct == bsiz) {
	    buf = realloc(buf, bsiz *= 2);
	    ptr = buf + ct;
	}
    }
    *ptr = 0;
    if (c == '\n')
	inungetc('\n');
    return buf;
}

/**/
void
inithist(void)
{
    histentct = histsiz;
    histentarr = (Histent) zcalloc(histentct * sizeof *histentarr);
}
