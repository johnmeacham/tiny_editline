#include "editline.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CTL(x)          ((x) & 0x1F)
#define ISCTL(x)        (CTL(x) == (x))
#define UNCTL(x)        ((x) + 64)
#define META(x)         ((x) | 0x80)
#define ISMETA(x)       ((x) & 0x80)
#define UNMETA(x)       ((x) & 0x7F)

#ifdef EDITLINE_DEBUG
static void char_show(char c);
#endif
static int editline_char(struct editline_state *state, unsigned char ch);

/* static void putnum(unsigned short n) */
/* { */
/*         char buf[12], *s = buf; */
/*         itoa(n,buf,10); */
/*         while(*s++) */
/*                 editline_putchar(*s); */

/*         /1* unsigned char i = 0; *1/ */
/*         /1* do { *1/ */
/*         /1*         buf[i++] = n % 10 + '0'; *1/ */
/*         /1* } while ((n /= 10) > 0); *1/ */
/*         /1* while (i--) *1/ */
/*         /1*         editline_putchar(buf[i]); *1/ */
/* } */

static void putnum(unsigned short n)
{
        char buf[12], *s = buf;
        do {
                *s++ = n % 10 + '0';
        } while ((n /= 10) > 0);
        while (s-- != buf)
                editline_putchar(*s);
}

//#define putchars(x,y) do {editline_putchar(x); editline_putchar(y); } while(0)
static void putchar2(char x, char y)  {
        editline_putchar(x);
        editline_putchar(y);
}
static void putchars(char *s)  {
        while(*s++)
                editline_putchar(*s);
}

static void csi(char ch)
{
        putchar2('\033','[');
        editline_putchar(ch);
}

static void csi_n(int num, char ch)
{
        putchar2('\033','[');
        putnum(num);
        editline_putchar(ch);
}

static void show_cursor(bool show)
{
        csi('?');
        putchar2('2','5');
        editline_putchar(show ? 'h' : 'l');
}


static void move_cursor(int8_t n)
{
        if (n)
                csi_n(n > 0 ? n : -n, n > 0 ? 'C' : 'D');
}

static void redraw_current_command(struct editline_state *state)
{

        editline_putchar('\r');
        csi_n(999, 'H');
        for (unsigned char i = 0; i < state->len; i++)
                editline_putchar(editline_buf(state)[i]);
        csi('K');
        editline_putchar('\r');
        move_cursor(state->pos);
}

void editline_redraw(struct editline_state *state)
{
        show_cursor(false);
        csi_n(2, 'J');
        if (STATUSLINES) {
                csi_n(STATUSLINES + 2, ';');
                putchar2('9','9');
                putchar2('9','r');
                csi_n(STATUSLINES + 1, 'H');
                for (char i = 0; i < 10; i++)
                        editline_putchar('-');
        }
        redraw_current_command(state);
        show_cursor(true);
}



static void
print_rest(struct editline_state *state)
{
        csi('s');
        for (unsigned char i = state->pos; i < state->len; i++)
                editline_putchar(editline_buf(state)[i]);
        csi('K');
        putchar2(' ', ' ');
        csi('u');
}


#if STATUSLINES
void begin_statusline(struct editline_state *state, int n)
{
        show_cursor(false);
        csi_n(n + 1, 'H');
        editline_putchar('\r');
}

void end_statusline(struct editline_state *state)
{
        csi('K');
        csi_n(999, 'H');
        editline_putchar('\r');
        move_cursor(state->pos);
        show_cursor(true);
}
#endif

/* this translates terminal codes for special keys to
 * the functionally equivalent control codes.
 *
 * calls editline_char on each code returning its result.
 * returns NULL otherwise.
 *
 *
 * up arrow      ^P   (previous)
 * down arrow    ^N   (next)
 * right arrow   ^F   (forward)
 * left arrow    ^B   (back)
 * home          ^A   (beginning of line)
 * end           ^E   (end of line)
 * delete        ^D   (delete char)
 *
 * */
int got_char(struct editline_state *s, char ch)
{
        if (!s->escape)  {
                if (ch == CTL('[')) {
                        s->escape = 1;
                        return EL_NOTHING;
                } else
                        return editline_char(s, ch);
        } else if (s->escape == 1) {
                if (ch == '[') {
                        s->escape = 2;
                        return EL_NOTHING;
                } else {
                        s->escape = 0;
//                        editline_char(s, CTL('['));
                        return editline_char(s, META(ch));
                }
        } else if (s->escape == 2) {
                s->escape = 0;
                /*                  ABCD..F..H  */
                const char str[] = "PNFB\0E\0A";
                if (ch >= 'A' && ch <= 'H')
                        return editline_char(s, CTL(str[ch - 'A']));
                if (ch >= '0' && ch <= '9') {
                        s->escape = ch;
                        return EL_NOTHING;
                }
                editline_char(s, CTL('['));
                editline_char(s, '[');
                return editline_char(s, ch);
        } else if (s->escape >= '0' && s->escape <= '9') {
                if (ch == '~') {
                        ch = s->escape;
                        s->escape = 0;
                        switch (ch) {
                        case '1':
                                return editline_char(s, CTL('A'));
                        case '3':
                                return editline_char(s, CTL('D'));
                        case '4':
                                return editline_char(s, CTL('E'));
                        }
                }
                editline_char(s, CTL('['));
                editline_char(s, '[');
                editline_char(s, s->escape);
                return editline_char(s, ch);
        }
        return EL_NOTHING;
}

