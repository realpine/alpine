/*
 * $Id: colorconf.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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

#ifndef PINE_COLORCONF_INCLUDED
#define PINE_COLORCONF_INCLUDED


#include "conftype.h"
#include "../pith/state.h"
#include "../pith/conf.h"


#define HEADER_WORD "Header "
#define KW_COLORS_HDR "KEYWORD COLORS"
#define ADDHEADER_COMMENT N_("[ Use the AddHeader command to add colored headers in MESSAGE TEXT screen ]")
#define EQ_COL 37

#define SPACE_BETWEEN_DOUBLEVARS 3
#define SAMPLE_LEADER "---------------------------"
#define SAMP1 "[Sample ]"
#define SAMP2 "[Default]"
#define SAMPEXC "[Exception]"
#define SBS 1	/* space between samples */
#define COLOR_BLOB "<    >"
#define COLOR_BLOB_TRAN "<TRAN>"
#define COLOR_BLOB_NORM "<NORM>"
#define COLOR_BLOB_NONE "<NONE>"
#define COLOR_BLOB_LEN 6
#define COLOR_INDENT 3
#define COLORNOSET "  [ Colors below may not be set until color is turned on above ]"


/*
 * The CONF_S's varmem field serves dual purposes.  The low two bytes
 * are reserved for the pre-defined color index (though only 8 are 
 * defined for the nonce, and the high order bits are for the index
 * of the particular SPEC_COLOR_S this CONF_S is associated with.
 * Capiche?
 */
#define	CFC_ICOLOR(V)		((V)->varmem & 0xffff)
#define	CFC_ICUST(V)		((V)->varmem >> 16)
#define	CFC_SET_COLOR(I, C)	(((I) << 16) | (C))
#define	CFC_ICUST_INC(V)	CFC_SET_COLOR(CFC_ICUST(V) + 1, CFC_ICOLOR(V))
#define	CFC_ICUST_DEC(V)	CFC_SET_COLOR(CFC_ICUST(V) - 1, CFC_ICOLOR(V))

#define	CFC_ICOLOR_NORM		(1000)
#define	CFC_ICOLOR_NONE		(1001)


extern int treat_color_vars_as_text;


/* exported protoypes */
void     color_config_screen(struct pine *, int);
int	 color_setting_tool(struct pine *, int, CONF_S **, unsigned);
char    *sampleexc_text(struct pine *, struct variable *);
int	 color_related_var(struct pine *, struct variable *);
int	 color_holding_var(struct pine *, struct variable *);
char	*sample_text(struct pine *, struct variable *);
char	*color_parenthetical(struct variable *);
void	 add_color_setting_disp(struct pine *, CONF_S **, struct variable *, CONF_S *,
			        struct key_menu *, struct key_menu *, HelpType,
			        int, int, char *, char *, int);


#endif /* PINE_COLORCONF_INCLUDED */
