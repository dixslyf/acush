#import "/docs/constants.typ": *
#import "/docs/lib.typ": *

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
  to display the command history as a numbered list.

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

= Self-Diagnosis and Evaluation

Implemented features:

- The shell implements the `prompt` built-in command.
  The prompt is set to the second argument of `prompt`.
  If extraneous or no arguments are provided,
  an error message is printed to the standard error stream
  and the shell continues using the current prompt.

- The shell implements the `pwd` built-in command.
  When executed, the current working directory of the shell is printed
  to the standard output stream.
  If extraneous arguments are provided,
  an error message is printed to the standard error stream
  and the working directory is left unchanged.

- The shell implements the `cd` built-in command.
  If no arguments are provided,
  the working directory is set to the path specified
  in the `HOME` environment variable.
  If the `HOME` environment variable is not set,
  an error message is printed to the standard error stream
  and the working directory is left unchanged.

  If a single argument is provided,
  the working directory is set to the path specified by that argument
  if it is a valid directory path.
  If the argument is a `-`,
  the working directory is set to the path specified by `OLDPWD`.
  The path is then printed to the standard output stream.
  However, if `OLDPWD` is not set,
  an error message is printed to the standard error stream
  and the working directory is left unchanged.

  If the argument is not `-` or a valid directory path,
  or extraneous arguments are provided,
  an error message is printed to the standard error stream
  and the working directory is left unchanged.

  If the working directory is successfully changed,
  the `OLDPWD` environment variable is set to the
  working directory before the change,
  and the `PWD` environment variable is set to the
  new working directory.

- The shell correctly handles the expansion of wildcard characters
  relative to the current working directory.
  Specifically, `?` serves as a wildcard for a single character
  while `*` serves as a wildcard for any number of characters.

- The shell supports all three forms of redirection:
  `>` for standard output redirection,
  `<` for standard input redirection and
  `2>` for standard error redirection.
  The corresponding standard stream is redirected to the path
  represented by the word following `>`, `<` or `2>`.
  If multiple words are specified after `>`, `<` or `2>`,
  the shell treats the situation as a parse error
  and prints an error message to the standard error stream
  without executing anything.
  If multiple redirections are specified for the same standard stream
  (e.g., `ls > out1 > out2`),
  the shell follows Bash's behaviour,
  creating each file but only using the rightmost file for the final redirection
  — the file descriptors for the other files are closed as appropriate.

- The shell supports piping the standard output stream of one command
  to the standard input stream of another
  using the `|` symbol.
  Producer child processes close the file descriptor for the read end of the pipe
  while consumer child processes close the file descriptor for the write end of the pipe.
  The shell process only closes both ends of the pipe
  after both producer and consumer child processes have been spawned.

- The shell supports background job execution using `&` syntax.
  If a job is executed in the background,
  the shell does _not_ wait for the job to complete
  before prompting the user for the next command line.
  Background child processes are correctly consumed
  using the `waitpid()` function after they terminate
  to prevent zombies.
  Other jobs can be specified after the `&`.

- The shell supports sequential job execution using `;` syntax.
  The ending `;` can be omitted if the sequential job is the last in the command line.
  When a sequential job is executed,
  the shell waits for all child processes of the job
  to terminate before proceeding with the next job in the command line.
  Only after all sequential jobs in the command line complete
  does the shell prompt the user for the next command line.

- The shell supports command history navigation using the `Up` and `Down` arrow keys.
  If there is no previous command line or the current history item is the first,
  pressing the `Up` arrow does not do anything.
  Similarly, if there is no next command line in the history,
  pressing the `Down` arrow does not do anything.
  The shell also remembers the new command line that has not yet been
  added to the command history —
  this new command line can be navigated to seamlessly
  as though it was part of the command history.
  However, the shell only commits the new command line to the history
  after the user presses `Enter`.

