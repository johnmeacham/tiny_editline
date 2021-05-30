#include "editline.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


static int editline_char(struct editline *state, char ch);

static void putnum(unsigned short n)
{
        char buf[12], *s = buf;
        do {
                *s++ = n % 10 + '0';
        } while ((n /= 10) > 0);
        while (s-- != buf)
                user_putchar(*s);
}

static void putchar2(char x, char y)
{
        user_putchar(x);
        user_putchar(y);
}

static void csi(char ch)
{
        putchar2('\033', '[');
        user_putchar(ch);
}

static void csi_n(int num, char ch)
{
        putchar2('\033', '[');
        putnum(num);
        user_putchar(ch);
}

static void show_cursor(bool show)
{
        csi('?');
        putchar2('2', '5');
        user_putchar(show ? 'h' : 'l');
}


static void move_cursor(int8_t n)
{
        if (n)
                csi_n(n > 0 ? n : -n, n > 0 ? 'C' : 'D');
}

static void
print_from(struct editline *state, int pos)
{
        show_cursor(false);
        for (unsigned char i = pos; i < state->len; i++)
                user_putchar(state->buf[i]);
        csi('K');
        move_cursor(- (state->len - state->pos));
        show_cursor(true);
}

static void redraw_current_command(struct editline *state)
{
        show_cursor(false);
        user_putchar('\r');
        csi_n(92, 'm');
        user_putchar(PROMPT);
        csi_n(0, 'm');
        print_from(state, 0);
}

void editline_redraw(struct editline *state)
{
        csi_n(2, 'J');
        redraw_current_command(state);
}

static void
print_rest(struct editline *state)
{
        print_from(state, state->pos);
}

/* reverse memory in place */
static void memrev(char *mem, size_t len)
{
        for (int i = 0; i < len >> 1; i++) {
                char tmp = mem[i];
                mem[i] = mem[len - i - 1];
                mem[len - i - 1] = tmp;
        }
}


/* generalization of rotation, swap the x characters with the z characters
 * separated by y characters. Turns X | Y | Z  into Z | Y | X. */
static void memswap(char *mem, size_t x, size_t y, size_t z)
{
        memrev(mem, x);
        memrev(mem + x, y);
        memrev(mem + x + y, z);
        memrev(mem, x + y + z);
}



void editline_hide_command(struct editline *s)
{
        user_putchar('\r');
        csi('K');
}
void editline_restore_command(struct editline *s)
{
        redraw_current_command(s);
}

/* should be called after redraw each time to ensure your status lines don't get
 * clobbered by scrolling */
void reserve_statuslines(struct editline *state, int n)
{
        csi_n(n + 1, ';');
        putchar2('9', '9');
        putchar2('9', 'r');
}

void begin_statusline(struct editline *state, int n)
{
        show_cursor(false);
        csi_n(n + 1, 'H');
        user_putchar('\r');
}

void end_statusline(struct editline *state)
{
        csi('K');
        csi_n(999, 'H');
        user_putchar('\r');
        move_cursor(state->pos + 1);
        show_cursor(true);
}

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
int editline_process_char(struct editline *s, char ch)
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
                /* unknown CSI sequence */
                return EL_NOTHING;
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
                /* unknown CSI sequence */
                return EL_NOTHING;
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

static void delete_chars(struct editline *state, int pos, int len)
{
        if (pos < 0) {
                len += pos;
                pos = 0;
        }
        if (pos + len > (int)state->len)
                len = (int)state->len - pos;
        if (len <= 0)
                return;
        memmove(state->buf + pos, state->buf + pos + len, state->len - (pos + len));
        state->len -= len;
}
static void move_cursor_to(struct editline *state, int pos)
{
        if (pos < 0)
                pos = 0;
        if (pos > state->len)
                pos = state->len;
        move_cursor(pos - state->pos);
        state->pos = pos;
}

/* look for word boundries */
static bool is_bow(struct editline *s, int p)
{
        return p <= 0 || (p < s->len && s->buf[p - 1] == ' ' && s->buf[p] != ' ');
}
static bool is_eow(struct editline *s, int p)
{
        return p >= s->len || (p > 0 && s->buf[p - 1] != ' ' && s->buf[p] == ' ');
}


static uint8_t search_eow(struct editline *state, int npos)
{
        if (npos > state->len)
                return state->len;
        while (!is_eow(state, npos))
                npos++;
        return npos;
}

static uint8_t search_bow(struct editline *state, int npos)
{
        if (npos < 0)
                return 0;
        while (!is_bow(state, npos))
                npos--;
        return npos;
}


