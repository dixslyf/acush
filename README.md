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

Additional details can be found in the accompanying report (see [_Report_](#report)).

## Building and Running

First, ensure you have the following installed:

- `make`

- A C compiler (e.g., `gcc`, `clang`)

To build the shell, run the following in the project's root directory:

```sh
make
```

After building, you can run the shell with:

```sh
build/acush
```

To clean up build artifacts, run:

```sh
make clean
```

### Nix

This repository also provides a [Nix](https://nixos.org/) flake.

To build `acush`, run:

```sh
nix build github:dixslyf/acush

# Or statically linked
nix build github:dixslyf/acush-static
```

To run `acush` without installing it, run:

```sh
nix run github:dixslyf/acush

# Or statically linked
nix run github:dixslyf/acush-static
```

## Report

As with most university assignments,
a report was required.
The report for `acush` is written with [Typst](https://typst.app).
Markup files can be found in the `report/` directory.

The compiled report can be accessed from the latest release
[here](https://github.com/dixslyf/acush/releases/latest/download/report.pdf) (PDF).

### Compiling

To compile the report, ensure you have Typst installed.
Then, run the following in the project's root directory:

```sh
typst compile report/report.typ
```

This will output a PDF document at `report/report.pdf`.

#### Nix

The report can also be compiled with Nix.
To do so, run:

```sh
nix build github:dixslyf/acush#report
```

The resulting PDF document can then be accessed through the usual `result` symlink.

## A Note on Testing

This project does not include automated tests.

Under the constraints of the university assignment,
testing was unfortunately required to be conducted manually
by running the shell interactively,
verifying behaviour case-by-case
and providing screenshots as proof.
While we developed and followed a formal list of test cases (see the report),
automating them was not feasible given these restrictions.

## Authors

- Dixon Sean Low Yan Feng ([@dixslyf](https://github.com/dixslyf))

- Khon Min Thite ([@khonminthite](https://github.com/khonminthite))
