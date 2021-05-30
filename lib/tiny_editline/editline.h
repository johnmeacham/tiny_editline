#ifndef EDITLINE_H
#define EDITLINE_H

#include <inttypes.h>
#include <stdio.h>

// configuration
#define BUFSIZE 80
#define HISTSIZE 128
/* enable all commands dealing with words, adds a bit of code but doesn't affect
 * ram or speed */
#define ENABLE_WORDS true
#define ENABLE_HISTORY (HISTSIZE > 0)

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
        EL_EXIT,        // got EOF
        EL_REDRAW,      // screen has been cleared and redrawn.
        EL_DATA,        // a full command is ready in editline_buf
        EL_QDATA,       // data that was aborted via ^Q
        EL_UNKNOWN      // unknown control or alt code, value stored in key.
};

struct editline_state {
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

#define EDITLINE_STATE_INIT { 0 }

static inline char *editline_buf(struct editline_state *state)
{
        return state->buf;
}


// should be provided by user.
void editline_putchar(char ch);
//#define editline_putchar(ch) putchar(ch)

// clear screen and set up terminal.
void editline_redraw(struct editline_state *state);
// if flag is set, a redraw will be enforced on next character.
void editline_set_redraw_flag(struct editline_state *state);

// status line routines to display a fixed amount of data at the top of the
// screen. these should be used in a pair. lines are numbered
// starting at zero.
void begin_statusline(struct editline_state *state, int n);
void end_statusline(struct editline_state *state);
void reserve_statuslines(struct editline_state *state, int n);

// call this when a new keypress comes in. It will return one of EL_NOTHING,
// EL_EXIT, EL_REDRAW, or EL_DATA
int got_char(struct editline_state *s, char ch);

void add_history(struct editline_state *, char *data);

#endif
