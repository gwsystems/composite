The _Composite_ Component-Based OS
==================================

This is the source code for the _Composite_ component-based OS.  Even
low-level system policies such as scheduling, memory mapping, and
synchronization are defined as discrete user-level components.  Each
component exports an interface used to harness its functionality, and
components are composed together to form an executable system.

Please see http://composite.seas.gwu.edu for publications and
more information.

Branches
--------

- `master` is the original system with a full user-level set of components.
- `ppos` is the Speck kernel emphasizing scalable predictability.
- `tcaps` has mainly been integrated into `ppos`, but a few unrelated pieces remain.

Research features of _Composite_
--------------------------------

See a summary of the research directions of _Composite_ at http://composite.seas.gwu.edu.

Where to start -- a tour of the source code
-------------------------------------------

- Please read the _Composite_ [posts](http://www.seas.gwu.edu/~gparmer/posts.html).

- Join the compositeos@googlegroups.com mailing list.
    We use a #slack for our internal development, so this is exceedingly low throughput (1 email every 6 months).

- To run *Composite*, you start by reading the installation and usage
  summary in `docs/installation_usage_summary.md`.

_Composite_ system support
--------------------------

- x86-32
- Qemu with 32 bit, x86 support

Important note
--------------

**The code is pre-alpha quality.  Some parts are quite solid, many
  others are absolutely not.  Please consult with us to determine if
  it is right for your use-case.**

Licensing
---------

This code is licensed under the GPL version 2.0 with the class path exception unless otherwise noted (significant portions of user-level are BSD):

```
The Composite Component-Based OS
Copyright (C) 2009 Gabriel Parmer

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Linking this library statically or dynamically with other
modules is making a combined work based on this library. Thus,
the terms and conditions of the GNU General Public License
cover the whole combination.

As a special exception, the copyright holders of this library
give you permission to link this library with independent modules
to produce an executable, regardless of the license terms of
these independent modules, and to copy and distribute the resulting
executable under terms of your choice, provided that you also meet,
for each linked independent module, the terms and conditions of
the license of that module. An independent module is a module which
is not derived from or based on this library. If you modify this
library, you may extend this exception to your version of the
library, but you are not obligated to do so. If you do not wish to
do so, delete this exception statement from your version.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
```

This license is not set in stone, and we would be willing to negotiate
on a case-by-case basis for more business-friendly terms.  The license
should not prevent you from using this OS, as alternatives can be
arranged.  It _should_ prevent you from stealing the work and claiming
it as your own.

Support
-------

We'd like to sincerely thank our sponsors.  The _Composite_
Component-Based OS development effort has been supported by grants
from the National Science Foundation (NSF) under awards `CNS 1137973`,
`CNS 1149675`, and `CNS 1117243`.
