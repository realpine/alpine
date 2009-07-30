#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: termout.gen.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006-2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */
#include <system.h>
#include <general.h>

#include "../../c-client/mail.h"	/* for MAILSTREAM and friends */
#include "../../c-client/osdep.h"
#include "../../c-client/rfc822.h"	/* for soutr_t and such */
#include "../../c-client/misc.h"	/* for cpystr proto */
#include "../../c-client/utf8.h"	/* for CHARSET and such*/
#include "../../c-client/imap4r1.h"

#include "../../pith/osdep/color.h"
#include "../../pith/charconv/filesys.h"

#include "../../pith/debug.h"
#include "../../pith/newmail.h"
#include "../../pith/filter.h"
#include "../../pith/handle.h"
#include "../../pith/conf.h"
#include "../../pith/mimedesc.h"

#include "../../pico/estruct.h"
#include "../../pico/pico.h"
#include "../../pico/keydefs.h"
#include "../../pico/osdep/color.h"

#include "../status.h"
#include "../radio.h"
#include "../folder.h"
#include "../keymenu.h"
#include "../send.h"
#include "../mailindx.h"

int   _line  = FARAWAY;
int   _col   = FARAWAY;

#include "termout.gen.h"


#define	PUTLINE_BUFLEN	256



/*
 * Generic tty output routines...
 */

/*----------------------------------------------------------------------
      Printf style output line to the screen at given position, 0 args

  Args:  x -- column position on the screen
         y -- row position on the screen
         line -- line of text to output

 Result: text is output
         cursor position is update
  ----*/
void
PutLine0(int x, int y, register char *line)
{
    MoveCursor(x,y);
    Write_to_screen(line);
}



/*----------------------------------------------------------------------
  Output line of length len to the display observing embedded attributes

 Args:  x      -- column position on the screen
        y      -- column position on the screen
        line   -- text to be output
        length -- length of text to be output

 Result: text is output
         cursor position is updated
  ----------------------------------------------------------------------*/
