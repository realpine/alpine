/*
 * $Id: keydefs.h 120 2006-09-18 18:53:01Z hubert@u.washington.edu $
 *
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

#ifndef PICO_KEYDEFS_INCLUDED
#define PICO_KEYDEFS_INCLUDED

/*
 * Key value definitions for pico/pine
 */


/*
 * Char conversion constants
 * Make these different values than c-client's U8G_ERROR constants, though I don't
 * think that is necessary.
 * These values are not possible as regular UCS-4 values.
 */

#undef CTRL
#define CTRL		0x1000000
#define FUNC		0x2000000
#if defined(DOS) || defined(OS2)
#define MENU		0x4000000
#endif

#define KEY_BASE	0x40000000
#define CCONV_NEEDMORE	KEY_BASE+1	/* need more octets */
#define CCONV_BADCHAR	KEY_BASE+2	/* can tell it's bad conversion */
#define CCONV_EOF	KEY_BASE+3	/* can tell it's bad conversion */

/*
 * defs for keypad and function keys...
 */
#define KEY_UP		KEY_BASE+100
#define KEY_DOWN	KEY_BASE+101
#define KEY_RIGHT	KEY_BASE+102
#define KEY_LEFT	KEY_BASE+103
#define KEY_PGUP	KEY_BASE+104
#define KEY_PGDN	KEY_BASE+105
#define KEY_HOME	KEY_BASE+106
#define KEY_END		KEY_BASE+107
#define KEY_DEL		KEY_BASE+108
#define BADESC		KEY_BASE+109
#define KEY_MOUSE	KEY_BASE+110	/* Fake key to indicate mouse event. */
#define KEY_SCRLUPL	KEY_BASE+111
#define KEY_SCRLDNL	KEY_BASE+112
#define KEY_SCRLTO	KEY_BASE+113
#define KEY_XTERM_MOUSE	KEY_BASE+114
#define KEY_DOUBLE_ESC	KEY_BASE+115
#define KEY_SWALLOW_Z	KEY_BASE+116
#define KEY_SWAL_UP	KEY_BASE+117	/* These four have to be in the same order */
#define KEY_SWAL_DOWN	KEY_BASE+118	/* as KEY_UP, KEY_DOWN, ...                */
#define KEY_SWAL_RIGHT	KEY_BASE+119
#define KEY_SWAL_LEFT	KEY_BASE+120
#define KEY_KERMIT	KEY_BASE+121
#define KEY_JUNK	KEY_BASE+122
#define KEY_RESIZE	KEY_BASE+123  /* Fake key to cause resize. */

#define KEY_MENU_FLAG	0x1000		/* maybe 0x10000000 ?? */

#define KEY_MASK	0x13FF		/* ??? */

/*
 * Don't think we are using the fact that this is zero anywhere,
 * but just in case we'll leave it.
 */
#define NO_OP_COMMAND	0x0	/* no-op for short timeouts      */

#define NO_OP_IDLE	KEY_BASE+124	/* no-op for >25 second timeouts */
#define READY_TO_READ	KEY_BASE+125
#define BAIL_OUT	KEY_BASE+126
#define PANIC_NOW	KEY_BASE+127
#define READ_INTR	KEY_BASE+128
#define NODATA		KEY_BASE+129
#define KEY_UTF8	KEY_BASE+130
#define KEY_UNKNOWN	KEY_BASE+131
 
/*
 * defines for function keys
 * I think this depend on being in order but not on the specific values
 */
#define F1      KEY_BASE+200      /* Function key one              */
#define F2      KEY_BASE+201      /* Function key two              */
#define F3      KEY_BASE+202      /* Function key three            */
#define F4      KEY_BASE+203      /* Function key four             */
#define F5      KEY_BASE+204      /* Function key five             */
#define F6      KEY_BASE+205      /* Function key six              */
#define F7      KEY_BASE+206      /* Function key seven            */
#define F8      KEY_BASE+207      /* Function key eight            */
#define F9      KEY_BASE+208      /* Function key nine             */
#define F10     KEY_BASE+209      /* Function key ten              */
#define F11     KEY_BASE+210      /* Function key eleven           */
#define F12     KEY_BASE+211      /* Function key twelve           */

/* 1st tier pine function keys */
#define PF1	F1
#define PF2	F2
#define PF3	F3
#define PF4	F4
#define PF5	F5
#define PF6	F6
#define PF7	F7
#define PF8	F8
#define PF9	F9
#define PF10	F10
#define PF11	F11
#define PF12	F12

#define PF2OPF(x)	(x + 0x10)
#define PF2OOPF(x)	(x + 0x20)
#define PF2OOOPF(x)	(x + 0x30)

/* 2nd tier pine function keys */
#define OPF1	PF2OPF(PF1)
#define OPF2	PF2OPF(PF2)
#define OPF3	PF2OPF(PF3)
#define OPF4	PF2OPF(PF4)
#define OPF5	PF2OPF(PF5)
#define OPF6	PF2OPF(PF6)
#define OPF7	PF2OPF(PF7)
#define OPF8	PF2OPF(PF8)
#define OPF9	PF2OPF(PF9)
#define OPF10	PF2OPF(PF10)
#define OPF11	PF2OPF(PF11)
#define OPF12	PF2OPF(PF12)

/* 3rd tier pine function keys */
#define OOPF1	PF2OOPF(PF1)
#define OOPF2	PF2OOPF(PF2)
#define OOPF3	PF2OOPF(PF3)
#define OOPF4	PF2OOPF(PF4)
#define OOPF5	PF2OOPF(PF5)
#define OOPF6	PF2OOPF(PF6)
#define OOPF7	PF2OOPF(PF7)
#define OOPF8	PF2OOPF(PF8)
#define OOPF9	PF2OOPF(PF9)
#define OOPF10	PF2OOPF(PF10)
#define OOPF11	PF2OOPF(PF11)
#define OOPF12	PF2OOPF(PF12)

/* 4th tier pine function keys */
#define OOOPF1	PF2OOOPF(PF1)
#define OOOPF2	PF2OOOPF(PF2)
#define OOOPF3	PF2OOOPF(PF3)
#define OOOPF4	PF2OOOPF(PF4)
#define OOOPF5	PF2OOOPF(PF5)
#define OOOPF6	PF2OOOPF(PF6)
#define OOOPF7	PF2OOOPF(PF7)
#define OOOPF8	PF2OOOPF(PF8)
#define OOOPF9	PF2OOOPF(PF9)
#define OOOPF10	PF2OOOPF(PF10)
#define OOOPF11	PF2OOOPF(PF11)
#define OOOPF12	PF2OOOPF(PF12)

#endif	/* PICO_KEYDEFS_INCLUDED */
