#define _GNU_SOURCE 1

#include "editline.h"

#if !ENABLE_DEBUG
#define NDEBUG
#endif

#include <string.h>
#include <ctype.h>
#include <assert.h>


static int editline_char(struct editline *state, char ch);
typedef uint8_t bufptr_t;

static char *buf(struct editline *s)
{
#if ENABLE_HISTORY
        return s->buf + s->hcur;
#else
        return s->buf;
#endif
}

/* terminal commands */

static void putchar2(char x, char y)
{
        user_putchar(x);
        user_putchar(y);
}


static void putnum(unsigned short n)
{
        char buf[12], *s = buf;
        do {
                *s++ = n % 10 + '0';
        } while ((n /= 10) > 0);
        while (s-- != buf)
                user_putchar(*s);
}


static void csi_n(int num, char ch)
{
        putchar2('\033', '[');
        putnum(num);
        user_putchar(ch);
}

/* static void csi_nn(int x, int y,  char ch) { */
/*         csi_n(x,';'); */
/*         putnum(y); */
/*         user_putchar(ch); */
/* } */

static void csi(char ch)
{
        putchar2('\033', '[');
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
print_from(struct editline *state, bufptr_t pos)
{
        show_cursor(false);
        bufptr_t cpos = pos;
        for (; state->buf[cpos]; cpos++)
                user_putchar(state->buf[cpos]);
        csi('K');
        move_cursor(- (cpos - pos));
        show_cursor(true);
}

static void
redraw_current_command(struct editline *state)
{
        show_cursor(false);
        user_putchar('\r');
        csi_n(92, 'm');
        user_putchar(PROMPT);
        csi('m');
        print_from(state, state->hcur);
        move_cursor(state->pos);
}

/* assume cursor is at state->pos and we are on hcur */
static void
print_rest(struct editline *state)
{
        assert(!state->hcur);
        print_from(state, state->pos);
}

void editline_redraw(struct editline *state)
{
        csi_n(2, 'J');
        redraw_current_command(state);
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

/* saturating */
static bufptr_t nhistory(struct editline *s, bufptr_t chp, int n)
{
        char *ch = s->buf + chp;
        if (n > 0) {
                char *limit = s->buf + BUFSIZE;
                char *cz = memchr(ch, 0, limit - ch);
                assert(cz);
                assert(cz < limit);
                for (; n > 0; n--) {
                        if (cz + 1 >= limit || !cz[1] || cz[1] == '\177')
                                break;
                        char *nz = memchr(cz + 1, 0, limit - (cz + 1));
                        if (!nz)
                                break;
                        ch = cz + 1;
                        cz = nz;
                }
        } else {
                for (; n < 0; n++) {
                        assert(ch >= s->buf);
                        if (ch == s->buf)
                                break;
                        char *nz = memrchr(s->buf, 0, ch - s->buf - 1);
                        if (!nz)
                                ch = s->buf;
                        else
                                ch = nz + 1;
                }
        }
        return ch - s->buf;
}

/*
 * Return items from history or NULL if no entry exists. Slot zero always has the
 * current command being edited.
 */

char *editline_history(struct editline *s, int n)
{
        return s->buf + nhistory(s, 0, n);
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

static void raw_insert(struct editline *s, int pos, int len)
{
        memmove(s->buf + pos + len, s->buf + pos, BUFSIZE - (pos + len));
        memset(s->buf + pos, ' ', len);
}

static void raw_delete(struct editline *s, int pos, int len)
{
        memmove(s->buf + pos, s->buf + pos + len, BUFSIZE - (pos + len));
        memset(s->buf + BUFSIZE - (pos + len), '\177', pos + len);
}

static void realize_history(struct editline *state, bool always_promote)
{
        if (!state->hcur)
                return;
        int cl = strlen(state->buf) + 1;
        raw_delete(state, 0, cl);
        state->hcur -= cl;
        if (!state->hcur && always_promote)
                return;
        int hl = strlen(state->buf + state->hcur) + 1;
        if (always_promote || state->hcur + 2 * hl  >= BUFSIZE) {
                //memswap(state->buf, cl + 1, state->hcur - (cl + 1), hl + 1);
                memswap(state->buf, 0, state->hcur, hl);
        } else {
                raw_insert(state, 0, hl);
                memcpy(state->buf, state->buf + state->hcur + hl, hl);
        }
        state->hcur = 0;
        assert(state->len == hl - 1);
}

static void delete_chars(struct editline *state, int pos, int len)
{
        realize_history(state, false);
        assert(pos >= 0);
        assert(pos + len <= state->len);
        assert(pos + len < BUFSIZE);
        /* if (pos < 0) { */
        /*         len += pos; */
        /*         pos = 0; */
        /* } */
        /* if (pos + len > (int)state->len) */
        /*         len = (int)state->len - pos; */
        if (len <= 0)
                return;
        raw_delete(state, pos, len);
        state->len -= len;
}


/* pads with spaces, pos may be greater than len */
static bool insert_chars(struct editline *state, int pos, int len)
{
        realize_history(state, false);
        if (len + state->len >= BUFSIZE - 1)
                return false;
        raw_insert(state, pos, len);
        state->len += len;
        return true;
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

/* look for word boundries, these take absolute positions in the buffer. */
static bool is_bow(struct editline *s, int p)
{
        assert(p >= 0);
        char a = p ? s->buf[p - 1] : 0, b = s->buf[p];
        return !a || (a == ' ' && b && b != a);
}
static bool is_eow(struct editline *s, int p)
{
        assert(p >= 0);
        char a = p ? s->buf[p - 1] : 0, b = s->buf[p];
        return !b || (a && a != b && b == ' ');
}


/* these take local position */
static uint8_t search_eow(struct editline *state, int npos, bool proper)
{
        assert(npos >= 0);
        if (!buf(state)[npos])
                return npos;
        npos += proper;
        while (!is_eow(state, state->hcur + npos))
                npos++;
        return npos;
}

static uint8_t search_bow(struct editline *state, int npos, bool proper)
{
        assert(npos >= 0);
        if (npos == 0)
                return 0;
        npos -= proper;
        while (!is_bow(state, state->hcur + npos))
                npos--;
        return npos;
}

static void
clear_head(struct editline *state)
{
        raw_delete(state, 0, strlen(state->buf));
        state->len = state->pos = state->hcur = 0;
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
                print_rest(state);
//                print_from(state, state->pos);
                break;
        case CTL('F'): move_cursor_to(state, npos + 1); break;
        case CTL('B'): move_cursor_to(state, npos - 1); break;
        case CTL('A'): move_cursor_to(state, 0); break;
        case CTL('E'): move_cursor_to(state, state->len); break;
#if ENABLE_WORDS
        case META('f'): move_cursor_to(state, search_eow(state, npos, true)); break;
        case META('b'): move_cursor_to(state, search_bow(state, npos, true)); break;
        case META('a'): move_cursor_to(state, search_bow(state, npos, false)); break;
        case META('e'): move_cursor_to(state, search_eow(state, npos, false)); break;
        case META('d'):
                npos = search_eow(state, npos, true);
                delete_chars(state, state->pos, npos - state->pos);
                print_rest(state);
                break;
        case META('u'):
        case META('c'):
        case META('l'):
                realize_history(state, false);
                npos = search_eow(state, npos, true);
                int i = state->pos;
                while (state->buf[i] == ' ')
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
                realize_history(state, false);
                /* find boundries of the two words we are going to swap */
                int eos = search_eow(state, npos, true);
                int bos = search_bow(state, eos, false);
                int bof = search_bow(state, bos, true);
                int eof = search_eow(state, bof, false);
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
                npos = search_bow(state, npos, true);
                delete_chars(state, npos, state->pos - npos);
                move_cursor_to(state, npos);
                print_rest(state);
                break;
#endif
        case CTL('T'): {
                realize_history(state, false);
                if (!state->buf[npos])
                        npos--;
                if (npos < 1)
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
                delete_chars(state, state->pos, state->len - state->pos);
                assert(state->len == state->pos);
//               state->len = state->pos;
                csi('K');
                break;
#if ENABLE_HISTORY
        case CTL('P'):
        case CTL('N'):
                state->hcur = nhistory(state, state->hcur, ch == CTL('P') ? 1 : -1);
                state->len = strlen(buf(state));
                state->pos = state->len;
                redraw_current_command(state);
                break;
#endif
#if ENABLE_DEBUG
        case CTL('V'):
                putchar2('\r', '\n');
                csi_n(2, 'm');
                putchar2('p', ':');
                putnum(state->pos);
                putchar2('h', ':');
                putnum(state->hcur);
                putchar2('l', ':');
                putnum(state->len);
                putchar2('\r', '\n');
                csi('m');
                for (int i = 0; i < BUFSIZE; i++)
                        debug_color_char(state->buf[i]);
                putchar2('\r', '\n');
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
                clear_head(state);
                redraw_current_command(state);
                break;
        case META('v'):;
                char ins[] = "hello";
                if (insert_chars(state, state->pos, sizeof(ins) - 1)) {
                        memcpy(state->buf + state->pos, ins, sizeof(ins) - 1);
                        print_rest(state);
                        move_cursor_to(state, state->pos + sizeof(ins) - 1);
                }
                break;
        case '\r':
        case '\n':
                putchar2('\r', '\n');
                realize_history(state, true);
                return  EL_COMMAND;
        default:
                if (ISMETA(ch) || ISCTL(ch))
                        return EL_UNKNOWN;
                if (insert_chars(state, state->pos, 1))  {
                        state->buf[state->pos++] = ch;
                        user_putchar(ch);
                        print_rest(state);
                }
        }
        return EL_NOTHING;
}

void editline_command_complete(struct editline *state, bool add_to_history)
{
        if (!add_to_history)
                clear_head(state);
        if (state->buf[0]) {
                raw_insert(state, 0, 1);
                state->buf[0] = 0;
                state->pos = state->len = 0;
        }
        assert(!state->hcur);
        redraw_current_command(state);
}

void
debug_color_char(char c)
{
        if (ISMETA(c)) {
                csi_n(7, 'm');
                c = UNMETA(c);
        }
        if (ISCTL(c) || c == '\177') {
                csi_n(94, 'm');
                c ^= 64;
        }
        user_putchar(c);
        csi('m');
}

