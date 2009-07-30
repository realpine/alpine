#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: color.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
#endif

/* ========================================================================
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

#include <system.h>
#include <general.h>

#include "../../../pith/osdep/color.h"
#include "../../../pith/osdep/collate.h"


static COLOR_PAIR *the_rev_color;
static char   *_nfcolor, *_nbcolor, *_rfcolor, *_rbcolor;
static char   *_last_fg_color, *_last_bg_color;
static int     _force_fg_color_change, _force_bg_color_change;


/* * * * * * * PITH-REQUIRED COLOR ROUTINES * * * * * * */

/* internal prototypes */
char	*alpine_color_name(char *);
int	 alpine_valid_rgb(char *s);

int
pico_usingcolor(void)
{
    return(TRUE);
}


int
pico_hascolor(void)
{
    return(TRUE);
}


/*
 * Web Alpine Color Table
 */
static struct color_table {
    int		 number;
    char	*rgb;
    struct {
	char	*s,
		 l;
    } name;
} webcoltab[] = {
    {COL_BLACK,		"  0,  0,  0",	{"black",	5}},
    {COL_RED,		"255,  0,  0",	{"red",		3}},
    {COL_GREEN,		"  0,255,  0",	{"green",	5}},
    {COL_YELLOW,	"255,255,  0",	{"yellow",	6}},
    {COL_BLUE,		"  0,  0,255",	{"blue",	4}},
    {COL_MAGENTA,	"255,  0,255",	{"magenta",	7}},
    {COL_CYAN,		"  0,255,255",	{"cyan",	4}},
    {COL_WHITE,		"255,255,255",	{"white",	5}},
    {8,			"192,192,192",	{"color008",	8}},	/* light gray */
    {9,			"128,128,128",	{"color009",	8}},	/* gray */
    {10,		" 64, 64, 64",	{"color010",	8}},	/* dark gray */
    {COL_YELLOW,	"255,255,  0",	{"color011",	8}},
    {COL_BLUE,		"  0,  0,255",	{"color012",	8}},
    {COL_MAGENTA,	"255,  0,255",	{"color013",	8}},
    {COL_CYAN,		"  0,255,255",	{"color014",	8}},
    {COL_WHITE,		"255,255,255",	{"color015",	8}},
    {8,			"192,192,192",	{"colorlgr",	8}},	/* light gray */
    {9,			"128,128,128",	{"colormgr",	8}},	/* gray */
    {10,		" 64, 64, 64",	{"colordgr",	8}},	/* dark gray */
    {16,		"000,000,000",	{"color016",	8}},
    {17,		"000,000,095",	{"color017",	8}},
    {18,		"000,000,135",	{"color018",	8}},
    {19,		"000,000,175",	{"color019",	8}},
    {20,		"000,000,215",	{"color020",	8}},
    {21,		"000,000,255",	{"color021",	8}},
    {22,		"000,095,000",	{"color022",	8}},
    {23,		"000,095,095",	{"color023",	8}},
    {24,		"000,095,135",	{"color024",	8}},
    {25,		"000,095,175",	{"color025",	8}},
    {26,		"000,095,215",	{"color026",	8}},
    {27,		"000,095,255",	{"color027",	8}},
    {28,		"000,135,000",	{"color028",	8}},
    {29,		"000,135,095",	{"color029",	8}},
    {30,		"000,135,135",	{"color030",	8}},
    {31,		"000,135,175",	{"color031",	8}},
    {32,		"000,135,215",	{"color032",	8}},
    {33,		"000,135,255",	{"color033",	8}},
    {34,		"000,175,000",	{"color034",	8}},
    {35,		"000,175,095",	{"color035",	8}},
    {36,		"000,175,135",	{"color036",	8}},
    {37,		"000,175,175",	{"color037",	8}},
    {38,		"000,175,215",	{"color038",	8}},
    {39,		"000,175,255",	{"color039",	8}},
    {40,		"000,215,000",	{"color040",	8}},
    {41,		"000,215,095",	{"color041",	8}},
    {42,		"000,215,135",	{"color042",	8}},
    {43,		"000,215,175",	{"color043",	8}},
    {44,		"000,215,215",	{"color044",	8}},
    {45,		"000,215,255",	{"color045",	8}},
    {46,		"000,255,000",	{"color046",	8}},
    {47,		"000,255,095",	{"color047",	8}},
    {48,		"000,255,135",	{"color048",	8}},
    {49,		"000,255,175",	{"color049",	8}},
    {50,		"000,255,215",	{"color050",	8}},
    {51,		"000,255,255",	{"color051",	8}},
    {52,		"095,000,000",	{"color052",	8}},
    {53,		"095,000,095",	{"color053",	8}},
    {54,		"095,000,135",	{"color054",	8}},
    {55,		"095,000,175",	{"color055",	8}},
    {56,		"095,000,215",	{"color056",	8}},
    {57,		"095,000,255",	{"color057",	8}},
    {58,		"095,095,000",	{"color058",	8}},
    {59,		"095,095,095",	{"color059",	8}},
    {60,		"095,095,135",	{"color060",	8}},
    {61,		"095,095,175",	{"color061",	8}},
    {62,		"095,095,215",	{"color062",	8}},
    {63,		"095,095,255",	{"color063",	8}},
    {64,		"095,135,000",	{"color064",	8}},
    {65,		"095,135,095",	{"color065",	8}},
    {66,		"095,135,135",	{"color066",	8}},
    {67,		"095,135,175",	{"color067",	8}},
    {68,		"095,135,215",	{"color068",	8}},
    {69,		"095,135,255",	{"color069",	8}},
    {70,		"095,175,000",	{"color070",	8}},
    {71,		"095,175,095",	{"color071",	8}},
    {72,		"095,175,135",	{"color072",	8}},
    {73,		"095,175,175",	{"color073",	8}},
    {74,		"095,175,215",	{"color074",	8}},
    {75,		"095,175,255",	{"color075",	8}},
    {76,		"095,215,000",	{"color076",	8}},
    {77,		"095,215,095",	{"color077",	8}},
    {78,		"095,215,135",	{"color078",	8}},
    {79,		"095,215,175",	{"color079",	8}},
    {80,		"095,215,215",	{"color080",	8}},
    {81,		"095,215,255",	{"color081",	8}},
    {82,		"095,255,000",	{"color082",	8}},
    {83,		"095,255,095",	{"color083",	8}},
    {84,		"095,255,135",	{"color084",	8}},
    {85,		"095,255,175",	{"color085",	8}},
    {86,		"095,255,215",	{"color086",	8}},
    {87,		"095,255,255",	{"color087",	8}},
    {88,		"135,000,000",	{"color088",	8}},
    {89,		"135,000,095",	{"color089",	8}},
    {90,		"135,000,135",	{"color090",	8}},
    {91,		"135,000,175",	{"color091",	8}},
    {92,		"135,000,215",	{"color092",	8}},
    {93,		"135,000,255",	{"color093",	8}},
    {94,		"135,095,000",	{"color094",	8}},
    {95,		"135,095,095",	{"color095",	8}},
    {96,		"135,095,135",	{"color096",	8}},
    {97,		"135,095,175",	{"color097",	8}},
    {98,		"135,095,215",	{"color098",	8}},
    {99,		"135,095,255",	{"color099",	8}},
    {100,		"135,135,000",	{"color100",	8}},
    {101,		"135,135,095",	{"color101",	8}},
    {102,		"135,135,135",	{"color102",	8}},
    {103,		"135,135,175",	{"color103",	8}},
    {104,		"135,135,215",	{"color104",	8}},
    {105,		"135,135,255",	{"color105",	8}},
    {106,		"135,175,000",	{"color106",	8}},
    {107,		"135,175,095",	{"color107",	8}},
    {108,		"135,175,135",	{"color108",	8}},
    {109,		"135,175,175",	{"color109",	8}},
    {110,		"135,175,215",	{"color110",	8}},
    {111,		"135,175,255",	{"color111",	8}},
    {112,		"135,215,000",	{"color112",	8}},
    {113,		"135,215,095",	{"color113",	8}},
    {114,		"135,215,135",	{"color114",	8}},
    {115,		"135,215,175",	{"color115",	8}},
    {116,		"135,215,215",	{"color116",	8}},
    {117,		"135,215,255",	{"color117",	8}},
    {118,		"135,255,000",	{"color118",	8}},
    {119,		"135,255,095",	{"color119",	8}},
    {120,		"135,255,135",	{"color120",	8}},
    {121,		"135,255,175",	{"color121",	8}},
    {122,		"135,255,215",	{"color122",	8}},
    {123,		"135,255,255",	{"color123",	8}},
    {124,		"175,000,000",	{"color124",	8}},
    {125,		"175,000,095",	{"color125",	8}},
    {126,		"175,000,135",	{"color126",	8}},
    {127,		"175,000,175",	{"color127",	8}},
    {128,		"175,000,215",	{"color128",	8}},
    {129,		"175,000,255",	{"color129",	8}},
    {130,		"175,095,000",	{"color130",	8}},
    {131,		"175,095,095",	{"color131",	8}},
    {132,		"175,095,135",	{"color132",	8}},
    {133,		"175,095,175",	{"color133",	8}},
    {134,		"175,095,215",	{"color134",	8}},
    {135,		"175,095,255",	{"color135",	8}},
    {136,		"175,135,000",	{"color136",	8}},
    {137,		"175,135,095",	{"color137",	8}},
    {138,		"175,135,135",	{"color138",	8}},
    {139,		"175,135,175",	{"color139",	8}},
    {140,		"175,135,215",	{"color140",	8}},
    {141,		"175,135,255",	{"color141",	8}},
    {142,		"175,175,000",	{"color142",	8}},
    {143,		"175,175,095",	{"color143",	8}},
    {144,		"175,175,135",	{"color144",	8}},
    {145,		"175,175,175",	{"color145",	8}},
    {146,		"175,175,215",	{"color146",	8}},
    {147,		"175,175,255",	{"color147",	8}},
    {148,		"175,215,000",	{"color148",	8}},
    {149,		"175,215,095",	{"color149",	8}},
    {150,		"175,215,135",	{"color150",	8}},
    {151,		"175,215,175",	{"color151",	8}},
    {152,		"175,215,215",	{"color152",	8}},
    {153,		"175,215,255",	{"color153",	8}},
    {154,		"175,255,000",	{"color154",	8}},
    {155,		"175,255,095",	{"color155",	8}},
    {156,		"175,255,135",	{"color156",	8}},
    {157,		"175,255,175",	{"color157",	8}},
    {158,		"175,255,215",	{"color158",	8}},
    {159,		"175,255,255",	{"color159",	8}},
    {160,		"215,000,000",	{"color160",	8}},
    {161,		"215,000,095",	{"color161",	8}},
    {162,		"215,000,135",	{"color162",	8}},
    {163,		"215,000,175",	{"color163",	8}},
    {164,		"215,000,215",	{"color164",	8}},
    {165,		"215,000,255",	{"color165",	8}},
    {166,		"215,095,000",	{"color166",	8}},
    {167,		"215,095,095",	{"color167",	8}},
    {168,		"215,095,135",	{"color168",	8}},
    {169,		"215,095,175",	{"color169",	8}},
    {170,		"215,095,215",	{"color170",	8}},
    {171,		"215,095,255",	{"color171",	8}},
    {172,		"215,135,000",	{"color172",	8}},
    {173,		"215,135,095",	{"color173",	8}},
    {174,		"215,135,135",	{"color174",	8}},
    {175,		"215,135,175",	{"color175",	8}},
    {176,		"215,135,215",	{"color176",	8}},
    {177,		"215,135,255",	{"color177",	8}},
    {178,		"215,175,000",	{"color178",	8}},
    {179,		"215,175,095",	{"color179",	8}},
    {180,		"215,175,135",	{"color180",	8}},
    {181,		"215,175,175",	{"color181",	8}},
    {182,		"215,175,215",	{"color182",	8}},
    {183,		"215,175,255",	{"color183",	8}},
    {184,		"215,215,000",	{"color184",	8}},
    {185,		"215,215,095",	{"color185",	8}},
    {186,		"215,215,135",	{"color186",	8}},
    {187,		"215,215,175",	{"color187",	8}},
    {188,		"215,215,215",	{"color188",	8}},
    {189,		"215,215,255",	{"color189",	8}},
    {190,		"215,255,000",	{"color190",	8}},
    {191,		"215,255,095",	{"color191",	8}},
    {192,		"215,255,135",	{"color192",	8}},
    {193,		"215,255,175",	{"color193",	8}},
    {194,		"215,255,215",	{"color194",	8}},
    {195,		"215,255,255",	{"color195",	8}},
    {196,		"255,000,000",	{"color196",	8}},
    {197,		"255,000,095",	{"color197",	8}},
    {198,		"255,000,135",	{"color198",	8}},
    {199,		"255,000,175",	{"color199",	8}},
    {200,		"255,000,215",	{"color200",	8}},
    {201,		"255,000,255",	{"color201",	8}},
    {202,		"255,095,000",	{"color202",	8}},
    {203,		"255,095,095",	{"color203",	8}},
    {204,		"255,095,135",	{"color204",	8}},
    {205,		"255,095,175",	{"color205",	8}},
    {206,		"255,095,215",	{"color206",	8}},
    {207,		"255,095,255",	{"color207",	8}},
    {208,		"255,135,000",	{"color208",	8}},
    {209,		"255,135,095",	{"color209",	8}},
    {210,		"255,135,135",	{"color210",	8}},
    {211,		"255,135,175",	{"color211",	8}},
    {212,		"255,135,215",	{"color212",	8}},
    {213,		"255,135,255",	{"color213",	8}},
    {214,		"255,175,000",	{"color214",	8}},
    {215,		"255,175,095",	{"color215",	8}},
    {216,		"255,175,135",	{"color216",	8}},
    {217,		"255,175,175",	{"color217",	8}},
    {218,		"255,175,215",	{"color218",	8}},
    {219,		"255,175,255",	{"color219",	8}},
    {220,		"255,215,000",	{"color220",	8}},
    {221,		"255,215,095",	{"color221",	8}},
    {222,		"255,215,135",	{"color222",	8}},
    {223,		"255,215,175",	{"color223",	8}},
    {224,		"255,215,215",	{"color224",	8}},
    {225,		"255,215,255",	{"color225",	8}},
    {226,		"255,255,000",	{"color226",	8}},
    {227,		"255,255,095",	{"color227",	8}},
    {228,		"255,255,135",	{"color228",	8}},
    {229,		"255,255,175",	{"color229",	8}},
    {230,		"255,255,215",	{"color230",	8}},
    {231,		"255,255,255",	{"color231",	8}}
};


