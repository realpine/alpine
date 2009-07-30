/*
 * $Id: adrbklib.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PITH_ADRBKLIB_INCLUDED
#define PITH_ADRBKLIB_INCLUDED


#include "../pith/indxtype.h"
#include "../pith/remtype.h"
#include "../pith/store.h"
#include "../pith/string.h"


/*
 *  Some notes:
 *
 * The order that the address book is stored in on disk is the order that
 * the entries will be displayed in. When an address book is opened,
 * if it is ReadWrite the sort order is checked and it is sorted if it is
 * out of order.
 *
 * Considerable complexity that previously existed in the address book
 * code has been removed. Cpu speeds have improved significantly and
 * memory has increased dramatically, as well. The cost of the complexity
 * of the lookup file and hashtables and all that stuff is thought to be
 * more than the benefits. See pre-5.00 pine adrbklib.h for the way it
 * used to be.
 *
 * The display code has remained the same.
 * There is also some allocation happening
 * in addrbook.c. In particular, the display is a window into an array
 * of rows, at least one row per addrbook entry plus more for lists.
 * Each row is an AddrScrn_Disp structure and those should typically take
 * up 6 or 8 bytes. A cached copy of addrbook entries is not kept, just
 * the element number to look it up (and often get it out of the EntryRef
 * cache). In order to avoid having to allocate all those rows, this is
 * also in the form of a cache. Only 3 * screen_length rows are kept in
 * the cache, and the cache is always a simple interval of rows. That is,
 * rows between valid_low and valid_high are all in the cache. Row numbers
 * in the display list are a little mysterious. There is no origin. That
 * is, you don't necessarily know where the start or end of the display
 * is. You only know how to go forward and backward and how to "warp"
 * to new locations in the display and go forward and backward from there.
 * This is because we don't know how many rows there are all together. It
 * is also a way to avoid searching through everything to get somewhere.
 * If you want to go to the End, you warp there and start backwards instead
 * of reading through all the entries to get there. If you edit an entry
 * so that it sorts into a new location, you warp to that new location to
 * save processing all of the entries in between.
 *
 *
 * Notes about RFC1522 encoding:
 *
 * If the fullname field contains other than US-ASCII characters, it is
 * encoded using the rules of RFC1522 or its successor. The value actually
 * stored in the file is encoded, even if it matches the character set of
 * the user. This is so it is easy to pass the entry around or to change
 * character sets without invalidating entries in the address book. When
 * a fullname is displayed, it is first decoded. If the fullname field is
 * encoded as being from a character set other than the user's character
 * set, that will be retained until the user edits the field. Then it will
 * change over to the user's character set. The comment field works in
 * the same way, as do the "phrase" fields of any addresses. On outgoing
 * mail, the correct character set will be retained if you use ComposeTo
 * from the address book screen. However, if you use a nickname from the
 * composer or ^T from the composer, the character set will be lost if it
 * is different from the user's character set.
 *
 *
 * Notes about RemoteViaImap address books:
 *
 * There are currently two types of address books, Local and Imap. Local means
 * it is in a local file. Imap means it is stored in a folder on a remote
 * IMAP server. The folder is a regular folder containing mail messages but
 * the messages are special. The first message is a header message. The last
 * message is the address book data. In between messages are old versions of
 * the address book data. The address book data is stored in the message as
 * it would be on disk, with no fancy mime encoding or anything. When it is
 * used the data from the last message in the folder is copied to a local file
 * and then it is used just like a local file. The local file is a cache for
 * the remote data. We can tell the remote data has been changed by looking
 * at the Date of the last message in the remote folder. If we see it has
 * changed we copy the whole file over again and replace the cache file.
 * A possibly quicker way to tell if it has changed is if the UID has
 * changed or the number of messages in the folder has changed. We use those
 * methods if possible since they don't require opening a new stream and
 * selecting the folder.  There is one metadata file for address book data.
 * The name of that file is stored in the pinerc file. It contains the names
 * of the cache files for RemoveViaImap address books plus other caching
 * information for those address books (uid...).
 */

#define NFIELDS 11 /* one more than num of data fields in addrbook entry */

/*
 * The type a_c_arg_t is a little confusing. It's the type we use in
 * place of adrbk_cntr_t when we want to pass an adrbk_cntr_t in an argument.
 * We were running into problems with the integral promotion of adrbk_cntr_t
 * args. A_c_arg_t has to be large enough to hold a promoted adrbk_cntr_t.
 * So, if adrbk_cntr_t is unsigned short, then a_c_arg_t needs to be int if
 * int is larger than short, or unsigned int if short is same size as int.
 * Since usign16_t always fits in a short, a_c_arg_t of unsigned int should
 * always work for !HUGE. For HUGE, UINT32 will be either an unsigned int
 * or an unsigned long. If it is an unsigned long, then a_c_arg_t better be
 * an unsigned long, too. If it is an unsigned int, then a_c_arg_t could
 * be an unsigned int, too. However, if we just make it unsigned long, then
 * it will be the same in all cases and big enough in all cases.
 */

