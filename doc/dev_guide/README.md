# Composite Developers Manual

An initial attempt at codifying the philosophy, structure, build system, and debugging of the Composite OS.
Find the manual:

- [Here](./composite_dev_manual_toc.md) on github, and
- in a nicely rendered in this directory that will also include the distributed interface, library, and component documentation.

To generate (update) this documentation,

```
$ make all
```

Note, that this requires `python` version 2.

## Generating the `github` web documentation

```
$ make web
```
As we use markdown, github renders well by default.
Thus this rule mainly ensures that the auto-generated images are presence and up-to-date.

## Generating a PDF

```
$ make pdf
```
This will create the `composite_dev_manual.pdf` from the constituent `xx_*.md` files (using the order specified by their `xx` values).

If you wish to create a new versioned manual (in `composite_dev_manual_<version>.pdf`) that is git-tracked, then:

```
$ make version
```

Followed by the appropriate `git` commands to untrack the previous, and track the next, manual.

We generate the pdf using `pandoc` and the `eisvogel` [template](https://github.com/Wandmalfarbe/pandoc-latex-template).
We do no checks to make sure that you have all necessary packages installed.
The requirements include at least

- `latex`
- `pandoc`
- `graphviz`

Installing all of these in a debian derivative:
```
apt-get install texlive-full pandoc graphviz
```
Please do a pull request if I'm missing any necessary dependencies here.