/* Interpret keycodes, compatible with emacs and command line.
 *
 * ^P   previous line   (up)
 * ^N   next line       (down)
 * ^F   forward char    (right)
 * ^B   back char       (left)
 * ^A   beginning of line  (home)
 * ^E   end of line        (end)
 * ^D   delete char     (delete)
 * ^H   backspace       (backspace)
 * ^Q   delete whole line (perhaps adding it to history)
 * ^K   delete from cursor until end of line
 * ^U   delete from cursor until beginning of line
 * ^L   redraw screen
 * ^M   enter
 */

static void delete(struct editline_state *state, int pos, int len) {
        if (pos < 0) {
                len += pos;
                pos = 0;
        }
        if (pos + len > (int)state->len)
                len = (int)state->len - pos;
        if(len <= 0)
                return;
        memmove(editline_buf(state) + pos ,  editline_buf(state) + pos + len , state->len - (pos + len));
        state->len -= len;
}
static void move_cursor_to(struct editline_state *state, int pos) {
        if (pos < 0)
                pos = 0;
        if (pos > state->len)
                pos = state->len;
        move_cursor(pos - state->pos);
        state->pos = pos;

}
static int
editline_char(struct editline_state *state, unsigned char ch)
{
        unsigned char npos = state->pos;
        switch (ch) {
        case CTL('D'):
                if (state->pos == 0 && state->len == 0)
                        return EL_EXIT;
                if (state->pos >= state->len)
                        return EL_NOTHING;
                delete(state, state->pos, 1);
                print_rest(state);
                return EL_NOTHING;
        case CTL('L'):
                editline_redraw(state);
                return EL_REDRAW;
        case 0177:
        case CTL('H'):
                if (!state->pos)
                        return EL_NOTHING;
                editline_putchar('\b');
                delete(state, state->pos - 1, 1);
                state->pos--;
                print_rest(state);
                return EL_NOTHING;
        case CTL('A'):
                move_cursor_to(state, 0);
                break;
        case CTL('E'):
                move_cursor_to(state, state->len);
                break;
        case CTL('F'):
                move_cursor_to(state, state->pos + 1);
                break;
        case CTL('B'):
                move_cursor_to(state, state->pos - 1);
                break;
        case META('f'):
                while(npos < state->len && state->buf[npos] != ' ')
                        npos++;
                while(npos < state->len && state->buf[npos] == ' ')
                        npos++;
                move_cursor_to(state, npos);
                break;
        case META('b'):
                while(npos > 0 && state->buf[npos] != ' ')
                        npos--;
                while(npos > 0 && state->buf[npos] == ' ')
                        npos--;
                while(npos > 0 && state->buf[npos - 1] != ' ')
                        npos--;
                move_cursor_to(state, npos);
                break;
        case CTL('K'):
                state->len = state->pos;
                csi('K');
                break;
        case CTL('P'):
        case CTL('N'): {
                int8_t inc = ch == CTL('P') ? -1 : 1;
                for (uint8_t i = state->hcur + inc; state->hist[i % HISTSIZE] != 4; i += inc) {
                        if (state->hist[i % HISTSIZE] == 1) {
                                state->hcur = i++ % HISTSIZE;
                                int j = 0;
                                while (state->hist[i % HISTSIZE] >= 0x20)
                                        state->buf[j++] = state->hist[i++ % HISTSIZE];
                                state->pos = 0;
                                state->len = j;
                                redraw_current_command(state);
                                break;
                        }
                }
        }
        break;
        // debug
#ifdef EDITLINE_DEBUG
        case CTL('V'):
                putchar('\r');
                putchar('\n');
                printf("pos: %i hend: %i hcur: %i len: %i\r\n", state->pos, state->hend, state->hcur, state->len);
                for (int i = 0; i < BUFSIZE; i++)
                        char_show(state->buf[i]);
                putchar('\r');
                putchar('\n');
                putchar('\n');
                for (int i = 0; i < HISTSIZE; i++)
                        char_show(state->hist[i]);
                putchar('\r');
                putchar('\n');
                redraw_current_command(state);
                break;
#endif
        case CTL('U'):
                delete(state, 0, state->pos);
                move_cursor_to(state, 0);
                print_rest(state);
                break;
        case CTL('Q'):
        case '\r':
        case '\n':
                if (!state->len)
                        return EL_NOTHING;
                editline_buf(state)[state->len] = 0;
                state->pos = 0;
                state->len = 0;
                putchar('\r');
                putchar('\n');
                add_history(state, editline_buf(state));
                return ch == CTL('Q') ? EL_QDATA : EL_DATA;
        default:
                if (state->len >= BUFSIZE - 1)
                        return EL_NOTHING;
                state->len++;
                memmove(editline_buf(state) + state->pos + 1,  editline_buf(state) + state->pos, state->len - state->pos - 1);
                editline_buf(state)[state->pos++] = ch;
                putchar(ch);
                print_rest(state);
        }
        return EL_NOTHING;
}

void add_history(struct editline_state *s, char *data)
{
        if (!data[0])
                return;
        /* find the end of history */
        uint8_t end = 0;
        for (; end < HISTSIZE; end++)
                if (s->hist[end] == '\x04')
                        break;
        s->hist[end++ % HISTSIZE] = 1;
        for (int i = 0; data[i]; i++, end++)
                s->hist[end  % HISTSIZE] = data[i];
        s->hcur = end % HISTSIZE;
        s->hist[end++ % HISTSIZE] = '\x04';
}

#ifdef EDITLINE_DEBUG
void
char_show(char c)
{
        if (ISCTL(c)) {
                editline_putchar('^');
                char_show(UNCTL(c));
        } else if (ISMETA(c)) {
                editline_putchar('M');
                editline_putchar('-');
                char_show(UNMETA(c));
        } else if (!isprint(c)) {
                printf("\\x%02x", (unsigned char)c);
        } else
                editline_putchar(c);
}
#endif


