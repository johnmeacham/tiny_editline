#include <stdio.h>
#include "setup_stdio.h"

#ifdef __AVR__

#include <avr/power.h>

#define SERIAL_BUFFER_SIZE 32

static int cput(char, FILE *);
static int cget(FILE *);

static FILE IO = FDEV_SETUP_STREAM(cput, cget, _FDEV_SETUP_RW);

bool char_available(void)
{
        return UCSR0A & _BV(RXC0);
}

static int
cget(FILE *f)
{
        loop_until_bit_is_set(UCSR0A, RXC0);
        return UDR0;
}


void
setup_stdio(void)
{
        power_usart0_enable();
#undef BAUD
#define BAUD 1200
#include <util/setbaud.h>
        UBRR0H = UBRRH_VALUE;
        UBRR0L = UBRRL_VALUE;
        UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
        UCSR0B = _BV(RXEN0) | _BV(TXEN0);
        stdout = &IO;
        stderr = &IO;
        stdin = &IO;
}

static int
cput(char c, FILE *f)
{
        if (c == '\n') {
                loop_until_bit_is_set(UCSR0A, UDRE0);
                UDR0 = '\r';
        }
        loop_until_bit_is_set(UCSR0A, UDRE0);
        UDR0 = c;
        return 0;
}
#else

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

static struct termios orig_termios;
void reset_stdio(void)
{
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        puts("\033[r\033[999;1H");
}


void setup_stdio(void)
{
        if (!isatty(STDIN_FILENO))
                return;
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
                return;
        struct termios raw = orig_termios;
        //raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
                return;
        atexit(reset_stdio);
}

bool char_available(void)
{
        return true;
}
#endif

