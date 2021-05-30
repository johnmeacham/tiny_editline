#include <stdio.h>
#include <stdbool.h>
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
        /* we loop here since the terminal may not be connected right away. */
        while (!char_available());
        /* we seed the char with ^L for a forced redraw */
        for (int ch = CTL('L'); ch != EOF; ch = getchar()) {
                switch (editline_process_char(&elstate, ch)) {
                case EL_REDRAW:
                        reserve_statuslines(&elstate, 4);
                        begin_statusline(&elstate, 0);
                        puts("^F forward-char ^B back-char M-f forward-word M-b back-word");
                        begin_statusline(&elstate, 1);
                        puts("^P previous command ^N next command ^A BOL ^E EOL ^D Delete");
                        begin_statusline(&elstate, 2);
                        puts("^H backspace");
                        end_statusline(&elstate);
                        continue;
                case EL_COMMAND:
                        if (elstate.buf[0]) {
                                putchar('<');
                                fputs(elstate.buf, stdout);
                                putchar('>');
                                putchar('\n');
                        }
                        editline_command_complete(&elstate, elstate.buf[0]);
                case EL_NOTHING:
                        /* quit on control d with empty line */
                        if (elstate.key == CTL('D') && !elstate.len && !elstate.pos)
                                return 0;
                default:
                        continue;
                }
        }
        return 0;
}
