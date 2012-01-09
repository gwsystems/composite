This is the source code for the COMPOSITE component-based OS.  Even
low-level system policies such as scheduling, memory mapping, and
synchronization are defined as discrete user-level components.  Each
component exports an interface used to harness its functionality, and
components are composed together to form an executable system.

Composite currently supports a platform with:
- x86-32
- booting using Linux 2.6.33 or 2.6.36 (see Hijack support for booting
  information)
- networking

Research features of COMPOSITE currently include:
- Mutable protection domains -- hardware protection boundaries can be
  raised or lowered dynamically in a controlled manner to trade fault
  isolation for performance
- Hierarchical resource management (HiRes) -- resource management
  decisions concerning CPU, memory, and I/O can be delegated to
  applications so they can control their allocations.  However, even
  malicious subsystems cannot use this power to interfere with other
  subsystems.  In many ways this is a generalization of
  virtualization.
- User-level scheduling -- threads and interrupts are scheduled by
  user-level components.  The COMPOSITE kernel does not have a
  scheduler!
- Memory scheduling -- memory in COMPOSITE is dynamically transferred
  between protection domains in the system based on the percieved
  impact that the additional allocation will make on predictability
  and performance.  In this way memory is "scheduled" by allocating it
  over a window of time to specific parts of the system to minimize
  memory usage in embedded systems.
- Secure bulletin board system -- COMPOSITE was used in the verifiable
  election based on Scantegrity in Takoma Park, MD.  It provided a
  secure webpage for verifying ballots after the election.

Source code: This package includes the source code for COMPOSITE.
Please read the doc/ directory for more information, or join the
compositeos@googlegroups.com mailing list.
