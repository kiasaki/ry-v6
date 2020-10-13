# ry

_Simple terminal text editor built and configured with lua_

## Description / History

Built in C and Lua, this simple editor is my editor. I've tried on many
occasions to build a minimal terminal text editor that meets my basic needs
... and failed to many times with all kinds of languages to get to a point
where I could switch to it full time.

Hopefully this time, I get it done and get it right. The goal is to suit my
text editing needs, now, if you have similar needs feel free to adopt this
fun little editor. If you wish for something, feel free to add it in.

Given that it's built in the same language it can be scripted/configured with
it should be easy for you to mold this editor into one that suits your needs
if you are used to the vim family of editors.

## Features

**Implemented**

- Normal mode movement
- Insert mode buffer  editing

**Comming soon**

- Opening and closing buffers
- Command mode
- Visual mode (+ visual line)
- Search mode
- Directory mode (minimode)
- Marks
- Auto-complete (based on tokens)
- Linting & Formatting

## Implemented Keybindings

- Normal mode
  - <kbd>i</kbd> Enter insert mode
  - <kbd>A</kbd> Go to the end of the line and enter insert mode
  - <kbd>o</kbd> Add a line below and enter insert mode
  - <kbd>esc</kbd> Cancel keys entered
  - <kbd>h</kbd> Move cursor left
  - <kbd>l</kbd> Move cursor right
  - <kbd>j</kbd> Move cursor down
  - <kbd>k</kbd> Move cursor up
  - <kbd>0</kbd> Move cursor to the beginning of the line
  - <kbd>$</kbd> Move cursor to the end of the line
  - <kbd>g g</kbd> Move cursor to the beginning of the file
  - <kbd>G</kbd> Move cursor to the end of the file
- Insert mode
  - <kbd>esc</kbd> Enter normal mode
  - <kbd>ctrl-c</kbd> Enter normal mode
  - <kbd>backspace</kbd> Deletes a character left of the cursor
  - <kbd>del</kbd> Deletes a character right of the cursor
  - <kbd>tab</kbd> Insert spaces
  - <kbd>enter</kbd> Inserts a new line
  - <kbd>any</kbd> Inserts the character you typed

## License

See `LICENSE` file.
