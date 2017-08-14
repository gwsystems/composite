Auto-formatting and composite
=============================
Composite now has an autoformat script. This file describes its operation.

Dependencies
------------
The script relies on clang-format being installed. This package should be
availible from your distributions package manager (or Homebrew on Mac.)


Running the script
------------------
Go to the root directory of composite and run this command:
```
./tools/format.sh
```
And then run `git diff` to see if it changed anything :)


Script Configuration
--------------------
The formatter is configured according to the rules in `.clang_format`.
Valid rules can be found at:
https://clang.llvm.org/docs/ClangFormatStyleOptions.html


Disabling formatting
---------------------
Clang-format understands also special comments that switch formatting in a delimited range.
The code between a comment // clang-format off or /* clang-format off */
up to a comment // clang-format on or /* clang-format on */ will not be formatted. 
The comments themselves will be formatted (aligned) normally.


