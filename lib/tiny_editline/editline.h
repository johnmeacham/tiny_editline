#ifndef EDITLINE_H
#define EDITLINE_H

#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>

// configuration
#define BUFSIZE 80
#define HISTSIZE 128
/* enable all commands dealing with words, adds a bit of code but doesn't affect
 * ram or speed */
#define ENABLE_WORDS true
#define ENABLE_HISTORY (HISTSIZE > 0)
#define PROMPT  ';'

/* META-k can be typed as ALT-k or ESC k */
#define CTL(x)          ((x) & 0x1F)
#define ISCTL(x)        (CTL(x) == (x))
#define UNCTL(x)        ((x) + 64)
#define META(x)         ((x) | 0x80)
#define ISMETA(x)       (!!((x) & 0x80))
#define UNMETA(x)       ((x) & 0x7F)

// action.
enum {
        EL_NOTHING = 0,
        EL_REDRAW,      // screen has been cleared and redrawn.
        EL_COMMAND,     // a full command is ready in the buffer
        EL_UNKNOWN      // unknown control or alt code, value stored in key.
};

struct editline {
        // buffer
        uint8_t pos, len;
        char buf[BUFSIZE];
#if ENABLE_HISTORY
        // history
        char hist[HISTSIZE];
        uint8_t hend, hcur;
#endif
        // escape state
        uint8_t escape;
        // last key pressed.
        char key;
};

#define EDITLINE_INIT { 0 }

// This should be implemented by the user of the library.
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

// manually add something to history.
void editline_add_history(struct editline *, char *data);

#endif
