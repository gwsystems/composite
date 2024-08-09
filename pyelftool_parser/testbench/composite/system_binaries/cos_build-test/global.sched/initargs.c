#include <initargs.h>
static struct kv_entry *__initargs_autogen_3[] = {};
static struct kv_entry __initargs_autogen_2 = { key: "param", vtype: VTYPE_ARR, val: { arr: { sz: 0, kvs: __initargs_autogen_3 } } };
static struct kv_entry __initargs_autogen_6 = { key: "4", vtype: VTYPE_STR, val: { str: "init" } };
static struct kv_entry *__initargs_autogen_5[] = {&__initargs_autogen_6};
static struct kv_entry __initargs_autogen_4 = { key: "execute", vtype: VTYPE_ARR, val: { arr: { sz: 1, kvs: __initargs_autogen_5 } } };
static struct kv_entry __initargs_autogen_7 = { key: "captbl_end", vtype: VTYPE_STR, val: { str: "88" } };
static struct kv_entry __initargs_autogen_8 = { key: "compid", vtype: VTYPE_STR, val: { str: "3" } };
static struct kv_entry *__initargs_autogen_1[] = {&__initargs_autogen_8, &__initargs_autogen_7, &__initargs_autogen_4, &__initargs_autogen_2};
static struct kv_entry __initargs_autogen_0 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 4, kvs: __initargs_autogen_1 } } };

struct initargs __initargs_root = { type: ARGS_IMPL_KV, d: { kv_ent: &__initargs_autogen_0 } };