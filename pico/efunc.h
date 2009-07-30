/*
 * $Id: efunc.h 807 2007-11-09 01:21:33Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2007 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 *
 * Program:	Pine's composer and pico's function declarations
 */

/*	EFUNC.H:	MicroEMACS function declarations and names

		This file list all the C code functions used by MicroEMACS
	and the names to use to bind keys to them. To add functions,
	declare it here in both the extern function list and the name
	binding table.

	Update History:

	Daniel Lawrence
*/

#ifndef	EFUNC_H
#define	EFUNC_H


/*	External function declarations		*/
/* attach.c */
extern	int AskAttach(char *, size_t, LMLIST **);
extern	int SyncAttach(void);
extern	int intag(UCS *, int);
extern	char *prettysz(off_t);
extern  int AttachError(void);
extern	char *QuoteAttach(char *, size_t);
extern  int	getccol(int);

/* basic.c */
extern	int gotobol(int, int);
extern	int backchar(int, int);
extern	int gotoeol(int, int);
extern	int forwchar(int, int);
extern	int gotoline(int, int);
extern	int gotobob(int, int);
extern	int gotoeob(int, int);
extern	int forwline(int, int);
extern	int backline(int, int);
extern	int gotobop(int, int);
extern	int gotoeop(int, int);
extern	int forwpage(int, int);
extern	int backpage(int, int);
extern  int scrollupline(int, int);
extern  int scrolldownline(int, int);
extern  int scrollto(int, int);
extern	int setmark(int, int);
extern	int swapmark(int, int);
extern	int setimark(int, int);
extern	int swapimark(int, int);
extern	int mousepress(int, int);
extern	int toggle_xterm_mouse(int, int);
extern	void swap_mark_and_dot_if_mark_comes_first(void);
extern	int backchar_no_header_editor(int, int);
extern  int getgoal(struct LINE *);

/* bind.c */
extern	UCS normalize_cmd(UCS c, UCS list[][2], int sc);
extern	int whelp(int, int);
extern	int wscrollw(int, int, char **, int);
extern	int normal(int, int (*)[2], int);
extern	void rebindfunc(int (*)(int, int),int (*)(int, int));
extern	int bindtokey(UCS c, int (*f)(int, int));

/* browse.c */
extern	int FileBrowse(char *, size_t, char *, size_t, char *, size_t, int, LMLIST **);
extern	int ResizeBrowser(void);
extern	void set_browser_title(char *);
extern  void zotlmlist(LMLIST *);
extern	int time_to_check(void);
extern  int LikelyASCII(char *);

/* buffer.c */
extern	int anycb(void);
extern	struct BUFFER *bfind(char *, int, int);
extern	int bclear(struct BUFFER *);
extern	int packbuf(char **, int *, int);
extern	void readbuf(char **);

/* composer.c */
extern	int InitMailHeader(struct pico_struct *);
extern	int ResizeHeader(void);
extern	int HeaderEditor(int, int);
extern	void PaintHeader(int, int);
extern	void ArrangeHeader(void);
extern	int ToggleHeader(int);
extern	int HeaderLen(void);
extern	int UpdateHeader(int);
extern	int entry_line(int, int);
extern	int call_builder(struct headerentry *, int *, char **);
extern	void call_expander(void);
extern	void ShowPrompt(void);
extern	int packheader(void);
extern	void zotheader(void);
extern	void display_for_send(void);
extern	VARS_TO_SAVE *save_pico_state(void);
extern	void restore_pico_state(VARS_TO_SAVE *);
extern	void free_pico_state(VARS_TO_SAVE *);
extern	void HeaderPaintCursor(void);
extern	void PaintBody(int);
extern	int AppendAttachment(char *, char *, char *);

