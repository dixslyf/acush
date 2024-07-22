#import "/docs/constants.typ": *

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
  Lexing is performed in two stages (see @sec-lexing).

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

== Input Handling

The code for input handling is found in `src/input.h` and `src/input.c`.

Most terminals and terminal emulators default to _canonical mode_,
in which input is line-buffered, editable and only sent to the program after the user enters a newline character.
However, to implement command history navigation with the `Up` and `Down` arrow keys,
the shell program must be able to access the character sequences for `Up` and `Down` immediately.
Hence, the terminal needs to be put into _non-canonical mode_,
in which said features are disabled
— in particular, input is made available to the program immediately
after the user enters a key or combination of keys.

When gathering input, the shell program puts the terminal into non-canonical mode
by disabling the `ICANON` flag using the `tcsetattr` function
from `termios.h`.
The shell also disables the `ECHO` flag
to prevent the terminal from automatically
displaying user input characters
so that the character sequence for `Up` arrow, `Down` arrow and
other keys or key combinations are not printed.
Prior to calling `tcsetattr`,
the attributes of the terminal are saved using `tcgetattr`
— these attributes are restored after input handling.

Although the shell only requires
disabling line-buffering and automatic echoing,
non-canonical mode also disables useful features like line-editing.
Unfortunately, there does not appear to be a portable method
of disabling only line-buffering.
Ergo, the shell program re-implements
some line-editing features, such as deleting characters with backspace.
Re-implementing such features is complex and typically handled using external libraries like GNU~Readline.
However, since us students are, sadly, not allowed to use external libraries,
the shell program re-implements them from scratch.

To implement command history navigation,
the shell must be able to detect the `Up` and `Down` arrow keys.
The terminal sends the `Up` and `Down` arrow keys to the program as ANSI CSI (control sequence introducer) escape code sequences,
which begin with the prefix `\e[` (`ESC` followed by `[`).
The `Up` and `Down` arrow keys are represented as `\e[A` and `\e[B` respectively.
The shell detects these CSI escape sequences
and uses the command history stored in the shell context data structure
to select a previous or next command.

However, printing the selected command in the history
to the terminal is not straightforward either.
The shell program first sends backspaces (`\b`) and other terminal control characters
to erase the current line of input from the terminal display.
Only after the current line of input is erased
is the selected command printed to standard output
to display it to the user.

== Lexing <sec-lexing>

The code for lexing is found in `src/raw_lex.h`, `src/raw_lex.c`, `src/lex.h` and `src/lex.c`.

The purpose of lexing is turn a string into a sequence of lexical tokens.
Tokens are units of text that are meaningful to the language.
In the shell's case, the language is a command line
whose grammar is specified by the assignment instructions.
Examples of tokens for a shell may include:
words~(quoted or unquoted strings), ampersands~(`&`), pipes~(`|`) and semicolons~(`;`).
However, it is important to note that what constitutes a token is up to the implementation.
For example, an implementor may decide to have quotes (`'` and `"`) as tokens and parse strings at a later stage;
a different implementor, however,
may decide to have quoted strings as tokens,
such that string parsing is performed at the lexical tokenisation stage.

In the case of our shell,
we split lexing into two stages.
The first stage is termed _raw_ lexing (code in `src/raw_lex.h` and `src/raw_lex.c`)
and performs a low-level lex pass over the input.
At this stage,
no string parsing, glob expansion, handling of escape sequences or removal of whitespace is performed.
That is, the result of the raw lex is _lossless_
— it is possible to rebuild the original string input
from the resulting tokens.
Instead of using the `strtok()` family of functions,
raw lexing iterates over each character one by one.
This provides much more flexibility and allows us
to lex losslessly without any loss in performance
(since `strtok()` iterates over the characters internally anyway).

The second stage (code in `src/lex.h` and `src/lex.c`)
refines the tokens from raw lexing
by performing string parsing, expanding wildcards,
handling escapes sequences and removing whitespace.
Thus, the resulting tokens are at a higher level
and more easily parsed by the parser.
Unfortunately, as the second-stage lexer has more complex tasks
to perform, its implementation is also more complicated.
For example, it has to perform I/O to expand globs
and keep track of what token it is currently trying to lex.
To handle this complexity, the second-stage lexer is implemented
as a finite state machine,
with its current state dependent on
what token type it is currently trying to lex.

This two-stage approach was chosen for several reasons:

- _Maintainability and Flexibility_:
  Using two stages is more flexible.
  If the grammar changes or new tokens need to be added,
  adjustments can be made to the appropriate stage without overhauling the entire lexer.
  Furthermore, debugging and testing is made much easier.

- _Rebuilding of the original input_:
  The original input can be recreated using the tokens from raw lexing.
  This can be useful for more robust and detailed error reporting in future.

- _Separation of concerns_:
  The raw lexing focuses on basic tokenisation without context.
  The second stage of lexing focuses on more complex tasks
  that require context or specific rules.
  This separation of concerns makes the codebase more modular and easier to maintain.

- _Simplifies the parser_:
  The token stream the parser sees is in a clean and high-level form.
  Hence, the parser does not need to handle complex tasks like string parsing and glob expansion,
  and can instead focus on handling the grammar of the shell.
  This reduces the complexity of the parser significantly.

Nonetheless, our approach has downsides:

- The second-stage lexer is complex and has, arguably, too many responsibilities.
  Instead of letting it handle escape sequences, string parsing, glob expansion and whitespace removal all at once,
  its complexity can be reduced by splitting it into separate components.
  That is, we could have used more than two stages of lexing
  to handle those tasks separately using a pipeline architecture.

- Since lexing consists of multiple stages,
  there is likely a small performance hit.
  However, we believe this is a tradeoff worth making —
  we sacrifice a bit of performance for better structure and readability.

- The tokens from the raw lexer do not keep track of their indices
  into the original input string.
  If we want to use the raw tokens for better error reporting in future,
  we would need to keep track of their indices or spans within the original input string.

== Parsing

The code for parsing is found in `src/parse.h` and `src/parse.c`.

The purpose of parsing is to transform the sequence of tokens from lexing
into an abstract syntax tree~(AST).
An AST is a tree representation of the input text
to facilitate subsequent operations.
For the shell, the types of nodes for the AST are based on the grammar provided
by the assignment instructions, with additional information
to facilitate the execution of the AST.
For convenience, the provided grammar is repeated below:

#align(center,
```
  <command line> ::= <job>
                   | <job> '&'
                   | <job> '&' <command line>
                   | <job> ';'
                   | <job> ';' <command line>
                   | '!' <string>
           <job> ::= <command>
                   | <job> '|' <command>
       <command> ::= <simple command>
                   | <simple command> '<' <pathname>
                   | <simple command> '>' <pathname>
                   | <simple command> '2>' <pathname>
<simple command> ::= <pathname>
                   | <simple command> <token>
```
)

As an example, the following is a textual representation of the AST
for the string `"echo hello > out & ls src | grep shell.c ; ls directory-that-does-not-exist 2 > error-out ; cat < error-out"`:

#align(center,
```
ROOT
  COMMAND_LINE
    job count: 4
    BACKGROUND JOB
      command count: 1
      COMMAND
        SIMPLE COMMAND
          argc: 2
          argv: echo hello
        redirect type: 0
        redirect file: out
    FOREGROUND JOB
      command count: 2
      COMMAND
        SIMPLE COMMAND
          argc: 2
          argv: ls src
      COMMAND
        SIMPLE COMMAND
          argc: 2
          argv: grep shell.c
    FOREGROUND JOB
      command count: 1
      COMMAND
        SIMPLE COMMAND
          argc: 2
          argv: ls directory-that-does-not-exist
        redirect type: 2
        redirect file: error-out
    FOREGROUND JOB
      command count: 1
      COMMAND
        SIMPLE COMMAND
          argc: 1
          argv: cat
        redirect type: 1
        redirect file: error-out
```
)

There are many different types of parsers and ways to implement them.
Many pieces of software use tools, such as Flex and Bison, to generate their lexers and parsers.
Of course, since we cannot use these tools for this assignment,
the parser (and lexer) are handwritten.
In this case, the parser was implemented as a _recursive descent parser_.
This choice was made for the following reasons:

- _Top-down parsing_:
  Recursive descent parsers parse the input from the root node
  and work recursively downwards to the leaf nodes.
  Top-down parsing is often much more intuitive than bottom-up parsing.

- _Direct representation of the grammar_:
  The structure of a recursive descent parser
  mirrors the grammar rules directly,
  making it easier to see how the parser relates to the grammar.
  In particular, each non-terminal in the grammar is implemented as a separate function in the parser's code.
  This makes the parser easy to read and write by hand.