void
PutLine0n8b(int x, int y, register char *line, int length, HANDLE_S *handles)
{
    unsigned char c;
    int is_inv = 0, is_bold = 0, is_uline = 0, is_fg = 0, is_bg = 0;
#ifdef	_WINDOWS
    int hkey = 0;
#endif

    MoveCursor(x,y);

    while(length--){

	c = (unsigned char) *line++;

	if(c == (unsigned char) TAG_EMBED && length){

	    length--;

	    switch(*line++){
	      case TAG_INVON :
		StartInverse();
		is_inv = 1;
		break;

	      case TAG_INVOFF :
		EndInverse();
		is_inv = 0;
		break;

	      case TAG_BOLDON :
		StartBold();
		is_bold = 1;
		break;

	      case TAG_BOLDOFF :
		EndBold();
		is_bold = 0;
		break;

	      case TAG_ITALICON : /* express italic as uline in terminal */
	      case TAG_ULINEON :
		StartUnderline();
		is_uline = 1;
		break;

	      case TAG_ITALICOFF : /* express italic as uline in terminal */
	      case TAG_ULINEOFF :
		EndUnderline();
		is_uline = 0;
		break;

	      case TAG_HANDLE :
		length -= *line + 1;	/* key length plus length tag */
		if(handles){
		    int  key, n, current_key = 0;

		    for(key = 0, n = *line++; n; n--) /* forget Horner? */
		      key = (key * 10) + (*line++ - '0');

#if	_WINDOWS
		    hkey = key;
#endif

		    if(handles->using_is_used){
			HANDLE_S *h;

			for(h = handles; h; h = h->next)
			  if(h->is_used)
			    break;
			
			if(h)
			  current_key = h->key;
		    }
		    else
		      current_key = handles->key;

		    if(key == current_key){
			COLOR_PAIR *curcolor = NULL;
			COLOR_PAIR *revcolor = NULL;

			if(handles->color_unseen
			   && (curcolor = pico_get_cur_color())
			   && (colorcmp(curcolor->fg, ps_global->VAR_NORM_FORE_COLOR)
			       || colorcmp(curcolor->bg, ps_global->VAR_NORM_BACK_COLOR))
			   && (revcolor = apply_rev_color(curcolor,
							  ps_global->index_color_style)))
			  (void) pico_set_colorp(revcolor, PSC_NONE);
			else{

			    if(pico_usingcolor() &&
			       ps_global->VAR_SLCTBL_FORE_COLOR &&
			       ps_global->VAR_SLCTBL_BACK_COLOR){
				pico_set_normal_color();
			    }
			    else
			      EndBold();

			    StartInverse();
			    is_inv = 1;
			}

			if(curcolor)
			  free_color_pair(&curcolor);

			if(revcolor)
			  free_color_pair(&revcolor);
		    }
		}
		else{
		    /* BUG: complain? */
		    line += *line + 1;
		}

		break;

	      case TAG_FGCOLOR :
		if(length < RGBLEN){
		    dprint((9,
			       "FGCOLOR not proper length, ignoring\n"));
		    length = 0;
		    break;
		}

		(void)pico_set_fg_color(line);
		is_fg = 1;
		length -= RGBLEN;
		line += RGBLEN;
		break;

	      case TAG_BGCOLOR :
		if(length < RGBLEN){
		    dprint((9,
			       "BGCOLOR not proper length, ignoring\n"));
		    length = 0;
		    break;
		}

		(void)pico_set_bg_color(line);
		is_bg = 1;
		length -= RGBLEN;
		line += RGBLEN;
		break;

	      case TAG_EMBED:			/* literal "embed" char */
		Writechar(TAG_EMBED, 0);
		break;

	      case TAG_STRIKEON :		/* unsupported text markup */
	      case TAG_STRIKEOFF :
	      case TAG_BIGON :
	      case TAG_BIGOFF :
	      case TAG_SMALLON :
	      case TAG_SMALLOFF :
	      default :				/* Eat unrecognized tag - TAG_BIGON, etc */
		break;
	    }					/* tag with handle, skip it */
	}	
	else
	  Writechar(c, 0);
    }


#if	_WINDOWS_X
    if(hkey) {
	char *tmp_file = NULL, ext[32], mtype[128];
	HANDLE_S *h;
	extern HANDLE_S *get_handle (HANDLE_S *, int);

	if((h = get_handle(handles, hkey)) && h->type == Attach){
	    ext[0] = '\0';
	    strncpy(mtype, body_type_names(h->h.attach->body->type), sizeof(mtype));
	    mtype[sizeof(mtype)-1] = '\0';
	    if (h->h.attach->body->subtype) {
		strncat (mtype, "/", sizeof(mtype)-strlen(mtype)-1);
		mtype[sizeof(mtype)-1] = '\0';
		strncat (mtype, h->h.attach->body->subtype, sizeof(mtype)-strlen(mtype)-1);
		mtype[sizeof(mtype)-1] = '\0';
	    }

	    if(!set_mime_extension_by_type(ext, mtype)){
		char *p, *extp = NULL;

		if((p = get_filename_parameter(NULL, 0, h->h.attach->body, &extp)) != NULL){
		    if(extp){
			strncpy(ext, extp, sizeof(ext));
			ext[sizeof(ext)-1] = '\0';
		    }

		    fs_give((void **) &p);
		}
	    }

	    if(ext[0] && (tmp_file = temp_nam_ext(NULL, "im", ext))){
		FILE *f = our_fopen(tmp_file, "w");

		mswin_registericon(x, h->key, tmp_file);

		fclose(f);
		our_unlink(tmp_file);
		fs_give((void **)&tmp_file);
	    }
	}
    }
#endif
    if(is_inv){
	dprint((9,
		   "INVERSE left on at end of line, turning off now\n"));
	EndInverse();
    }
    if(is_bold){
	dprint((9,
		   "BOLD left on at end of line, turning off now\n"));
	EndBold();
    }
    if(is_uline){
	dprint((9,
		   "UNDERLINE left on at end of line, turning off now\n"));
	EndUnderline();
    }
    if(is_fg || is_bg)
      pico_set_normal_color();

}


/*----------------------------------------------------------------------
      Printf style output line to the screen at given position, 1 arg

 Input:  position on the screen
         line of text to output

 Result: text is output
         cursor position is update
  ----------------------------------------------------------------------*/
void
/*VARARGS2*/
PutLine1(int x, int y, char *line, void *arg1)
{
    char buffer[PUTLINE_BUFLEN];

    snprintf(buffer, sizeof(buffer), line, arg1);
    buffer[sizeof(buffer)-1] = '\0';
    PutLine0(x, y, buffer);
}


/*----------------------------------------------------------------------
      Printf style output line to the screen at given position, 2 args

 Input:  position on the screen
         line of text to output

 Result: text is output
         cursor position is update
  ----------------------------------------------------------------------*/
void
/*VARARGS3*/
PutLine2(int x, int y, char *line, void *arg1, void *arg2)
{
    char buffer[PUTLINE_BUFLEN];

    snprintf(buffer, sizeof(buffer), line, arg1, arg2);
    buffer[sizeof(buffer)-1] = '\0';
    PutLine0(x, y, buffer);
}


/*----------------------------------------------------------------------
      Printf style output line to the screen at given position, 3 args

 Input:  position on the screen
         line of text to output

 Result: text is output
         cursor position is update
  ----------------------------------------------------------------------*/
void
/*VARARGS4*/
PutLine3(int x, int y, char *line, void *arg1, void *arg2, void *arg3)
{
    char buffer[PUTLINE_BUFLEN];

    snprintf(buffer, sizeof(buffer), line, arg1, arg2, arg3);
    buffer[sizeof(buffer)-1] = '\0';
    PutLine0(x, y, buffer);
}