#define adrbk_cntr_t UINT32  /* addrbook counter type                */
typedef unsigned long a_c_arg_t;     /* type of arg passed for adrbk_cntr_t  */
#define NO_NEXT ((adrbk_cntr_t)-1)
#define MAX_ADRBK_SIZE (2000000000L) /* leave room for extra display lines   */

/*
 * The value NO_NEXT is reserved to mean that there is no next address, or that
 * there is no address number to return. This is similar to getc returning
 * -1 when there is no char to get, but since we've defined this to be
 * unsigned we need to reserve one of the valid values for this purpose.
 * With current implementation it needs to be all 1's, so memset initialization
 * will work correctly.
 */

typedef enum {NotSet, Single, List} Tag;


/* This is what is actually used by the routines that manipulate things */
typedef struct adrbk_entry {
    char *nickname;    /* UTF-8                                         */
    char *fullname;    /* of simple addr or list (stored in UTF-8)      */
    union addr {
        char *addr;    /* for simple Single entries                     */
        char **list;   /* for distribution lists                        */
    } addr;
    char *fcc;         /* fcc specific for when sending to this address */
    char *extra;       /* comments field (stored in UTF-8)              */
    char  referenced;  /* for detecting loops during lookup             */
    Tag   tag;         /* single addr (Single) or a list (List)         */
} AdrBk_Entry;


typedef struct abook_tree_node {
    struct abook_tree_node *down;	/* next letter of nickname */
    struct abook_tree_node *right;	/* alternate letter of nickname */
    char                    value;	/* character at this node */
    adrbk_cntr_t            entrynum;	/* use NO_NEXT as no-data indicator */
} AdrBk_Trie;


/* information useful for displaying the addrbook */
typedef struct width_stuff {
    int max_nickname_width;
    int max_fullname_width;
    int max_addrfield_width;
    int max_fccfield_width;
    int third_biggest_fullname_width;
    int third_biggest_addrfield_width;
    int third_biggest_fccfield_width;
} WIDTH_INFO_S;


typedef struct expanded_list {
    adrbk_cntr_t          ent;
    struct expanded_list *next;
} EXPANDED_S;


typedef enum {Local, Imap} AdrbkType;


#ifndef IMAP_IDLE_TIMEOUT
#define IMAP_IDLE_TIMEOUT	(10L * 60L)	/* seconds */
#endif
#ifndef FILE_VALID_CHK_INTERVAL
#define FILE_VALID_CHK_INTERVAL (      15L)	/* seconds */
#endif
#ifndef LOW_FREQ_CHK_INTERVAL
#define LOW_FREQ_CHK_INTERVAL	(240)		/* minutes */
#endif

typedef struct adrbk {
    AdrbkType	  type;                /* type of address book               */
    char         *orig_filename;       /* passed in filename                 */
    char         *filename;            /* addrbook filename                  */
    char         *our_filecopy;        /* session copy of filename contents  */
    FILE         *fp;                  /* fp for our_filecopy                */
    adrbk_cntr_t  count;               /* how many entries in addrbook       */
    adrbk_cntr_t  del_count;           /* how many #DELETED entries in abook */
    AdrBk_Entry  *arr;                 /* array of entries                   */
    AdrBk_Entry  *del;                 /* array of deleted entries           */
    AdrBk_Trie   *nick_trie;
    AdrBk_Trie   *addr_trie;
    AdrBk_Trie   *full_trie;
    AdrBk_Trie   *revfull_trie;
    time_t        last_change_we_know_about;/* to look for others changing it*/
    time_t        last_local_valid_chk;/* when valid check was done          */
    unsigned      flags;               /* see defines in alpine.h (DEL_FILE...)*/
    WIDTH_INFO_S  widths;              /* helps addrbook.c format columns    */
    int           sort_rule;
    EXPANDED_S   *exp;                 /* this is for addrbook.c to use.  A
			       list of expanded list entry nums is kept here */
    EXPANDED_S   *checks;              /* this is for addrbook.c to use.  A
			       list of checked entry nums is kept here */
    EXPANDED_S   *selects;             /* this is for addrbook.c to use.  A
			       list of selected entry nums is kept here */
    REMDATA_S    *rd;
} AdrBk;


