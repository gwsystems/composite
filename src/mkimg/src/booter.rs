// The booter requires information about the components that exist in
// the system, and how to link (via sinvs), and load them.  It also
// requires the components themselves. Instead of defining a binary
// format that would need to stay consistent between rust and C, we're
// going to generate C code with very few dependencies, and
// compile/link in into the booter.
//
// All of the components and their schedule/ids are serialized into
// the "components" key with an array of key = compname, val = id
// pairs.  The ids correspond to the initialization schedule (higher
// id values should be initialized first).  The "sinv" key is an array
// of {name, client id, server id, client fn addr, client ucap addr,
// server fn addr} tuples.
//
// The component objects are part of a tarball that is accessed via
// the same k/v space.  The "objects" key indexes the array of actual
// value space objects.
