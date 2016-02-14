CONTRIBUTING
============
## Submitting changes
Please send a GitHub Pull Request with a clear list of what you've done (read more about [pull requests](http://help.github.com/pull-requests/)). 

## Features
If you have an idea for new features just [open an issue](https://github.com/vurtun/zahnrad/issues) with your suggestion.
  * Find and correct spelling mistakes
  * Add (insert your favorite platform or render backend here) demo implementation (some possibilities: DirectX 9/DirectX 10/DirectX 11 and win32 with OpenGL)
  * Add clipboard user callbacks back into all demos
  * Add additional widgets [some possible widgets](http://doc.qt.io/qt-5/widget-classes.html#the-widget-classes)
  * Add support for multiple pointers for touch input devices (probably requires to rewrite mouse handling in `struct zr_input`)
  * Extend xlib demo to support image drawing with arbitrary image width and height
  * Change cursor in `zr_widget_edit_box` and `zr_widget_edit_field` to thin standard cursor version used in editors
  * Extend piemenu to support submenus (another ring around the first ring or something like [this:](http://gdj.gdj.netdna-cdn.com/wp-content/uploads/2013/02/ui+concepts+13.gif)) and turn it into a default library widget.
  * Add label describing the currently active piemenu entry
  * Maybe write a piemenu text only version for platforms that do not want or can use images 
  * Rewrite the chart API to support a better range of charts (maybe take notes from Javascript chart frameworks) 
  * Create an API to allow scaling between groups (maybe extend and convert the demo example)
  * Add multiple Tab support (maybe use `zr_group` and add a header)  
  * Come up with a better way to provide and create widget and window styles
  * Add tables with scaleable column width
  * Extend context to not only support overlapping windows but tiled windows as well

## Bugs
  * Seperator widget is currently bugged and does not work as intended
  * Text handling is still a little bit janky and probably needs to be further tested and polished
  * `zr_edit_buffer` with multiline flag is bugged for '\n', need to differentiate between visible and non-visible characters

## Coding conventions                         
  * Only use C89 (ANSI C)
  * Do not use any compiler specific extensions
  * For indent use four spaces
  * Do not typedef structs, unions and enums
  * Variable, object and function names should always be lowercase and use underscores instead of camel case
  * Whitespace after for, while, if, do and switch
  * Always use parentheses if you use the sizeof operator (e.g: sizeof(struct zr_context) and not sizeof struct zr_context)
  * Beginning braces on the new line for functions and on the same line otherwise.
  * If function becomes to big either a.) create a subblock inside the function and comment or b.) write a functional function 
  * Only use fixed size types (zr_uint, zr_size, ...) if you really need to and use basic types otherwise
  * Do not include any header files in either zahnrad.h or zahnrad.c
  * Do not add dependencies rather write your own version if possible
  * Write correct commit messages: (http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html) 