/*
 * The definitions starting here have to do with the virtual scrolling
 * region view into the addressbooks. That is, the display.
 *
 * We could make every use of an AdrBk_Entry go through a function call
 * like adrbk_get_ae(). Instead, we try to be smart and avoid the extra
 * function calls by knowing when the addrbook entry is still valid, either
 * because we haven't called any functions that could invalidate it or because
 * we have locked it in the cache. If we do lock it, we need to be careful
 * that it eventually gets unlocked. That can be done by an explicit
 * adrbk_get_ae(Unlock) call, or it is done implicitly when the address book
 * is written out. The reason it can get invalidated is that the abe that
 * we get returned to us is just a pointer to a cached addrbook entry, and
 * that entry can be flushed from the cache by other addrbook activity.
 * So we need to be careful to make sure the abe is certain to be valid
 * before using it.
 *
 * Data structures for the display of the address book. There's one
 * of these structures per line on the screen.
 *
 * Types: Title -- The title line for the different address books. It has
 *		   a ptr to the text of the Title line.
 * ClickHereCmb -- This is the line that says to click here to
 *                 expand.  It changes types into the individual expanded
 *                 components once it is expanded.  It doesn't have any data
 *                 other than an implicit title. This is only used with the
 *                 combined-style addrbook display.
 * ListClickHere --This is the line that says to click here to
 *                 expand the members of a distribution list. It changes
 *                 types into the individual expanded ListEnt's (if any)
 *                 when it is expanded. It has a ptr to an AdrBk_Entry.
 *    ListEmpty -- Line that says this is an empty distribution list. No data.
 *        Empty -- Line that says this is an empty addressbook. No data.
 *    ZoomEmpty -- Line that says no addrs in zoomed view. No data.
 * AddFirstGLob -- Place holder for adding an abook. No data.
 * AddFirstPers -- Place holder for adding an abook. No data.
 *   DirServers -- Place holder for accessing directory servers. No data.
 *       Simple -- A single addressbook entry. It has a ptr to an AdrBk_Entry.
 *                 When it is displayed, the fields are usually:
 *                 <nickname>       <fullname>       <address or another nic>
 *     ListHead -- The head of an address list. This has a ptr to an
 *		   AdrBk_Entry.
 *                 <blank line> followed by
 *                 <nickname>       <fullname>       "DISTRIBUTION LIST:"
 *      ListEnt -- The rest of an address list. It has a pointer to its
 *		   ListHead element and a ptr (other) to this specific address
 *		   (not a ptr to another AdrBk_Entry).
 *                 <blank>          <blank>          <address or another nic>
 *         Text -- A ptr to text. For example, the ----- lines and
 *		   whitespace lines.
 *    NoAbooks  -- There are no address books at all.
 *   Beginnning -- The (imaginary) elements before the first real element
 *          End -- The (imaginary) elements after the last real element
 */
typedef enum {DlNotSet, Empty, ZoomEmpty, AddFirstPers, AddFirstGlob,
	      AskServer, Title, Simple, ListHead, ListClickHere,
	      ListEmpty, ListEnt, Text, Beginning, End, NoAbooks,
	      ClickHereCmb, TitleCmb} LineType;
/* each line of address book display is one of these structures */
typedef struct addrscrn_disp {
    union {
        struct {
            adrbk_cntr_t  ab_element_number; /* which addrbook entry     */
	    adrbk_cntr_t  ab_list_offset;    /* which member of the list */
        }addrbook_entry;
        char        *text_ptr;               /* a UTF-8 string */
    }union_to_save_space;
    LineType       type;
} AddrScrn_Disp;
#define usst union_to_save_space.text_ptr
#define elnum union_to_save_space.addrbook_entry.ab_element_number
#define l_offset union_to_save_space.addrbook_entry.ab_list_offset

#define entry_is_checked    exp_is_expanded
#define entry_get_next      exp_get_next
#define entry_set_checked   exp_set_expanded
#define entry_unset_checked exp_unset_expanded
#define any_checked         exp_any_expanded
#define howmany_checked     exp_howmany_expanded

#define entry_is_selected   exp_is_expanded
#define entry_set_selected  exp_set_expanded
#define entry_unset_selected exp_unset_expanded
#define any_selected        exp_any_expanded
#define howmany_selected    exp_howmany_expanded

#define entry_is_deleted    exp_is_expanded
#define entry_set_deleted   exp_set_expanded
#define howmany_deleted     exp_howmany_expanded

#define entry_is_added      exp_is_expanded
#define entry_set_added     exp_set_expanded

/*
 * Argument to expand_address and build_address_internal is a BuildTo,
 * which is either a char * address or an AdrBk_Entry * (if we have already
 * looked it up in an addrbook).
 */
typedef enum {Str, Abe} Build_To_Arg_Type;
typedef struct build_to {
    Build_To_Arg_Type type;
    union {
	char        *str;  /* normal looking address string */
	AdrBk_Entry *abe;  /* addrbook entry */
    }arg;
} BuildTo;


