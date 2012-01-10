The _Composite_ Component-Based OS
==================================

This is the source code for the _Composite_ component-based OS.  Even
low-level system policies such as scheduling, memory mapping, and
synchronization are defined as discrete user-level components.  Each
component exports an interface used to harness its functionality, and
components are composed together to form an executable system.

Please see www.seas.gwu.edu/~gparmer/composite for publications and
more information.

Research features of _Composite_
--------------------------------

- Mutable protection domains -- hardware protection boundaries can be
  raised or lowered _dynamically_ and in a controlled manner to trade
  fault isolation for performance

- Hierarchical resource management (HiRes) -- resource management
  decisions concerning CPU, memory, and I/O can be delegated to
  applications so they can control their allocations.  However, even
  malicious subsystems cannot use this power to interfere with other
  subsystems.  In many ways this is a generalization of
  virtualization.

- User-level scheduling -- threads and interrupts are scheduled by
  user-level components.  The _Composite_ kernel _does not have a
  scheduler_!

- Memory scheduling -- memory in _Composite_ is dynamically transferred
  between protection domains in the system based on the percieved
  impact that the additional allocation will make on predictability
  and performance while minimizing memory usage.  In this way memory
  is "scheduled" by allocating it over a window of time to specific
  parts of the system.

- Secure bulletin board system -- _Composite_ was used in the verifiable
  election based on Scantegrity in Takoma Park, MD.  It provided a
  secure webpage for verifying ballots after the election.

Where to start -- a tour of the source code
-------------------------------------------

- Please read the doc/ directory for more information on how the code
  is organized
- Join the compositeos@googlegroups.com mailing list

_Composite_ system support
--------------------------

- x86-32
- booting using Linux 2.6.33 or 2.6.36 (see Hijack support for booting
  information)
- networking

Important note
--------------

**The code is pre-alpha quality.  Some parts are quite solid, many
  others are absolutely not.  Please consult with us to determine if
  it is right for your use-case.**

Licensing
---------

This code is licensed under the GPL version 2.0:

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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
```

This license is not set in stone, and we would be willing to negotiate
on a case-by-case basis for more business-friendly terms.  The license
should not prevent you from using this OS, as alternatives can be
arranged.  It _should_ prevent you from stealing the work and claiming
it as your own.
