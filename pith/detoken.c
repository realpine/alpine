#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: detoken.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#include "../pith/headers.h"
#include "../pith/detoken.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/status.h"
#include "../pith/pattern.h"
#include "../pith/reply.h"
#include "../pith/mailindx.h"
#include "../pith/options.h"


/*
 * Hook to read signature from local file
 */
char	*(*pith_opt_get_signature_file)(char *, int, int, int);


/*
 * Internal prototypes
 */
char	*detoken_guts(char *, int, ENVELOPE *, ACTION_S *, REDRAFT_POS_S **, int, int *);
char	*handle_if_token(char *, char *, int, ENVELOPE *, ACTION_S *, char **);
char	*get_token_arg(char *, char **);


/*
 * Detokenize signature or template files.
 *
 * If is_sig, we always use literal sigs before sigfiles if they are
 * defined. So, check for role->litsig and use it. If it doesn't exist, use
 * the global literal sig if defined. Else the role->sig file or the
 * global signature file.
 *
 * If !is_sig, use role->template.
 *
 * So we start with a literal signature or a signature or template file.
 * If that's a file, we read it first. The file could be remote.
 * Then we detokenize the literal signature or file contents and return
 * an allocated string which the caller frees.
 *
 * Args    role -- See above about what happens depending on is_sig.
 *		   relative to the pinerc dir.
 *          env -- The envelope to use for detokenizing. May be NULL.
 *  prenewlines -- How many blank lines should be included at start.
 * postnewlines -- How many blank lines should be included after.
 *       is_sig -- This is a signature (not a template file).
 *  redraft_pos -- This is a return value. If it is non-NULL coming in,
 *		   then the cursor position is returned here.
 *         impl -- This is a combination argument which is both an input
 *		   argument and a return value. If it is non-NULL and = 0,
 *		   that means that we want the cursor position returned here,
 *		   even if that position is set implicitly to the end of
 *		   the output string. If it is = 1 coming in, that means
 *		   we only want the cursor position to be set if it is set
 *		   explicitly. If it is 2, or if redraft_pos is NULL,
 *                 we don't set it at all.
 *		   If the cursor position gets set explicitly by a
 *		   _CURSORPOS_ token in the file then this is set to 2
 *		   on return. If the cursor position is set implicitly to
 *		   the end of the included file, then this is set to 1
 *                 on return.
 *
 * Returns -- An allocated string is returned.
 */
char *
detoken(ACTION_S *role, ENVELOPE *env, int prenewlines, int postnewlines,
	int is_sig, REDRAFT_POS_S **redraft_pos, int *impl)
{
    char *ret = NULL,
         *src = NULL,
         *literal_sig = NULL,
         *sigfile = NULL;

    if(is_sig){
	/*
	 * If role->litsig is set, we use it;
	 * Else, if VAR_LITERAL_SIG is set, we use that;
	 * Else, if role->sig is set, we use that;
	 * Else, if VAR_SIGNATURE_FILE is set, we use that.
	 * This can be a little surprising if you set the VAR_LITERAL_SIG
	 * and don't set a role->litsig but do set a role->sig. The
	 * VAR_LITERAL_SIG will be used, not role->sig. The reason for this
	 * is mostly that it is much easier to display the right stuff
	 * in the various config screens if we do it that way. Besides,
	 * people will typically use only literal sigs or only sig files,
	 * there is no reason to mix them, so we don't provide support to
	 * do so.
	 */
	if(role && role->litsig)
	  literal_sig = role->litsig;
	else if(ps_global->VAR_LITERAL_SIG)
	  literal_sig = ps_global->VAR_LITERAL_SIG;
	else if(role && role->sig)
	  sigfile = role->sig;
	else
	  sigfile = ps_global->VAR_SIGNATURE_FILE;
    }
    else if(role && role->template)
      sigfile = role->template;

    if(literal_sig)
      src = get_signature_lit(literal_sig, prenewlines, postnewlines, is_sig,1);
    else if(sigfile && pith_opt_get_signature_file)
      src = (*pith_opt_get_signature_file)(sigfile, prenewlines, postnewlines, is_sig);
	
    if(src){
	if(*src)
	  ret = detoken_src(src, FOR_TEMPLATE, env, role, redraft_pos, impl);

	fs_give((void **)&src);
    }

    return(ret);
}


