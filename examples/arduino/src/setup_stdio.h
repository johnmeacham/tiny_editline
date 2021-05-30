#ifndef SETUP_STDIO_H
#define SETUP_STDIO_H

#include <stdbool.h>


/* set up stdio at 9600 baud */
void setup_stdio(void);
/* set up stdio at 38400 baud */
void setup_fast_stdio(void);
/* check if character might be available, just a hint. */
bool char_available(void);



#endif /* end of include guard: SETUP_STDIO_H */