char *
colorx(int color)
{
    int i;
    static char cbuf[12];

    for(i = 0; i < sizeof(webcoltab) / sizeof(struct color_table); i++)
      if(color == webcoltab[i].number)
	return(webcoltab[i].rgb);

    sprintf(cbuf, "color%3.3d", color);
    return(cbuf);
}


/*
 * Return a pointer to an rgb string for the input color. The output is 11
 * characters long and looks like rrr,ggg,bbb.
 *
 * Args    colorName -- The color to convert to ascii rgb.
 *
 * Returns  Pointer to a static buffer containing the rgb string.
 */
char *
color_to_asciirgb(char *colorName)
{
    int		i;
    static char c_to_a_buf[3][RGBLEN+1];
    static int  whichbuf = 0;

    whichbuf = (whichbuf + 1) % 3;

    for(i = 0; i < sizeof(webcoltab) / sizeof(struct color_table); i++)
      if(!strucmp(webcoltab[i].name.s, colorName))
	return(webcoltab[i].rgb);
	  
    /*
     * If we didn't find the color it could be that it is the
     * normal color (MATCH_NORM_COLOR) or the none color
     * (MATCH_NONE_COLOR). If that is the case, this strncpy thing
     * will work out correctly because those two strings are
     * RGBLEN long. Otherwise we're in a bit of trouble. This
     * most likely means that the user is using the same pinerc on
     * two terminals, one with more colors than the other. We didn't
     * find a match because this color isn't present on this terminal.
     * Since the return value of this function is assumed to be
     * RGBLEN long, we'd better make it that long.
     * It still won't work correctly because colors will be screwed up,
     * but at least the embedded colors in filter.c will get properly
     * sucked up when they're encountered.
     */
    strncpy(c_to_a_buf[whichbuf], "xxxxxxxxxxx", RGBLEN);  /* RGBLEN is 11 */
    i = strlen(colorName);
    strncpy(c_to_a_buf[whichbuf], colorName, (i < RGBLEN) ? i : RGBLEN);
    c_to_a_buf[whichbuf][RGBLEN] = '\0';
    return(c_to_a_buf[whichbuf]);
}