- The shell implements the `history` built-in command.
  When executed, the command history is printed to the standard output stream
  as a numbered list starting from `1`.
  If extraneous arguments are provided,
  an error message is printed to the standard error stream.

- The shell supports history expansion using `!` syntax.
  If `!` is followed by an integer,
  the command in the command history list
  corresponding to that index is re-executed.
  The repeated command is appended to the command history list.
  If the index is out of range,
  an error message is printed to the standard error stream.

  If `!` is followed by a string,
  the shell searches the latest command in the command history list
  prefixed with the string and re-executes it.
  The repeated command is appended to the command history list.
  If no matching command could be found,
  an error message is printed to the standard error stream.

- The shell inherits the environment from its parent process.
  This can be verified by executing the `env` program to view the environment variables.

- The shell implements the `exit` built-in command.
  When `exit` is called without any arguments,
  the shell terminates with an exit code of `0`.
  If `exit` is called with a single integer argument,
  the shell terminates with the exit code specified by the argument.
  If the argument is out of range or not an integer,
  or extraneous arguments are provided,
  the shell prints an error message to the standard error stream
  _without_ terminating.

- The shell does not terminate if the user enters `CTRL-C`, `CTRL-\` or `CTRL-Z`.

- Slow system calls (e.g., `read`) are immediately restarted
  when a child process exits and sends a `SIGCHLD` signal to the shell process.

- Whitespace is not significant except for delimiting tokens of a command line.

Differences from Bash:

// TODO:

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

= Test Evidence

#let test-counter = counter("test-counter")
#let test-case(text) = [
  #test-counter.step()
  *Test case #test-counter.display("1:")* #text
]

#let test-case-image(path, width: 75%) = align(
  center,
  image("/docs/graphics/" + path, width: width)
)

== External Programs

#test-case[External program without full path and arguments]

This test case tests a common use case of the shell
— calling external programs without their full path and arguments.

#test-case-image("external-program-without-arguments.png")

Indeed, the shell executes `echo`, `ls` and `ps` without issue.

#test-case[External program without full path and with a small number of arguments]

Similar to the previous case, but with a small number of arguments.

#test-case-image("external-program-small-number-of-arguments.png")

The shell correctly passes the arguments to and executes the external programs.

#test-case[External program with full path and without arguments]

Similar to the previous cases, but the executable is specified by its full path.

#test-case-image("external-program-full-without-arguments.png")

Indeed, the shell executes `echo`, `ls` and `ps` without issue.

#test-case[External program with full path and a small number of arguments]

Similar to the previous cases, but the executable is specified by its full path.

#test-case-image("external-program-full-small-number-of-arguments.png")

The shell correctly passes the arguments to and executes the external programs.

#test-case[External program without full path and with a large number of arguments]

Similar to the previous cases, but with a small number of arguments.

#test-case-image("external-program-large-number-of-arguments.png")

The shell correctly passes the arguments to and executes the external programs.

#test-case[External program with full path and a large number of arguments]

#test-case-image("external-program-full-large-number-of-arguments.png")

#test-case[Long-running external program]

This test case tests that the shell waits for the child process to terminate before prompting for the next command line.

#test-case-image("long-running-external-program.png")

It is difficult to demonstrate with an image,
but the shell correctly waits for `sleep 3` to terminate
before it prompts the user for the next command.
In the screenshot, we can see that the shell is waiting for `sleep 5` to terminate.

#test-case[Graphical external program]

This test case tests that the shell can spawn graphical applications
and waits for the application to exit before prompting for the next command line.

#test-case-image("graphical-external-program.png")

The shell correctly spawns the Firefox web browser.
We also see that the shell does not prompt the user for the next command yet
as Firefox has not terminated.
Note that the warnings and errors are from Firefox, not from the shell.

#test-case[Non-existent program specified without full path]

The shell should print an error message to the standard error stream
if a program specified without its full path cannot be found in the `PATH` environment variable.

#test-case-image("external-program-nonexistent.png")

#test-case[Non-existent program specified with full path]

The shell should print an error message to the standard error stream
if a program specified without its full path cannot be found in the `PATH` environment variable.

#test-case-image("external-program-full-nonexistent.png")

== Built-in Commands

=== `prompt`

#test-case[`prompt` with a single argument]

`prompt` with a single argument should set the prompt to the argument.

#test-case-image("prompt-single-argument.png")

We change the prompt twice, first to `$` and then to `"john doe:"`.
The screenshot shows that the prompt is indeed changed according to the given argument.

#test-case[`prompt` without arguments]

`prompt` without arguments should print an error message to standard error.

#test-case-image("prompt-without-arguments.png")

Indeed, when no arguments are given, an error message is printed.

#test-case[`prompt` with two or more arguments]

`prompt` with two or more arguments should print an error message to standard error without changing the prompt.

#test-case-image("prompt-two-or-more-arguments.png")

Indeed, when two or more arguments are given, an error message is printed.

#test-case[`prompt` in a background job]

Executing `prompt` in a background job should not change the prompt.

```
prompt john$ &
```

=== `cd` and `pwd`

#test-case[`cd` without any arguments and `pwd`]

`cd` without any arguments should change the working directory to the user's home directory
and set the `OLDPWD` and `PWD` environment variables.
`pwd` should show the home directory.

```
pwd
env | grep PWD
cd
pwd
env | grep PWD
```

#test-case[`cd` with a valid relative path and `pwd`]

`cd` with a valid relative path argument should change the current working directory to that path
and set the `OLDPWD` and `PWD` environment variables.
`pwd` should show the absolute path of the directory.

From the home directory:

```
pwd
env | grep PWD
cd Documents
pwd
env | grep PWD
```

#test-case[`cd` with a valid absolute path and `pwd`]

`cd` with a valid absolute path argument should change the current working directory to that path
and set the `OLDPWD` and `PWD` environment variables.
`pwd` should show the absolute path of the directory.

```
pwd
env | grep PWD
cd /
pwd
env | grep PWD
```

(separate screenshot)
```
pwd
env | grep PWD
cd /usr/bin
pwd
env | grep PWD
```

#test-case[`cd` with `-` and `pwd`]

"`cd -`" should change the current working directory to the previous working directory
and set the `OLDPWD` and `PWD` environment variables.
`pwd` should show the absolute path of the directory.

```
  pwd
  env | grep PWD
  cd /
  pwd
  env | grep PWD
  cd -
  pwd
  env | grep PWD
  cd -
  pwd
  env | grep PWD
