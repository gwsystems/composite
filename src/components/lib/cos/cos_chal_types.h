#pragma once

/* x86_64 prot_domain_t bits:
 *
 * |000......000|  pcid  |mpk key|
 * |31........16|15.....4|3.....0|
 */
#define PROTDOM_MPK_KEY(prot_domain) ((prot_domain) & 0xF)
#define PROTDOM_ASID(prot_domain) (((prot_domain) >> 4) & 0xFFF)
#define PROTDOM_INIT(asid, mpk_key) ((prot_domain_t)((asid << 4) | mpk_key))

typedef unsigned int  prot_domain_tag_t;  /* additional architecture protection domain context (e.g. PKRU, ASID) */
