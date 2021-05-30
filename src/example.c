#include <stdio.h>
#include <stdbool.h>
#include "editline.h"
#include "setup_stdio.h"

struct editline_state elstate = EDITLINE_STATE_INIT;

void editline_putchar(char ch) {
        putchar((unsigned char)ch);
}

int main()
{
        setup_fast_stdio();

        /* we loop here since the terminal may not be connected right away.
         * you can explicitly redraw with ^L too */
        add_history(&elstate, "0123456789");
        add_history(&elstate, "hello this is a test of the emergency broadcast system.");

        while(!char_available());

        /* we seed the char with a forced redraw */
        for(char ch = CTL('L');;ch = getchar()) {
                switch (got_char(&elstate, ch)) {
                case EL_REDRAW:
                        reserve_statuslines(&elstate, 4);
                        begin_statusline(&elstate, 0);
                        puts("^F forward-char ^B back-char M-f forward-word M-b back-word");
                        puts("^P previous command ^N next command ^A BOL ^E EOL ^D Delete");
                        puts("^H backspace");
                        end_statusline(&elstate);
                        continue;
                case EL_DATA:
                        putchar('<');
                        fputs(elstate.buf, stdout);
                        putchar('>');
                        putchar('\n');
//                        printf("<%s>\n",elstate.buf);
                default:
                        continue;
                }
        }

        return 0;
}
