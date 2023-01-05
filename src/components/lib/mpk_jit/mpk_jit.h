#include "cos_types.h"
#include "cos_debug.h"

static int
mpk_jit_search(u8_t *src, u8_t *pat, size_t len, size_t max)
{
	unsigned int i, j;

	/* naive pattern search */
	for (i = 0; i < max; i++) {
		for (j = 0; j < len; j++) {
			if (src[i + j] != pat[j]) break;
		}
 
		if (j == len) return i;
	}

	return -1;
}

static int 
mpk_jit_replace(u8_t *src, u8_t *orig, u8_t *replace, size_t len, size_t src_len)
{
	unsigned int i;
	int pos;

	pos = mpk_jit_search(src, orig, len, src_len);
	if (pos == -1) return 0;

	for (i = 0; i < len; i++) {
		src[pos + i] = replace[i];
	}
	
	return pos;
}

#define JIT_TOK_PLACEHOLDER   0xDEADBEEFDEADBEEFUL
#define JIT_INV_PLACEHOLDER   0x0123456789ABCDEFUL
#define JIT_MPK_PLACEHOLDER   0xFFFFFFFEU
#define JIT_SRVFN_PLACEHOLDER 0x1212121212121212UL

/* FIXME: automate this value */
#define JIT_CALLGATE_LEN_BYTES 550

static void
mpk_jit_jitcallgate(vaddr_t callgate, vaddr_t server_fn, u32_t cli_pkey, u32_t srv_pkey, u64_t cli_tok, u64_t srv_tok, invtoken_t inv_tok, sinvcap_t cap_no)
{
	u64_t tok_placeholder = JIT_TOK_PLACEHOLDER;
	u64_t inv_placeholder = JIT_INV_PLACEHOLDER;
	u32_t mpk_placeholder = JIT_MPK_PLACEHOLDER;
	u64_t srvfn_placeholder = JIT_SRVFN_PLACEHOLDER;

	/* 
	 * FIXME: 
	 *      1) Error handling. There are a few ways this could go wrong
	 *         if one of these values is in the compiled binary already.
	 *      2) Statically determine these offsets to avoid searching which
	 *         could caused the errors in (1).
     */
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&tok_placeholder, (u8_t *)&cli_tok, sizeof(u64_t), JIT_CALLGATE_LEN_BYTES)) BUG();
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&tok_placeholder, (u8_t *)&cli_tok, sizeof(u64_t), JIT_CALLGATE_LEN_BYTES)) BUG();
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&tok_placeholder, (u8_t *)&srv_tok, sizeof(u64_t), JIT_CALLGATE_LEN_BYTES)) BUG();
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&tok_placeholder, (u8_t *)&srv_tok, sizeof(u64_t), JIT_CALLGATE_LEN_BYTES)) BUG();
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&inv_placeholder, (u8_t *)&cap_no,  sizeof(u64_t), JIT_CALLGATE_LEN_BYTES)) BUG();
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&inv_placeholder, (u8_t *)&inv_tok, sizeof(u64_t), JIT_CALLGATE_LEN_BYTES)) BUG();
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&mpk_placeholder, (u8_t *)&srv_pkey, sizeof(u32_t), JIT_CALLGATE_LEN_BYTES)) BUG();
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&mpk_placeholder, (u8_t *)&cli_pkey, sizeof(u32_t), JIT_CALLGATE_LEN_BYTES)) BUG();
	if (!mpk_jit_replace((u8_t *)callgate, (u8_t *)&srvfn_placeholder, (u8_t *)&server_fn, sizeof(u64_t), JIT_CALLGATE_LEN_BYTES)) BUG();

}