int
pico_is_good_color(char *s)
{
    return(alpine_color_name(s) != NULL || alpine_valid_rgb(s));
}


int
alpine_valid_rgb(char *s)
{
    int i, j;

    /* has to be three spaces or decimal digits followed by a dot.*/

    for(i = 0; i < 3; i++){
	int n = 0;

	for(j = 0; j < 3; j++, s++) {
	    if(*s == ' '){
		if(n)
		  return(FALSE);
	    }
	    else if(isdigit((unsigned char) *s)){
		n = (n * 10) + (*s - '0');
	    }
	    else
	      return(FALSE);
	}

	if (i < 2 && *s++ != ',')
	  return(FALSE);
    }

    return (TRUE);
}



char *
alpine_color_name(char *s)
{
    if(s){
	int i;

	if(!struncmp(s, MATCH_NORM_COLOR, RGBLEN) || !struncmp(s, MATCH_NONE_COLOR, RGBLEN))
	  return(s);
	else if(*s == ' ' || isdigit(*s)){
	    /* check for rgb string instead of name */
	    for(i = 0; i < sizeof(webcoltab) / sizeof(struct color_table); i++)
	      if(!strncmp(webcoltab[i].rgb, s, RGBLEN))
		return(webcoltab[i].name.s);
	}
	else{
	    for(i = 0; i < sizeof(webcoltab) / sizeof(struct color_table); i++)
	      if(!struncmp(webcoltab[i].name.s, s, webcoltab[i].name.l))
		return(webcoltab[i].name.s);
	}
    }

    return(NULL);
}


