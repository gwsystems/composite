#ifndef COS_STUB_X86_64_H
#define COS_STUB_X86_64_H

typedef word_t (*callgate_fn_t)(word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2);

static word_t
cos_ul_sinv(callgate_fn_t *callgate, word_t p0, word_t p1, word_t p2, word_t p3)
{
    word_t r1, r2;
    return ((callgate_fn_t)callgate)(p0, p1, p2, p3, &r1, &r2);
}

static word_t
cos_ul_sinv_2rets(void *callgate, word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)
{
    return ((callgate_fn_t)callgate)(p0, p1, p2, p3, r1, r2);
}

/* TODO: some docs */
#define COS_SINV(ucap, args...) \
    ((likely(ucap->data)) ? (cos_ul_sinv(callgate, args)) : (word_t)cos_sinv(ucap->cap_no, args));

#define COS_SINV_2RETS(ucap, args...) \
	((likely(ucap->data)) ? (cos_ul_sinv_2rets(callgate, args)) : (word_t)cos_sinv_2rets(ucap->cap_no, args));

/*
 * Macros for defining C stubs for the client and server. These
 * functions are called through the PLT-like level of indirection on
 * interface functions that ensure the the invocation information
 * (struct usr_inv_cap) which includes the capability id.
 */
#define COS_CLIENT_STUB(_type, _client_fn, ...) _type __cosrt_c_##_client_fn(__VA_ARGS__)

#define COS_CLIENT_INVCAP \
    register struct usr_inv_cap *uc asm ("rax"); \
    register callgate_fn_t callgate asm ("r11");    

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
