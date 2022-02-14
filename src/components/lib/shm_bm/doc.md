## shm_bm

### Description

This library provides a slab-like memory allocator interface to allocate fixed-size blocks of memory from a region of memory shared between a server component and a number of client components. This allows the passing of user-defined objects that do not fit into syncronous invocation calls between a client and server with minimal additional overhead.

### Usage and Assumptions

Most of the code for this library is generated at compile-time by the preprocessor for performance. The compiler can make significant optimizations to the allocation, freeing, and address space translation routines by knowing the size of the memory available and the size of individual allocations. As such, the code in `shm_bm.h` provides little documentation because all of the user-facing code is preprocessor generated.

This section provides general usecase documentation. For full interface documentation, skip to the end of this document.

The interface for a particular user-defined object must be created at compile-time. The following code creates an interface that can allocate the `testobj` struct. This declaration must be in a header file that is shared by both the server component and all the client components.

```c
/* test_interface.h */ 

#include <shm_bm.h>

struct testobj {
    int  id;
    char name[12];
}

SHM_BM_INTERFACE_CREATE(testobj, sizeof (struct obj_test), 2048);

```

The `SHM_BM_INTERFACE_CREATE` takes 3 parameters: the name of the interface, the size of the object that is being allocated, and the maximum number of objects that can be allocated at once (the size of the shared memory region).

The server also has metadata that must be initialized at compile-time using `SHM_BM_SERVER_INIT` which takes the name of the interface to initialize:

```c
/* test_server.c */

#include <test_interface.h>

SHM_BM_SERVER_INIT(testobj);

```

Note that the name parameter must match the name of the interface created using `SHM_BM_INTERFACE_CREATE`.

The interface is now properly created and initialized and can be used by both client components and the server component. All functions in the interface end with the name of the interface:

```c						
shm_bm_clt_create_testobj(shm_bm_t *shm);
shm_bm_clt_alloc_testobj(shm_bm_t shm, shm_objid_t *objid);

shm_bm_srv_map_testobj(cbuf_t id);
shm_bm_srv_take_testobj(shm_reqtok_t reqtok, shm_objid_t objid);
shm_bm_srv_borrow_testobj(shm_reqtok_t reqtok, shm_objid_t objid);
shm_bm_srv_transfer_testobj(shm_reqtok_t reqtok, shm_objid_t objid);

shm_bm_free_testobj(void *ptr);
```

These functions are created by the preprocessor. The following typedefs are used by all instances of the interface:

- `shm_bm_t`: An opaque reference for a client to the shared memory region it creates. The client gets a unique `shm_bm_t` for every shared memory region it creates. 

- `shm_reqtok_t`: A token that a client can use to make requests to the server to do something with an object that it has allocated. The client gets a token for every shared memory region it maps to the server's address space. The token is used by the server to determine which shared memory region to get an object from.

- `shm_objid_t`: An identifer for an object in the shared memory region. When a client allocates an object from a shared memory region, it gets a `shm_objid_t` that can be used by the server to reference that object.

A client can create a shared memory region using the `testobj` interface using the following call:

```c
/* test_client.c */

shm_bm_t     shm;
shm_reqtok_t reqtok;
cbuf_t       id;

id = shm_bm_clt_create_testobj(&shm);
/* send id to server in return for a shm_reqtok_t */
```

The `cbuf_t` that is returned can be used to map the shared memory region into the server component. This identifier can be sent over syncronous invocation to the server for the server to map:

```c
/* test_server.c */

/* using id sent from client */
shm_reqtok_t reqtok = shm_bm_srv_map(id)
/* return reqtok to client */
```

The `shm_reqtok_t` should be returned to the client over the syncronouse invocation call for the client to use. The client can now use this shared memory region to allocate `testobj`s:

```c
/* test_client.c */

shm_objid_t     objid;
struct testobj *obj;

obj = (struct testobj *) shm_bm_clt_alloc_testobj(shm, &objid);
/* ... do something with obj */

/* send reqtok and objid to the server for server to use object */
```

The server can use the object that the client allocated using both the `reqtok` and `objid` sent from the client. The server looks up which shared memory region the object belongs to using the `reqtok`. Note that the client that sent the request token must be the one that recieved that request token. The server authenticates the component to the shared memory region for the request token using the component's invocation token, and will return 0 if the authentication fails. This means that clients can not make requests for the server to use shared memory regions of other clients. The following call allows the server to get a pointer to the object sent from the client:

```c
/* test_server.c */

struct testobj *obj;

/* using the reqtok and objid sent from the client */
obj = (struct testobj *) shm_bm_srv_take_testobj(reqtok, objid);

```

All objects allocted from the shared memory region have a reference count. `shm_bm_srv_take_testobj` increments the reference count for the object it is taking a reference to. This means that the server and client must free the object in order for the memory to be reclaimed. The following code frees an object in a shared memory region and can be called by either the client or server:

```c
shm_bm_free_testobj(obj);
```