/*----------------------------------------------------------------------
      Printf style output line to the screen at given position, 4 args

 Args:  x -- column position on the screen
        y -- column position on the screen
        line -- printf style line of text to output

 Result: text is output
         cursor position is update
  ----------------------------------------------------------------------*/
void
/*VARARGS5*/
PutLine4(int x, int y, char *line, void *arg1, void *arg2, void *arg3, void *arg4)
{
    char buffer[PUTLINE_BUFLEN];

    snprintf(buffer, sizeof(buffer), line, arg1, arg2, arg3, arg4);
    buffer[sizeof(buffer)-1] = '\0';
    PutLine0(x, y, buffer);
}



/*----------------------------------------------------------------------
      Printf style output line to the screen at given position, 5 args

 Args:  x -- column position on the screen
        y -- column position on the screen
        line -- printf style line of text to output

 Result: text is output
         cursor position is update
  ----------------------------------------------------------------------*/
void
/*VARARGS6*/
PutLine5(int x, int y, char *line, void *arg1, void *arg2, void *arg3, void *arg4, void *arg5)
{
    char buffer[PUTLINE_BUFLEN];

    snprintf(buffer, sizeof(buffer), line, arg1, arg2, arg3, arg4, arg5);
    buffer[sizeof(buffer)-1] = '\0';
    PutLine0(x, y, buffer);
}


/*----------------------------------------------------------------------
       Output a UTF-8 line to the screen, centered

  Input:  Line number to print on, string to output
  
 Result:  String is output to screen
          Returns column number line is output on
  ----------------------------------------------------------------------*/
int
Centerline(int line, char *string)
{
    int width, col;

    width = (int) utf8_width(string);

    if (width > ps_global->ttyo->screen_cols)
      col = 0;
    else
      col = (ps_global->ttyo->screen_cols - width) / 2;

    PutLine0(line, col, string);

    return(col);
}



/*----------------------------------------------------------------------
     Clear specified line on the screen

 Result: The line is blanked and the cursor is left at column 0.

  ----*/
void
ClearLine(int n)
{
    if(ps_global->in_init_seq)
      return;

    MoveCursor(n, 0);
    CleartoEOLN();
}



/*----------------------------------------------------------------------
     Clear specified lines on the screen

 Result: The lines starting at 'x' and ending at 'y' are blanked
	 and the cursor is left at row 'x', column 0

  ----*/
void
ClearLines(int x, int y)
{
    int i;

    for(i = x; i <= y; i++)
      ClearLine(i);

    MoveCursor(x, 0);
}



/*----------------------------------------------------------------------
    Indicate to the screen painting here that the position of the cursor
 has been disturbed and isn't where these functions might think.
 ----*/
void
clear_cursor_pos(void)
{
    _line = FARAWAY;
    _col  = FARAWAY;
}


/*----------------------------------------------------------------------
    Write a character to the screen, keeping track of cursor position

   Args: ch -- character to output. The stream of characters coming to
               this function is expected to be UTF-8. State is kept  between
	       calls in order to collect up the octets needed for a single
	       Unicode character.

 Result: character output
         cursor position variables updated
  ----*/
void
Writechar(unsigned int ch, int new_esc_len)
{
    static   unsigned char  cbuf[6];
    static   unsigned char *cbufp = cbuf;

    if(cbufp < cbuf+sizeof(cbuf)){
	unsigned char *inputp;
	unsigned long remaining_octets;
	UCS ucs;

	*cbufp++ = (unsigned char) ch;
	inputp = cbuf;
	remaining_octets = (cbufp - cbuf) * sizeof(unsigned char);
	ucs = (UCS) utf8_get(&inputp, &remaining_octets);

	switch(ucs){
	  case U8G_ENDSTRG:	/* incomplete character, wait */
	  case U8G_ENDSTRI:	/* incomplete character, wait */
	    break;

	  default:
	    if(ucs & U8G_ERROR || ucs == UBOGON){
		/*
		 * None of these cases is supposed to happen. If it
		 * does happen then the input stream isn't UTF-8
		 * so something is wrong. Treat each character in the
		 * input buffer as a separate error character and
		 * print a '?' for each.
		 */
		for(inputp = cbuf; inputp < cbufp; inputp++){
		    int width = 0;

		    if(_col + width <= ps_global->ttyo->screen_cols){
			Writewchar('?');
			width++;
		    }
		}

		cbufp = cbuf;	/* start over */
	    }
	    else{

		/* got a good character */
		Writewchar(ucs);

		/* update the input buffer */
		if(inputp >= cbufp)	/* this should be the case */
		  cbufp = cbuf;
		else{		/* extra chars for some reason? */
		    unsigned char *q, *newcbufp;

		    newcbufp = (cbufp - inputp) + cbuf;
		    q = cbuf;
		    while(inputp < cbufp)
		      *q++ = *inputp++;

		    cbufp = newcbufp;
		}
	    }

	    break;
	}
    }
    else{			/* error */
	Writewchar('?');
	cbufp = cbuf;		/* start over */
    }
}
