# tiny_editline
Tiny editline library giving readline style command line editing and history to
embedded systems such as 8 bit MCUs.

This library provides line editing functionality, such as handling of arrow
keys, standard control commands and command line history. It uses a portable
subset of terminal codes and only 5 bytes of RAM in addition to the buffer.

It has no dependencies, not even malloc as it does not allocate anything on
the heap.

## Notable features

- Tiny, ram usage is 5 bytes + buffer size and stack use is constant. No
  heap is used and code is ~2k depending on options.
- Implements the word manipulation ALT- codes as well as control codes.
- Single packed buffer for history and working space that stores as much
  history and allows as long of commands as will fit in it.
- clever swapping around of history in place to further get the most out of
  ram, Running commands from history brings them to the front without
  evicting anything.
- Clean support for handling unrecognized keycodes for implemetning things like
  autocomplete.
- Supports statuslines, unchanging lines at the top of the screen that are
  not affected by scrolling, useful for apps  where you want a constant
  status display.

## Caveats
- Input line is always at the last line of the screen to avoid using non
  portable codes to determine screen size.
- No multiline processing.
- Autocomplete/hinting support is minimal.

## Using

implement 'user_putchar' that will output a character to your output device.
This might write to a USART or just call putchar if it is implemented.

call 'editline_process_char' for each character typed by the user. This
might be in a loop with getchar or triggered by an interrupt. Examine its
return value to determine whether a complete command is ready or take other
action.

After you process a command call 'editline_command_complete' along with a
flag saying whether you want to store it in the history buffer or discard
it.

## Supported editing commands

### Basic Editing

        ^F (Right arrow) - move cursor one character forward
        ^B (Left arrow)  - move cursor one character backwards
        ^A (Home)        - move cursor to begining of line
        ^E (End)         - move cursor to end of line
        ^D (Delete)      - delete character under cursor
        ^H (Backspace)   - delete character before cursor
        ^Q               - erase whole line
        ^K               - delete from cursor until end of line
        ^U               - delete from cursor until beginning of line
        ^L               - clear and redraw screen
        ^M (Enter)       - enter command
        ^T               - transpose characters, dragging character forward.

### History

        ^P (Up arrow)    - previous from history
        ^N (Down arrow)  - next from history

### Word manipulation

        ^W               - delete to beginning of current word
        ALT-Backspace    - delete to beginning of current word

        ALT-d            - delete to end of current word
        ALT-u            - uppercase word
        ALT-l            - lowercase word
        ALT-c            - capitalize word

        ALT-f            - move to next end of word
        ALT-b            - move to previous beginning of word

        ALT-a            - move to beginning of current word
        ALT-e            - move to end of current word

        ALT-t            - exchange previous with current word
