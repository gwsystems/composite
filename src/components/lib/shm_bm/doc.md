## shm_bm

### Description

This library provides a slab-like memory allocator interface to allocate fixed-size blocks of memory from a region of memory shared between a number of components. This allows the passing of user-defined objects that do not fit into syncronous invocation calls between a client and server with minimal additional overhead.

### Usage and Assumptions

Most of the code for this library is generated at compile-time by the preprocessor for performance. The compiler can make significant optimizations to the allocation, freeing, and address space translation routines by knowing the size of the memory available and the size of individual allocations. As such, the code in `shm_bm.h` provides little documentation because all of the user-facing code is preprocessor generated.

This section provides general usecase documentation. For full interface documentation, skip to the end of this document.

The interface for a particular user-defined object must be created at compile-time. The following code creates an interface that can allocate the `testobj` struct. This declaration must be in a header file that is shared by all the components that wish to use the generated interface.

```c
/* test_interface.h */ 

#include <shm_bm.h>

struct testobj {
    int  id;
    char name[12];
}

SHM_BM_INTERFACE_CREATE(testobj, sizeof (struct obj_test), 2048);
```

The `SHM_BM_INTERFACE_CREATE` takes 3 parameters: the name of the interface, the size of the object that is being allocated, and the maximum number of objects that can be allocated at once (the size of the shared memory region). All functions in the interface end with the name of the interface:

```c				
shm_bm_size_testobj();
shm_bm_create_testobj(void *mem, size_t memsz);
shm_bm_alloc_testobj(shm_bm_t shm, shm_objid_t *objid);
shm_bm_take_testobj(shm_bm_t shm, shm_objid_t objid);
shm_bm_borrow_testobj(shm_bm_t shm, shm_objid_t objid);
shm_bm_transfer_testobj(shm_bm_t shm, shm_objid_t objid);
shm_bm_free_testobj(void *ptr);
```

These functions are created by the preprocessor. The following typedefs are used by all instances of the interface:

- `shm_bm_t`: An opaque reference for a component to the shared memory region it creates. The client gets a unique `shm_bm_t` for every shared memory region it creates. 

- `shm_bm_objid_t`: An identifer for an object in the shared memory region. When a component allocates an object from a shared memory region, it gets a `shm_objid_t` that can be used another component to reference that object.

This library offloads the allocation of shared memory to other libraries and instead is responsible for managing allocated memory and facilitaing its use in IPC. As such, the user must allocate memory using another interface provided by the operating system. This example allocates shared memory using the `capmgr`. The library requires that 1) the allocated memory used by the library is aligned on a power-of-2 boundry specified by `SHM_BM_ALIGN` included in `shm_bm.h` (see below for the reason for this condition) and 2) The allocated memory is big enough for the library to manage. The interface provides a function to get the amount of memory required by the interface:

```c
/* component1.c */

void    *mem
size_t   memsz;
cbuf_t   id;

memsz = round_up_to_page(shm_bm_size_testobj());
id = memmgr_shared_page_allocn_aligned(memsz/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *)&mem);
```

The library can now manage this memory created by the operating system. A component can now use this shared memory region using the `testobj` interface using the following:

```c
/* component1.c */

shm_bm_t shm;

shm = shm_bm_create_testobj(mem, shmsz);
if (!shm) {
    printc("FAILURE: could not create shm from allocated memory\n");
    return;
}
shm_bm_init_testobj(shm);
```

Note that `shm_bm_create_{name}` returns 0 if the library can not manage the memory. This occurs if the two conditions above for the allocated memory are not satified. `shm_bm_init_{name}` will zero-out the memory and initialize the object-tracking data structures that are internal to the library. Only one component who will use this memory should make this call, and it should only be done at initialization as it will deallocate and overwrite any objects that have been allocated from the memory and may be in use by other components.

Other components can use this shared memory by making the same calls. Note that only the first component makes a call to `shm_bm_init_{name}.` Again, this example uses the functionality provided by the `capmgr` component to share memory between components, but this is not required.

```c
/* component2.c */

unsigned long npages;
void         *mem;
shm_bm_t      shm;

/* using id sent from component1 */
npages = memmgr_shared_page_map_aligned(id, SHM_BM_ALIGN, (vaddr_t *)&mem);
shm = shm_bm_create_testobj(mem, npages * PAGE_SIZE);
```

Both components using the shared memory can now use it to allocate objects and send them between eachother. A component can allocate a `testobj` from the shared memory region using the following:

```c
/* component1.c */

shm_bm_objid_t  objid;
struct testobj *obj;

obj = shm_bm_alloc_testobj(shm, &objid);
```

Another component using the shared memory can use the object that this component allocated using both the and `objid`. The allocating component can send this id to the component using synchronous invocation, and the other component can get a pointer to it using the following:

```c
/* component2.c */

struct testobj *obj;

/* using the objid sent from the client */
obj = shm_bm_take_testobj(shm, objid);

```

All objects allocted from the shared memory region have a reference count. `shm_bm_take_{name}` and `shm_bm_alloc_{name}` increment the reference count for the object they are allocating/taking a reference to. This means that all components that make these calls must free the object in order for the memory to be reclaimed. The following code frees an object in a shared memory region:

```c
shm_bm_free_testobj(obj);
```

Note that the free interface does not require knowledge of which shared memory region it came from; this is by design. All shared memory regions created by `shm_bm_create_{name}` are aligned in the components' virtual address space on a power-of-2 alignment. This alignment is specified in `shm_bm.h` by `SHM_BM_ALIGN`. As such, `shm_bm_free_{name}` can get a pointer to the header of the shared memory region by masking out the bits of the address less significant than the alignment. `shm_bm_free_{name}` decrements the reference count of the object, and afterwards if the reference count is zero, it marks the object as free for reallocation.

There are instances where a component might want to avoid the overhead of updating the reference count and having to free an object that it is borrowing from another component. The following call will allow the server to skip this overhead, with the assumption that it is only borrowing the object for the lifetime of the syncronous call from the other component and the other component is still responsible for freeing:

```c
/* test_server.c */

struct testobj *obj;

/* using the reqtok and objid sent from the client */
obj = (struct testobj *) shm_bm_borrow_testobj(reqtok, objid);

```

Similarily, if a component would like to completely transfer ownership of an object from itself to the another component, with the assumption that the former component will no longer use the object and the latter component will now be responsible for freeing, the component that wants to take ownership of the object can make the following call:

```c
/* test_server.c */

struct testobj *obj;

/* using the reqtok and objid sent from the client */
obj = (struct testobj *) shm_bm_transfer_testobj(reqtok, objid);

```

### Interface Documentation

```c
SHM_BM_INTERFACE_CREATE(name, objsz, nobj)	
```
Preprocessor macro that creates a shm_bm interface for {name}. Must be called in a header file shared by the server and clients that will use the interface
- (param) `name`: the name of the interface
- (param) `objsz`: the size of objects allocated from the shared memory
- (param) `nobj`: the maximum number of objects that can be allocated at once from the shared memory


```c
size_t shm_bm_size_{name}(void);
```
Computes the memory size required for the {name} interface. 
- (returns) the size of memory required for the interface in bytes.


```c
shm_bm_t shm_bm_create_{name}(void *mem, size_t memsz);
```
Creates a `shm_bm_t` that allows the interface to manage the shared memory pointed to by `mem`. Returns 0 if the interface is unable to manage the inputted memory.
- (param) `mem`: a pointer to the shared memory to manage.
- (param) `memsz`: the size (in bytes) of the inputted memory.
- (returns) a `shm_bm_t` value that the component can use to allocate from the shared memory


```c
void shm_bm_init_{name}(shm_bm_t shm);
```
Initializes the shared memory to be used to allocate objects. This includes zeroing out the data memory and initialzing the internal object-tracking data structures (bitmap and refcnts).
- (param) `shm`: the shared memory region to initialize


```c
void *shm_bm_alloc_{name}(shm_bm_t shm, shm_bm_objid_t *objid);
```
Allocates an object from the shared memory region referenced by `shm`. 
- (param) `shm`: the shared memory region to allocate from.
- (returns) a pointer to the allocated object, `NULL` if no more objects to allocate
- (returns) a `shm_bm_objid_t` value that identifies the allocated object


```c
void *shm_bm_take_{name}(shm_bm_t shm, shm_objid_t objid);
```
Gets a pointer to the object identified by `objid` in the shared memory region. The server must free the object before the pointer goes out of scope.
- (param) `shm`:  the shared memory region the object was allocate d from
- (param) `objid`: identifier for the object in the shared memory region to access
- (returns) a pointer to the object in the component's VAS. Returns NULL if the the object identified by `objid` has not been allocated or if `objid` is invalid.


```c
void *shm_bm_borrow_{name}(shm_reqtok_t reqtok, shm_objid_t objid);
```
Same as `shm_bm_take_{name}` but does not increment the reference count of the object, meaning the calling component does not need to free the pointer. Used if the lifetime of the component's access to the object is limited and the component wants to avoid the overhead of having to free the pointer.
- (param) `shm`:  the shared memory region the object was allocate d from
- (param) `objid`: identifier for the object in the shared memory region to access
- (returns) a pointer to the object in the component's VAS. Returns NULL if the the object identified by `objid` has not been allocated or if `objid` is invalid.


```c
void *shm_bm_srv_transfer_{name}(shm_reqtok_t reqtok, shm_objid_t objid);
```
Same as `shm_bm_take_{name}` but does not increment the reference count of the object, meaning the calling component is now the owner of the object. The assumes the component that send the `objid` reliquishes its access to the object and that the calling component is responsible for freeing the pointer.
- (param) `shm`:  the shared memory region the object was allocate d from
- (param) `objid`: identifier for the object in the shared memory region to access
- (returns) a pointer to the object in the component's VAS. Returns NULL if the the object identified by `objid` has not been allocated or if `objid` is invalid.


```c
void shm_bm_free_{name}(void *ptr);
```
Decrements the reference count of the object referenced by `ptr`. If there are no more reference to the object, the memory is marked for reallocation.
- (param) `ptr`: A pointer to the object to free.