/*
 * Filter the source string from the template file and return an allocated
 * copy of the result with text replacements for the tokens.
 * Fill in offset in redraft_pos.
 *
 * This is really inefficient but who cares? It's just cpu time.
 */
char *
detoken_src(char *src, int for_what, ENVELOPE *env, ACTION_S *role,
	    REDRAFT_POS_S **redraft_pos, int *impl)
{
    int   loopcnt = 25;		/* just in case, avoid infinite loop */
    char *ret, *str1, *str2;
    int   done = 0;

    if(!src)
      return(src);
    
    /*
     * We keep running it through until it stops changing so user can
     * nest calls to token stuff.
     */
    str1 = src;
    do {
	/* short-circuit if no chance it will change */
	if(strindex(str1, '_'))
	  str2 = detoken_guts(str1, for_what, env, role, NULL, 0, NULL);
	else
	  str2 = str1;

	if(str1 && str2 && (str1 == str2 || !strcmp(str1, str2))){
	    done++;				/* It stopped changing */
	    if(str1 && str1 != src && str1 != str2)
	      fs_give((void **)&str1);
	}
	else{					/* Still changing */
	    if(str1 && str1 != src && str1 != str2)
	      fs_give((void **)&str1);
	    
	    str1 = str2;
	}

    } while(str2 && !done && loopcnt-- > 0);

    /*
     * Have to run it through once more to get the redraft_pos and
     * to remove any backslash escape for a token.
     */
    if((str2 && strindex(str2, '_')) ||
       (impl && *impl == 0 && redraft_pos && !*redraft_pos)){
	ret = detoken_guts(str2, for_what, env, role, redraft_pos, 1, impl);
	if(str2 != src)
	  fs_give((void **)&str2);
    }
    else if(str2){
	if(str2 == src)
	  ret = cpystr(str2);
	else
	  ret = str2;
    }

    return(ret);
}


/*
 * The guts of the detokenizing routines. Filter the src string looking for
 * tokens and replace them with the appropriate text. In the case of the
 * cursor_pos token we set redraft_pos instead.
 *
 * Args        src --  The source string
 *        for_what --  
 *             env --  Envelope to look in for token replacements.
 *     redraft_pos --  Return the redraft offset here, if non-zero.
 *       last_pass --  This is a flag to tell detoken_guts whether or not to do
 *                     the replacement for _CURSORPOS_. Leave it as is until
 *                     the last pass. We need this because we want to defer
 *                     cursor placement until the very last call to detoken,
 *                     otherwise we'd have to keep track of the cursor
 *                     position as subsequent text replacements (nested)
 *                     take place.
 *                     This same flag is also used to decide when to eliminate
 *                     backslash escapes from in front of tokens. The only
 *                     use of backslash escapes is to escape an entire token.
 *                     That is, \_DATE_ is a literal _DATE_, but any other
 *                     backslash is a literal backslash. That way, nobody
 *                     but wackos will have to worry about backslashes.
 *         impl -- This is a combination argument which is both an input
 *		   argument and a return value. If it is non-NULL and 0
 *		   coming in, that means that we should set redraft_pos,
 *		   even if that position is set implicitly to the end of
 *		   the output string. If it is 1 coming in, that means
 *		   we only want the cursor position to be set if it is set
 *		   explicitly. If it is 2 coming in (or if
 *                 redraft_pos is NULL) then we don't set it at all.
 *		   If the cursor position gets set explicitly by a
 *		   _CURSORPOS_ token in the file then this is set to 2
 *		   on return. If the cursor position is set implicitly to
 *		   the end of the included file, then this is set to 1
 *                 on return.
 *
 * Returns   pointer to alloced result
 */
