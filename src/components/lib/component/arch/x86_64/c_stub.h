#ifndef COS_STUB_X86_64_H
#define COS_STUB_X86_64_H


/*
 * Macros for defining C stubs for the client and server. These
 * functions are called through the PLT-like level of indirection on
 * interface functions that ensure the the invocation information
 * (struct usr_inv_cap) which includes the capability id.
 */
#define COS_CLIENT_STUB(_type, _client_fn, ...) _type __cosrt_c_##_client_fn(__VA_ARGS__)

#define COS_CLIENT_INVCAP register struct usr_inv_cap *uc asm ("rax");

/*
 * We marshal and demarshal double word arguments..
 * Nothing special to do for this architecture.
 */
#define COS_ARG_DWORD_TO_WORD(dw, wh, wl)               \
                wl = 0;                  \
                wh = dw

#define COS_ARG_WORDS_TO_DWORD(wa, wb, dw)              \
                dw = wa

#endif /* COS_STUB_X86_H */
