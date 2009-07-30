#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: icache.c 874 2007-12-15 02:51:06Z hubert@u.washington.edu $";
#endif

/*
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

#include "../pith/headers.h"
#include "../pith/icache.h"
#include "../pith/mailindx.h"
#include "../pith/flag.h"
#include "../pith/msgno.h"
#include "../pith/status.h"
#include "../pith/pineelt.h"

/*
 * Internal prototypes
 */


/*
 *           * * *  Index entry cache manager  * * *
 */


/*
 * Erase a particular entry in the cache.
 */
void
clear_index_cache_ent(MAILSTREAM *stream, long int msgno, unsigned int flags)
{
    long          rawno = -1L;
    PINELT_S    **peltp;
    MESSAGECACHE *mc;

    if(stream){
	if(flags && IC_USE_RAW_MSGNO)
	  rawno = msgno;
	else
	  rawno = mn_m2raw(sp_msgmap(stream), msgno);

	if(rawno > 0L && rawno <= stream->nmsgs){
	    mc = mail_elt(stream, rawno);
	    if(mc && mc->sparep){
		peltp = (PINELT_S **) &mc->sparep;
		if((*peltp)->ice){
		    /*
		     * This is intended to be a lightweight reset of
		     * just the widths and print_format strings. For example,
		     * the width of the screen changed and nothing else.
		     * We simply unset the widths_done bit and it
		     * is up to the drawer to free and recalculate the
		     * print_format strings and to reset the widths.
		     *
		     * The else case is a clear of the entire cache entry
		     * leaving behind only the empty structure.
		     */
		    if(flags & IC_CLEAR_WIDTHS_DONE){
			(*peltp)->ice->widths_done = 0;

			/* also zero out hash value */
			(*peltp)->ice->id = 0;

			if((*peltp)->ice->tice){
			    (*peltp)->ice->tice->widths_done = 0;

			    /* also zero out hash value */
			    (*peltp)->ice->tice->id = 0;
			}
		    }
		    else
		      clear_ice(&(*peltp)->ice);
		}
	    }
	}
    }
}


void
clear_index_cache(MAILSTREAM *stream, unsigned int flags)
{
    long rawno;

    if(stream){
	set_need_format_setup(stream);
	for(rawno = 1L; rawno <= stream->nmsgs; rawno++)
	  clear_index_cache_ent(stream, rawno, flags | IC_USE_RAW_MSGNO);
    }
}


void
clear_index_cache_for_thread(MAILSTREAM *stream, PINETHRD_S *thrd, MSGNO_S *msgmap)
{
    unsigned long msgno;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return;

    msgno = mn_raw2m(msgmap, thrd->rawno);

    clear_index_cache_ent(stream, msgno, 0);

    if(thrd->next)
      clear_index_cache_for_thread(stream, fetch_thread(stream, thrd->next),
				   msgmap);

    if(thrd->branch)
      clear_index_cache_for_thread(stream, fetch_thread(stream, thrd->branch),
				   msgmap);
}


void
clear_icache_flags(MAILSTREAM *stream)
{
    sp_set_icache_flags(stream, 0);
}


void
set_need_format_setup(MAILSTREAM *stream)
{
    sp_set_icache_flags(stream, sp_icache_flags(stream) | SP_NEED_FORMAT_SETUP);
}


int
need_format_setup(MAILSTREAM *stream)
{
    return(sp_icache_flags(stream) & SP_NEED_FORMAT_SETUP);
}


void
set_format_includes_msgno(MAILSTREAM *stream)
{
    sp_set_icache_flags(stream, sp_icache_flags(stream) | SP_FORMAT_INCLUDES_MSGNO);
}


int
format_includes_msgno(MAILSTREAM *stream)
{
    return(sp_icache_flags(stream) & SP_FORMAT_INCLUDES_MSGNO);
}


void
set_format_includes_smartdate(MAILSTREAM *stream)
{
    sp_set_icache_flags(stream, sp_icache_flags(stream) | SP_FORMAT_INCLUDES_SMARTDATE);
}


int
format_includes_smartdate(MAILSTREAM *stream)
{
    return(sp_icache_flags(stream) & SP_FORMAT_INCLUDES_SMARTDATE);
}


/*
 * Almost a free_ice, but we leave the memory there for the ICE_S.
 */
void
clear_ice(ICE_S **ice)
{
    if(ice && *ice){
	free_ifield(&(*ice)->ifield);

	if((*ice)->linecolor)
	  free_color_pair(&(*ice)->linecolor);

	if((*ice)->tice)
	  clear_ice(&(*ice)->tice);

	/* do these one at a time so we don't clear tice */
	(*ice)->color_lookup_done = 0;
	(*ice)->to_us = 0;
	(*ice)->cc_us = 0;
	(*ice)->plus = 0;
	(*ice)->id = 0;
    }
}


void
free_ice(ICE_S **ice)
{
    if(ice && *ice){

	if((*ice)->tice)
	  free_ice(&(*ice)->tice);

	clear_ice(ice);

	fs_give((void **) ice);
    }
}


void
free_ifield(IFIELD_S **ifld)
{
    if(ifld && *ifld){
	free_ifield(&(*ifld)->next);
	free_ielem(&(*ifld)->ielem);
	fs_give((void **) ifld);
    }
}