/* Display lines used up by each top-level addrbook, counting blanks */
#define LINES_PER_ABOOK (3)
/* How many of those lines are visible (not blank) */
#define VIS_LINES_PER_ABOOK (2)
/* How many extra lines are between the personal and global sections */
#define XTRA_LINES_BETWEEN (1)
/* How many lines does the PerAdd line take, counting blank line */
#define LINES_PER_ADD_LINE (2)
/* Extra title lines above first entry that are shown when the combined-style
   display is turned on. */
#define XTRA_TITLE_LINES_IN_OLD_ABOOK_DISP (4)

typedef enum {DlcNotSet,
	      DlcPersAdd,		/* config screen only */
	      DlcGlobAdd,		/*    "      "    "   */

	      DlcTitle,			/* top level displays */
	      DlcTitleNoPerm,		/*  "    "      "     */
	      DlcSubTitle,		/*  "    "      "     */
	      DlcTitleBlankTop,		/*  "    "      "     */
	      DlcGlobDelim1,		/*  "    "      "     */
	      DlcGlobDelim2,		/*  "    "      "     */
	      DlcDirDelim1,		/*  "    "      "     */
	      DlcDirDelim2,		/*  "    "      "     */
	      DlcDirAccess,		/*  "    "      "     */
	      DlcDirSubTitle,		/*  "    "      "     */
	      DlcDirBlankTop,		/*  "    "      "     */

	      DlcTitleDashTopCmb,	/* combined-style top level display */
	      DlcTitleCmb,		/*     "      "    "    "      "    */
	      DlcTitleDashBottomCmb,	/*     "      "    "    "      "    */
	      DlcTitleBlankBottomCmb,	/*     "      "    "    "      "    */
	      DlcClickHereCmb,		/*     "      "    "    "      "    */
	      DlcTitleBlankTopCmb,	/*     "      "    "    "      "    */
	      DlcDirDelim1a,		/*     "      "    "    "      "    */
	      DlcDirDelim1b,		/*     "      "    "    "      "    */
	      DlcDirDelim1c,		/*     "      "    "    "      "    */

	      DlcEmpty,			/* display of a single address book */
	      DlcZoomEmpty,		/*    "                             */
	      DlcNoPermission,		/*    "                             */
	      DlcSimple,		/*    "                             */
	      DlcListHead,		/*    "                             */
	      DlcListClickHere,		/*    "                             */
	      DlcListEmpty,		/*    "                             */
	      DlcListEnt,		/*    "                             */
	      DlcListBlankTop,		/*    "                             */
	      DlcListBlankBottom,	/*    "                             */
	      DlcNoAbooks,		/*    "                             */

	      DlcOneBeforeBeginning,	/* used in both */
	      DlcTwoBeforeBeginning,	/*  "   "   "   */
	      DlcBeginning,		/*  "   "   "   */
	      DlcEnd} DlCacheType;

typedef enum {Initialize, FirstEntry, LastEntry, ArbitraryStartingPoint,
	      DoneWithCache, FlushDlcFromCache, Lookup} DlMgrOps;
typedef enum {Warp, DontWarp} HyperType;

