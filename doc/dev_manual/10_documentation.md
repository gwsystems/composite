# Documentation Generation

Composite is promoting three forms of institutionalized documentation:

1. The `doc.md` files in each component, library, and interfaces encourage the high-level documentation of each body of software.
	For component, this should focus on the purpose and functionality of that component.
	Interfaces should focus on their functional description; similar for libraries.
1. The dependencies for bodies of software that provide the `doc.md` file are calculated, and a figure denoting them is generated.
	These are generated from the dependency (interface and library) and export (interface) specifications used by the build system.
1. The functional documentation from comments in the libraries and interfaces is pulled into the documentation.
	This requires a strict format, and requires elaboration.

The API documentation is pulled out from the comments in a specific interface and library header file.
For an interface `src/components/interface/if/`, the documentation generation system looks for `if.h`.
For a library `src/components/lib/l/`, the documentation generation system looks for `l.h`.
Unfortunately, right now this is *not* configurable.

Within the header files, two types of documentation are pulled out.
First, there is API-level documentation that is not specific to each function.
This must follow (*exactly*) the format:

``` c
/***
 * header comment here.
 *
 * - list here
 */
```

The documentation generation system specifically looks for the line explicitly containing `/***` to start interpreting the comment.
The entire comment is interpreted as markdown, and the prefix ` *` and ` * ` are removed, leaving only the markdown.
The comment is terminated with a line containing exactly ` */`.
So follow the Composite style guide exactly for the comment formats here.

Second, there is per-function (group) documentation.
These take the form:

``` c
/**
 * Function `hw` and `hw2` print hello world in mind-blowing ways.
 *
 * - @str    - provides a strong to print along-side hello world.
 * - @return - returns the number of characters printed.
 */
int hw(char *str);
static inline int
hw2(char *arg1)
{
        return hw(str);
}
/***/
```

The comment begins with a line containing (*exactly*) `/**`, and ends with ` */`.
Again, note the spacing.
As with the header comments, the ` *` and ` * ` are removed, leaving only interpreted markdown.

Any `static inline` functions that include implementations must follow the Composite style's placement of `{` and `}` alone on lines.
This is used to remove the body of the function from the documentation.

The above comment will output the following documentation:

---

``` c
int hw(char *str);
static inline int
hw2(char *arg1)
{ ... }
```

Function `hw` and `hw2` print hello world in mind-blowing ways.

- @str    - provides a strong to print along-side hello world.
- @return - returns the number of characters printed.

---

Note that the line containing `/***/` tells the documentation generation system that the function prototypes associated with the previous comment.
In many cases, the system is able to implicitly figure out which functions belong to which comments using a simple rule: if we hit a new comment, we start the next group of figures.
I recommend to add in the `/***/` explicitly if you notice that the documentation is pulling in too much code.

The following sections include the auto-generated documentation.
