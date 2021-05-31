#ifndef EDITLINE_H
#define EDITLINE_H

#include <stdint.h>
#include <stdbool.h>

// configuration. you can define these to be global variables if you wish them
// to be configurable at run time.
#define BUFSIZE 128
#define PROMPT  ';'

/* enable features that may affect code size */
#define ENABLE_WORDS   true   /* all word editing commands */
#define ENABLE_HISTORY true   /* history, ctrl-[pn] */
#define ENABLE_DEBUG   false  /* debug key & assertions. needs stdio. big!*/

/* META-k can be typed as ALT-k or ESC k */
#define CTL(x)          (char)((x) & 0x1F)
#define ISCTL(x)        (!((x) & ~0x1f))
#define UNCTL(x)        (char)((x) + 64)
#define META(x)         (char)((x) | 0x80)
#define ISMETA(x)       (bool)((x) & 0x80)
#define UNMETA(x)       (char)((x) & 0x7F)

// action.
enum {
        EL_NOTHING = 0,
        EL_REDRAW,      // screen has been cleared and redrawn.
        EL_COMMAND,     // a full command is ready in the buffer
        EL_UNKNOWN      // unknown control or alt code, value stored in key.
};

struct editline {
        // key decoding
        char escape, key;
        // buffer
        uint8_t pos, len, hcur;
        char buf[BUFSIZE];
};

#define EDITLINE_INIT {  0 }


// These should be implemented by the user of the library.
void user_putchar(char ch);

// call this for each character typed and take action based on the return value.
int editline_process_char(struct editline *s, char ch);

// these can be used to hide and restore the current command, so that you may
// write to the screen without interfering.
void editline_hide_command(struct editline *s);
void editline_restore_command(struct editline *s);

// clear screen and set up terminal manually. Alternatively you can arrange to
// pass ^L to editline_process_char.
void editline_redraw(struct editline *state);

// Status line routines to display a fixed amount of data at the top of the
// screen that is not affected by scrolling.
//
// These should be called after a EL_REDRAW event since it will have cleared the
// status line state.
//
// if reserve_statuslines is not called then the lines will be clobbered when
// the terminal scrolls.
void reserve_statuslines(struct editline *state, int n);
void begin_statusline(struct editline *state, int n);
void end_statusline(struct editline *state);

// call this after an EL_COMMAND was returned once you are done processing it.
void editline_command_complete(struct editline *state, bool add_to_history);

// print a character using color to indicate control/meta status
void debug_color_char(char c);

#endif
