Frequently Asked Questions
==========================

Build System
------------

   - **I'm typing `make` as root and I'm getting all sorts of errors?** The
     makefile assumes that the system is configured in a specific way
     when `make` is used.  If, for instance, `cos.ko` is not inserted
     in the system (i.e. it is not listed in `lsmod`), then `make`
     will fail when it attempts to `rmmod cos`.

   - **When I do `make` as root, it blocks indefinitely on `rmmod trans`?**
     You need to kill all _translator helpers_ before this module can
     be removed.  For instance, `killall print` will remove the
     terminal printer translator helper.
    
   - **I'm getting a bunch of protection problems when I try and `make` as a normal user, or can't `make cp`?**
     You probably did a `make` or `make init` as root in the Composite
     source directory.  Bad.  As root `cd <COMPOSITESRC>; make
     distclean`.  Then as a normal user, go back into the Composite
     source directory, and do the normal `make init`.

   - **When I `make init`, the system crashes soon there-after**
     We have not identified the problem.  It is annoying.  Fortunately,
     there is a work-around that I have found to be successful: instead
     of `make init`, use `nice -n -20 make init`.  The problem appears to
     be a preemption of the make process after `insmod cos.ko`, and this
     prevents that.

   - **When I build the system with `-O3`, I get warnings about
       alloc_page and free_page** 
     Change the order of the dependencies within the `Makefile`: put
     cbuf_c first.  What's happening is that the cbuf library needs to
     allocate pages, but that is only done in the library.  If that
     library is linked into the component _after_ the memory manager,
     then the memory manager's functions for alloc and free page will
     not get linked in.

Runscripts
----------

   - **Runscripts are a little ugly and hard to work with; is there any other way?**
     Currently there is not.  However, we are developing a
     strongly-typed language called `cfuse` that is turing complete and
     provides strong abstraction of common subgraphs to make
     run-scripts much easier to deal with.  Hold out for now, relief is
     on the way!
