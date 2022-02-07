## shm_bm

### Description

This library provides a simple interface for shared memory allocation to faciliate the passing of messages/data between components that do not fit into synchronous invocation calls.

### Usage and Assumptions

This library provides an interface for a slab-like allocator of a shared memory pool to facilitate message passing between components. A shared memory region is instantiated with the ability to allocate objects of a fixed size. 

Internally, objects are stored in a power of 2 amount of space. This is to make resolving pointers from the bitmap faster using bit shits instead of multiplication and division. If an shared memory region is created to allocate objects that are not of a power of 2 size, the size of the object is rounded up to the nearest power of 2.  

The shared memory region is aligned in a component's VAS on a power of 2 boundry. This is in order to provide a free API that does not require a reference to the shared memory header. To resolve the pointer to the shared memory region header from a pointer to an object in the region, the bits of the address that are less significant than the allocation boundry are masked out. 

Creating a shared memory region with space to allocate 64 user-defined objects:
```c
struct obj {
    int  id;
    char string[10];
};

shm_bm_t shm;
cbuf_t   id;

id = shm_bm_create(&shm, sizeof (struct obj), 64);
```

`shm` is an opaque identifier for the shared memory that is used by the library.The cbuf identifier `id` that is returned is used to map this shared memory pool into another component. This component can send the identifier to another component using a syncronous invocation, and the other component can make the following call to get a `shm_bm_t` that identifies the shared memory pool in the calling component's address space:

```c
shm_bm_t shm = shm_bm_map(id);
```

With the shared memory region mapped in a component's address space, either from a call to `shm_bm_create` or `shm_bm_map`, objects can be allocated and freed with: 

```c
shm_bufid_t objid;
struct obj *object;

object = shm_bm_obj_alloc(shm, &id);
...
shm_bm_obj_free(object);
```

The `shm_bufid_t` `objid` is an identifier for the allocated object in the shared memory region that can be used to reference the object accross virtual address spaces. If another component has used `shm_bm_map` to map this shared memory pool into their VAS, they can get a pointer to that object in their own VAS using the identifier:

```c
struct obj *object = shm_bm_obj_use(shm, objid);
```

This call will increment the reference count of the object, meaning that both components need to call `shm_bm_obj_free` on the object in order to free the allocated memory in the pool. Alternatively, a call to:
```c
struct obj *object = shm_bm_obj_take(shm, objid)
```

 will not update the reference count. This can be used to transfer ownership of the object from the allocating component to the calling component, or if the calling component is only momentarily 'borrowing' the object so that the freeing overhead can be avoided.