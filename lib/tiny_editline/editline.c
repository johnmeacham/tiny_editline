#include "editline.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#ifdef EDITLINE_DEBUG
static void char_show(char c);
#endif
static int editline_char(struct editline_state *state, unsigned char ch);

static void putnum(unsigned short n)
{
        char buf[12], *s = buf;
        do {
                *s++ = n % 10 + '0';
        } while ((n /= 10) > 0);
        while (s-- != buf)
                editline_putchar(*s);
}

static void putchar2(char x, char y)
{
        editline_putchar(x);
        editline_putchar(y);
}

static void csi(char ch)
{
        putchar2('\033', '[');
        editline_putchar(ch);
}

static void csi_n(int num, char ch)
{
        putchar2('\033', '[');
        putnum(num);
        editline_putchar(ch);
}

static void show_cursor(bool show)
{
        csi('?');
        putchar2('2', '5');
        editline_putchar(show ? 'h' : 'l');
}


static void move_cursor(int8_t n)
{
        if (n)
                csi_n(n > 0 ? n : -n, n > 0 ? 'C' : 'D');
}

static void redraw_current_command(struct editline_state *state)
{
        show_cursor(false);
        editline_putchar('\r');
        csi_n(999, 'H');
        for (unsigned char i = 0; i < state->len; i++)
                editline_putchar(editline_buf(state)[i]);
        csi('K');
        editline_putchar('\r');
        move_cursor(state->pos);
        show_cursor(true);
}

void editline_redraw(struct editline_state *state)
{
        csi_n(2, 'J');
        redraw_current_command(state);
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

/* should be called after redraw each time to ensure your status lines don't get
 * clobbered by scrolling */
void reserve_statuslines(struct editline_state *state, int n) {
        csi_n(n + 1, ';');
        putchar2('9', '9');
        putchar2('9', 'r');
}

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

static void delete_chars(struct editline_state *state, int pos, int len)
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
static void move_cursor_to(struct editline_state *state, int pos)
{
        if (pos < 0)
                pos = 0;
        if (pos > state->len)
                pos = state->len;
        move_cursor(pos - state->pos);
        state->pos = pos;
}

/* look for word boundries */
static bool is_bow(struct editline_state *s, int p)
{
        return p <= 0 || (p < s->len && s->buf[p - 1] == ' ' && s->buf[p] != ' ');
}
static bool is_eow(struct editline_state *s, int p)
{
        return p >= s->len || (p > 0 && s->buf[p - 1] != ' ' && s->buf[p] == ' ');
}


static uint8_t search_eow(struct editline_state *state, int npos)
{
        if (npos > state->len)
                return state->len;
        while (!is_eow(state, npos))
                npos++;
        return npos;
}

static uint8_t search_bow(struct editline_state *state, int npos)
{
        if (npos < 0)
                return 0;
        while (!is_bow(state, npos))
                npos--;
        return npos;
}


static int
editline_char(struct editline_state *state, unsigned char ch)
{
        state->key = ch;
        unsigned char npos = state->pos;
        switch (ch) {
        case CTL('L'):
                editline_redraw(state);
                return EL_REDRAW;
        case CTL('D'):
                if (state->pos == 0 && state->len == 0)
                        return EL_EXIT;
                delete_chars(state, state->pos, 1);
                print_rest(state);
                break;
        case 0x7f:
        case CTL('H'):
                if (!state->pos)
                        return EL_NOTHING;
                move_cursor_to(state, state->pos - 1);
                delete_chars(state, state->pos, 1);
                print_rest(state);
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
                delete_chars(state, 0, state->pos);
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
                putchar2('\r', '\n');
                add_history(state, editline_buf(state));
                return ch == CTL('Q') ? EL_QDATA : EL_DATA;
        default:
                if (ISMETA(ch) || ISCTL(ch))
                        return EL_UNKNOWN;
                if (state->len >= BUFSIZE - 1)
                        return EL_NOTHING;
                state->len++;
                memmove(editline_buf(state) + state->pos + 1,  editline_buf(state) + state->pos, state->len - state->pos - 1);
                editline_buf(state)[state->pos++] = ch;
                editline_putchar(ch);
                print_rest(state);
        }
        return EL_NOTHING;
}

void add_history(struct editline_state *s, char *data)
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
        } else if (c == '\177') {
                putchar2('\\', '1');
                putchar2('7', '7');
        } else
                editline_putchar(c);
}
#endif


