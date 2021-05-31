#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "editline.h"
#include "setup_stdio.h"

struct editline elstate = EDITLINE_INIT;

void user_putchar(char ch)
{
        putchar((unsigned char)ch);
}

int main()
{
        setup_stdio();
        /* we loop here since we cannot tell if someone is listening on arduino */
        while (!char_available());
        /* we seed the char with ^L for a forced initial redraw. */
        for (int ch = CTL('L'); ch != EOF; ch = getchar()) {
                switch (editline_process_char(&elstate, ch)) {
                case EL_REDRAW:
                        reserve_statuslines(&elstate, 4);
                        begin_statusline(&elstate, 0);
                        puts("^F forward-char ^B back-char M-f forward-word M-b back-word");
                        begin_statusline(&elstate, 1);
                        puts("^P previous command ^N next command ^A BOL ^E EOL ^D Delete");
                        begin_statusline(&elstate, 2);
                        puts("^H backspace M-q to exit example.");
                        end_statusline(&elstate);
                        break;
                case EL_COMMAND:
                        if (elstate.buf[0]) {
                                putchar('<');
                                for (char *c = elstate.buf; *c; c++)
                                        putchar(*c);
                                putchar('>');
                                putchar('\n');
                        }
                        editline_command_complete(&elstate, !strchr(elstate.buf, 'x'));
                        break;
                case EL_UNKNOWN:
                        /* quit on meta-q */
                        if (elstate.key == META('q'))
                                return 0;
                        break;
                default:
                        break;
                }
                begin_statusline(&elstate, 3);
                debug_color_char(elstate.key);
                end_statusline(&elstate);
                fflush(stdout);
        }
        return 0;
}