char *
detoken_guts(char *src, int for_what, ENVELOPE *env, ACTION_S *role,
	     REDRAFT_POS_S **redraft_pos, int last_pass, int *impl)
{
#define MAXSUB 500
    char          *p, *q = NULL, *dst = NULL;
    char           subbuf[MAXSUB+1], *repl;
    INDEX_PARSE_T *pt;
    long           l, cnt = 0L;
    int            sizing_pass = 1, suppress_tokens = 0;

    if(!src)
      return(NULL);

top:

    /*
     * The tokens we look for begin with _. The only escaping mechanism
     * is a backslash in front of a token. This will give you the literal
     * token. So \_DATE_ is a literal _DATE_.
     * Tokens like _word_ are replaced with the appropriate text if
     * word is recognized. If _word_ is followed immediately by a left paren
     * it is an if-else thingie. _word_(match_this,if_text,else_text) means to
     * replace that with either the if_text or else_text depending on whether
     * what _word_ (without the paren) would produce matches match_this or not.
     */
    p = src;
    while(*p){
	switch(*p){
	  case '_':		/* possible start of token */
	    if(!suppress_tokens &&
	       (pt = itoktype(p+1, for_what | DELIM_USCORE)) != NULL){
		char *free_this = NULL;

		p += (strlen(pt->name) + 2);	/* skip over token */

		repl = subbuf;
		subbuf[0] = '\0';

		if(pt->ctype == iCursorPos){
		    if(!last_pass){	/* put it back */
			subbuf[0] = '_';
			strncpy(subbuf+1, pt->name, sizeof(subbuf)-2);
			subbuf[sizeof(subbuf)-1] = '\0';
			strncat(subbuf, "_", sizeof(subbuf)-strlen(subbuf)-1);
			subbuf[sizeof(subbuf)-1] = '\0';
		    }

		    if(!sizing_pass){
			if(q-dst < cnt+1)
			  *q = '\0';

			l = strlen(dst);
			if(redraft_pos && impl && *impl != 2){
			    if(!*redraft_pos){
				*redraft_pos =
				 (REDRAFT_POS_S *)fs_get(sizeof(**redraft_pos));
				memset((void *)*redraft_pos, 0,
				       sizeof(**redraft_pos));
				(*redraft_pos)->hdrname = cpystr(":");
			    }

			    (*redraft_pos)->offset  = l;
			    *impl = 2;	/* set explicitly */
			}
		    }
		}
		else if(pt->what_for & FOR_REPLY_INTRO)
		  repl = get_reply_data(env, role, pt->ctype,
					subbuf, sizeof(subbuf)-1);

		if(*p == LPAREN){		/* if-else construct */
		    char *skip_ahead;

		    repl = free_this = handle_if_token(repl, p, for_what,
						       env, role,
						       &skip_ahead);
		    p = skip_ahead;
		}

		if(repl && repl[0]){
		    if(sizing_pass)
		      cnt += (long)strlen(repl);
		    else{
			strncpy(q, repl, cnt-(q-dst));
			dst[cnt] = '\0';
			q += strlen(repl);
		    }
		}

		if(free_this)
		  fs_give((void **)&free_this);
	    }
	    else{	/* unrecognized token, treat it just like text */
		suppress_tokens = 0;
		if(sizing_pass)
		  cnt++;
		else if(q-dst < cnt+1)
		  *q++ = *p;

		p++;
	    }

	    break;

	  case BSLASH:
	    /*
	     * If a real token follows the backslash, then the backslash
	     * is here to escape the token. Otherwise, it's just a
	     * regular character.
	     */
	    if(*(p+1) == '_' &&
	       ((pt = itoktype(p+2, for_what | DELIM_USCORE)) != NULL)){
		/*
		 * Backslash is escape for literal token.
		 * If we're on the last pass we want to eliminate the
		 * backslash, otherwise we keep it.
		 * In either case, suppress_tokens will cause the token
		 * lookup to be skipped above so that the token will
		 * be treated as literal text.
		 */
		suppress_tokens++;
		if(last_pass){
		    p++;
		    break;
		}
		/* else, fall through and keep backslash */
	    }
	    /* this is a literal backslash, fall through */
	
	  default:
	    if(sizing_pass)
	      cnt++;
	    else if(q-dst < cnt+1)
	      *q++ = *p;	/* copy the character */

	    p++;
	    break;
	}
    }

    if(!sizing_pass && q-dst < cnt+1)
      *q = '\0';

    if(sizing_pass){
	sizing_pass = 0;
	/*
	 * Now we're done figuring out how big the answer will be. We
	 * allocate space for it and go back through filling it in.
	 */
	cnt = MAX(cnt, 0L);
	q = dst = (char *)fs_get((cnt + 1) * sizeof(char));
	goto top;
    }

    /*
     * Set redraft_pos to character following the template, unless
     * it has already been set.
     */
    if(dst && impl && *impl == 0 && redraft_pos && !*redraft_pos){
	*redraft_pos = (REDRAFT_POS_S *)fs_get(sizeof(**redraft_pos));
	memset((void *)*redraft_pos, 0, sizeof(**redraft_pos));
	(*redraft_pos)->offset  = strlen(dst);
	(*redraft_pos)->hdrname = cpystr(":");
	*impl = 1;
    }

    if(dst && cnt >= 0)
      dst[cnt] = '\0';

    return(dst);
}


