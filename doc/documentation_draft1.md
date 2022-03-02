CRT Namespace API Documentation
=============================

Why?
-----------------------
We want to allow components to share shared virtual address spaces (VAS), while using MPK for protection.
Abstractions are required to reasonably share VASes, and control inter-component interactions accordingly.
Some components may not share a VAS with any other components for maximum security.
Others, like a scheduler or networking subsystem, should share a VAS with each other component that depends on them to enable fast interactions, while still mutually protecting their memory from each other.
This emphasizes an important point: address spaces and protection are *orthogonal* -- sharing a VAS does *not* imply access to each other's memory.
The latter use case crucially requires "disjoint-ness" between certain components, which will be discussed later.
We require an API to facilitate this control.

How?
-----------------------
This API uses and *manages* three *namespaces*.
While only two are accessed directly by the API (the third is implicitly managed), it is worth presenting all three to fully understand the code.
These are:

1. Virtual Address Space Namespace

    The key functionality is to enable multiple components to exist in the same virtual address space.
	Hence we need VAS namespaces to track which components share which VAS.

2. ASID namespace

    ASIDs, or Address Space Identifiers, are for TLB tagging.
	Each page table has an ASID, and its entries in the TLB are tagged as such.
	This allows for the TLB to not be completely flushed when the page table changes.
	As there are a finite number of ASIDs, the system must be able to control between which components switching causes TLB flushes.
	For example, if two page-tables share an ASID namespace, they have different ASIDs, thus avoid switching.
	However, if two page-tables use ASIDs from different namespaces, switching between them *might* require TLB flushes.
	The API enables explicitly controlling this mapping of page-tables into ASID namespaces, thus limiting when TLB flushes are required.

3. MPK namespace

	Memory Protection Keys (MPKs) can be used to enable memory protection between components that can interact without kernel mediation.
	Carefully switching protection domains (active MPK keys) can be done at user-level, thus speeding up inter-component communication.
	MPK is not managed explicitly by this API, as without the VAS sharing, MPK is not useful.
	Thus, in managing VASes, the implementation transparently manages MPKs.


Relationships between namespaces
-----------------------
It's useful to understand the mappings between these namespaces and components to understand the API.

- 1 component -> 1 VAS range -- Each component is assigned as VAS "name", or range of allowed virtual addresses.
- 1 component -> 1 MPK name -- Each component is assigned a single MPK key.
- 1 component -> many VASes -- A single component with a single name (VAS range + MPK key) can be mapped into many VASes only if no other component with the same name exists in any of those VASes.
- 1 VAS -> 1 ASID name -- In the current implementation, each page-table corresponds to a VAS, and each is assigned an ASID name.
- 1 VAS -> many components -- Each VAS contains potentially many components, each with separate names (MPK keys and virtual address ranges).
- Namespaces (of any type) can be disjoint from one another, in which case we know their names never conflict.

Names
-----------------------
- Properties of names within a namespace
    We must track the state of all names within a namespace. The possible states are:

    1. reserved
        - Reserved names are potentially available to be used by that namespace. If a name is not reserved, it cannot be used (become allocated) within that namespace; however it may be possible for that name to be used for aliasing a component within a VAS namespace.
    2. allocated
        - Allocated names are used up within that namespace.
    3. aliased
        - This state is only for VAS namespaces, ASID namespaces can not be aliased.
        - For a component to be aliased into a name, it must already be allocated within an ancestor namespace of this namespace and the name in question must be unreserved (and therefore unallocated).

- How names are assigned
    - In ASID NSs, names are assigned by first available, and the API user shouldn't know nor care which name is used.
    - In VAS NSs, names are assigned by the addresses that a component is linked into (identified using its entry address).
	    The API user will implicitly set which name a component will have by setting the component's `baseaddr` in the composition script.

- Number of names
    - ASID NSs: 1024 on x86-32
    - VAS NSs: 512 (in 32 bit)
    - MPK names: 15 (again, this is implicit and not exposed to the user, but direct allocations into the same VAS namespace will fail beyond 15 components for this reason; use the split functions to make beyond 15 allocations possible via the aliased state)


The API and terminology
-----------------------
- Namespace creation
    - These functions do a basic initialization of a namespace, and sets all names to be reserved and unallocated.
        - `int crt_ns_asids_init(struct crt_ns_asid *asids)`
        - `int crt_ns_vas_init(struct crt_ns_vas *new, struct crt_ns_asid *asids)`
            - Assigns an ASID name within an existing ASID NS to the new VAS namespace
    - These functions initialize a new namespace with the unallocated names left over from an existing namespace
        - Leads to two disjoint namespaces
        - Importantly, once a namespace is used to create a split, it is impossible to make any more allocations within it
        - `int crt_ns_asids_split(struct crt_ns_asid *new, struct crt_ns_asid *existing)`
        - `int crt_ns_vas_split(struct crt_ns_vas *new, struct crt_ns_vas *existing, struct crt_ns_asid *asids)`

- Namespace allocation
    - Allocate (or alias) an existing component into an existing namespace
        - `int crt_ns_vas_alloc_in(struct crt_ns_vas *vas, struct crt_comp *c)`
    - **Not yet implemented\** Create a component directly within a VAS namespace
        - `int crt_comp_create_in_vas(struct crt_comp *c, struct crt_ns_vas *vas, struct crt_ns_asid * asids)`

@Linnea: Might simplify things to instead do `crt_ns_vas_alias(struct crt_ns_vas *child, struct crt_ns_vas *parent)` to add all of the `parent`'s components into `child`.
I think that this is the thing that we always intend to do, so why not make the API do it!
I think that doing component at a time will actually break things (imagine if we alias into one child but not the other? do we use callgates or not?)
Further, if this is the case, should we even do alias, or when we split, should we just always alias the parent NS in?
Sorry for iterating on this so much; I know it is annoying.

Example of use
-----------------------
For now, the booter must be modified directly to use this API. In the future this will be specifiable via the composition scripts.

4 component system (one booter). Components 2 and 3 are in the same namespace, and component 4 is in a disjoint namespace

```
/* Assume:
 * Namespaces have been allocated via slab allocator
 *      Not shown, but it looks like:
 *      ns_asid1 = ss_ns_asid_alloc();
 *      ss_ns_asid_activate(ns_asid1);
 * Components have been allocated and initialized as normal
 */

struct crt_ns_asid *ns_asid1;
struct crt_ns_vas *ns_vas1;
struct crt_ns_vas *ns_vas2;

/* initialize ASID NS, VAS NS, allocate components 2 and 3 within it */
if(crt_ns_asids_init(ns_asid1) != 0) BUG();

if(crt_ns_vas_init(ns_vas1, ns_asid1) != 0) BUG();

if(crt_ns_vas_alloc_in(ns_vas1, boot_comp_get(3)) != 0) BUG();
if(crt_ns_vas_alloc_in(ns_vas1, boot_comp_get(2)) != 0) BUG();

/* Create new ASID NS and VAS NS via split */
if(ss_ns_asid_split(ns_asid2, ns_asid1) != 0) BUG();
if(ss_ns_vas_split(ns_vas2, ns_vas1, ns_asid2) != 0) BUG();

/* Allocate component 4 as normal, within ns_vas2 */
if(crt_ns_vas_alloc_in(ns_vas1, boot_comp_get(4)) != 0) BUG();


```
