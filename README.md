# acush: A Unix Shell Written in C

`acush` is a Unix shell written in C.
It was written for a university assignment on operating systems.

The following is an overview of the requirements `acush` had to satisfy
as specified by the assignment:

- **Built-in commands:** `cd`, `pwd`, `history`, `prompt` (for customizing the prompt), and `exit`.

- **Globbing:** Filename expansion with `*` and `?`.

- **Redirection:** Input/output/error redirection via `<`, `>`, and `2>`.

- **Pipelines:** Command chaining with `|`.

- **Background jobs:** Run processes in the background using `&`.

- **Sequential jobs:** Execute multiple commands in sequence with `;`.

- **Command history:** Navigate with arrow keys; view using `history`.

- **History expansion:** Repeat previous commands with `!<number>` or `!<prefix>`.

- **Quoting and escaping:** Handle single/double quotes and escape sequences.

- **Signal handling:** Do not terminate on `SIGINT` (`Ctrl+C`), `SIGQUIT` (`Ctrl+\`), and `SIGTSTP` (`Ctrl+Z`).

- **Environment:** Inherits environment variables from the parent shell.

Additional details can be found in the accompanying report (see [_Compiling the Report_](#compiling-the-report)).

## Building and Running

First, ensure you have the following dependencies:

- make

- gcc (or any other C compiler)

To build the shell, run the following in the project's root directory:

```
make
```

After building, you can run the shell with:

```
build/acush
```

To clean up build artifacts, run:

```
make clean
```

### Nix

This repository also provides a [Nix](https://nixos.org/) flake.

To build `acush`, run:

```
nix build github:dixslyf/acush
```

To run `acush` without installing it, run:

```
nix run github:dixslyf/acush
```

## Compiling the Report

The report for `acush` is written with [Typst](https://typst.app).
Markup files can be found in the `report/` directory.

To compile the report, ensure you have Typst installed.
Then, run the following in the project's root directory:

```
typst compile report/report.typ
```

This will create a PDF document at `report/report.pdf`.

### Nix

The report can also be compiled with Nix.
To do so, run:

```
nix build github:dixslyf/acush#report
```

The resulting PDF document can then be accessed through the usual `result` symlink.