/*
 * The DlCacheTypes are the types that a dlcache element can be labeled.
 * The idea is that there needs to be enough information in the single
 * cache element by itself so that you can figure out what the next and
 * previous dl rows are by just knowing this one row.
 *
 * In the top-level display, there are DlcTitle lines or DlcTitleNoPerm
 * lines, which are the same except we know that we can't access the
 * address book in the latter case. DlcSubTitle lines follow each of the
 * types of Title lines, and Titles within a section are separated by
 * DlcTitleBlankTop lines, which belong to (have the same adrbk_num as)
 * the Title they are above.
 * If there are no address books and no directory servers defined, we
 * have a DlcNoAbooks line. When we are displaying an individual address
 * book (not in the top-level display) there is another set of types. An
 * empty address book consists of one line of type DlcEmpty. An address
 * book without read permission is a DlcNoPermission. Simple address book
 * entries consist of a single DlcSimple line. Lists begin with a
 * DlcListHead. If the list is not expanded the DlcListHead is followed by
 * a DlcListClickHere. If it is known to be a list with no members the
 * DlcListHead is followed by a DlcListEmpty. If there are members and
 * the list is expanded, each list member is a single line of type
 * DlcListEnt. Two lists are separated by a DlcListBlankBottom belonging
 * to the first list. A list followed or preceded by a DlcSimple address
 * row has a DlcListBlank(Top or Bottom) separating it from the
 * DlcSimple. Above the top row of the display is an imaginary line of
 * type DlcOneBeforeBeginning. Before that is a DlcTwoBeforeBeginning. And
 * before that all the lines are just DlcBeginning lines. After the last
 * display line is a DlcEnd.
 *
 * The DlcDirAccess's are indexed by adrbk_num (re-used for this).
 * Adrbk_num -1 means access all of the servers.
 * Adrbk_num 0 ... n_serv -1 means access all a particular server.
 * Adrbk_num n_serv means access as if from composer using config setup.
 *
 * Here are the types of lines and where they fall in the top-level display:
 *
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcTwoBeforeBeginning
 * (not a visible line)			DlcOneBeforeBeginning
 *     Title				DlcTitle (or TitleNoPerm)
 *       Subtitle			DlcSubTitle
 * ---this is blank----------------	DlcTitleBlankTop
 *     Title				DlcTitle (or TitleNoPerm)
 *       Subtitle			DlcSubTitle
 * ---this is blank----------------	DlcGlobDelim1
 * ---this is blank----------------	DlcGlobDelim2
 *     Title				DlcTitle (or TitleNoPerm)
 *       Subtitle			DlcSubTitle
 * ---this is blank----------------	DlcTitleBlankTop
 *     Title				DlcTitle (or TitleNoPerm)
 *       Subtitle			DlcSubTitle
 * ---this is blank----------------	DlcDirDelim1
 * ---this is blank----------------	DlcDirDelim2
 *     Directory (query server 1)	DlcDirAccess (adrbk_num 0)
 *       Subtitle 			DlcDirSubTitle (adrbk_num 0)
 * ---this is blank----------------	DlcDirBlankTop
 *     Directory (query server 2)	DlcDirAccess (adrbk_num 1)
 *       Subtitle 			DlcDirSubTitle (adrbk_num 1)
 * (not a visible line)			DlcEnd
 * (not a visible line)			DlcEnd
 *
 *
 * There is a combined-style display triggered by the F_CMBND_ABOOK_DISP
 * feature. It's a mixture of the top-level and open addrbook displays. When an
 * addrbook is opened the rest of the addrbooks don't disappear from the
 * screen. In this view, the ClickHere lines can be replaced with the entire
 * contents of the addrbook, but the other stuff remains on the screen, too.
 * Here are the types of lines and where they fall in the
 * combined-style display:
 *
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcTwoBeforeBeginning
 * (not a visible line)			DlcOneBeforeBeginning
 * --------------------------------	DlcTitleDashTopOld
 *     Title				DlcTitleOld
 * --------------------------------	DlcTitleDashBottomOld
 * ---this is blank----------------	DlcTitleBlankBottom
 *     ClickHere			DlcClickHereOld
 * ---this is blank----------------	DlcTitleBlankTop
 * --------------------------------	DlcTitleDashTopOld
 *     Title				DlcTitleOld
 * --------------------------------	DlcTitleDashBottomOld
 * ---this is blank----------------	DlcTitleBlankBottom
 *     ClickHere			DlcClickHereOld
 * ---this is blank----------------	DlcDirDelim1
 * --------------------------------	DlcDirDelim1a
 * Directories                      	DlcDirDelim1b
 * --------------------------------	DlcDirDelim1c
 * ---this is blank----------------	DlcDirDelim2
 *     Directory (query server 1)	DlcDirAccess (adrbk_num 0)
 *       Subtitle 			DlcDirSubTitle (adrbk_num 0)
 * ---this is blank----------------	DlcDirBlankTop
 *     Directory (query server 2)	DlcDirAccess (adrbk_num 1)
 *       Subtitle 			DlcDirSubTitle (adrbk_num 1)
 * (not a visible line)			DlcEnd
 * (not a visible line)			DlcEnd
 *
 * If there are no addrbooks in either of the two sections, or no Directory
 * servers, then that section is left out of the display. If there is only
 * one address book and no Directories, then the user goes directly into the
 * single addressbook view which looks like:
 *
 * if(no entries in addrbook)
 *
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcTwoBeforeBeginning
 * (not a visible line)			DlcOneBeforeBeginning
 *     Empty or NoPerm or NoAbooks	DlcEmpty, DlcZoomEmpty, DlcNoPermission,
 *					or DlcNoAbooks
 * (not a visible line)			DlcEnd
 * (not a visible line)			DlcEnd
 *
 * else
 *
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcTwoBeforeBeginning
 * (not a visible line)			DlcOneBeforeBeginning
 *     Simple Entry			DlcSimple
 *     Simple Entry			DlcSimple
 *     Simple Entry			DlcSimple
 *					DlcListBlankTop
 *     List Header			DlcListHead
 *          Unexpanded List		DlcListClickHere
 *          or
 *          Empty List			DlcListEmpty
 *          or
 *          List Entry 1		DlcListEnt
 *          List Entry 2		DlcListEnt
 *					DlcListBlankBottom
 *     List Header			DlcListHead
 *          List Entry 1		DlcListEnt
 *          List Entry 2		DlcListEnt
 *          List Entry 3		DlcListEnt
 *					DlcListBlankBottom
 *     Simple Entry			DlcSimple
 *					DlcListBlankTop
 *     List Header			DlcListHead
 *          Unexpanded List		DlcListClickHere
 * (not a visible line)			DlcEnd
 * (not a visible line)			DlcEnd
 *
 * The config screen view is similar to the top-level view except there
 * is no directory section (it has it's own config screen) and if there
 * are zero personal addrbooks or zero global addrbooks then a placeholder
 * line of type DlcPersAdd or DlcGlobAdd takes the place of the DlcTitle
 * line.
 */