void
free_ielem(IELEM_S **il)
{
    if(il && *il){
	free_ielem(&(*il)->next);
	if((*il)->freeprintf && (*il)->print_format)
	  fs_give((void **) &(*il)->print_format);

	if((*il)->freecolor && (*il)->color)
	  free_color_pair(&(*il)->color);

	if((*il)->freedata && (*il)->data)
	  fs_give((void **) &(*il)->data);
	
	fs_give((void **) il);
    }
}


/*
 * Returns the index cache entry associated with this message.
 * If it doesn't already exist it is instantiated.
 */
ICE_S *
fetch_ice(MAILSTREAM *stream, long unsigned int rawno)
{
    PINELT_S    **peltp;
    MESSAGECACHE *mc;

    if(!stream || rawno < 1L || rawno > stream->nmsgs)
      return NULL;

    if(!(mc = mail_elt(stream, rawno)))
      return NULL;

    /*
     * any private elt data yet?
     */
    if((*(peltp = (PINELT_S **) &mc->sparep) == NULL)){
	*peltp = (PINELT_S *) fs_get(sizeof(PINELT_S));
	memset(*peltp, 0, sizeof(PINELT_S));
    }

    if((*peltp)->ice == NULL)
      (*peltp)->ice = new_ice();

    if(need_format_setup(stream) && setup_header_widths)
      (*setup_header_widths)(stream);

    return((*peltp)->ice);
}


ICE_S **
fetch_ice_ptr(MAILSTREAM *stream, long unsigned int rawno)
{
    PINELT_S    **peltp;
    MESSAGECACHE *mc;

    if(!stream || rawno < 1L || rawno > stream->nmsgs)
      return NULL;

    if(!(mc = mail_elt(stream, rawno)))
      return NULL;

    /*
     * any private elt data yet?
     */
    if((*(peltp = (PINELT_S **) &mc->sparep) == NULL)){
	*peltp = (PINELT_S *) fs_get(sizeof(PINELT_S));
	memset(*peltp, 0, sizeof(PINELT_S));
    }

    return(&(*peltp)->ice);
}


ICE_S *
copy_ice(ICE_S *src)
{
    ICE_S *head = NULL;

    if(src){
	head                    = new_ice();

	head->color_lookup_done = src->color_lookup_done;
	head->widths_done       = src->widths_done;
	head->to_us             = src->to_us;
	head->cc_us             = src->cc_us;
	head->plus              = src->plus;
	head->id                = src->id;

	if(src->linecolor)
	  head->linecolor = new_color_pair(src->linecolor->fg, src->linecolor->bg);

	if(src->ifield)
	  head->ifield = copy_ifield(src->ifield);

	if(src->tice)
	  head->tice = copy_ice(src->tice);
    }

    return(head);
}


IFIELD_S *
copy_ifield(IFIELD_S *src)
{
    IFIELD_S *head = NULL;

    if(src){
	head          = new_ifield(NULL);

	if(src->next)
	  head->next = copy_ifield(src->next);

	head->ctype   = src->ctype;
	head->width   = src->width;
	head->leftadj = src->leftadj;

	if(src->ielem)
	  head->ielem = copy_ielem(src->ielem);
    }

    return(head);
}


IELEM_S *
copy_ielem(IELEM_S *src)
{
    IELEM_S *head = NULL;

    if(src){
	head          = new_ielem(NULL);

	if(src->next)
	  head->next = copy_ielem(src->next);

	head->type = src->type;
	head->wid  = src->wid;

	if(src->color){
	    head->color     = new_color_pair(src->color->fg, src->color->bg);
	    head->freecolor = 1;
	}

	if(src->data){
	    head->data     = cpystr(src->data);
	    head->datalen  = strlen(head->data);
	    head->freedata = 1;
	}

	if(src->print_format){
	    head->print_format = cpystr(src->print_format);
	    head->freeprintf   = strlen(head->print_format) + 1;
	}
    }

    return(head);
}


ICE_S *
new_ice(void)
{
    ICE_S *ice;

    ice = (ICE_S *) fs_get(sizeof(ICE_S));
    memset(ice, 0, sizeof(ICE_S));
    return(ice);
}


/*
 * Create new IFIELD_S, zero it out, and insert it at end.
 */
IFIELD_S *
new_ifield(IFIELD_S **ifieldp)
{
    IFIELD_S *ifield, *ip;

    ifield = (IFIELD_S *) fs_get(sizeof(*ifield));
    memset(ifield, 0, sizeof(*ifield));

    if(ifieldp){
	ip = *ifieldp;
	if(ip){
	    for(ip = (*ifieldp); ip && ip->next; ip = ip->next)
	      ;

	    ip->next = ifield;
	}
	else
	  *ifieldp = ifield;
    }

    return(ifield);
}


/*
 * Create new IELEM_S, zero it out, and insert it at end.
 */
IELEM_S *
new_ielem(IELEM_S **ielemp)
{
    IELEM_S *ielem, *ip;

    ielem = (IELEM_S *) fs_get(sizeof(*ielem));
    memset(ielem, 0, sizeof(*ielem));

    if(ielemp){
	ip = *ielemp;
	if(ip){
	    for(ip = (*ielemp); ip && ip->next; ip = ip->next)
	      ;

	    ip->next = ielem;
	}
	else
	  *ielemp = ielem;
    }

    return(ielem);
}