- _Suitable for LL($k$) grammars_:
  LL($k$) grammars are those that can be parsed by an LL($k$) parser,
  which scans the input from *l*\eft to right and uses *l*\eftmost derivation
  with at most $k$ lookahead tokens to decide which production rule to apply.
  The given grammar is an LL($k$) grammar,
  which makes a recursive descent parser suitable.

Each parse function builds an AST node for its corresponding non-terminal symbol
by calling the parse functions for lower-level nodes.
A higher-level parse function uses the node to build its own higher-level node.
For example, the `parse_job()` function calls `parse_cmd()`
to build command nodes,
which become the children of a job node.

The highest level node is a _root_ node.
While we could have used a command line node as the highest level node type,
using a separate root node type allows us to represent
an empty input from the user,
which would have been invalid for a command line node
according to the grammar.

One major disadvantage of using a recursive descent parser
is that it is limited to LL($k$) grammars.
If more shell constructs and features need to be implemented in future
(e.g., subshells, variable expansion, conditionals),
there is no guarantee that the grammar will remain LL($k$)
or even context-free
— there would need to be a rewrite of the parser in such a scenario.
Furthermore, recursive descent parsers may not be as efficient as
other types of parsers (e.g., LR parsers),
especially if the grammar changes to require more complex lookahead or backtracking.

== Running Commands

The code for running commands can be found in `src/run.h` and `src/run.c`.
Implementations of built-in commands can be found in `src/builtins.h` and `src/builtins.c`.

Once the AST has been built for a command line input,
it is passed to the `run_ast()` function.
To execute the AST, the function traverses it top-down in a pre-order fashion.
Each AST node type has a corresponding run function that performs the necessary steps to realise and execute the node.
This includes calling the corresponding run function for its immediate child nodes,
similar to how the recursive descent parser works.
For example, `run_job_descs()` creates the necessary pipes for commands in the job (among other tasks)
before delegating the execution of each individual command
to the `run_cmd()` function.

The exception is the simple command node type
--- built-in commands and external commands are handled differently
even though they are both represented as simple command nodes
(the parser should not and does not have the responsibility of distinguishing between
built-in commands and external commands).
In particular, external commands are always executed in a separate child process
whereas built-in commands are executed within the shell process
unless they are part of a background job,
in which case they execute in a child process.
This behaviour follows what most shells, such as Bash, seem to do.

An important implication of executing foreground built-ins within the shell process
is that each function implementing a built-in must accept file descriptors
for the standard output, input and error streams
as parameters
for handling piping and redirection.
Outputs from and inputs to the built-ins
through redirected or piped standard streams
are achieved by directly reading from or writing to the file descriptors
(e.g., using `dprintf()`).
The reason for this approach is that it is inappropriate to redirect
the standard streams for the shell process since outputs from and inputs to the shell
should be independent of those for built-in commands.

While closing the file descriptor of an unused end of a pipe in a child process is straightforward,
closing the file descriptors for pipes in the shell process is tricky.
The shell process does not need to use the pipes;
however, it must keep their file descriptors open long enough for the child processes to inherit them.
The approach taken is to only close the file descriptors of a pipe
after spawning the consumer command
(i.e., the command on the right of the pipe).
An alternative is to close the write end after spawning the producer,
then close the read end after spawning the consumer
--- the producer would inherit both ends of the pipe,
but the consumer would only inherit the read end.
This is an equally valid approach,
but the former approach was chosen
due to the lack of symmetry surrounding
the inheritance of the pipe file descriptors in this second approach.

Child processes spawned as part of the same job
are placed into the same process group,
whose group leader is the first process spawned for the job. 
This is done so that the process group can be set as the
foreground process group for the terminal
(using `tcsetpgrp()`)
to allow them to receive signals.
For example, the user can enter `CTRL-C` to
send the `SIGINT` signal to each process
in the job to cancel them
— a feature provided by most shells.

= Source Code Listing

#figure(
  caption: `Makefile`,
  display-code(read("Makefile"), lang: "Make")
)

#for file in (
  "shell.h",
  "shell.c",
  "input.h",
  "input.c",
  "raw_lex.h",
  "raw_lex.c",
  "lex.h",
  "lex.c",
  "builtins.h",
  "builtins.c",
  "parse.h",
  "parse.c",
  "run.h",
  "run.c",
) {
  file = "src/" + file
  figure(
    caption: raw(file),
    display-c(read(file))
  )
}