typedef struct dl_cache {
    long         global_row; /* disp_list row number */
    adrbk_cntr_t dlcelnum;   /* which elnum from that addrbook */
    adrbk_cntr_t dlcoffset;  /* offset in a list, only for ListEnt rows */
    short        adrbk_num;  /* which address book we're related to */
    DlCacheType  type;       /* type of this row */
    AddrScrn_Disp dl;	     /* the actual dl that goes with this row */
} DL_CACHE_S;


typedef enum {Nickname, Fullname, Addr, Filecopy, Comment, Notused,
	      Def, WhenNoAddrDisplayed, Checkbox, Selected} ColumnType;

/*
 * Users can customize the addrbook display, so this tells us which data
 * is in a particular column and how wide the column is. There is an
 * array of these per addrbook, of length NFIELDS (number of possible cols).
 */
typedef struct column_description {
    ColumnType type;
    WidthType  wtype;
    int        req_width; /* requested width (for fixed and percent types) */
    int        width;     /* actual width to use */
    int        old_width;
} COL_S;


/* address book attributes for peraddrbook type */
#define GLOBAL		0x1	/* else it is personal */
#define REMOTE_VIA_IMAP	0x2	/* else it is a local file  */


typedef enum {TotallyClosed, /* hash tables not even set up yet               */
	      Closed,     /* data not read in, no display list                */
	      NoDisplay,  /* data is accessible, no display list              */
	      HalfOpen,   /* data not accessible, initial display list is set */
	      ThreeQuartOpen, /* like HalfOpen without partial_close          */
	      Open        /* data is accessible and display list is set       */
	     } OpenStatus;

/*
 * There is one of these per addressbook.
 */
typedef struct peraddrbook {
    unsigned        	type;
    AccessType          access;
    OpenStatus          ostatus;
    char               *abnick,	             /* kept internally in UTF-8 */
		       *filename;
    AdrBk              *address_book;        /* the address book handle */
    int                 gave_parse_warnings;
    COL_S               disp_form[NFIELDS];  /* display format */
    int			nick_is_displayed;   /* these are for convenient, */
    int			full_is_displayed;   /* fast access.  Could get   */
    int			addr_is_displayed;   /* same info from disp_form. */
    int			fcc_is_displayed;
    int			comment_is_displayed;
    STORE_S	       *so;                  /* storage obj for addrbook
						temporarily stored here   */
} PerAddrBook;


/*
 * This keeps track of the state of the screen and information about all
 * the address books. We usually only have one of these but sometimes
 * we save a version of this state (with save_state) and re-call the
 * address book functions. Then when we pop back up to where we were
 * (with restore_state) the screen and the state of the address books
 * is restored to what it was.
 */
typedef struct addrscreenstate {
    PerAddrBook   *adrbks;       /* array of addrbooks                    */
    int		   initialized,  /* have we done at least simple init?    */
                   n_addrbk,     /* how many addrbooks are there          */
                   how_many_personals, /* how many of those are personal? */
                   cur,          /* current addrbook                      */
                   cur_row,      /* currently selected line               */
                   old_cur_row,  /* previously selected line              */
                   l_p_page;	 /* lines per (screen) page               */
    long           top_ent;      /* index in disp_list of top entry on screen */
    int            ro_warning,   /* whether or not to give warning        */
                   checkboxes,   /* whether or not to display checkboxes  */
                   selections,   /* whether or not to display selections  */
                   do_bold,      /* display selections in bold            */
                   no_op_possbl, /* user can't do anything with current conf */
                   zoomed,       /* zoomed into view only selected entries */
                   config,       /* called from config screen             */
                   n_serv,       /* how many directory servers are there  */
                   n_impl;       /* how many of those have impl bit set   */
#ifdef	_WINDOWS
    long	   last_ent;	 /* index of last known entry		  */
#endif
} AddrScrState;


