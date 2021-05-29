#include <stdio.h>
#include "setup_stdio.h"

#ifdef __AVR__

#include <avr/power.h>

#define SERIAL_BUFFER_SIZE 32

static int cput(char, FILE *);
static int cget(FILE *);

static FILE IO = FDEV_SETUP_STREAM(cput, cget, _FDEV_SETUP_RW);

bool char_available(void) {
        return UCSR0A & _BV(RXC0);
}

static int
cget(FILE *f)
{
        loop_until_bit_is_set(UCSR0A, RXC0);
        return UDR0;
}

// standard debug stdio setup, talk on the serial port at 9600 8N1
void
setup_stdio(void)
{
        power_usart0_enable();
#define BAUD 9600
#include <util/setbaud.h>
        UBRR0H = UBRRH_VALUE;
        UBRR0L = UBRRL_VALUE;
        UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
        UCSR0B = _BV(RXEN0) | _BV(TXEN0);
        stdout = &IO;
        stderr = &IO;
        stdin = &IO;
}

void
setup_fast_stdio(void)
{
        power_usart0_enable();
#undef BAUD
#define BAUD 38400
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
        if(c == '\n') {
                loop_until_bit_is_set(UCSR0A, UDRE0);
                UDR0 = '\r';
        }
        loop_until_bit_is_set(UCSR0A, UDRE0);
        UDR0 = c;
        return 0;
}
#else
void setup_stdio(void) {};
void setup_fast_stdio(void) {};
bool char_available(void) { return true; }
#endif

