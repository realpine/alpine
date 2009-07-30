/*
 * $Id: titlebar.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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
 */

#ifndef PINE_TITLEBAR_INCLUDED
#define PINE_TITLEBAR_INCLUDED


#include "../pith/context.h"
#include "../pith/msgno.h"
#include "../pith/osdep/color.h"


typedef struct titlebarcontainer {
    char       titlebar_line[6*MAX_SCREEN_COLS+1];
    COLOR_PAIR color;
} TITLE_S;


typedef enum {TitleBarNone = 0, FolderName, MessageNumber, MsgTextPercent,
		TextPercent, FileTextPercent, ThrdIndex,
		ThrdMsgNum, ThrdMsgPercent} TitleBarType;


/* exported protoypes */
void	    end_titlebar(void);
void	    push_titlebar_state(void);
void	    pop_titlebar_state(void);
void	    mark_titlebar_dirty(void);
char	   *set_titlebar(char *, MAILSTREAM *, CONTEXT_S *, char *, MSGNO_S *, int,
			 TitleBarType, long, long, COLOR_PAIR *);
void	    redraw_titlebar(void);
TITLE_S	   *format_titlebar(void);
COLOR_PAIR *current_titlebar_color(void);
void	    update_titlebar_message(void);
void	    update_titlebar_percent(long);
void	    update_titlebar_lpercent(long);
void	    titlebar_stream_closing(MAILSTREAM *);
int         update_titlebar_status(void);
void        check_cue_display(char *);


#endif /* PINE_TITLEBAR_INCLUDED */
