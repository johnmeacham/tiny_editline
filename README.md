# tiny_editline
Tiny editline library giving basic command line editing and history to
embedded systems such as 8 bit MCUs.

This library provides basic line editing functionality, such as handling of
arrow keys, standard control commands and command line history. It uses a
portable subset of terminal codes. It uses a tiny amount of code and RAM,
consisting of mainly the buffer.


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

        // todo
        ALT-t            - exchange previous with current word
        ALT-r            - undo changes