Note that the free interface does not require knowledge of which shared memory region it came from; this is by design. All shared memory regions created by `shm_bm_create_{name}` are aligned in the components' virtual address space on a power-of-2 alignment. This alignment is specified in `shm_bm.h` by `SHM_BM_ALIGN`. As such, `shm_bm_free_{name}` can get a pointer to the header of the shared memory region by masking out the bits of the address less significant than the alignment. `shm_bm_free_{name}` decrements the reference count of the object, and afterwards if the reference count is zero, it marks the object as free for reallocation.

There are instances where the server might want to avoid the overhead of updating the reference count and having to free an object that it is borrowing from a client. The following call will allow the server to skip this overhead, with the assumption that it is only borrowing the object for the lifetime of the syncronous call from the client and the client is still responsible for freeing:

```c
/* test_server.c */

struct testobj *obj;

/* using the reqtok and objid sent from the client */
obj = (struct testobj *) shm_bm_srv_borrow_testobj(reqtok, objid);

```

Similarily, if the client would like to completely transfer ownership of an object from itself to the server, with the assumption that the client will no longer use the object and the server will be responsible for freeing, the server can make the following call:

```c
/* test_server.c */

struct testobj *obj;

/* using the reqtok and objid sent from the client */
obj = (struct testobj *) shm_bm_srv_transfer_testobj(reqtok, objid);

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
SHM_BM_SERVER_INIT(name)
```
Preprocessor macro the initialize the server metadata for the {name} interface. Must be called globally in the server's code.
- (param) `name`: name of the interface to initialize, must match name in call to `SHM_BM_INTERFACE_CREATE`


```c
cbuf_t shm_bm_clt_create_{name}(shm_bm_t *shm);
```
Creates a shared shared memory region to alloc objects for the {name} interface. Shared memory regions are aligned on a boundary specified by `SHM_BM_ALIGN`.
- (returns) a `cbuf_t` that identifies the shared memory, used to map to the server's VAS.
- (returns) a `shm_bm_t` value that the client can use to allocate from the shared memory


```c
void *shm_bm_clt_alloc_{name}(shm_bm_t shm, shm_objid_t *objid);
```
Allocates an object from the shared memory region referenced by `shm`. 
- (param) `shm`: the shared memory region created by the client to allocate from.
- (returns) a pointer to the allocated object, 0 if no more objects to allocate
- (returns) a `shm_objid_t` value that identifies the allocated object


```c
shm_reqtok_t shm_bm_srv_map_{name}(cbuf_t id);
```
Maps the shared memory region identified by `id` into the server's address space and creates an entry in its service table for the shared memory region. The server uses this table to track shared memory regions it is managing and authenticate components when they want the server to access their shared memory. The server generates a request token that the component can use to make requests of the server to use the shared memory that is being mapped.
- (param) `id`: a `cbuf_t` identifing the shared memory allocated by the `memmgr`.
- (returns) a `shm_reqtok_t` that the client who created the shared memory region can use to ask the server to access it.


```c
void *shm_bm_srv_take_{name}(shm_reqtok_t reqtok, shm_objid_t objid);
```
Gets a pointer to the object identified by `objid` in the shared memory region that the request token `reqtok` accesses and incrementes the reference count of the object. The client that invoked the server with the request token is authenticated by the server, so only the client that was granted the request token make the request. The server must free the object before the pointer goes out of scope.
- (param) `reqtok`: request token to access a shared memory region
- (param) `objid`: identifier for the object in the shared memory region to access
- (returns) a pointer to the object in the server's VAS. Returns 0 if the server fails to authenticate the client to the shared memory (the client was not the one that received the request token) or the object identified by `objid` has not been allocated.


```c
void *shm_bm_srv_borrow_{name}(shm_reqtok_t reqtok, shm_objid_t objid);
```
Same as `shm_bm_srv_take_{name}` but does not increment the reference count of the object, meaning the server does not need to free the pointer. Used if the lifetime of the server's access to the object is limited and the server wants to avoid the overhead of having to free the pointer. The client must still free the pointer.
- (param) `reqtok`: request token to access a shared memory region
- (param) `objid`: identifier for the object in the shared memory region to access
- (returns) a pointer to the object in the server's VAS. Returns 0 if the server fails to authenticate the client to the shared memory (the client was not the one that received the request token) or the object identified by `objid` has not been allocated.


```c
void *shm_bm_srv_transfer_{name}(shm_reqtok_t reqtok, shm_objid_t objid);
```
Same as `shm_bm_srv_take_{name}` but does not increment the reference count of the object, meaning the server is now the owner of the object. The assumes the client reliquishes its access to the object and that the server is responsible for freeing the pointer.
- (param) `reqtok`: request token to access a shared memory region
- (param) `objid`: identifier for the object in the shared memory region to access
- (returns) a pointer to the object in the server's VAS. Returns 0 if the server fails to authenticate the client to the shared memory (the client was not the one that received the request token) or the object identified by `objid` has not been allocated.


```c
void shm_bm_free_{name}(void *ptr);
```
Decrements the reference count of the object referenced by `ptr`. If there are no more reference to the object, the memory is marked for reallocation.
- (param) `ptr`: A pointer to the object to free.