/*
 * Do the if-else part of the detokenization for one case of if-else.
 * The input src should like (match_this, if_matched, else)...
 * 
 * Args expands_to -- This is what the token to the left of the paren
 *                    expanded to, and this is the thing we're going to
 *                    compare with the match_this part.
 *             src -- The source string beginning with the left paren.
 *        for_what -- 
 *             env -- 
 *      skip_ahead -- Tells caller how long the (...) part was so caller can
 *                    skip over that part of the source.
 *
 * Returns -- an allocated string which is the answer, or NULL if nothing.
 */
char *
handle_if_token(char *expands_to, char *src, int for_what, ENVELOPE *env,
		ACTION_S *role, char **skip_ahead)
{
    char *ret = NULL;
    char *skip_to;
    char *match_this, *if_matched, *else_part;

    if(skip_ahead)
      *skip_ahead = src;

    if(!src || *src != LPAREN){
	dprint((1,"botch calling handle_if_token, missing paren\n"));
	return(ret);
    }

    if(!*++src){
	q_status_message(SM_ORDER, 3, 3,
	    "Unexpected end of token string in Reply-LeadIn, Sig, or template");
	return(ret);
    }

    match_this = get_token_arg(src, &skip_to);
    
    /*
     * If the match_this argument is a token, detokenize it first.
     */
    if(match_this && *match_this == '_'){
	char *exp_match_this;

	exp_match_this = detoken_src(match_this, for_what, env,
				     role, NULL, NULL);
	fs_give((void **)&match_this);
	match_this = exp_match_this;
    }

    if(!match_this)
      match_this = cpystr("");

    if(!expands_to)
      expands_to = "";

    src = skip_to;
    while(src && *src && (isspace((unsigned char)*src) || *src == ','))
      src++;

    if_matched = get_token_arg(src, &skip_to);
    src = skip_to;
    while(src && *src && (isspace((unsigned char)*src) || *src == ','))
      src++;

    else_part = get_token_arg(src, &skip_to);
    src = skip_to;
    while(src && *src && *src != RPAREN)
      src++;

    if(src && *src == RPAREN)
      src++;

    if(skip_ahead)
      *skip_ahead = src;

    if(!strcmp(match_this, expands_to)){
	ret = if_matched;
	if(else_part)
	  fs_give((void **)&else_part);
    }
    else{
	ret = else_part;
	if(if_matched)
	  fs_give((void **)&if_matched);
    }

    fs_give((void **)&match_this);

    return(ret);
}


char *
get_token_arg(char *src, char **skip_to)
{
    int   quotes = 0, done = 0;
    char *ret = NULL, *p;

    while(*src && isspace((unsigned char)*src))	/* skip space before string */
      src++;

    if(*src == RPAREN){
	if(skip_to)
	  *skip_to = src;

	return(ret);
    }

    p = ret = (char *)fs_get((strlen(src) + 1) * sizeof(char));
    while(!done){
	switch(*src){
	  case QUOTE:
	    if(++quotes == 2)
	      done++;

	    src++;
	    break;

	  case BSLASH:	/* don't count \" as a quote, just copy */
	    if(*(src+1) == BSLASH || *(src+1) == QUOTE){
		src++;  /* skip backslash */
		*p++ = *src++;
	    }
	    else
	      src++;

	    break;
	
	  case SPACE:
	  case TAB:
	  case RPAREN:
	  case COMMA:
	    if(quotes)
	      *p++ = *src++;
	    else
	      done++;
	    
	    break;

	  case '\0':
	    done++;
	    break;

	  default:
	    *p++ = *src++;
	    break;
	}
    }

    *p = '\0';
    if(skip_to)
      *skip_to = src;
    
    return(ret);
}