/*
 * Sets color to (fg,bg).
 * Flags == PSC_NONE  No alternate default if fg,bg fails.
 *       == PSC_NORM  Set it to Normal color on failure.
 *       == PSC_REV   Set it to Reverse color on failure.
 *
 * If flag PSC_RET is set, returns an allocated copy of the previous
 * color pair, otherwise returns NULL.
 */
COLOR_PAIR *
pico_set_colors(char *fg, char *bg, int flags)
{
    int         uc;
    COLOR_PAIR *cp = NULL, *rev = NULL;

    if(flags & PSC_RET)
      cp = pico_get_cur_color();

    if(!((uc = pico_usingcolor())
	 && fg && bg
	 && pico_set_fg_color(fg) && pico_set_bg_color(bg))){

	if(uc && flags & PSC_NORM){
	    pico_set_normal_color();
	}
	else if(flags & PSC_REV){
	    if((rev = pico_get_rev_color()) != NULL){
		pico_set_fg_color(rev->fg);	/* these will succeed */
		pico_set_bg_color(rev->bg);
	    }
	}
    }

    return(cp);
}



void
pico_nfcolor(char *s)
{
    if(_nfcolor){
	free(_nfcolor);
	_nfcolor = NULL;
    }

    if(s){
	_nfcolor = (char *)malloc(strlen(s)+1);
	if(_nfcolor)
	  strcpy(_nfcolor, s);
    }
}


