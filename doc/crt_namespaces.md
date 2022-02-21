CRT Namespace API Documentation
=============================

Why?
-----------------------
We want to allow components to exist in a shared virtual address space, and access each other via MPK. However, this functionality needs to be controlled. Some components may not be shared with any components for maximum security. Others, like a scheduler or DPDK, should be able to have shared access to all other components in the system, without giving all of these components access to each other. The latter use case crucially requires "disjoint-ness" between certain components, which will be discussed later. 

Essentially, we require an API to facilitate this control.

How?
-----------------------
This API uses the following three namespaces, while only two are accessed directly by the API user (the third is implicit within the other two), it is worth presenting all three for full understanding of the code. These are:
1. Virtual Address Space Namespace

    The key functionality is to enable multiple components to exist in the same virtual address space, hence we need VAS namespaces to track which components belong to which shared VAS.

2. ASID namespace

    ASIDs, or Address Space Identifiers are for TLB tagging. Each page table has an ASID, and its entries in the TLB are tagged as such. This allows for the TLB to not be completely flushed when the page table changes. We need a namespace for ASIDs because multiple components will execute within the same virtual address space; components within one shared VAS should have the same ASID so that entries aren't flushed when those components execute AND components in different VASs must have different ASIDs so that those entries are flushed when switching between non-shared components.

3. MPK namespace

    This is the implicit namespace; nevertheless it is key because MPK names are the backbone of enabling VAS sharing. MPK namespaces correspond and are tracked with the VAS namespaces.

Relationships between namespaces
-----------------------
It's useful to understand the mappings between the namespaces, components, etc to understand the API.

- 1 component -> many VASs, has the same name within each VAS
- 1 component -> 1 MPK name
- 1 ASID -> 1 VAS
- 1 VAS -> many components
- Namespaces (of any type) can be disjoint from one another 

Names
-----------------------
- Properties of names within a namespace
    - We must track the state of all names within a namespace. The possible states are:
        1. reserved
            - Reserved names are potentially available to be used by that namespace. If a name is not reserved, it cannot be used (become allocated) within that namespace; however it may be possible for that name to be used for aliasing a component within a VAS namespace.
        2. allocated
            - Allocated names are used up within that namespace.
        2. aliased
            - This state is only for VAS namespaces, ASID namespaces can not be aliased.
            - For a component to be aliased into a name, it must already be allocated within an ancestor namespace of this namespace and the name in question must be unreserved (and therefore unallocated).

- How names are assigned
    - In ASID NSs, names are assigned by first available, and the API user shouldn't know nor care which name is used
    - In VAS NSs, names are assigned by the component's entry address. The API user will implicitly set which name a component will have by setting the component's `baseaddr` in the composition script.

- Number of names
    - ASID NSs: 1024
    - VAS NSs: 512 (in 32 bit)
    - MPK names: 16 (again, this is implicit and not exposed to the user, but direct allocations into the same VAS namespace will fail beyond 16 components for this reason; use the split functions to make beyond 16 allocations possible via the aliased state)


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
    - **Not yet implemented)* Create a component directly within a VAS namespace
        - `int crt_comp_create_in_vas(struct crt_comp *c, struct crt_ns_vas *vas, struct crt_ns_asid * asids)`

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