```

#test-case[`cd` with two or more arguments]

`cd` with two or more arguments should print an error message
to standard error without changing the working directory
or modifying the `OLDPWD` and `PWD` environment variables.

```
pwd
env | grep PWD
cd hello world
pwd
env | grep PWD
cd the quick brown fox jumps over the lazy dog
pwd
env | grep PWD
```

#test-case[`cd .`]

`cd .` should change the working directory to the current directory~(i.e., the current directory does not change).
The `OLDPWD` and `PWD` environment variables should be set to the current working directory.

```
pwd
env | grep PWD
cd .
pwd
env | grep PWD
```

#test-case[`cd ..`]

`cd ..` should change the working directory to the parent directory.
The `OLDPWD` environment variable should be set to the original current working directory
while `PWD` should be set to the parent working directory.

```
pwd
env | grep PWD
cd ..
pwd
env | grep PWD
```

#test-case[`cd` with complex relative path]

`cd` should work with complex relative paths
that use `.` and `..` in various sections of the path.
The `OLDPWD` and `PWD` environment variables should be updated.

// TODO:
From the assignment directory:
```
pwd
env | grep PWD
cd ./src/../build/../.
pwd
env | grep PWD
```

#test-case[`cd` with complex absolute path]

`cd` should work with complex absolute paths
that use `.` and `..` in various sections of the path.
The `OLDPWD` and `PWD` environment variables should be updated.

// TODO:
```
pwd
env | grep PWD
cd /etc/../usr/../.
pwd
env | grep PWD
```

#test-case[`cd` with a non-existent relative path]

`cd` with a non-existent relative path should print an error message to standard error
without changing the working directory or modifying the `OLDPWD` and `PWD` environment variables.

```
pwd
env | grep PWD
cd blahblahblah
pwd
env | grep PWD
```

#test-case[`cd` with a non-existent absolute path]

`cd` with a non-existent absolute path should print an error message to standard error
without changing the working directory or modifying the `OLDPWD` and `PWD` environment variables.

```
pwd
env | grep PWD
cd /blah/blah/blah
pwd
env | grep PWD
```

#test-case[`cd` without any arguments but with an unset `HOME` environment variable]

`cd` without any arguments but with an unset `HOME` environment variable
should print an error message to standard error
without changing the working directory or modifying the `OLDPWD` and `PWD` environment variables.

#test-case-image("cd-without-arguments-no-home.png")

#test-case[`cd` with `-` but with an unset `OLDPWD` environment variable]

"`cd -`" with an unset `OLDPWD` environment variable
should print an error message to standard error
without changing the working directory or modifying the `OLDPWD` and `PWD` environment variables.

#test-case-image("cd-dash-no-oldpwd.png")

#test-case[`cd` in a background job]

Executing `cd` in a background job should not change the current working directory.

```
pwd
env | grep PWD
cd &
cd /usr/bin &
cd .. &
pwd
env | grep PWD
```

#test-case[`pwd` in a background job]

Executing `pwd` in a background job should still print the current working directory.

```
pwd &
```

=== `history`

#test-case[`history` without arguments when there are no previous commands]

When there are no previous commands,
the `history` built-in without arguments should show a command history as a numbered list
containing a single entry,
which is `history` command itself.

// TODO:
From a fresh start of the shell:
```
history
(should only contain one entry: 1. history)
```

#test-case[`history` without arguments when there are previous commands]

The `history` built-in without arguments should show the command history as a numbered list.
The `history` command itself should be in the command history.

First, we populate the command history with `echo hello`, `ls` and `ps`.
Then, we execute `history`:
#test-case-image("history-without-arguments.png")

Indeed, the commands executed appear in the history in their order of execution.

#test-case[`history` with arguments]

Calling the `history` built-in with arguments should print an error message to the standard error stream.

#test-case-image("history-with-arguments.png")

#test-case[`history` in a background job]

Executing `history` in a background job should still print the command history.

```
history &
```

=== `exit`

// TODO:

#test-case[`exit` without any arguments]
```
exit
```

#test-case[`exit` with in-range non-negative integer exit code]
```
(in build/shell) exit 0
(in bash) echo $?
```

```
(in build/shell) exit 1
(in bash) echo $?
```

```
(in build/shell) exit 255
(in bash) echo $?
```

#test-case[`exit` with non-negative integer exit code outside `0`--`255`]

According to the POSIX manual for `exit`,
if an exit code outside the range `0`--`255` (inclusive) is specified,
the exit status of the program is undefined.
However, the program should still exit.

```
(in build/shell) exit -1
(in build/shell) exit 256
(in build/shell) exit -32767
(in build/shell) exit 32767
```

#test-case[`exit` with out-of-range integer exit code]

When an exit code out of range of C's `int` data type is specified,
`exit` should print an error message to the standard error stream
without exiting the shell.

```
(in build/shell) exit -2147483649
(in build/shell) exit 2147483648
```

#test-case[`exit` with non-integer exit code]

```
exit helloworld
```

#test-case[`exit` with two or more arguments]

If two or more arguments are specified to `exit`,
`exit` should print an error message to the standard error stream
without exiting the shell.

```
exit 123 123
exit hello world
```

#test-case[`exit` in a background job]

Executing `exit` in a background job should not cause the shell to exit.

```
exit &
exit 0 &
exit 1 &
```

== History Navigation

#test-case[Upwards history navigation when there is no previous command]

When the shell is first started without entering any commands,
pressing the `Up` arrow should not do anything
since there is no previous command.
Similarly, if there were previous commands,
but we have navigated to the first command,
pressing `Up` should not do anything.

#test-case-image("history-navigation-up-no-prev-a.png")

It is difficult to demonstrate with an image,
but the `Up` arrow was pressed in the screenshot above.
We observe that indeed nothing happens.

#test-case-image("history-navigation-up-no-prev-b.png")

Again, it is difficult to demonstrate with an image,
but pressing the `Up` arrow four times
navigates upwards in the command history,
moving to `ps`, `ls` and `echo hello`.
Once on `echo hello`, the fourth `Up` arrow does not change
the command line since there are no more previous commands.

#test-case[Upwards history navigation when there are previous commands]

When there are previous commands,
pressing the `Up` arrow should move up through the history by one entry.

Pressing the `Up` arrow should change the command line to
the previous command in the command history.

First, we execute `echo hello`, `ls` and `ps`
to populate the command history.

Pressing the `Up` arrow for the first time:
#test-case-image("history-navigation-up-with-prev-a.png")

Pressing the `Up` arrow for the second time:
#test-case-image("history-navigation-up-with-prev-b.png")

Pressing the `Up` arrow for the third time:
#test-case-image("history-navigation-up-with-prev-c.png")

#test-case[Downwards history navigation when there is no next command]

When the currently selected history item is the latest (new command line),
pressing the `Down` arrow should not do anything
since there is no next command.

#test-case-image("history-navigation-down-no-next.png")

It is difficult to demonstrate with an image,
but the `Down` arrow was pressed in the screenshot.
We observe that the command line does not change since there is no next command to navigate to.

#test-case[Downwards history navigation when there are next commands]

When the command history has been navigated upwards by pressing the `Up` arrow,
pressing `Down` should perform the opposite,
navigating through the history by one entry downwards.

```
echo hello
ls
ps
(press up arrow)
(press up arrow)
(press up arrow)
(take screenshot, should be at echo hello)
(press down arrow and take screenshot)
(press down arrow and take screenshot)
(press down arrow and take screenshot, should be blank command line)
```

First, we populate the command history by executing `echo hello`, `ls` and `ps`.
Then, we press the `Up` arrow three times to navigate to the first entry
in the command history:

#test-case-image("history-navigation-down-with-next-a.png")

Pressing the `Down` arrow for the first time:
#test-case-image("history-navigation-down-with-next-b.png")

Pressing the `Down` arrow for the second time:
#test-case-image("history-navigation-down-with-next-c.png")

Pressing the `Down` arrow for the third time:
#test-case-image("history-navigation-down-with-next-d.png")

#test-case[History navigation should remember the contents of the new command line]

When there is partial input on the latest command line
and the `Up` arrow key is used to navigate up the command history,
returning to the latest command line should remember the partial input.

// TODO:
```
echo hello
ls
ps
(type in without pressing enter) hello world
(press up arrow)
(press up arrow)
(press up arrow, take screenshot)
(press down arrow)
(press down arrow)
(press down arrow, take screenshot --- should be "hello world")
```

== Command Repetition

#test-case[`!` with a valid index]

When using `!` with a valid index,
the command corresponding to that index in the command history
should be re-executed.

We populate the command history with several commands,
show the history via the `history` built-in,
then repeat the commands using `!`.

#test-case-image("exclam-valid-idx.png")

Indeed, the correct commands are re-executed.

#test-case[`!` with an invalid index]

When using `!` with an invalid index,
an error message should be printed to the standard error stream.

We populate the command history with several commands and
show the command history via the `history` built-in.
Then, we enter `!0` and `!100` as examples of invalid indices:

#test-case-image("exclam-invalid-idx.png")

The shell correctly prints an error message to the standard error stream.

#test-case[`!` with a matching string]

When using `!` with a matching string,
the shell searches through the command history upwards
to find the first matching command.
The matching command is re-executed.

We populate the command history with several commands,
show the history via the `history` built-in,
then repeat the commands using `!` followed by matching prefixes.

#test-case-image("exclam-matching-str.png")

Indeed, the correct commands are re-executed using various prefixes.

#test-case[`!` with a non-matching string]

When using `!` with an non-matching string,
an error message should be printed to the standard error stream.

We populate the command history with several commands and
show the command history via the `history` built-in.
Then, we enter `!hello` and `!f` as examples of non-matching prefixes:

// FIXME: #test-case-image("exclam-nonmatching-str.png")

The shell correctly prints an error message to the standard error stream.

== Wildcard Expansion

#test-case[Expansion of `*` alone when there are matching files in the current working directory]

`*` should be expanded to all files in the current working directory.

#test-case-image("wildcard-asterisk-alone.png")

#test-case[Expansion of `*` in combination with other text when there are matching files in the current working directory]

#test-case-image("wildcard-asterisk-mixed.png")

#test-case[Expansion of multiple `*` in combination with other text when there are matching files in the current working directory]

#test-case-image("wildcard-asterisk-multi.png")

#test-case[Expansion of `*` across directories]

#test-case-image("wildcard-asterisk-across-directories.png")

#test-case[Expansion of `*` with no matching files]

When there are no matching files,
the token should be taken literally.

#test-case-image("wildcard-asterisk-no-match.png")

#test-case[Expansion of `?` alone when there are matching files in the current working directory]

// TODO:
```
touch a
touch b
touch c
echo ?
```

#test-case[Expansion of `?` in combination with other text when there are matching files in the current working directory]

#test-case-image("wildcard-question.png")

#test-case[Expansion of `?` across directories]

// TODO:
```
mkdir foo
touch foo/bar
touch foo/baz
touch foo/helloworld
echo ?oo/ba?
echo ???/???
```

#test-case[Expansion of `?` with no matching files]

When there are no matching files, the token should be taken literally.

#test-case-image("wildcard-question-no-match.png")

== Redirections

#test-case[Redirecting standard output]

#test-case-image("redirect-stdout.png")

#test-case[Redirecting standard input]

#test-case-image("redirect-stdin.png")

#test-case[Redirecting standard error]

#test-case-image("redirect-stderr.png")

#test-case[Redirecting all standard streams]

#test-case-image("redirect-all.png")

#test-case[Repeated redirections]

When redirections for the same standard stream is specified more than once,
the last (rightmost) redirection takes priority.
In the case of redirecting standard output and standard error,
all files are still created.

#test-case-image("redirect-repeated.png")

#test-case[Missing redirection file]

These commands should result in a parse error
without any commands executed.

#test-case-image("redirect-missing-file.png")

== Pipelines

#test-case[Pipeline with one pipe]

#test-case-image("pipeline-single.png")

#test-case[Pipeline with multiple pipes]

The test should print `1` on success as there is only one line.

#test-case-image("pipeline-multi.png")

#test-case[Pipeline with command exiting with non-zero exit code]

If a command in a pipeline exits with a non-zero exit code,
the pipeline should still work and the command's standard output stream
should still be piped to the standard input stream of the next command.

// TODO:
```
ls doesntexist | wc -l | cat
```

#test-case[Pipeline with missing command]

When a pipeline is specified with a missing command after a pipe,
the shell should print an error message to the standard error stream
indicating a parse error.

// TODO:
```
echo hello world | | grep world
echo hello world |
echo hello world | | grep world |
```

== Job Execution

#test-case[Single background job]

Running a command in the background using `&` should
run the command in the background.
The shell should prompt the user for the next command immediately
without waiting for the executed command to terminate.

#test-case-image("job-bg-single.png")

#test-case[Multiple background jobs]

Running multiple commands in the background using `&`
should spawn all commands in the background.
The shell should prompt the user for the next command immediately
without waiting for any of the executed commands to terminate.

#test-case-image("job-bg-multi.png")

#test-case[Multiple background jobs with non-zero exit codes]

If a command in a list of background jobs exits with a non-zero exit code,
the subsequent jobs should still execute.

#test-case-image("job-bg-multi-non-zero.png")

#test-case[Clean-up of background job zombies]

When spawning child processes for background jobs,
the shell should not leave any zombie processes behind.

```
echo hello & ls & uname &
ps (should not show echo, ls or uname)
```

```
sleep 5 & sleep 5 & sleep 5 & sleep 5 & sleep 5 &
ps (should show sleep in the process list)
(wait for 5 seconds for sleep to exit)
ps
```

#test-case[Single sequential job with terminating `;`]

For a sequential job, having a `;` at the end of the command line should be allowed.

#test-case-image("job-fg-single.png")

#test-case[Multiple sequential jobs]

If there are multiple sequential jobs,
each job should execute in order.
Later jobs should only start after earlier jobs finish.

#test-case-image("job-fg-multi-a.png")
#test-case-image("job-fg-multi-b.png")

#test-case[Multiple sequential jobs with non-zero exit codes]

If a command in a list of sequential jobs exits with a non-zero exit code,
the subsequent jobs should still execute.

#test-case-image("job-fg-multi-non-zero.png")

== String Parsing

(All the test cases below should only print one line.)

#test-case[Simple double quoted string]
```
printf '%s\\n' "helloworld123"
```

#test-case[Double quoted string with whitespace]
```
printf '%s\\n' "   hello world   123   "
```

#test-case[Double quoted string with special characters]
```
printf '%s\\n' "&;!|<>2>'*?["
```

#test-case[Double quoted string with escape sequences]
```
printf '%s\\n' "\\"
printf '%s\\n' "\""
printf '%s\\n' "\ \h\e\l\l\o\ \w\o\r\l\d\ \1\2\3\ "
```

#test-case[Simple single quoted string]
```
printf '%s\\n' 'helloworld123'
```

#test-case[Single quoted string with whitespace]
```
printf '%s\\n' '   hello world   123   '
```

#test-case[Single quoted string with special characters]
```
printf '%s\\n' '&;!|<>2>"*?['
```

#test-case[Single quoted string with escape sequences]
```
printf '%s\\n' '\\'
printf '%s\\n' '\''
printf '%s\\n' '\ \h\e\l\l\o\ \w\o\r\l\d\ \1\2\3\ '
```

#test-case[String concatenation]
```
printf '%s\\n' " hello "' world '123" &;!|<>"'2>"*?[ '
```

== Complex Command Lines

#test-case[Mixture of sequential and background jobs]

```
echo hello world & sleep 3; ls -lt ; ps &
```

#test-case[Mixture of piping and redirections]

```
echo "hello world" | grep hello > hello
cat < hello | grep world | cat 2> cat-error
```

#test-case[Mixture of piping, redirections, sequential jobs and background jobs]

```
(below is one line)
echo "hello world" > hello | grep hello | cat & ls -lt > ls-out ; cat < ls-out ; ps -e & ls nonexistent 2> ls-error
```

== Miscellaneous

#test-case[Inheritance of parent environment]
```
env
```

#test-case[Empty command line]
```
Press enter a few times without entering any other input, then take a screenshot
```

#test-case[Non-termination on `CTRL-C`, `CTRL-Z` and `CTRL-\`]
```
(start the shell)
(ctrl-c, take screenshot)
(ctrl-z, take screenshot)
(ctrl-\, take screenshot)
```

#test-case[Insignificance of whitespace]

```
sleep 3&
sleep 3 &
echo hello;
echo hello ;
echo hello|grep hello|cat
echo hello>out
ls nonexistent2>err
cat<out
```

#test-case[Compilation]

```
make clean
make
build/shell
```

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