void
pico_nbcolor(char *s)
{
    if(_nbcolor){
	free(_nbcolor);
	_nbcolor = NULL;
    }

    if(s){
	_nbcolor = (char *)malloc(strlen(s)+1);
	if(_nbcolor)
	  strcpy(_nbcolor, s);
    }
}

void
pico_rfcolor(char *s)
{
    if(_rfcolor){
	free(_rfcolor);
	_rfcolor = NULL;
    }

    if(s){
	_rfcolor = (char *)malloc(strlen(s)+1);
	if(_rfcolor)
	  strcpy(_rfcolor, s);
	
	if(the_rev_color)
	  strcpy(the_rev_color->fg, _rfcolor);
    }
    else if(the_rev_color)
      free_color_pair(&the_rev_color);
}

void
pico_rbcolor(char *s)
{
    if(_rbcolor){
	free(_rbcolor);
	_rbcolor = NULL;
    }

    if(s){
	_rbcolor = (char *)malloc(strlen(s)+1);
	if(_rbcolor)
	  strcpy(_rbcolor, s);
	
	if(the_rev_color)
	  strcpy(the_rev_color->bg, _rbcolor);
    }
    else if(the_rev_color)
      free_color_pair(&the_rev_color);
}


void
pico_endcolor(void)
{
    if(_nfcolor){
	free(_nfcolor);
	_nfcolor = NULL;
    }

    if(_nbcolor){
	free(_nbcolor);
	_nbcolor = NULL;
    }

    if(_rfcolor){
	free(_rfcolor);
	_rfcolor = NULL;
    }

    if(_rbcolor){
	free(_rbcolor);
	_rbcolor = NULL;
    }

    if(the_rev_color)
      free_color_pair(&the_rev_color);
}


