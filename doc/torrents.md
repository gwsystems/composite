# The Torrent Interface

*Composite* supports first-class, component-based system design.  A significant benefit of system development in this style is the generality of system construction:  a single component is a unit of functionality, and is abstracted behind its interfaces and its interface dependencies, thus can be inserted into a component composition as long as all interfaces match up.  This can support a form of polymorphism whereby a more general interface can be substituted where a less general interface is required.

The act of composing a software system from disjoint components has significant promise for many reasons including the traditional ones:  code reuse, system flexibility and customizability, efficiency of resource management, and minimality of the resulting system.  However, for these benefits to be actualized, a number of key interfaces must be pervasive throughout the system.  If each component exports a different interface, then system customizability is significantly restricted.  A strong counter-example is the UNIX command line which has the most permissive interface: streams of arbitrary text.  Such an interface is perhaps too permissive.  It would be difficult to implement efficient, low-level services using it, and we would lose significant type-checking power to statically determine if a composition even makes sense (i.e. if a client and server component believe a given interface provides the same service).

The `torrent` interface is our solution to this problem.  This document briefly outlines and introduces this interface.  More details will be provided later, but for now, anyone wishing to use *composite* should be familiar with this interface.

## The Functional Specification

What follows is a _future_ version of the `torrent` interface that fixes all current bugs.  I think that it is important to understand how it is intended to be used, and later we will detail how the current implementation is different from this final destination interface. 

First, `torrents` address resources within a hierarchical namespace, similar to a file system hierarchy.  This is not telling the entire story, and there is a much more generic way to actually view addressing in the system, but I'll ignore this here.  Each component that exports a `torrent` interface must define how this namespace is used.  The most trivial torrent exposes an empty namespace (`/`).  This, for instance, is applicable for device drivers that don't discriminate between different torrents/connections -- they just send and receive packets.

```C
td_t tsplit(spdid_t spdid, td_t tid, char *param, size_t len, tor_flags_t tflags, long evtid);
void trelease(td_t tid);
int tmerge(td_t td, td_t td_into, char *param, size_t len);

ssize_t tread(td_t td, cbufp_t *cb, int *off);
ssize_t twrite(td_t td, cbufp_t cbid, int off, size_t sz);
ssize_t treadv(td_t td, agg_cbuf_t *cbvect);
ssize_t twritev(td_t td, agg_cbuf_t *cbvect, size_t sz);

td_t tcall(td_t td, char *param, size_t len, evt_t e, agg_cbuf_t *cbvect);

ssize_t trmeta(td_t td, const char *key, size_t klen, char *retval, size_t max_rval_len);
int twmeta(td_t td, const char *key, size_t klen, const char *val, size_t vlen);
```

I'll break this interface into four sections:

- `tsplit`, `trelease`, `tmerge` are functions for manipulating torrents.  They create torrents from an existing torrent (like `accept`), release the resource (like `close`), or removing the underlying resource backing a torrent (like `unlink`).
- `tread`, `twrite`, `treadv`, and `twritev` are used to read and write to the resources backing the torrents.  The non-`*v` variants read and write `cbuf`s from the resource.  This is the equivalent of `read` and `write` in UNIX, are implemented with zero-copy.  The `*v` variants can be used to return an aggregate `cbuf` which could represent the _entire file_ (for a file-based resource) in one call (and are closer to the UNIX `readv` system calls).
- `tcall` is a representative function that is a composite of other `torrent` functions.  It essentially performs the following sequence: `tsplit -> treadv -> trelease`.  If at any point, the next operation is not ready (i.e. the resource is not immediately available), then the torrent is returned representing the resource.  The event will be triggered when it is available.  There are other functions that represent common composite torrent operations, but we only discuss `tcall` here.
- `t*meta` reads or writes meta-data associated with a given torrent.  This enables the equivalent of `fstat` and `lseek`.

There are two main supporting components for torrents: `cbuf`s and `evt`s.

### `cbuf`s: *Composite* Buffers

Here we discuss two forms of data movement between components, `cbuf`s and aggregate `cbuf`s.  Specifically, we will discuss persistent cbufs, which impose no lifetime requirements on the data (i.e. it can be referenced by a component at times unrelated to function call lifetimes).  Aggregate cbufs are simply arrays of `<cbuf id, offset, size>` tuples.  These tuples correspond to contiguous parts of a file (for a file-based resource).

`cbuf`s are an efficient, predictable, cached, cross-domain, shared memory system.  They provide a simple API to create buffers, send them to other components via a cbuf identifier, means to convert that identifier to the shared memory (that is read-only at that point), and to dereference that cbuf when a component is done with it.  They are used as the underlying data-passing medium, where-as the torrent API is used for control-mediation.

### `evt`s: Event identifiers

The entire `torrent` interface is asynchronous.  Threads don't block on any of the function calls.  Instead, events are used to enable threads to know when resources are available.  The primary functions of the event interface is to provide an `evt_t` identifier that can be blocked on waiting for an event, and triggered by the component providing the resource to wake-up the thread.

The event component is somewhat complex in that it supports event groups and functionality similar to `epoll`, but we will avoid that complexity here, and discuss the simple interface:  `evt_create`, `evt_free`, `evt_wait`, and `evt_trigger`.  The first two are fairly self explanatory.  The last two enable the thread to block waiting, and to trigger a specific event.  The added complexity of the full interface is only concerned with enabling a thread to block waiting for one of multiple event sources.

# The Current `torrent` Interface

The current API has a number of changes from the API proposed above.

1. `tread` and `twrite` use transient memory cbufs.  They should *not be used* by any new components.
2. `treadp` and `twritep` are used instead.
3. The `spdid` argument heads each function.  We don't use this, and it is _not_ a good access control mechanism, thus needs replacement.
4. `ssize_t` and `size_t` are `int` instead.  Just a cleanup.
5. We don't currently have aggregate cbufs!
6. `tcall` and its future brethren don't yet exist.