/*
 * AddrBookScreen and AddrBookConfig are the maintenance screens, all the
 * others are selection screens. The AddrBookConfig screen is an entry
 * point from the Setup/Addressbooks command in the main menu. Those that
 * end in Com are called from the pico HeaderEditor, either while in the
 * composer or while editing an address book entry.  SelectManyNicks
 * returns a comma-separated list of nicknames. SelectAddrLccCom and
 * SelectNicksCom return a comma-separated list of nicknames.
 * SelectNickTake, SelectNickCom, and SelectNick all return a single
 * nickname.  The ones that returns multiple nicknames or multiple
 * addresses all allow ListMode. They are SelectAddrLccCom,
 * SelectNicksCom, and SelectMultNoFull.
 */
typedef enum {AddrBookScreen,	   /* maintenance screen                     */
	      AddrBookConfig,	   /* config screen                          */
	      SelectAddrLccCom,	   /* returns list of nicknames of lists     */
	      SelectNicksCom,	   /* just like SelectAddrLccCom, but allows
				      selecting simple *and* list entries    */
	      SelectNick,	   /* returns single nickname                */
	      SelectNickTake,	   /* Same as SelectNick but different help  */
	      SelectNickCom,	   /* Same as SelectNick but from composer   */
	      SelectManyNicks,	   /* Returns list of nicks */
	      SelectAddr,	   /* Returns single address */
	      SelectAddrNoFull,	   /* Returns single address without fullname */
	      SelectMultNoFull	   /* Returns mult addresses without fullname */
	     } AddrBookArg;


typedef struct save_state_struct {
    AddrScrState *savep;
    OpenStatus   *stp;
    DL_CACHE_S   *dlc_to_warp_to;
} SAVE_STATE_S;


typedef struct act_list {
    PerAddrBook *pab;
    adrbk_cntr_t num,
		 num_in_dst;
    unsigned int skip:1,
		 dup:1;
} ACTION_LIST_S;


typedef struct ta_abook_state {
    PerAddrBook  *pab;
    SAVE_STATE_S  state;
} TA_STATE_S;


/*
 * Many of these should really only have a single value but we give them
 * an array for uniformity.
 */
typedef struct _vcard_info {
    char **nickname;
    char **fullname;
    char  *first;
    char  *middle;
    char  *last;
    char **fcc;
    char **note;
    char **title;
    char **tel;
    char **email;
} VCARD_INFO_S;


extern AddrScrState as;
extern jmp_buf      addrbook_changed_unexpectedly;
extern long         msgno_for_pico_callback;
extern BODY        *body_for_pico_callback;
extern ENVELOPE    *env_for_pico_callback;
extern int          ab_nesting_level;


/*
 * These constants are supposed to be suitable for use as longs where the longs
 * are representing a line number or message number.
 * These constants aren't suitable for use with type adrbk_cntr_t. There is
 * a constant called NO_NEXT which you probably want for that.
 */
#define NO_LINE         (2147483645L)
#define CHANGED_CURRENT (NO_LINE + 1L)


/*
 * The do-while stuff is so these are statements and can be written with
 * a following ; like a regular statement without worrying about braces and all.
 */
#define SKIP_SPACE(p) do{while(*p && *p == SPACE)p++;}while(0)
#define SKIP_TO_TAB(p) do{while(*p && *p != TAB)p++;}while(0)
#define RM_END_SPACE(start,end)                                             \
	    do{char *_ptr = end;                                            \
	       while(--_ptr >= start && *_ptr == SPACE)*_ptr = '\0';}while(0)
#define REPLACE_NEWLINES_WITH_SPACE(p)                                      \
		do{register char *_qq;                                      \
		   for(_qq = p; *_qq; _qq++)                                \
		       if(*_qq == '\n' || *_qq == '\r')                     \
			   *_qq = SPACE;}while(0)
#define DELETED "#DELETED-"
#define DELETED_LEN 9


#define ONE_HUNDRED_DAYS (60L * 60L * 24L * 100L)

/*
 * When address book entries are deleted, they are left in the file
 * with the nickname prepended with a string like #DELETED-96/01/25#, 
 * which stands for year 96, month 1, day 25 of the month. When one of
 * these entries is more than ABOOK_DELETED_EXPIRE_TIME seconds old,
 * then it will be totally removed from the address book the next time
 * an adrbk_write() is done. This is for emergencies where somebody
 * deletes something from their address book and would like to get it
 * back. You get it back by editing the nickname field manually to remove
 * the extra 18 characters off the front.
 */
#ifndef ABOOK_DELETED_EXPIRE_TIME
#define ABOOK_DELETED_EXPIRE_TIME   ONE_HUNDRED_DAYS
#endif


#ifdef	ENABLE_LDAP
typedef struct _cust_filt {
    char *filt;
    int   combine;
} CUSTOM_FILT_S;