static int
editline_char(struct editline *state, char ch)
{
        state->key = ch;
        unsigned char npos = state->pos;
        switch (ch) {
        case CTL('L'):
                editline_redraw(state);
                return EL_REDRAW;
        case 0x7f:
        case CTL('H'):
                if (!state->pos)
                        return EL_NOTHING;
                move_cursor_to(state, state->pos - 1);
        case CTL('D'):
                delete_chars(state, state->pos, 1);
                print_from(state, state->pos);
                break;
        case CTL('F'): move_cursor_to(state, npos + 1); break;
        case CTL('B'): move_cursor_to(state, npos - 1); break;
        case CTL('A'): move_cursor_to(state, 0); break;
        case CTL('E'): move_cursor_to(state, state->len); break;
#if ENABLE_WORDS
        case META('f'): move_cursor_to(state, search_eow(state, npos + 1)); break;
        case META('b'): move_cursor_to(state, search_bow(state, npos - 1)); break;
        case META('a'): move_cursor_to(state, search_bow(state, npos)); break;
        case META('e'): move_cursor_to(state, search_eow(state, npos)); break;
        case META('d'): ;
                npos = search_eow(state, npos + 1);
                delete_chars(state, state->pos, npos - state->pos);
                print_rest(state);
                break;
        case META('u'):
        case META('c'):
        case META('l'):
                npos = search_eow(state, npos + 1);
                int i = state->pos;
                while (i < state->len && state->buf[i] == ' ')
                        i++;
                if (ch == META('c') && i < npos) {
                        state->buf[i] = toupper(state->buf[i]);
                        i++;
                }
                if (ch == META('u'))
                        for (; i < npos; i++)
                                state->buf[i] = toupper(state->buf[i]);
                else
                        for (; i < npos; i++)
                                state->buf[i] = tolower(state->buf[i]);
                print_rest(state);
                move_cursor_to(state, npos);
                break;
        case META('t'): {
                /* find boundries of the two words we are going to swap */
                int eos = search_eow(state, npos + 1);
                int bos = search_bow(state, eos);
                int bof = search_bow(state, bos - 1);
                int eof = search_eow(state, bof);
                /* don't drag trailing whitespace with us */
                while (eos > 0 && state->buf[eos - 1] == ' ')
                        eos--;
                if (eof >= bos || bos >= eos)
                        break;
                int lof = eof - bof, low = bos - eof, los = eos - bos;
                memswap(state->buf + bof, lof, low, los);
                redraw_current_command(state);
                move_cursor_to(state, eos);
                break;
        }
        case META(CTL('H')):
        case META(0x7f):
        case CTL('W'):
                npos = search_bow(state, npos - 1);
                delete_chars(state, npos, state->pos - npos);
                move_cursor_to(state, npos);
                print_rest(state);
                break;
#endif
        case CTL('T'): {
                if (npos == state->len)
                        npos--;
                if (npos < 1 || state->len < 2)
                        break;
                npos--;
                char tmp = state->buf[npos];
                state->buf[npos] = state->buf[npos + 1];
                state->buf[npos + 1] = tmp;
                move_cursor_to(state, npos);
                print_rest(state);
                move_cursor_to(state, npos + 2);
                break;
        }
        case CTL('K'):
                state->len = state->pos;
                csi('K');
                break;
#if ENABLE_HISTORY
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
#endif
                // debug
#ifdef EDITLINE_DEBUG
        case CTL('V'):
                putchar('\r');
                putchar('\n');
                printf("pos: %i hend: %i hcur: %i len: %i\r\n", state->pos, state->hend, state->hcur, state->len);
                for (int i = 0; i < BUFSIZE; i++)
                        debug_char_show(state->buf[i]);
                putchar('\r');
                putchar('\n');
                putchar('\n');
                for (int i = 0; i < HISTSIZE; i++)
                        debug_char_show(state->hist[i]);
                putchar('\r');
                putchar('\n');
                redraw_current_command(state);
                break;
#endif
        case CTL('U'):
                delete_chars(state, 0, state->pos);
                move_cursor_to(state, 0);
                print_rest(state);
                break;
        case CTL('C'):
                putchar2('\r', '\n');
        case CTL('Q'):
                state->buf[state->len] = 0;
                state->pos = state->len = 0;
                redraw_current_command(state);
                break;
        case '\r':
        case '\n':
                putchar2('\r', '\n');
                state->buf[state->len] = 0;
                state->pos = state->len = 0;
                return  EL_COMMAND;
        default:
                if (ISMETA(ch) || ISCTL(ch))
                        return EL_UNKNOWN;
                if (state->len >= BUFSIZE - 1)
                        return EL_NOTHING;
                state->len++;
                memmove(state->buf + state->pos + 1,  state->buf + state->pos, state->len - state->pos - 1);
                state->buf[state->pos++] = ch;
                user_putchar(ch);
                print_rest(state);
        }
        return EL_NOTHING;
}

void editline_command_complete(struct editline *state, bool add_to_history)
{
        if (add_to_history)
                editline_add_history(state, state->buf);
        redraw_current_command(state);
}

void editline_add_history(struct editline *s, char *data)
{
#if ENABLE_HISTORY
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
#endif
}

/* show a printable version of char */
void
debug_char_show(char c)
{
        if (ISCTL(c)) {
                user_putchar('^');
                debug_char_show(UNCTL(c));
        } else if (ISMETA(c)) {
                user_putchar('M');
                user_putchar('-');
                debug_char_show(UNMETA(c));
        } else if (c == '\177') {
                putchar2('^', '?');
        } else
                user_putchar(c);
}


