#import "/constants.typ": *

// SETUP

#set page(
  paper: "a4",
  numbering: "1",
  number-align: right + top
)

#set heading(numbering: "1.")

#show heading.where(level: 1): it => [
  #set text(size: HEADING_SIZE, weight: "bold")
  #it
]

#show heading.where(level: 2): it => [
  #set text(size: SUBHEADING_SIZE, weight: "regular", style: "italic")
  #it
]

#show outline: it => {
  set par(leading: 1em)
  it
}

#set figure(placement: none)

#set block(above: 1.5em)

#set par(justify: true)

#set text(size: TEXT_SIZE, font: TEXT_FONT)

#set list(indent: LIST_INDENT)

#set terms(indent: LIST_INDENT)

#show math.equation: set text(size: 11pt)

#show link: it => {
  set text(fill: blue)
  underline(it)
}

#set list(indent: LIST_INDENT)
#set enum(indent: LIST_INDENT)

#show figure: set block(breakable: true)

#show table: it => {
  set par(justify: false)
  set text(size: 10pt)
  it
}

#set table(
  align: center + horizon,
  stroke: 0.8pt + black,
  inset: 8pt
)

#show table.cell.where(y: 0): it => {
  set text(weight: "bold")
  it
}

// CONTENT

#let date = datetime(
  year: 2024,
  month: 7,
  day: 27,
)

#align(center, [
  #text(size: TITLE_SIZE)[*ICT374 Assignment 2: Unix Shell Implementation in C*]

  #date.display("[day] [month repr:long], [year]")
  #grid(
    columns: (50%, 50%),
    text(size: SUBTITLE_SIZE)[
      Dixon Sean Low Yan Feng
    ],
    text(size: SUBTITLE_SIZE)[
      Khon Min Thite
    ],
  )
])

#outline(indent: auto)

= List of Files

- `Makefile`: Describes how to build the shell using `make`.

- `src/shell.h`, `src/shell.c`: Contains the `main` function and other code related to the high-level execution of the shell.

- `src/builtins.h`, `src/builtins.c`: Contains implementations for built-in commands (e.g., `pwd`, `cd`, `prompt`, etc.).

- `src/input.h`, `src/input.c`: Contains code for handling input from the user, including history navigation.

- `src/raw_lex.h`, `src/raw_lex.c`, `src/lex.h`, `src/lex.c`: Contains code for lexing a line of user input into a sequence of tokens.
Lexing is performed in two stages.

- `src/parse.h`, `src/parse.c`: Contains code for parsing the sequence of tokens from lexing into an abstract syntax tree (AST).

- `src/run.h`, `src/run.c`: Contains code for executing commands given their abstract syntax tree representation.

= Task Description

The task is to implement a Unix shell, similar to Bash, using the C programming language.
The shell must be able to handle at least 100 commands in a single command line
and at least 1000 arguments for each command.
Furthermore, the shell must satisfy the following requirements:

- `prompt` _built-in command_:
  The shell must provide a built-in command, `prompt`,
  to configure the shell's prompt.
  The prompt is set to the second argument of the command.
  For example, executing `prompt $` should alter the shell prompt to `$`.
  The default prompt is `%`.

- `pwd` _built-in command_:
  The shell must provide a built-in command, `pwd`,
  which, when executed, outputs the current working directory of the shell process.

- `cd` _built-in command_:
  The shell must provide a built-in command, `cd`, to navigate between directories.
  The working directory is set to the second argument if it represents an existing directory.
  When `cd` is executed without a second argument,
  the working directory is set to the user's home directory.

- _Handling of wildcard characters_:
  Tokens containing wildcard characters \* or \? must be treated as filenames requiring glob expansion.
  For instance, the command `ls *.c` may expand to `ls ex1.c ex2.c ex3.c`
  if the current working directory contains three matching files: `ex1.c`, `ex2.c` and `ex3.c`.

- _Redirection of standard input, output and error_:
  The shell must support redirection of standard input, output and error
  using `<`, `>` and `2>` respectively.
  For example:
  `ls > foo` should redirect the standard output of `ls` to the file `foo`, creating it if it does not exist and overwriting it if it does.
  `cat < foo` redirects the standard input of `cat` from the file `foo`.
  `ls directory-that-does-not-exist 2> error` redirects the standard error of `ls`
  when listing a non-existent directory to the file `error`.

- _Support for pipelines_:
  The shell must support pipelines using `|` syntax.
  For example, `ls | less` should connect the standard output of `ls` to
  the standard input of `less` via a pipe.

- _Background job execution_:
  The shell must support background job execution using `&` syntax.
  For example:
  `xterm &` starts the `xterm` command in the background.
  `sleep 20 & ps` starts `sleep 20` in the background and immediately executes `ps` without waiting for `sleep 20` to complete.

- _Sequential job execution_:
  The shell must support sequential job execution using `;` syntax.
  For example, `sleep 20; ps` executes `sleep 20` and, upon completion, executes `ps`.

- _Command history navigation_:
  The shell must maintain a history of entered commands
  and allow the user to navigate this history
  using the `Up` and `Down` arrow keys.

- `history` _built-in command_:
  The shell must provide a built-in command, `history`,
  to display the command historyas a numbered list.

- _History expansion_:
  The shell must support history expansion using `!` syntax.
  If `!` is followed by a positive integer,
  the command in the command history list (`history` built-in)
  corresponding to that number is re-executed.
  For example, `!12` repeats the twelfth command in the command history.
  If `!` is followed by a string `s`,
  the last command in the command history
  that begins with `s` is re-executed.
  For example, `!ec` repeats the last command in the command history beginning with `ec`.

- _Inheritance of shell environment_:
  The shell must inherit environment variables from its parent process.

- `exit` _built-in command_:
  The shell must provide a built-in command, `exit`, that terminates the shell.

- _Non-termination on `CTRL-C`, `CTRL-\` or `CTRL-Z`_:
  The shell must not be terminated when the user enters `CTRL-C`, `CTRL-\` or `CTRL-Z`.

= Solution Discussion

The main loop of the shell program consists of the following stages:

  1. _Input handling_: Receiving and handling user input for a command line.

  2. _Lexing_: Converting a command line string from the user into a sequence of tokens.

  3. _Parsing_: Converting the sequence of tokens from the lexer into an abstract syntax tree~(AST).

  4. _Running commands_: Executing the AST from the parser.

Furthermore, the shell program maintains a shell context data structure
that contains contextual information
for the shell,
such as the command history and prompt string.
In the following sections, we describe the main aspects of the implementation of each stage.

