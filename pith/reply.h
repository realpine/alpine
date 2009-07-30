/*
 * $Id: reply.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
 *
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

#ifndef PITH_REPLY_INCLUDED
#define PITH_REPLY_INCLUDED


#define ALLOWED_SUBTYPE(st) (!strucmp((st), "plain")		\
			     || !strucmp((st), "html")		\
			     || !strucmp((st), "enriched")	\
			     || !strucmp((st), "richtext"))

/* flags for get_body_part_text */
#define GBPT_NONE	0x00
#define GBPT_PEEK	0x01
#define GBPT_NOINTR	0x02
#define GBPT_DELQUOTES	0x04
#define	GBPT_HTML_OK	0x08


/* flags for reply_cp_addr */
#define RCA_NONE	0x00
#define RCA_NOT_US	0x01	/* copy addrs that aren't us    */
#define RCA_ONLY_US	0x02	/* copy only addrs that are us  */
#define RCA_ALL		0x04	/* copy all addrs, including us */


#include "../pith/repltype.h"
#include "../pith/filttype.h"
#include "../pith/indxtype.h"
#include "../pith/pattern.h"
#include "../pith/state.h"
#include "../pith/addrstring.h"


/* exported protoypes */
int	    reply_harvest(struct pine *, long, char *, ENVELOPE *, ADDRESS **,
			  ADDRESS **, ADDRESS **, ADDRESS **,int *);
ADDRESS    *reply_cp_addr(struct pine *, long, char *, char *,
			  ADDRESS *, ADDRESS *, ADDRESS *, int);
void        reply_append_addr(ADDRESS **, ADDRESS *);
ACTION_S   *set_role_from_msg(struct pine *, long, long, char *);
void	    reply_seed(struct pine *, ENVELOPE *, ENVELOPE *, ADDRESS *, ADDRESS *,
		       ADDRESS *, ADDRESS *, char **, int, char **);
int	    addr_lists_same(ADDRESS *, ADDRESS *);
int	    addr_in_env(ADDRESS *, ENVELOPE *);
void	    reply_fish_personal(ENVELOPE *, ENVELOPE *);
char	   *reply_build_refs(ENVELOPE *);
ADDRESS    *reply_resent(struct pine *, long, char *);
char	   *reply_subject(char *, char *, size_t);
char       *reply_quote_initials(char *);
char	   *reply_quote_str(ENVELOPE *);
int         reply_quote_str_contains_tokens(void);
BODY	   *reply_body(MAILSTREAM *, ENVELOPE *, BODY *, long, char *, void *,
		       char *, int, ACTION_S *, int, REDRAFT_POS_S **);
int	    reply_body_text(BODY *, BODY **);
char	   *reply_signature(ACTION_S *, ENVELOPE *, REDRAFT_POS_S **, int *);
void        get_addr_data(ENVELOPE *, IndexColType, char *, size_t);
void        get_news_data(ENVELOPE *, IndexColType, char *, size_t);
char       *get_reply_data(ENVELOPE *, ACTION_S *, IndexColType, char *, size_t);
void	    reply_delimiter(ENVELOPE *, ACTION_S *, gf_io_t);
void	    free_redraft_pos(REDRAFT_POS_S **);
int	    forward_mime_msg(MAILSTREAM *, long, char *, ENVELOPE *, PART **, void *);
void	    forward_delimiter(gf_io_t);
void	    reply_forward_header(MAILSTREAM *, long, char *, ENVELOPE *, gf_io_t, char *);
char	   *forward_subject(ENVELOPE *, int);
BODY	   *forward_body(MAILSTREAM *, ENVELOPE *, BODY *, long, char *, void *, int);
char	   *bounce_msg_body(MAILSTREAM *, long int, char *, char **, char *, ENVELOPE **, BODY **, int *);
int	    get_body_part_text(MAILSTREAM *, BODY *, long, char *, long,
			       gf_io_t, char *, char **, unsigned);
int	    quote_fold(long, char *, LT_INS_S **, void *);
int	    twsp_strip(long, char *, LT_INS_S **, void *);
int         post_quote_space(long, char *, LT_INS_S **, void *);
int	    sigdash_strip(long, char *, LT_INS_S **, void *);
char	   *body_partno(MAILSTREAM *, long, BODY *);
char	   *partno(BODY *, BODY *);
int	    fetch_contents(MAILSTREAM *, long, char *, BODY *);
BODY	   *copy_body(BODY *, BODY *);
PARAMETER  *copy_parameters(PARAMETER *);
ENVELOPE   *copy_envelope(ENVELOPE *);
char	   *reply_in_reply_to(ENVELOPE *);
char	   *generate_message_id(void);
char	   *generate_user_agent(void);
char       *rot13(char *);
ADDRESS    *first_addr(ADDRESS *);
char       *get_signature_lit(char *, int, int, int, int);
int         sigdashes_are_present(char *);
char	   *signature_path(char *, char *, size_t);
char       *simple_read_remote_file(char *, char *);
BODY	   *forward_multi_alt(MAILSTREAM *, ENVELOPE *, BODY *, long, char *, void *, gf_io_t, int);


#endif /* PITH_REPLY_INCLUDED */
