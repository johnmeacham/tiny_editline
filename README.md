# tiny_editline
Tiny editline library giving basic command line editing and history to
embedded systems such as 8 bit MCUs.

This library provides basic line editing functionality, such as handling of
arrow keys, standard control commands and command line history. It uses a
portable subset of terminal codes. It uses a tiny amount of code and RAM,
consisting of mainly the buffer.

It has no dependencies, not even malloc as it does not allocate anything on
the heap.

## Notable features

- Tiny, uses 230 bytes of ram for the buffer and is about 1.8k of code
  in its smallest configuration and 2.7k in its largest.
- Implements the word manipulation ALT- codes as well as control codes.
- History is packed into the buffer one after another, so there isn't a set
  number of history entries.
- Supports statuslines, unchanging lines at the top of the screen that are
  not affected by scrolling, useful for apps  where you want a constant
  status display.

## Using

implement 'user_putchar' that will output a character to your output device.
This might write to a USART or just call putchar if it is implemented.

call 'editline_process_char' for each character typed by the user. This
might be in a loop with getchar or triggered by an interrupt. examine its
return value to determine whether a complete command is ready or you need to
refresh the screen.

After you process a command call 'editline_command_complete' along with a
flag saying whether you want to store it in the history buffer.

## Supported editing commands

        ^P (Up arrow)    - previous from history
        ^N (Down arrow)  - next from history
        ^F (Right arrow) - move cursor one character forward
        ^B (Left arrow)  - move cursor one character backwards
        ^A (Home)        - move cursor to begining of line
        ^E (End)         - move cursor to end of line
        ^D (Delete)      - delete character under cursor
        ^H (Backspace)   - delete character before cursor
        ^Q               - erase whole line
        ^K               - delete from cursor until end of line
        ^U               - delete from cursor until beginning of line
        ^L               - redraw screen
        ^M (Enter)       - enter command
        ^W               - delete to beginning of current word
        ALT-Backspace    - delete to beginning of current word
        ^T               - transpose characters, dragging character forward.

        ALT-d            - delete to end of current word
        ALT-u            - uppercase word
        ALT-l            - lowercase word
        ALT-c            - capitalize word

        ALT-f            - move to next end of word
        ALT-b            - move to previous beginning of word

        ALT-a            - move to beginning of current word
        ALT-e            - move to end of current word

        ALT-t            - exchange previous with current word