#define RUN_LDAP	"LDAP: "
#define LEN_RL		6
#define QRUN_LDAP	"\"LDAP: "
#define LEN_QRL		7
#define LDAP_DISP	"[ LDAP Lookup ]"
#endif


/*
 * There are no restrictions on the length of any of the fields, except that
 * there are some restrictions in the current input routines.
 */

/*
 * The on-disk address book has entries that look like:
 *
 * Nickname TAB Fullname TAB Address_Field TAB Fcc TAB Comment
 *
 * An entry may be broken over more than one line but only at certain
 * spots. A continuation line starts with spaces (spaces, not white space).
 * One place a line break can occur is after any of the TABs. The other
 * place is in the middle of a list of addresses, between addresses.
 * The Address_Field may be either a simple address without the fullname
 * or brackets, or it may be an address list. An address list is
 * distinguished by the fact that it begins with "(" and ends with ")".
 * Addresses within a list are comma separated and each address in the list
 * may be a full rfc822 address, including Fullname and so on.
 *
 * Examples:
 * fred TAB Flintstone, Fred TAB fred@bedrock.net TAB fcc-flintstone TAB comment
 * or
 * fred TAB Flintstone, Fred TAB \n
 *    fred@bedrock.net TAB fcc-flintstone TAB \n
 *    comment
 * somelist TAB Some List TAB (fred, \n
 *    Barney Rubble <barney@bedrock.net>, wilma@bedrock.net) TAB \n
 *    fcc-for-some-list TAB comment
 */


/* exported prototypes */
AdrBk         *adrbk_open(PerAddrBook *, char *, char *, size_t, int);
int            adrbk_is_in_sort_order(AdrBk *, int);
adrbk_cntr_t   adrbk_count(AdrBk *);
AdrBk_Entry   *adrbk_get_ae(AdrBk *, a_c_arg_t);
AdrBk_Entry   *adrbk_lookup_by_nick(AdrBk *, char *, adrbk_cntr_t *);
AdrBk_Entry   *adrbk_lookup_by_addr(AdrBk *, char *, adrbk_cntr_t *);
char          *adrbk_formatname(char *, char **, char **);
void           adrbk_clearrefs(AdrBk *);
AdrBk_Entry   *adrbk_newentry(void);
AdrBk_Entry   *copy_ae(AdrBk_Entry *);
int            adrbk_add(AdrBk *, a_c_arg_t, char *, char *, char *, char *,
			 char *, Tag, adrbk_cntr_t *, int *, int, int, int);
int            adrbk_append(AdrBk *, char *, char *, char *, 
			    char *, char *, Tag, adrbk_cntr_t *);
int            adrbk_delete(AdrBk *, a_c_arg_t, int, int, int, int);
int            adrbk_listdel(AdrBk *, a_c_arg_t, char *);
int            adrbk_listdel_all(AdrBk *, a_c_arg_t);
int            adrbk_nlistadd(AdrBk *, a_c_arg_t,adrbk_cntr_t *,int *,char **,int,int,int);
void           adrbk_check_validity(AdrBk *, long);
MAILSTREAM    *adrbk_handy_stream(char *);
void           adrbk_close(AdrBk *);
void           adrbk_partial_close(AdrBk *);
void           note_closed_adrbk_stream(MAILSTREAM *);
int            adrbk_write(AdrBk *, a_c_arg_t, adrbk_cntr_t *, int *, int, int);
void           free_ae(AdrBk_Entry **);
void           exp_free(EXPANDED_S *);
int            exp_is_expanded(EXPANDED_S *, a_c_arg_t);
int            exp_howmany_expanded(EXPANDED_S *);
int            exp_any_expanded(EXPANDED_S *);
adrbk_cntr_t   exp_get_next(EXPANDED_S **);
void           exp_set_expanded(EXPANDED_S *, a_c_arg_t);
void           exp_unset_expanded(EXPANDED_S *, a_c_arg_t);
int            adrbk_sort(AdrBk *, a_c_arg_t, adrbk_cntr_t *, int);
int            any_ab_open(void);
void           init_ab_if_needed(void);
int            init_addrbooks(OpenStatus, int, int, int);
void           addrbook_reset(void);
void           addrbook_redo_sorts(void);
AccessType     adrbk_access(PerAddrBook *);
void           trim_remote_adrbks(void);
void           completely_done_with_adrbks(void);
void           init_abook(PerAddrBook *, OpenStatus);
int            adrbk_check_all_validity_now(void);
int            adrbk_check_and_fix(PerAddrBook *, int, int, int);
int            adrbk_check_and_fix_all(int, int, int);
void           adrbk_maintenance(void);
char         **parse_addrlist(char *);
char          *skip_to_next_addr(char *);
void           add_forced_entries(AdrBk *);


#endif /* PITH_ADRBKLIB_INCLUDED */