/* display.c */
extern	int vtinit(void);
extern	int vtterminalinfo(int);
extern	void vttidy(void);
extern	void update(void);
extern	void modeline(struct WINDOW *);
extern	void movecursor(int, int);
extern	void clearcursor(void);
extern	void mlerase(void);
extern	int mlyesno_utf8(char *, int);
extern	int mlyesno(UCS *, int);
extern	int mlreply_utf8(char *, char *, int, int, EXTRAKEYS *);
extern	int mlreply(UCS *, UCS *, int, int, EXTRAKEYS *);
extern	int mlreplyd_utf8(char *, char *, int, int, EXTRAKEYS *);
extern	int mlreplyd(UCS *, UCS *, int, int, EXTRAKEYS *);
extern	int mlwrite_utf8(char *, void *);
extern	int mlwrite(UCS *, void *);
extern	void emlwrite(char *, EML *);
extern	void emlwrite_ucs4(UCS *, EML *);
extern	void unknown_command(UCS);
extern	void scrolldown(struct WINDOW *, int, int);
extern	void scrollup(struct WINDOW *, int, int);
extern	int doton(int *, unsigned int *);
extern	int resize_pico(int, int);
extern	void zotdisplay(void);
extern	void pputc(UCS c, int a);
extern	void pputs(UCS *s, int a);
extern	void pputs_utf8(char *s, int a);
extern	void peeol(void);
extern	CELL *pscr(int, int);
extern	void pclear(int, int);
extern	int pinsert(CELL);
extern	int pdel(void);
extern	void wstripe(int, int, char *, int);
extern	void wkeyhelp(KEYMENU *);
extern	void get_cursor(int *, int *);
extern  unsigned vcellwidth_a_to_b(int row, int a, int b);
extern  int index_from_col(int row, int col);

/* file.c */
extern	int fileread(int, int);
extern	int insfile(int, int);
extern	int readin(char *, int, int);
extern	int filewrite(int, int);
extern	int filesave(int, int);
extern	int writeout(char *, int);
extern	char *writetmp(int, char *);
extern	int filename(int, int);
extern	int in_oper_tree(char *);
extern  int ifile(char *);

/* fileio.c */
extern	int ffropen(char *);
extern	int ffputline(CELL *, int);
extern	int ffgetline(UCS *, size_t, size_t *, int);

/* line.c */
extern	struct LINE *lalloc(int used);
extern	void lfree(struct LINE *);
extern	void lchange(int);
extern	int linsert(int n, UCS c);
extern	int geninsert(LINE **dotp, int *doto, LINE *linep, UCS c, int attb, int n, long *lines);
extern	int lnewline(void);
extern	int ldelete(long, int (*)(UCS));
extern	int lisblank(struct LINE *);
extern	void kdelete(void);
extern	int kinsert(UCS);
extern	long kremove(int);
extern	int ksize(void);
extern	void fdelete(void);
extern	int finsert(UCS);
extern	long fremove(int);
extern	void set_last_region_added(REGION *);
extern	REGION *get_last_region_added(void);

/* os.c */
extern	int o_insert(UCS);
extern	int o_delete(void);

/* pico.c */
extern	int pico(struct pico_struct *pm);
extern	void edinit(char *);
extern	int execute(UCS c, int f, int n);
extern	int quickexit(int, int);
extern	int abort_composer(int, int);
extern	int suspend_composer(int, int);
extern	int wquit(int, int);
extern	int ctrlg(int, int);
extern	int rdonly(void);
extern	int pico_help(char **, char *, int);
extern	void zotedit(void);
#ifdef	_WINDOWS
int	composer_file_drop(int, int, char *);
int	pico_cursor(int, long);
#endif

/* random.c */
extern	int showcpos(int, int);
extern	int tab(int, int);
extern	int newline(int, int);
extern	int forwdel(int, int);
extern	int backdel(int, int);
extern	int killtext(int, int);
extern	int yank(int, int);

/* region.c */
extern	int killregion(int, int);
extern	int deleteregion(int, int);
extern	int markregion(int);
extern  int getregion(REGION *, LINE *, int);
extern	void unmarkbuffer(void);

/* search.c */
extern	int forwsearch(int, int);
extern	int readpattern(char *, int);
extern	int forscan(int *, UCS *, LINE *, int, int);
extern	void chword(UCS *, UCS *);

/* spell.c */
#ifdef	SPELLER
extern	int spell(int, int);
#endif

/* window.c */
extern	int  pico_refresh(int, int);
extern	void redraw_pico_for_callback(void);

/* word.c */
extern	int wrapword(void);
extern	int backword(int, int);
extern	int forwword(int, int);
extern	int fillpara(int, int);
extern	int fillbuf(int, int);
extern	int inword(void);
extern	int quote_match(UCS *, LINE *, UCS *, size_t);
extern	int ucs4_isalnum(UCS);
extern	int ucs4_isalpha(UCS);
extern	int ucs4_isspace(UCS);
extern	int ucs4_ispunct(UCS);

#endif	/* EFUNC_H */
