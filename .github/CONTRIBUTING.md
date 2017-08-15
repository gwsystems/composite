# Contributing to Composite

Contributors to *Composite* should ensure that they follow these guidelines:

- Code should be written in adherence to the [style guide](../doc/style_guide/composite_coding_style.pdf).
  Most notably, it should be written to be readable.
  This often means that you must go through a fairly extensive phase after producing code that works to [clean it up](http://www2.seas.gwu.edu/~gparmer/posts/2016-03-07-code-craftsmanship.html).
- To assist with this process, a format script is provided. See the [usage guide](../doc/style_guide/auto_formatter.md).
- Code should be tested using components in `src/components/implementation/tests`.
  For low-level functionality, `micro_booter` is decent, but might need to be modified.
  For higher-level functionality, appropriate tests should be added.
  Details on executing the system can be found in `doc/README.md`.
- If you know some of the team members working on *Composite*, you should @-mention at least two people in any [Pull Request](http://www2.seas.gwu.edu/~gparmer/posts/2017-06-08-sweng-in-research.html) (PR) to get feedback.
- Fill out the PR template honestly.
  The reviewers will use your feedback from that template to guide their reviews, and noone will be happy if it isn't filled out accurately.
