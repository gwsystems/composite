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

use compose::Compose;
use initargs::ArgsKV;

// Create the program that contains all of the arguments to the booter
pub fn booter_serialize_args(comp: &Compose) -> String {
    let mut sinvs = Vec::new();
    let mut ids = Vec::new();

    for s in comp.sinvs().iter() {
        let mut sinv = Vec::new();
        sinv.push(ArgsKV::new_key(String::from("name"), s.name.clone()));
        sinv.push(ArgsKV::new_key(String::from("client"), String::from(format!("{}", s.client))));
        sinv.push(ArgsKV::new_key(String::from("server"), String::from(format!("{}", s.server))));
        sinv.push(ArgsKV::new_key(String::from("c_fn_addr"), String::from(format!("{}", s.c_fn_addr))));
        sinv.push(ArgsKV::new_key(String::from("c_ucap_addr"), String::from(format!("{}", s.c_ucap_addr))));
        sinv.push(ArgsKV::new_key(String::from("s_fn_addr"), String::from(format!("{}", s.s_fn_addr))));

        // Just an array of each of the maps for each sinv.  Arrays
        // have "_" keys (see initargs.h).
        sinvs.push(ArgsKV::new_arr(String::from("_"), sinv));
    }

    comp.ids().iter().for_each(|(n, (img, id))| ids.push(ArgsKV::new_key(format!("{}", id), img.clone())));

    let mut topkv = Vec::new();
    topkv.push(ArgsKV::new_arr(String::from("sinvs"), sinvs));
    topkv.push(ArgsKV::new_arr(String::from("components"), ids));

    let top = ArgsKV::new_top(topkv);

    top.serialize()
}
