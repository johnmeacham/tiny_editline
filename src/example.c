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
        do {
                editline_redraw(&elstate);
        }  while(!char_available());
        for(;;) {
                switch (got_char(&elstate, getchar())) {
                case EL_REDRAW:
                        //update_pinstate();
                        //update_pinread();
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