COLOR_PAIR *
pico_get_cur_color(void)
{
    return(new_color_pair(_last_fg_color, _last_bg_color));
}

/*
 * If inverse is a color, returns a pointer to that color.
 * If not, returns NULL.
 *
 * NOTE: Don't free this!
 */
COLOR_PAIR *
pico_get_rev_color(void)
{
    if(pico_usingcolor() && _rfcolor && _rbcolor &&
       pico_is_good_color(_rfcolor) && pico_is_good_color(_rbcolor)){
	if(!the_rev_color)
	  the_rev_color = new_color_pair(_rfcolor, _rbcolor);
	
	return(the_rev_color);
    }
    else
      return(NULL);
}


int
pico_set_fg_color(char *s)
{
    if(pico_is_good_color(s)){
	if(!struncmp(s, MATCH_NORM_COLOR, RGBLEN))
	  s = _nfcolor;
        else if(!struncmp(s, MATCH_NONE_COLOR, RGBLEN))
	  return(TRUE);

	/* already set correctly */
	if(!_force_fg_color_change
	   && _last_fg_color
	   && !strcmp(_last_fg_color, s))
	  return(TRUE);

	_force_fg_color_change = 0;
	if(_last_fg_color)
	  free(_last_fg_color);

	if((_last_fg_color = (char *) malloc(strlen(s) + 1)) != NULL)
	  strcpy(_last_fg_color, s);

	return(TRUE);
    }

    return(FALSE);
}


int
pico_set_bg_color(char *s)
{
    if(pico_is_good_color(s)){
	if(!struncmp(s, MATCH_NORM_COLOR, RGBLEN))
	  s = _nbcolor;
        else if(!struncmp(s, MATCH_NONE_COLOR, RGBLEN))
	  return(TRUE);

	/* already set correctly */
	if(!_force_bg_color_change
	   && _last_bg_color
	   && !strcmp(_last_bg_color, s))
	  return(TRUE);

	_force_bg_color_change = 0;
	if(_last_bg_color)
	  free(_last_bg_color);

	if((_last_bg_color = (char *) malloc(strlen(s) + 1)) != NULL)
	  strcpy(_last_bg_color, s);

	return(TRUE);
    }

    return(FALSE);
}


void
pico_set_normal_color(void)
{
    if(!_nfcolor || !_nbcolor ||
       !pico_set_fg_color(_nfcolor) || !pico_set_bg_color(_nbcolor)){
	(void)pico_set_fg_color(DEFAULT_NORM_FORE_RGB);
	(void)pico_set_bg_color(DEFAULT_NORM_BACK_RGB);
    }
}


char *
pico_get_last_fg_color(void)
{
    char *ret = NULL;

    if(_last_fg_color)
      if((ret = (char *)malloc(strlen(_last_fg_color)+1)) != NULL)
	strcpy(ret, _last_fg_color);

    return(ret);
}

char *
pico_get_last_bg_color(void)
{
    char *ret = NULL;

    if(_last_bg_color)
      if((ret = (char *)malloc(strlen(_last_bg_color)+1)) != NULL)
	strcpy(ret, _last_bg_color);

    return(ret);
}
