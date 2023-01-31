#ifndef COS_STUB_ARM_H
#define COS_STUB_ARM_H

/*
 * Macros for defining C stubs for the client and server. These
 * functions are called through the PLT-like level of indirection on
 * interface functions that ensure the the invocation information
 * (struct usr_inv_cap) which includes the capability id.
 */
#define COS_CLIENT_STUB(_type, _client_fn, ...) _type __cosrt_c_##_client_fn(__VA_ARGS__)

#define COS_CLIENT_INVCAP struct usr_inv_cap *uc = cos_inv_cap();

/*
 * It looks like ARM DWORD representation in registers (and on stack):
 * DWORD -> rE = DWORDLO, rO = DWORDHI
 * rE -> even numbered register, rO -> odd numbered register
 *
 * ex: x = (1<<32) | 2;
 *     fn (x);
 *     register layout inside fn will be: r0 <- 2, r1 <- 1
 * 
 * - For server side (without stubs), ABI takes care of it if we marshal 
 *    that dword in the registers correctly.. 
 * - For the server stubs we have, we demarshal ourselves, so the macros here takes
 *    good care of that.. 
 * - For some arguments, we don't rely on ABI in anyway, in such cases, we should not use these macros, instead have generic macros for dword to word conversions..
 */

#define COS_ARG_DWORD_TO_WORD(dw, wh, wl)		\
		wh = (dw << 32) >> 32;			\
		wl = (dw >> 32)	

#define COS_ARG_WORDS_TO_DWORD(wa, wb, dw)		\
		dw = ((dword_t)wb << 32) | (dword_t)wa

#endif /* COS_STUB_ARM_H */
