#[derive(Debug, Clone)]
enum ArgsValType {
    Str(String),
    Arr(Vec<ArgsKV>)
}

#[derive(Debug, Clone)]
pub struct ArgsKV {
    key: String,
    val: ArgsValType
}

struct VarNamespace {
    id: u32
}

impl VarNamespace {
    fn new() -> VarNamespace {
        VarNamespace {
            id: 0
        }
    }

    fn fresh_name(&mut self) -> String {
        let id = self.id;
        self.id = self.id + 1;
        format!("__initargs_autogen_{}", id)
    }
}

impl ArgsKV {
    pub fn new_key(key: String, val: String) -> ArgsKV {
        ArgsKV { key: key, val: ArgsValType::Str(val) }
    }

    pub fn new_arr(key: String, val: Vec<ArgsKV>) -> ArgsKV {
        ArgsKV { key: key, val: ArgsValType::Arr(val) }
    }

    pub fn new_top(val: Vec<ArgsKV>) -> ArgsKV {
        ArgsKV { key: String::from("_"), val: ArgsValType::Arr(val) }
    }

    // This provides code generation for the data-structure containing
    // the initial arguments for the component.  Return a string
    // accumulating new definitions, and another accumulating arrays.
    fn serialize_rec(&self, ns: &mut VarNamespace) -> (String, Vec<String>) {
        match &self {
            ArgsKV { key: k, val: ArgsValType::Str(ref s) } => { // base case
                let kv_name = ns.fresh_name();
                (format!(r#"static struct kv_entry {} = {{ key: "{}", vtype: VTYPE_STR, val: {{ str: "{}" }} }};
"#, kv_name, k, s),
                 vec![format!("&{}", kv_name)])
            },
            ArgsKV { key: k, val: ArgsValType::Arr(ref kvs) } => {
                let arr_val_name = ns.fresh_name(); // the array value structure
                let arr_name = ns.fresh_name();     // the actual array
                // recursive call to serialize all nested K/Vs
                let strs = kvs.iter().fold((String::from(""), Vec::new()), |(t,s), kv| {
                    let (t1, s1) = kv.serialize_rec(ns);

                    let mut exprs = Vec::new();
                    exprs.extend(s1);
                    exprs.extend(s);

                    (format!("{}{}", t, t1), exprs)
                });
                (format!(r#"{}static struct kv_entry *{}[] = {{{}}};
static struct kv_entry {} = {{ key: "{}", vtype: VTYPE_ARR, val: {{ arr: {{ sz: {}, kvs: {} }} }} }};
"#,
                         strs.0, arr_name, strs.1.join(", "), arr_val_name, k, kvs.len(), arr_name),
                 vec![format!("&{}", arr_val_name)])
            }
        }
    }

    // Generate the c data-structure for the initial arguments to be
    // paired with the cosargs library
    pub fn serialize(&self) -> String {
        let mut ns = VarNamespace::new();

        format!("#include <initargs.h>
{}
struct initargs __initargs_root = {{ type: ARGS_IMPL_KV, d: {{ kv_ent: &__initargs_autogen_0 }} }};", self.serialize_rec(&mut ns).0)
    }
}
