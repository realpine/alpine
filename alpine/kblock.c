#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: kblock.c 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $";
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

#include "headers.h"
#include "kblock.h"
#include "status.h"
#include "titlebar.h"
#include "radio.h"
#include "../pith/conf.h"
#include "../pith/state.h"

#ifdef KEYBOARD_LOCK


/*
 * Internal prototypes
 */
void	 redraw_kl_body(void);
void	 redraw_klocked_body(void);
void	 draw_klocked_body(char *, char *);


static char *klockin, *klockame;


void
redraw_kl_body(void)
{
    ClearScreen();

    set_titlebar(_("KEYBOARD LOCK"), ps_global->mail_stream,
		 ps_global->context_current, ps_global->cur_folder, NULL,
		 1, FolderName, 0, 0, NULL);

    PutLine0(6,3 ,
       _("You may lock this keyboard so that no one else can access your mail"));
    PutLine0(8, 3 ,
       _("while you are away.  The screen will be locked after entering the "));
    PutLine0(10, 3 ,
       _("password to be used for unlocking the keyboard when you return."));
    fflush(stdout);
}


void
redraw_klocked_body(void)
{
    ClearScreen();

    set_titlebar(_("KEYBOARD LOCK"), ps_global->mail_stream,
		 ps_global->context_current, ps_global->cur_folder, NULL,
		 1, FolderName, 0, 0, NULL);

    PutLine2(6, 3, _("This keyboard is locked by %s <%s>."),klockame, klockin);
    PutLine0(8, 3, _("To unlock, enter password used to lock the keyboard."));
    fflush(stdout);
}


void
draw_klocked_body(char *login, char *username)
{
    klockin = login;
    klockame = username;
    redraw_klocked_body();
}


/*----------------------------------------------------------------------
          Execute the lock keyboard command

    Args: None

  Result: keyboard is locked until user gives password
  ---*/

int
lock_keyboard(void)
{
    struct pine *ps = ps_global;
    char inpasswd[80], passwd[80], pw[80];
    HelpType help = NO_HELP;
    int i, times, old_suspend, flags;

    passwd[0] = '\0';
    redraw_kl_body();
    ps->redrawer = redraw_kl_body;

    times = atoi(ps->VAR_KBLOCK_PASSWD_COUNT);
    if(times < 1 || times > 5){
	dprint((2,
	"Kblock-passwd-count var out of range (1 to 5) [%d]\n", times));
	times = 1;
    }

    inpasswd[0] = '\0';

    for(i = 0; i < times; i++){
	pw[0] = '\0';
	while(1){			/* input pasword to use for locking */
	    int rc;
	    char prompt[50];

	    if(i > 1)
	      snprintf(prompt, sizeof(prompt), _("Retype password to LOCK keyboard (Yes, again) : "));
	    else if(i)
	      snprintf(prompt, sizeof(prompt), _("Retype password to LOCK keyboard : "));
	    else
	      snprintf(prompt, sizeof(prompt), _("Enter password to LOCK keyboard : "));

	    flags = F_ON(F_QUELL_ASTERISKS, ps_global) ? OE_PASSWD_NOAST : OE_PASSWD;
	    rc =  optionally_enter(pw, -FOOTER_ROWS(ps), 0, sizeof(pw),
				    prompt, NULL, help, &flags);

	    if(rc == 3)
	      help = help == NO_HELP ? h_kb_lock : NO_HELP;
	    else if(rc == 1 || pw[0] == '\0'){
		q_status_message(SM_ORDER, 0, 2, _("Keyboard lock cancelled"));
		return(-1);
	    }
	    else if(rc != 4)
	      break;
	}

	if(!inpasswd[0]){
	    strncpy(inpasswd, pw, sizeof(inpasswd));
	    inpasswd[sizeof(inpasswd)-1] = '\0';
	}
	else if(strcmp(inpasswd, pw)){
	    q_status_message(SM_ORDER, 0, 2,
		_("Mismatch with initial password: keyboard lock cancelled"));
	    return(-1);
	}
    }

    if(want_to(_("Really lock keyboard with entered password"), 'y', 'n',
	       NO_HELP, WT_NORM) != 'y'){
	q_status_message(SM_ORDER, 0, 2, _("Keyboard lock cancelled"));
	return(-1);
    }

    draw_klocked_body(ps->VAR_USER_ID ? ps->VAR_USER_ID : "<no-user>",
		  ps->VAR_PERSONAL_NAME ? ps->VAR_PERSONAL_NAME : "<no-name>");

    ps->redrawer = redraw_klocked_body;
    if((old_suspend = F_ON(F_CAN_SUSPEND, ps_global)) != 0)
      F_TURN_OFF(F_CAN_SUSPEND, ps_global);

    while(strcmp(inpasswd, passwd)){
	if(passwd[0])
	  q_status_message(SM_ORDER | SM_DING, 3, 3,
		     _("Password to UNLOCK doesn't match password used to LOCK"));
        
        help = NO_HELP;
        while(1){
	    int rc;

	    flags = OE_DISALLOW_CANCEL
		      | (F_ON(F_QUELL_ASTERISKS, ps_global) ? OE_PASSWD_NOAST : OE_PASSWD);
	    rc =  optionally_enter(passwd, -FOOTER_ROWS(ps), 0, sizeof(passwd),
				   _("Enter password to UNLOCK keyboard : "),NULL,
				   help, &flags);
	    if(rc == 3) {
		help = help == NO_HELP ? h_oe_keylock : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
        }
    }

    if(old_suspend)
      F_TURN_ON(F_CAN_SUSPEND, ps_global);

    q_status_message(SM_ORDER, 0, 3, _("Keyboard Unlocked"));
    return(0);
}


#endif /* KEYBOARD_LOCK */
