#ifndef COS_STUB_X86_H
#define COS_STUB_X86_H

/*
 * Macros for defining C stubs for the client and server. These
 * functions are called through the PLT-like level of indirection on
 * interface functions that ensure the the invocation information
 * (struct usr_inv_cap) which includes the capability id.
 */
#define COS_CLIENT_STUB(_type, _client_fn, ...) CREGPARM(1) _type __cosrt_c_##_client_fn(struct usr_inv_cap *uc, __VA_ARGS__)

#define COS_CLIENT_INVCAP

/*
 * We marshal and demarshal double word arguments..
 * Nothing special to do for this architecture.
 */
#define COS_ARG_DWORD_TO_WORD(dw, wh, wl)               \
                wl = (dw << 32) >> 32;                  \
                wh = (dw >> 32)

#define COS_ARG_WORDS_TO_DWORD(wa, wb, dw)              \
                dw = ((dword_t)wa << 32) | (dword_t)wb

#endif /* COS_STUB_X86_H */
