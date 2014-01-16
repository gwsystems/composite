
cbufp.o:     file format elf32-i386


Disassembly of section .text:

00000000 <get_stk_data>:
{
	return cos___trans_cntl(((op << 16) | (channel & 0xFFFF)), addr, off);
}

static inline long get_stk_data(int offset)
{
       0:	55                   	push   %ebp
       1:	89 e5                	mov    %esp,%ebp
       3:	83 ec 10             	sub    $0x10,%esp
	unsigned long curr_stk_pointer;

	asm ("movl %%esp, %0;" : "=r" (curr_stk_pointer));
       6:	89 e0                	mov    %esp,%eax
       8:	89 45 fc             	mov    %eax,-0x4(%ebp)
	 * access.  We want to find the struct cos_stk (see the stkmgr
	 * interface) so that we can then offset into it and get the
	 * cpu_id.  This struct is at the _top_ of the current stack,
	 * and cpu_id is at the top of the struct (it is a u32_t).
	 */
	return *(long *)((curr_stk_pointer & ~(COS_STACK_SZ - 1)) + 
       b:	8b 45 fc             	mov    -0x4(%ebp),%eax
       e:	89 c2                	mov    %eax,%edx
      10:	81 e2 00 f0 ff ff    	and    $0xfffff000,%edx
      16:	8b 45 08             	mov    0x8(%ebp),%eax
			 COS_STACK_SZ - offset * sizeof(u32_t));
      19:	c1 e0 02             	shl    $0x2,%eax
	 * access.  We want to find the struct cos_stk (see the stkmgr
	 * interface) so that we can then offset into it and get the
	 * cpu_id.  This struct is at the _top_ of the current stack,
	 * and cpu_id is at the top of the struct (it is a u32_t).
	 */
	return *(long *)((curr_stk_pointer & ~(COS_STACK_SZ - 1)) + 
      1c:	89 d1                	mov    %edx,%ecx
      1e:	29 c1                	sub    %eax,%ecx
      20:	89 c8                	mov    %ecx,%eax
      22:	05 00 10 00 00       	add    $0x1000,%eax
      27:	8b 00                	mov    (%eax),%eax
			 COS_STACK_SZ - offset * sizeof(u32_t));
}
      29:	c9                   	leave  
      2a:	c3                   	ret    

0000002b <cos_get_thd_id>:
	 */
	return get_stk_data(CPUID_OFFSET);
}

static inline unsigned short int cos_get_thd_id(void)
{
      2b:	55                   	push   %ebp
      2c:	89 e5                	mov    %esp,%ebp
      2e:	83 ec 04             	sub    $0x4,%esp
	/* 
	 * see comments in the get_stk_data above.
	 */
	return get_stk_data(THDID_OFFSET);
      31:	c7 04 24 02 00 00 00 	movl   $0x2,(%esp)
      38:	e8 c3 ff ff ff       	call   0 <get_stk_data>
}
      3d:	c9                   	leave  
      3e:	c3                   	ret    

0000003f <cos_spd_id>:
{
	cos_mpd_cntl(COS_MPD_UPDATE, 0, 0);
}

static inline long cos_spd_id(void)
{
      3f:	55                   	push   %ebp
      40:	89 e5                	mov    %esp,%ebp
	return cos_comp_info.cos_this_spd_id;
      42:	a1 08 00 00 00       	mov    0x8,%eax
}
      47:	5d                   	pop    %ebp
      48:	c3                   	ret    

00000049 <cos_cas>:
	return ret;
}

static inline int 
cos_cas(unsigned long *target, unsigned long cmp, unsigned long updated)
{
      49:	55                   	push   %ebp
      4a:	89 e5                	mov    %esp,%ebp
      4c:	53                   	push   %ebx
      4d:	83 ec 10             	sub    $0x10,%esp
	char z;
	__asm__ __volatile__("lock cmpxchgl %2, %0; setz %1"
      50:	8b 55 08             	mov    0x8(%ebp),%edx
      53:	8b 4d 10             	mov    0x10(%ebp),%ecx
      56:	8b 45 0c             	mov    0xc(%ebp),%eax
      59:	8b 5d 08             	mov    0x8(%ebp),%ebx
      5c:	f0 0f b1 0a          	lock cmpxchg %ecx,(%edx)
      60:	0f 94 c0             	sete   %al
      63:	88 45 fb             	mov    %al,-0x5(%ebp)
			     : "+m" (*target),
			       "=a" (z)
			     : "q"  (updated),
			       "a"  (cmp)
			     : "memory", "cc");
	return (int)z;
      66:	0f be 45 fb          	movsbl -0x5(%ebp),%eax
}
      6a:	83 c4 10             	add    $0x10,%esp
      6d:	5b                   	pop    %ebx
      6e:	5d                   	pop    %ebp
      6f:	c3                   	ret    

00000070 <cos_cas_up>:

/* A uni-processor variant with less overhead but that doesn't
 * guarantee atomicity across cores. */
static inline int 
cos_cas_up(unsigned long *target, unsigned long cmp, unsigned long updated)
{
      70:	55                   	push   %ebp
      71:	89 e5                	mov    %esp,%ebp
      73:	53                   	push   %ebx
      74:	83 ec 10             	sub    $0x10,%esp
	char z;
	__asm__ __volatile__("cmpxchgl %2, %0; setz %1"
      77:	8b 55 08             	mov    0x8(%ebp),%edx
      7a:	8b 4d 10             	mov    0x10(%ebp),%ecx
      7d:	8b 45 0c             	mov    0xc(%ebp),%eax
      80:	8b 5d 08             	mov    0x8(%ebp),%ebx
      83:	0f b1 0a             	cmpxchg %ecx,(%edx)
      86:	0f 94 c0             	sete   %al
      89:	88 45 fb             	mov    %al,-0x5(%ebp)
			     : "+m" (*target),
			       "=a" (z)
			     : "q"  (updated),
			       "a"  (cmp)
			     : "memory", "cc");
	return (int)z;
      8c:	0f be 45 fb          	movsbl -0x5(%ebp),%eax
}
      90:	83 c4 10             	add    $0x10,%esp
      93:	5b                   	pop    %ebx
      94:	5d                   	pop    %ebp
      95:	c3                   	ret    

00000096 <section_fnptrs_execute>:
#define CDTOR __attribute__((destructor)) /* currently unused! */
#define CRECOV(fnname) long crecov_##fnname##_ptr __attribute__((section(".crecov"))) = (long)fnname

static inline void
section_fnptrs_execute(long *list)
{
      96:	55                   	push   %ebp
      97:	89 e5                	mov    %esp,%ebp
      99:	83 ec 18             	sub    $0x18,%esp
	int i;

	for (i = 0 ; i < list[0] ; i++) {
      9c:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
      a3:	eb 1a                	jmp    bf <section_fnptrs_execute+0x29>
		typedef void (*ctors_t)(void);
		ctors_t ctors = (ctors_t)list[i+1];
      a5:	8b 45 f0             	mov    -0x10(%ebp),%eax
      a8:	83 c0 01             	add    $0x1,%eax
      ab:	c1 e0 02             	shl    $0x2,%eax
      ae:	03 45 08             	add    0x8(%ebp),%eax
      b1:	8b 00                	mov    (%eax),%eax
      b3:	89 45 f4             	mov    %eax,-0xc(%ebp)
		ctors();
      b6:	8b 45 f4             	mov    -0xc(%ebp),%eax
      b9:	ff d0                	call   *%eax
static inline void
section_fnptrs_execute(long *list)
{
	int i;

	for (i = 0 ; i < list[0] ; i++) {
      bb:	83 45 f0 01          	addl   $0x1,-0x10(%ebp)
      bf:	8b 45 08             	mov    0x8(%ebp),%eax
      c2:	8b 00                	mov    (%eax),%eax
      c4:	3b 45 f0             	cmp    -0x10(%ebp),%eax
      c7:	7f dc                	jg     a5 <section_fnptrs_execute+0xf>
		typedef void (*ctors_t)(void);
		ctors_t ctors = (ctors_t)list[i+1];
		ctors();
	}
}
      c9:	c9                   	leave  
      ca:	c3                   	ret    

000000cb <constructors_execute>:

static void 
constructors_execute(void)
{
      cb:	55                   	push   %ebp
      cc:	89 e5                	mov    %esp,%ebp
      ce:	83 ec 18             	sub    $0x18,%esp
	extern long __CTOR_LIST__;
	section_fnptrs_execute(&__CTOR_LIST__);
      d1:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
      d8:	e8 b9 ff ff ff       	call   96 <section_fnptrs_execute>
}
      dd:	c9                   	leave  
      de:	c3                   	ret    

000000df <destructors_execute>:
static void 
destructors_execute(void)
{
      df:	55                   	push   %ebp
      e0:	89 e5                	mov    %esp,%ebp
      e2:	83 ec 18             	sub    $0x18,%esp
	extern long __DTOR_LIST__;
	section_fnptrs_execute(&__DTOR_LIST__);
      e5:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
      ec:	e8 a5 ff ff ff       	call   96 <section_fnptrs_execute>
}
      f1:	c9                   	leave  
      f2:	c3                   	ret    

000000f3 <recoveryfns_execute>:
static void 
recoveryfns_execute(void)
{
      f3:	55                   	push   %ebp
      f4:	89 e5                	mov    %esp,%ebp
      f6:	83 ec 18             	sub    $0x18,%esp
	extern long __CRECOV_LIST__;
	section_fnptrs_execute(&__CRECOV_LIST__);
      f9:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
     100:	e8 91 ff ff ff       	call   96 <section_fnptrs_execute>
}
     105:	c9                   	leave  
     106:	c3                   	ret    

00000107 <mman_alias_page>:
int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags); 
/* The invoking component (s_spd) must own the mapping. */
vaddr_t __mman_alias_page(spdid_t s_spd, vaddr_t s_addr, u32_t d_spd_flags, vaddr_t d_addr);
static inline vaddr_t
mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr, int flags)
{ return __mman_alias_page(s_spd, s_addr, ((u32_t)d_spd<<16)|flags, d_addr); }
     107:	55                   	push   %ebp
     108:	89 e5                	mov    %esp,%ebp
     10a:	83 ec 28             	sub    $0x28,%esp
     10d:	8b 55 08             	mov    0x8(%ebp),%edx
     110:	8b 45 10             	mov    0x10(%ebp),%eax
     113:	66 89 55 f4          	mov    %dx,-0xc(%ebp)
     117:	66 89 45 f0          	mov    %ax,-0x10(%ebp)
     11b:	0f b7 45 f0          	movzwl -0x10(%ebp),%eax
     11f:	89 c2                	mov    %eax,%edx
     121:	c1 e2 10             	shl    $0x10,%edx
     124:	8b 45 18             	mov    0x18(%ebp),%eax
     127:	89 d1                	mov    %edx,%ecx
     129:	09 c1                	or     %eax,%ecx
     12b:	0f b7 45 f4          	movzwl -0xc(%ebp),%eax
     12f:	8b 55 14             	mov    0x14(%ebp),%edx
     132:	89 54 24 0c          	mov    %edx,0xc(%esp)
     136:	89 4c 24 08          	mov    %ecx,0x8(%esp)
     13a:	8b 55 0c             	mov    0xc(%ebp),%edx
     13d:	89 54 24 04          	mov    %edx,0x4(%esp)
     141:	89 04 24             	mov    %eax,(%esp)
     144:	e8 fc ff ff ff       	call   145 <mman_alias_page+0x3e>
     149:	c9                   	leave  
     14a:	c3                   	ret    

0000014b <__cos_noret>:
#ifndef assert
/* 
 * Tell the compiler that we will not return, thus it can make the
 * static assertion that the condition is true past the assertion.
 */
__attribute__ ((noreturn)) static inline void __cos_noret(void) { while (1) ; }
     14b:	55                   	push   %ebp
     14c:	89 e5                	mov    %esp,%ebp
     14e:	eb fe                	jmp    14e <__cos_noret+0x3>

00000150 <__cvect_power_2>:
	cvect_t name = {.vect = {{.c.next = NULL}}} 

/* true or false: is v a power of 2 */
static inline int 
__cvect_power_2(const u32_t v)
{
     150:	55                   	push   %ebp
     151:	89 e5                	mov    %esp,%ebp
     153:	83 ec 10             	sub    $0x10,%esp
	/* Assume 2's complement */
	u32_t smallest_set_bit = (v & -v);
     156:	8b 45 08             	mov    0x8(%ebp),%eax
     159:	f7 d8                	neg    %eax
     15b:	23 45 08             	and    0x8(%ebp),%eax
     15e:	89 45 fc             	mov    %eax,-0x4(%ebp)
	return (v > 1 && smallest_set_bit == v);
     161:	83 7d 08 01          	cmpl   $0x1,0x8(%ebp)
     165:	76 0f                	jbe    176 <__cvect_power_2+0x26>
     167:	8b 45 fc             	mov    -0x4(%ebp),%eax
     16a:	3b 45 08             	cmp    0x8(%ebp),%eax
     16d:	75 07                	jne    176 <__cvect_power_2+0x26>
     16f:	b8 01 00 00 00       	mov    $0x1,%eax
     174:	eb 05                	jmp    17b <__cvect_power_2+0x2b>
     176:	b8 00 00 00 00       	mov    $0x0,%eax
}
     17b:	c9                   	leave  
     17c:	c3                   	ret    

0000017d <__cvect_init>:

static inline int 
__cvect_init(cvect_t *v)
{
     17d:	55                   	push   %ebp
     17e:	89 e5                	mov    %esp,%ebp
     180:	83 ec 28             	sub    $0x28,%esp
	int i;

	assert(v);
     183:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     187:	0f 94 c0             	sete   %al
     18a:	0f b6 c0             	movzbl %al,%eax
     18d:	85 c0                	test   %eax,%eax
     18f:	74 1c                	je     1ad <__cvect_init+0x30>
     191:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
     198:	e8 fc ff ff ff       	call   199 <__cvect_init+0x1c>
     19d:	b8 00 00 00 00       	mov    $0x0,%eax
     1a2:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     1a8:	e8 9e ff ff ff       	call   14b <__cos_noret>
	/* should be optimized away by the compiler: */
	assert(__cvect_power_2(CVECT_BASE));
     1ad:	c7 04 24 00 04 00 00 	movl   $0x400,(%esp)
     1b4:	e8 97 ff ff ff       	call   150 <__cvect_power_2>
     1b9:	85 c0                	test   %eax,%eax
     1bb:	0f 94 c0             	sete   %al
     1be:	0f b6 c0             	movzbl %al,%eax
     1c1:	85 c0                	test   %eax,%eax
     1c3:	74 1c                	je     1e1 <__cvect_init+0x64>
     1c5:	c7 04 24 58 00 00 00 	movl   $0x58,(%esp)
     1cc:	e8 fc ff ff ff       	call   1cd <__cvect_init+0x50>
     1d1:	b8 00 00 00 00       	mov    $0x0,%eax
     1d6:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     1dc:	e8 6a ff ff ff       	call   14b <__cos_noret>
	for (i = 0 ; i < (int)CVECT_BASE ; i++) v->vect[i].c.next = NULL;
     1e1:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
     1e8:	eb 11                	jmp    1fb <__cvect_init+0x7e>
     1ea:	8b 55 f4             	mov    -0xc(%ebp),%edx
     1ed:	8b 45 08             	mov    0x8(%ebp),%eax
     1f0:	c7 04 90 00 00 00 00 	movl   $0x0,(%eax,%edx,4)
     1f7:	83 45 f4 01          	addl   $0x1,-0xc(%ebp)
     1fb:	81 7d f4 ff 03 00 00 	cmpl   $0x3ff,-0xc(%ebp)
     202:	7e e6                	jle    1ea <__cvect_init+0x6d>

	return 0;
     204:	b8 00 00 00 00       	mov    $0x0,%eax
}
     209:	c9                   	leave  
     20a:	c3                   	ret    

0000020b <cvect_init>:
	__cvect_init(v);
}

static inline void 
cvect_init(cvect_t *v)
{
     20b:	55                   	push   %ebp
     20c:	89 e5                	mov    %esp,%ebp
     20e:	83 ec 18             	sub    $0x18,%esp
	__cvect_init(v);
     211:	8b 45 08             	mov    0x8(%ebp),%eax
     214:	89 04 24             	mov    %eax,(%esp)
     217:	e8 61 ff ff ff       	call   17d <__cvect_init>
}
     21c:	c9                   	leave  
     21d:	c3                   	ret    

0000021e <cvect_alloc>:

#ifdef CVECT_DYNAMIC

static cvect_t *
cvect_alloc(void)
{
     21e:	55                   	push   %ebp
     21f:	89 e5                	mov    %esp,%ebp
     221:	83 ec 28             	sub    $0x28,%esp
	cvect_t *v;
	
	v = CVECT_ALLOC();
     224:	e8 fc ff ff ff       	call   225 <cvect_alloc+0x7>
     229:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (NULL == v) return NULL;
     22c:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
     230:	75 07                	jne    239 <cvect_alloc+0x1b>
     232:	b8 00 00 00 00       	mov    $0x0,%eax
     237:	eb 0e                	jmp    247 <cvect_alloc+0x29>
	cvect_init(v);
     239:	8b 45 f4             	mov    -0xc(%ebp),%eax
     23c:	89 04 24             	mov    %eax,(%esp)
     23f:	e8 c7 ff ff ff       	call   20b <cvect_init>

	return v;
     244:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
     247:	c9                   	leave  
     248:	c3                   	ret    

00000249 <__cvect_free_rec>:
 * deallocated (i.e. at depth CVECT_BASE, all values are set to
 * CVECT_INIT_VAL).
 */
static inline void 
__cvect_free_rec(struct cvect_intern *vi, const int depth)
{
     249:	55                   	push   %ebp
     24a:	89 e5                	mov    %esp,%ebp
     24c:	83 ec 28             	sub    $0x28,%esp
	unsigned int i;
	
	assert(vi);
     24f:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     253:	0f 94 c0             	sete   %al
     256:	0f b6 c0             	movzbl %al,%eax
     259:	85 c0                	test   %eax,%eax
     25b:	74 1c                	je     279 <__cvect_free_rec+0x30>
     25d:	c7 04 24 b0 00 00 00 	movl   $0xb0,(%esp)
     264:	e8 fc ff ff ff       	call   265 <__cvect_free_rec+0x1c>
     269:	b8 00 00 00 00       	mov    $0x0,%eax
     26e:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     274:	e8 d2 fe ff ff       	call   14b <__cos_noret>
	if (depth > 1) {
     279:	83 7d 0c 01          	cmpl   $0x1,0xc(%ebp)
     27d:	7e 42                	jle    2c1 <__cvect_free_rec+0x78>
		for (i = 0 ; i < CVECT_BASE ; i++) {
     27f:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
     286:	eb 30                	jmp    2b8 <__cvect_free_rec+0x6f>
			if (vi[i].c.next != NULL) {
     288:	8b 45 f4             	mov    -0xc(%ebp),%eax
     28b:	c1 e0 02             	shl    $0x2,%eax
     28e:	03 45 08             	add    0x8(%ebp),%eax
     291:	8b 00                	mov    (%eax),%eax
     293:	85 c0                	test   %eax,%eax
     295:	74 1d                	je     2b4 <__cvect_free_rec+0x6b>
				__cvect_free_rec(vi[i].c.next, depth-1);
     297:	8b 45 0c             	mov    0xc(%ebp),%eax
     29a:	8d 50 ff             	lea    -0x1(%eax),%edx
     29d:	8b 45 f4             	mov    -0xc(%ebp),%eax
     2a0:	c1 e0 02             	shl    $0x2,%eax
     2a3:	03 45 08             	add    0x8(%ebp),%eax
     2a6:	8b 00                	mov    (%eax),%eax
     2a8:	89 54 24 04          	mov    %edx,0x4(%esp)
     2ac:	89 04 24             	mov    %eax,(%esp)
     2af:	e8 95 ff ff ff       	call   249 <__cvect_free_rec>
{
	unsigned int i;
	
	assert(vi);
	if (depth > 1) {
		for (i = 0 ; i < CVECT_BASE ; i++) {
     2b4:	83 45 f4 01          	addl   $0x1,-0xc(%ebp)
     2b8:	81 7d f4 ff 03 00 00 	cmpl   $0x3ff,-0xc(%ebp)
     2bf:	76 c7                	jbe    288 <__cvect_free_rec+0x3f>
				__cvect_free_rec(vi[i].c.next, depth-1);
			}
		}
	}
	/* assumes "vi" is aliased with the cvect_t: */
	CVECT_FREE(vi);
     2c1:	8b 45 08             	mov    0x8(%ebp),%eax
     2c4:	89 04 24             	mov    %eax,(%esp)
     2c7:	e8 fc ff ff ff       	call   2c8 <__cvect_free_rec+0x7f>
}
     2cc:	c9                   	leave  
     2cd:	c3                   	ret    

000002ce <cvect_free>:

static void 
cvect_free(cvect_t *v)
{
     2ce:	55                   	push   %ebp
     2cf:	89 e5                	mov    %esp,%ebp
     2d1:	83 ec 18             	sub    $0x18,%esp
	assert(v);
     2d4:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     2d8:	0f 94 c0             	sete   %al
     2db:	0f b6 c0             	movzbl %al,%eax
     2de:	85 c0                	test   %eax,%eax
     2e0:	74 1c                	je     2fe <cvect_free+0x30>
     2e2:	c7 04 24 08 01 00 00 	movl   $0x108,(%esp)
     2e9:	e8 fc ff ff ff       	call   2ea <cvect_free+0x1c>
     2ee:	b8 00 00 00 00       	mov    $0x0,%eax
     2f3:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     2f9:	e8 4d fe ff ff       	call   14b <__cos_noret>
	__cvect_free_rec(v->vect, CVECT_DEPTH);
     2fe:	8b 45 08             	mov    0x8(%ebp),%eax
     301:	c7 44 24 04 02 00 00 	movl   $0x2,0x4(%esp)
     308:	00 
     309:	89 04 24             	mov    %eax,(%esp)
     30c:	e8 38 ff ff ff       	call   249 <__cvect_free_rec>
}
     311:	c9                   	leave  
     312:	c3                   	ret    

00000313 <__cvect_lookup_rec>:
 * unrolling, and self-recursive function inlining) to turn this into
 * straight-line code.
 */
static inline struct cvect_intern *
__cvect_lookup_rec(struct cvect_intern *vi, const long id, const int depth)
{
     313:	55                   	push   %ebp
     314:	89 e5                	mov    %esp,%ebp
     316:	53                   	push   %ebx
     317:	83 ec 24             	sub    $0x24,%esp
	if (depth > 1) {
     31a:	83 7d 10 01          	cmpl   $0x1,0x10(%ebp)
     31e:	7e 63                	jle    383 <__cvect_lookup_rec+0x70>
		long n = id >> (CVECT_SHIFT * (depth-1));
     320:	8b 45 10             	mov    0x10(%ebp),%eax
     323:	8d 50 ff             	lea    -0x1(%eax),%edx
     326:	89 d0                	mov    %edx,%eax
     328:	c1 e0 02             	shl    $0x2,%eax
     32b:	01 d0                	add    %edx,%eax
     32d:	01 c0                	add    %eax,%eax
     32f:	8b 55 0c             	mov    0xc(%ebp),%edx
     332:	89 d3                	mov    %edx,%ebx
     334:	89 c1                	mov    %eax,%ecx
     336:	d3 fb                	sar    %cl,%ebx
     338:	89 d8                	mov    %ebx,%eax
     33a:	89 45 f4             	mov    %eax,-0xc(%ebp)
		if (vi[n & CVECT_MASK].c.next == NULL) return NULL;
     33d:	8b 45 f4             	mov    -0xc(%ebp),%eax
     340:	25 ff 03 00 00       	and    $0x3ff,%eax
     345:	c1 e0 02             	shl    $0x2,%eax
     348:	03 45 08             	add    0x8(%ebp),%eax
     34b:	8b 00                	mov    (%eax),%eax
     34d:	85 c0                	test   %eax,%eax
     34f:	75 07                	jne    358 <__cvect_lookup_rec+0x45>
     351:	b8 00 00 00 00       	mov    $0x0,%eax
     356:	eb 39                	jmp    391 <__cvect_lookup_rec+0x7e>
		return __cvect_lookup_rec(vi[n & CVECT_MASK].c.next, id, depth-1);
     358:	8b 45 10             	mov    0x10(%ebp),%eax
     35b:	8d 50 ff             	lea    -0x1(%eax),%edx
     35e:	8b 45 f4             	mov    -0xc(%ebp),%eax
     361:	25 ff 03 00 00       	and    $0x3ff,%eax
     366:	c1 e0 02             	shl    $0x2,%eax
     369:	03 45 08             	add    0x8(%ebp),%eax
     36c:	8b 00                	mov    (%eax),%eax
     36e:	89 54 24 08          	mov    %edx,0x8(%esp)
     372:	8b 55 0c             	mov    0xc(%ebp),%edx
     375:	89 54 24 04          	mov    %edx,0x4(%esp)
     379:	89 04 24             	mov    %eax,(%esp)
     37c:	e8 92 ff ff ff       	call   313 <__cvect_lookup_rec>
     381:	eb 0e                	jmp    391 <__cvect_lookup_rec+0x7e>
	}
	return &vi[id & CVECT_MASK];
     383:	8b 45 0c             	mov    0xc(%ebp),%eax
     386:	25 ff 03 00 00       	and    $0x3ff,%eax
     38b:	c1 e0 02             	shl    $0x2,%eax
     38e:	03 45 08             	add    0x8(%ebp),%eax
}
     391:	83 c4 24             	add    $0x24,%esp
     394:	5b                   	pop    %ebx
     395:	5d                   	pop    %ebp
     396:	c3                   	ret    

00000397 <__cvect_lookup>:

static inline struct cvect_intern *
__cvect_lookup(cvect_t *v, long id) 
{ 
     397:	55                   	push   %ebp
     398:	89 e5                	mov    %esp,%ebp
     39a:	83 ec 18             	sub    $0x18,%esp
	return __cvect_lookup_rec(v->vect, id, CVECT_DEPTH); 
     39d:	8b 45 08             	mov    0x8(%ebp),%eax
     3a0:	c7 44 24 08 02 00 00 	movl   $0x2,0x8(%esp)
     3a7:	00 
     3a8:	8b 55 0c             	mov    0xc(%ebp),%edx
     3ab:	89 54 24 04          	mov    %edx,0x4(%esp)
     3af:	89 04 24             	mov    %eax,(%esp)
     3b2:	e8 5c ff ff ff       	call   313 <__cvect_lookup_rec>
}
     3b7:	c9                   	leave  
     3b8:	c3                   	ret    

000003b9 <cvect_lookup>:

static inline void *
cvect_lookup(cvect_t *v, long id)
{
     3b9:	55                   	push   %ebp
     3ba:	89 e5                	mov    %esp,%ebp
     3bc:	83 ec 28             	sub    $0x28,%esp
	struct cvect_intern *vi;

	assert(v);
     3bf:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     3c3:	0f 94 c0             	sete   %al
     3c6:	0f b6 c0             	movzbl %al,%eax
     3c9:	85 c0                	test   %eax,%eax
     3cb:	74 1c                	je     3e9 <cvect_lookup+0x30>
     3cd:	c7 04 24 60 01 00 00 	movl   $0x160,(%esp)
     3d4:	e8 fc ff ff ff       	call   3d5 <cvect_lookup+0x1c>
     3d9:	b8 00 00 00 00       	mov    $0x0,%eax
     3de:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     3e4:	e8 62 fd ff ff       	call   14b <__cos_noret>
	assert(id >= 0);
     3e9:	8b 45 0c             	mov    0xc(%ebp),%eax
     3ec:	c1 e8 1f             	shr    $0x1f,%eax
     3ef:	85 c0                	test   %eax,%eax
     3f1:	74 1c                	je     40f <cvect_lookup+0x56>
     3f3:	c7 04 24 b8 01 00 00 	movl   $0x1b8,(%esp)
     3fa:	e8 fc ff ff ff       	call   3fb <cvect_lookup+0x42>
     3ff:	b8 00 00 00 00       	mov    $0x0,%eax
     404:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     40a:	e8 3c fd ff ff       	call   14b <__cos_noret>
	vi = __cvect_lookup(v, id);
     40f:	8b 45 0c             	mov    0xc(%ebp),%eax
     412:	89 44 24 04          	mov    %eax,0x4(%esp)
     416:	8b 45 08             	mov    0x8(%ebp),%eax
     419:	89 04 24             	mov    %eax,(%esp)
     41c:	e8 76 ff ff ff       	call   397 <__cvect_lookup>
     421:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (!vi) return NULL;
     424:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
     428:	75 07                	jne    431 <cvect_lookup+0x78>
     42a:	b8 00 00 00 00       	mov    $0x0,%eax
     42f:	eb 05                	jmp    436 <cvect_lookup+0x7d>
	return vi->c.val;
     431:	8b 45 f4             	mov    -0xc(%ebp),%eax
     434:	8b 00                	mov    (%eax),%eax
}
     436:	c9                   	leave  
     437:	c3                   	ret    

00000438 <cvect_lookup_addr>:

static inline void *
cvect_lookup_addr(cvect_t *v, long id)
{
     438:	55                   	push   %ebp
     439:	89 e5                	mov    %esp,%ebp
     43b:	83 ec 28             	sub    $0x28,%esp
	struct cvect_intern *vi;

	assert(v);
     43e:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     442:	0f 94 c0             	sete   %al
     445:	0f b6 c0             	movzbl %al,%eax
     448:	85 c0                	test   %eax,%eax
     44a:	74 1c                	je     468 <cvect_lookup_addr+0x30>
     44c:	c7 04 24 10 02 00 00 	movl   $0x210,(%esp)
     453:	e8 fc ff ff ff       	call   454 <cvect_lookup_addr+0x1c>
     458:	b8 00 00 00 00       	mov    $0x0,%eax
     45d:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     463:	e8 e3 fc ff ff       	call   14b <__cos_noret>
	assert(id >= 0);
     468:	8b 45 0c             	mov    0xc(%ebp),%eax
     46b:	c1 e8 1f             	shr    $0x1f,%eax
     46e:	85 c0                	test   %eax,%eax
     470:	74 1c                	je     48e <cvect_lookup_addr+0x56>
     472:	c7 04 24 68 02 00 00 	movl   $0x268,(%esp)
     479:	e8 fc ff ff ff       	call   47a <cvect_lookup_addr+0x42>
     47e:	b8 00 00 00 00       	mov    $0x0,%eax
     483:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     489:	e8 bd fc ff ff       	call   14b <__cos_noret>
	vi = __cvect_lookup(v, id);
     48e:	8b 45 0c             	mov    0xc(%ebp),%eax
     491:	89 44 24 04          	mov    %eax,0x4(%esp)
     495:	8b 45 08             	mov    0x8(%ebp),%eax
     498:	89 04 24             	mov    %eax,(%esp)
     49b:	e8 f7 fe ff ff       	call   397 <__cvect_lookup>
     4a0:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (!vi) return NULL;
     4a3:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
     4a7:	75 07                	jne    4b0 <cvect_lookup_addr+0x78>
     4a9:	b8 00 00 00 00       	mov    $0x0,%eax
     4ae:	eb 03                	jmp    4b3 <cvect_lookup_addr+0x7b>
	return vi;
     4b0:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
     4b3:	c9                   	leave  
     4b4:	c3                   	ret    

000004b5 <__cvect_expand_rec>:

static inline int
__cvect_expand_rec(struct cvect_intern *vi, const long id, const int depth)
{
     4b5:	55                   	push   %ebp
     4b6:	89 e5                	mov    %esp,%ebp
     4b8:	53                   	push   %ebx
     4b9:	83 ec 24             	sub    $0x24,%esp
	if (depth > 1) {
     4bc:	83 7d 10 01          	cmpl   $0x1,0x10(%ebp)
     4c0:	0f 8e 84 00 00 00    	jle    54a <__cvect_expand_rec+0x95>
		long n = id >> (CVECT_SHIFT * (depth-1));
     4c6:	8b 45 10             	mov    0x10(%ebp),%eax
     4c9:	8d 50 ff             	lea    -0x1(%eax),%edx
     4cc:	89 d0                	mov    %edx,%eax
     4ce:	c1 e0 02             	shl    $0x2,%eax
     4d1:	01 d0                	add    %edx,%eax
     4d3:	01 c0                	add    %eax,%eax
     4d5:	8b 55 0c             	mov    0xc(%ebp),%edx
     4d8:	89 d3                	mov    %edx,%ebx
     4da:	89 c1                	mov    %eax,%ecx
     4dc:	d3 fb                	sar    %cl,%ebx
     4de:	89 d8                	mov    %ebx,%eax
     4e0:	89 45 f0             	mov    %eax,-0x10(%ebp)
		if (vi[n & CVECT_MASK].c.next == NULL) {
     4e3:	8b 45 f0             	mov    -0x10(%ebp),%eax
     4e6:	25 ff 03 00 00       	and    $0x3ff,%eax
     4eb:	c1 e0 02             	shl    $0x2,%eax
     4ee:	03 45 08             	add    0x8(%ebp),%eax
     4f1:	8b 00                	mov    (%eax),%eax
     4f3:	85 c0                	test   %eax,%eax
     4f5:	75 28                	jne    51f <__cvect_expand_rec+0x6a>
			struct cvect_intern *new = CVECT_ALLOC();
     4f7:	e8 fc ff ff ff       	call   4f8 <__cvect_expand_rec+0x43>
     4fc:	89 45 f4             	mov    %eax,-0xc(%ebp)
			if (!new) return -1;
     4ff:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
     503:	75 07                	jne    50c <__cvect_expand_rec+0x57>
     505:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
     50a:	eb 43                	jmp    54f <__cvect_expand_rec+0x9a>
			vi[n & CVECT_MASK].c.next = new;
     50c:	8b 45 f0             	mov    -0x10(%ebp),%eax
     50f:	25 ff 03 00 00       	and    $0x3ff,%eax
     514:	c1 e0 02             	shl    $0x2,%eax
     517:	03 45 08             	add    0x8(%ebp),%eax
     51a:	8b 55 f4             	mov    -0xc(%ebp),%edx
     51d:	89 10                	mov    %edx,(%eax)
		}
		return __cvect_expand_rec(vi[n & CVECT_MASK].c.next, id, depth-1);
     51f:	8b 45 10             	mov    0x10(%ebp),%eax
     522:	8d 50 ff             	lea    -0x1(%eax),%edx
     525:	8b 45 f0             	mov    -0x10(%ebp),%eax
     528:	25 ff 03 00 00       	and    $0x3ff,%eax
     52d:	c1 e0 02             	shl    $0x2,%eax
     530:	03 45 08             	add    0x8(%ebp),%eax
     533:	8b 00                	mov    (%eax),%eax
     535:	89 54 24 08          	mov    %edx,0x8(%esp)
     539:	8b 55 0c             	mov    0xc(%ebp),%edx
     53c:	89 54 24 04          	mov    %edx,0x4(%esp)
     540:	89 04 24             	mov    %eax,(%esp)
     543:	e8 6d ff ff ff       	call   4b5 <__cvect_expand_rec>
     548:	eb 05                	jmp    54f <__cvect_expand_rec+0x9a>
	}
	return 0;
     54a:	b8 00 00 00 00       	mov    $0x0,%eax
}
     54f:	83 c4 24             	add    $0x24,%esp
     552:	5b                   	pop    %ebx
     553:	5d                   	pop    %ebp
     554:	c3                   	ret    

00000555 <__cvect_expand>:

static inline int 
__cvect_expand(cvect_t *v, long id)
{
     555:	55                   	push   %ebp
     556:	89 e5                	mov    %esp,%ebp
     558:	83 ec 18             	sub    $0x18,%esp
	return __cvect_expand_rec(v->vect, id, CVECT_DEPTH);
     55b:	8b 45 08             	mov    0x8(%ebp),%eax
     55e:	c7 44 24 08 02 00 00 	movl   $0x2,0x8(%esp)
     565:	00 
     566:	8b 55 0c             	mov    0xc(%ebp),%edx
     569:	89 54 24 04          	mov    %edx,0x4(%esp)
     56d:	89 04 24             	mov    %eax,(%esp)
     570:	e8 40 ff ff ff       	call   4b5 <__cvect_expand_rec>
}
     575:	c9                   	leave  
     576:	c3                   	ret    

00000577 <__cvect_set>:

static inline int 
__cvect_set(cvect_t *v, long id, void *val)
{
     577:	55                   	push   %ebp
     578:	89 e5                	mov    %esp,%ebp
     57a:	83 ec 28             	sub    $0x28,%esp
	struct cvect_intern *vi;
	
	assert(v);
     57d:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     581:	0f 94 c0             	sete   %al
     584:	0f b6 c0             	movzbl %al,%eax
     587:	85 c0                	test   %eax,%eax
     589:	74 1c                	je     5a7 <__cvect_set+0x30>
     58b:	c7 04 24 c0 02 00 00 	movl   $0x2c0,(%esp)
     592:	e8 fc ff ff ff       	call   593 <__cvect_set+0x1c>
     597:	b8 00 00 00 00       	mov    $0x0,%eax
     59c:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     5a2:	e8 a4 fb ff ff       	call   14b <__cos_noret>
	vi = __cvect_lookup(v, id);
     5a7:	8b 45 0c             	mov    0xc(%ebp),%eax
     5aa:	89 44 24 04          	mov    %eax,0x4(%esp)
     5ae:	8b 45 08             	mov    0x8(%ebp),%eax
     5b1:	89 04 24             	mov    %eax,(%esp)
     5b4:	e8 de fd ff ff       	call   397 <__cvect_lookup>
     5b9:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (NULL == vi) return -1;
     5bc:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
     5c0:	75 07                	jne    5c9 <__cvect_set+0x52>
     5c2:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
     5c7:	eb 0d                	jmp    5d6 <__cvect_set+0x5f>
	vi->c.val = val;
     5c9:	8b 45 f4             	mov    -0xc(%ebp),%eax
     5cc:	8b 55 10             	mov    0x10(%ebp),%edx
     5cf:	89 10                	mov    %edx,(%eax)

	return 0;
     5d1:	b8 00 00 00 00       	mov    $0x0,%eax
}
     5d6:	c9                   	leave  
     5d7:	c3                   	ret    

000005d8 <cvect_add>:
 *
 * Assume: id does not exist in v.
 */
static int
cvect_add(cvect_t *v, void *val, long id)
{
     5d8:	55                   	push   %ebp
     5d9:	89 e5                	mov    %esp,%ebp
     5db:	83 ec 18             	sub    $0x18,%esp
	assert(v);
     5de:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     5e2:	0f 94 c0             	sete   %al
     5e5:	0f b6 c0             	movzbl %al,%eax
     5e8:	85 c0                	test   %eax,%eax
     5ea:	74 1c                	je     608 <cvect_add+0x30>
     5ec:	c7 04 24 18 03 00 00 	movl   $0x318,(%esp)
     5f3:	e8 fc ff ff ff       	call   5f4 <cvect_add+0x1c>
     5f8:	b8 00 00 00 00       	mov    $0x0,%eax
     5fd:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     603:	e8 43 fb ff ff       	call   14b <__cos_noret>
	assert(val != CVECT_INIT_VAL);
     608:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
     60c:	0f 94 c0             	sete   %al
     60f:	0f b6 c0             	movzbl %al,%eax
     612:	85 c0                	test   %eax,%eax
     614:	74 1c                	je     632 <cvect_add+0x5a>
     616:	c7 04 24 70 03 00 00 	movl   $0x370,(%esp)
     61d:	e8 fc ff ff ff       	call   61e <cvect_add+0x46>
     622:	b8 00 00 00 00       	mov    $0x0,%eax
     627:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     62d:	e8 19 fb ff ff       	call   14b <__cos_noret>
	assert(id < CVECT_MAX_ID);
     632:	81 7d 10 ff ff 0f 00 	cmpl   $0xfffff,0x10(%ebp)
     639:	0f 9f c0             	setg   %al
     63c:	0f b6 c0             	movzbl %al,%eax
     63f:	85 c0                	test   %eax,%eax
     641:	74 1c                	je     65f <cvect_add+0x87>
     643:	c7 04 24 c8 03 00 00 	movl   $0x3c8,(%esp)
     64a:	e8 fc ff ff ff       	call   64b <cvect_add+0x73>
     64f:	b8 00 00 00 00       	mov    $0x0,%eax
     654:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     65a:	e8 ec fa ff ff       	call   14b <__cos_noret>
	assert(!cvect_lookup(v, id));
     65f:	8b 45 10             	mov    0x10(%ebp),%eax
     662:	89 44 24 04          	mov    %eax,0x4(%esp)
     666:	8b 45 08             	mov    0x8(%ebp),%eax
     669:	89 04 24             	mov    %eax,(%esp)
     66c:	e8 48 fd ff ff       	call   3b9 <cvect_lookup>
     671:	85 c0                	test   %eax,%eax
     673:	0f 95 c0             	setne  %al
     676:	0f b6 c0             	movzbl %al,%eax
     679:	85 c0                	test   %eax,%eax
     67b:	74 1c                	je     699 <cvect_add+0xc1>
     67d:	c7 04 24 20 04 00 00 	movl   $0x420,(%esp)
     684:	e8 fc ff ff ff       	call   685 <cvect_add+0xad>
     689:	b8 00 00 00 00       	mov    $0x0,%eax
     68e:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     694:	e8 b2 fa ff ff       	call   14b <__cos_noret>
	if (__cvect_set(v, id, val)) {
     699:	8b 45 0c             	mov    0xc(%ebp),%eax
     69c:	89 44 24 08          	mov    %eax,0x8(%esp)
     6a0:	8b 45 10             	mov    0x10(%ebp),%eax
     6a3:	89 44 24 04          	mov    %eax,0x4(%esp)
     6a7:	8b 45 08             	mov    0x8(%ebp),%eax
     6aa:	89 04 24             	mov    %eax,(%esp)
     6ad:	e8 c5 fe ff ff       	call   577 <__cvect_set>
     6b2:	85 c0                	test   %eax,%eax
     6b4:	74 41                	je     6f7 <cvect_add+0x11f>
		if (__cvect_expand(v, id)) return -1;
     6b6:	8b 45 10             	mov    0x10(%ebp),%eax
     6b9:	89 44 24 04          	mov    %eax,0x4(%esp)
     6bd:	8b 45 08             	mov    0x8(%ebp),%eax
     6c0:	89 04 24             	mov    %eax,(%esp)
     6c3:	e8 8d fe ff ff       	call   555 <__cvect_expand>
     6c8:	85 c0                	test   %eax,%eax
     6ca:	74 07                	je     6d3 <cvect_add+0xfb>
     6cc:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
     6d1:	eb 29                	jmp    6fc <cvect_add+0x124>
		if (__cvect_set(v, id, val)) return -1;
     6d3:	8b 45 0c             	mov    0xc(%ebp),%eax
     6d6:	89 44 24 08          	mov    %eax,0x8(%esp)
     6da:	8b 45 10             	mov    0x10(%ebp),%eax
     6dd:	89 44 24 04          	mov    %eax,0x4(%esp)
     6e1:	8b 45 08             	mov    0x8(%ebp),%eax
     6e4:	89 04 24             	mov    %eax,(%esp)
     6e7:	e8 8b fe ff ff       	call   577 <__cvect_set>
     6ec:	85 c0                	test   %eax,%eax
     6ee:	74 07                	je     6f7 <cvect_add+0x11f>
     6f0:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
     6f5:	eb 05                	jmp    6fc <cvect_add+0x124>
	}
	return 0;
     6f7:	b8 00 00 00 00       	mov    $0x0,%eax
}
     6fc:	c9                   	leave  
     6fd:	c3                   	ret    

000006fe <cvect_del>:
/* 
 * Assume: id is valid within v.
 */
static int 
cvect_del(cvect_t *v, long id)
{
     6fe:	55                   	push   %ebp
     6ff:	89 e5                	mov    %esp,%ebp
     701:	83 ec 18             	sub    $0x18,%esp
	assert(v);
     704:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     708:	0f 94 c0             	sete   %al
     70b:	0f b6 c0             	movzbl %al,%eax
     70e:	85 c0                	test   %eax,%eax
     710:	74 1c                	je     72e <cvect_del+0x30>
     712:	c7 04 24 78 04 00 00 	movl   $0x478,(%esp)
     719:	e8 fc ff ff ff       	call   71a <cvect_del+0x1c>
     71e:	b8 00 00 00 00       	mov    $0x0,%eax
     723:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     729:	e8 1d fa ff ff       	call   14b <__cos_noret>
	if (__cvect_set(v, id, (void*)CVECT_INIT_VAL)) return 1;
     72e:	c7 44 24 08 00 00 00 	movl   $0x0,0x8(%esp)
     735:	00 
     736:	8b 45 0c             	mov    0xc(%ebp),%eax
     739:	89 44 24 04          	mov    %eax,0x4(%esp)
     73d:	8b 45 08             	mov    0x8(%ebp),%eax
     740:	89 04 24             	mov    %eax,(%esp)
     743:	e8 2f fe ff ff       	call   577 <__cvect_set>
     748:	85 c0                	test   %eax,%eax
     74a:	74 07                	je     753 <cvect_del+0x55>
     74c:	b8 01 00 00 00       	mov    $0x1,%eax
     751:	eb 05                	jmp    758 <cvect_del+0x5a>
	return 0;
     753:	b8 00 00 00 00       	mov    $0x0,%eax
}
     758:	c9                   	leave  
     759:	c3                   	ret    

0000075a <__cos_cas>:
int lock_take_contention(cos_lock_t *l, union cos_lock_atomic_struct *result, 
			 union cos_lock_atomic_struct *prev_val, u16_t owner);

static inline int
__cos_cas(unsigned long *target, unsigned long cmp, unsigned long updated, int smp)
{
     75a:	55                   	push   %ebp
     75b:	89 e5                	mov    %esp,%ebp
     75d:	83 ec 0c             	sub    $0xc,%esp
	if (smp) return cos_cas(target, cmp, updated);
     760:	83 7d 14 00          	cmpl   $0x0,0x14(%ebp)
     764:	74 1b                	je     781 <__cos_cas+0x27>
     766:	8b 45 10             	mov    0x10(%ebp),%eax
     769:	89 44 24 08          	mov    %eax,0x8(%esp)
     76d:	8b 45 0c             	mov    0xc(%ebp),%eax
     770:	89 44 24 04          	mov    %eax,0x4(%esp)
     774:	8b 45 08             	mov    0x8(%ebp),%eax
     777:	89 04 24             	mov    %eax,(%esp)
     77a:	e8 ca f8 ff ff       	call   49 <cos_cas>
     77f:	eb 19                	jmp    79a <__cos_cas+0x40>
	else     return cos_cas_up(target, cmp, updated);
     781:	8b 45 10             	mov    0x10(%ebp),%eax
     784:	89 44 24 08          	mov    %eax,0x8(%esp)
     788:	8b 45 0c             	mov    0xc(%ebp),%eax
     78b:	89 44 24 04          	mov    %eax,0x4(%esp)
     78f:	8b 45 08             	mov    0x8(%ebp),%eax
     792:	89 04 24             	mov    %eax,(%esp)
     795:	e8 d6 f8 ff ff       	call   70 <cos_cas_up>
}
     79a:	c9                   	leave  
     79b:	c3                   	ret    

0000079c <__lock_take>:

static inline int
__lock_take(cos_lock_t *l, int smp)
{
     79c:	55                   	push   %ebp
     79d:	89 e5                	mov    %esp,%ebp
     79f:	53                   	push   %ebx
     7a0:	83 ec 34             	sub    $0x34,%esp
	union cos_lock_atomic_struct result, prev_val;
	unsigned int curr    = cos_get_thd_id();
     7a3:	e8 83 f8 ff ff       	call   2b <cos_get_thd_id>
     7a8:	0f b7 c0             	movzwl %ax,%eax
     7ab:	89 45 ec             	mov    %eax,-0x14(%ebp)
	u16_t        owner;

	prev_val.c.owner = prev_val.c.contested = 0;
     7ae:	66 c7 45 e6 00 00    	movw   $0x0,-0x1a(%ebp)
     7b4:	0f b7 45 e6          	movzwl -0x1a(%ebp),%eax
     7b8:	66 89 45 e4          	mov    %ax,-0x1c(%ebp)
	result.v = 0;
     7bc:	c7 45 e8 00 00 00 00 	movl   $0x0,-0x18(%ebp)
     7c3:	eb 01                	jmp    7c6 <__lock_take+0x2a>
			int ret;

			ret = lock_take_contention(l, &result, &prev_val, owner);
			if (ret < 0) return ret;
			/* try to take the lock again */
			goto restart;
     7c5:	90                   	nop
	prev_val.c.owner = prev_val.c.contested = 0;
	result.v = 0;
	do {
restart:
		/* Atomically copy the entire 32 bit structure */
		prev_val.v         = l->atom.v;
     7c6:	8b 45 08             	mov    0x8(%ebp),%eax
     7c9:	8b 00                	mov    (%eax),%eax
     7cb:	89 45 e4             	mov    %eax,-0x1c(%ebp)
		owner              = prev_val.c.owner;
     7ce:	0f b7 45 e4          	movzwl -0x1c(%ebp),%eax
     7d2:	66 89 45 f2          	mov    %ax,-0xe(%ebp)
		result.c.owner     = curr;
     7d6:	8b 45 ec             	mov    -0x14(%ebp),%eax
     7d9:	66 89 45 e8          	mov    %ax,-0x18(%ebp)
		result.c.contested = 0;
     7dd:	66 c7 45 ea 00 00    	movw   $0x0,-0x16(%ebp)
		assert(owner != curr); /* No recursive lock takes allowed */
     7e3:	0f b7 45 f2          	movzwl -0xe(%ebp),%eax
     7e7:	3b 45 ec             	cmp    -0x14(%ebp),%eax
     7ea:	0f 94 c0             	sete   %al
     7ed:	0f b6 c0             	movzbl %al,%eax
     7f0:	85 c0                	test   %eax,%eax
     7f2:	74 1c                	je     810 <__lock_take+0x74>
     7f4:	c7 04 24 d0 04 00 00 	movl   $0x4d0,(%esp)
     7fb:	e8 fc ff ff ff       	call   7fc <__lock_take+0x60>
     800:	b8 00 00 00 00       	mov    $0x0,%eax
     805:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     80b:	e8 3b f9 ff ff       	call   14b <__cos_noret>
		/* Contention path: If there is an owner, whom is not
		 * us, go through the motions of blocking on the lock.
		 * This is hopefully the uncommon case. If not, some
		 * structural reconfiguration is probably going to be
		 * needed.  */
		if (unlikely(owner)) {
     810:	66 83 7d f2 00       	cmpw   $0x0,-0xe(%ebp)
     815:	0f 95 c0             	setne  %al
     818:	0f b6 c0             	movzbl %al,%eax
     81b:	85 c0                	test   %eax,%eax
     81d:	74 36                	je     855 <__lock_take+0xb9>
			int ret;

			ret = lock_take_contention(l, &result, &prev_val, owner);
     81f:	0f b7 45 f2          	movzwl -0xe(%ebp),%eax
     823:	89 44 24 0c          	mov    %eax,0xc(%esp)
     827:	8d 45 e4             	lea    -0x1c(%ebp),%eax
     82a:	89 44 24 08          	mov    %eax,0x8(%esp)
     82e:	8d 45 e8             	lea    -0x18(%ebp),%eax
     831:	89 44 24 04          	mov    %eax,0x4(%esp)
     835:	8b 45 08             	mov    0x8(%ebp),%eax
     838:	89 04 24             	mov    %eax,(%esp)
     83b:	e8 fc ff ff ff       	call   83c <__lock_take+0xa0>
     840:	89 45 f4             	mov    %eax,-0xc(%ebp)
			if (ret < 0) return ret;
     843:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
     847:	0f 89 78 ff ff ff    	jns    7c5 <__lock_take+0x29>
     84d:	8b 45 f4             	mov    -0xc(%ebp),%eax
     850:	e9 93 00 00 00       	jmp    8e8 <__lock_take+0x14c>
			/* try to take the lock again */
			goto restart;
		}
		assert(result.v == curr);
     855:	8b 45 e8             	mov    -0x18(%ebp),%eax
     858:	3b 45 ec             	cmp    -0x14(%ebp),%eax
     85b:	0f 95 c0             	setne  %al
     85e:	0f b6 c0             	movzbl %al,%eax
     861:	85 c0                	test   %eax,%eax
     863:	74 1c                	je     881 <__lock_take+0xe5>
     865:	c7 04 24 38 05 00 00 	movl   $0x538,(%esp)
     86c:	e8 fc ff ff ff       	call   86d <__lock_take+0xd1>
     871:	b8 00 00 00 00       	mov    $0x0,%eax
     876:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     87c:	e8 ca f8 ff ff       	call   14b <__cos_noret>
		/* Commit the new lock value, or try again */
	} while (unlikely(!__cos_cas((unsigned long *)&l->atom.v, prev_val.v, result.v, smp)));
     881:	8b 4d e8             	mov    -0x18(%ebp),%ecx
     884:	8b 55 e4             	mov    -0x1c(%ebp),%edx
     887:	8b 45 08             	mov    0x8(%ebp),%eax
     88a:	8b 5d 0c             	mov    0xc(%ebp),%ebx
     88d:	89 5c 24 0c          	mov    %ebx,0xc(%esp)
     891:	89 4c 24 08          	mov    %ecx,0x8(%esp)
     895:	89 54 24 04          	mov    %edx,0x4(%esp)
     899:	89 04 24             	mov    %eax,(%esp)
     89c:	e8 b9 fe ff ff       	call   75a <__cos_cas>
     8a1:	85 c0                	test   %eax,%eax
     8a3:	0f 94 c0             	sete   %al
     8a6:	0f b6 c0             	movzbl %al,%eax
     8a9:	85 c0                	test   %eax,%eax
     8ab:	0f 85 15 ff ff ff    	jne    7c6 <__lock_take+0x2a>
	assert(l->atom.c.owner == curr);
     8b1:	8b 45 08             	mov    0x8(%ebp),%eax
     8b4:	0f b7 00             	movzwl (%eax),%eax
     8b7:	0f b7 c0             	movzwl %ax,%eax
     8ba:	3b 45 ec             	cmp    -0x14(%ebp),%eax
     8bd:	0f 95 c0             	setne  %al
     8c0:	0f b6 c0             	movzbl %al,%eax
     8c3:	85 c0                	test   %eax,%eax
     8c5:	74 1c                	je     8e3 <__lock_take+0x147>
     8c7:	c7 04 24 a0 05 00 00 	movl   $0x5a0,(%esp)
     8ce:	e8 fc ff ff ff       	call   8cf <__lock_take+0x133>
     8d3:	b8 00 00 00 00       	mov    $0x0,%eax
     8d8:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     8de:	e8 68 f8 ff ff       	call   14b <__cos_noret>

	return 0;
     8e3:	b8 00 00 00 00       	mov    $0x0,%eax
}
     8e8:	83 c4 34             	add    $0x34,%esp
     8eb:	5b                   	pop    %ebx
     8ec:	5d                   	pop    %ebp
     8ed:	c3                   	ret    

000008ee <__lock_release>:

static inline int 
__lock_release(cos_lock_t *l, int smp) {
     8ee:	55                   	push   %ebp
     8ef:	89 e5                	mov    %esp,%ebp
     8f1:	83 ec 28             	sub    $0x28,%esp
	unsigned int curr = cos_get_thd_id();
     8f4:	e8 32 f7 ff ff       	call   2b <cos_get_thd_id>
     8f9:	0f b7 c0             	movzwl %ax,%eax
     8fc:	89 45 f4             	mov    %eax,-0xc(%ebp)
	union cos_lock_atomic_struct prev_val;

	prev_val.c.owner = prev_val.c.contested = 0;
     8ff:	66 c7 45 f2 00 00    	movw   $0x0,-0xe(%ebp)
     905:	0f b7 45 f2          	movzwl -0xe(%ebp),%eax
     909:	66 89 45 f0          	mov    %ax,-0x10(%ebp)
	do {
		assert(sizeof(union cos_lock_atomic_struct) == sizeof(u32_t));
		prev_val.v = l->atom.v; /* local copy of lock */
     90d:	8b 45 08             	mov    0x8(%ebp),%eax
     910:	8b 00                	mov    (%eax),%eax
     912:	89 45 f0             	mov    %eax,-0x10(%ebp)
		/* If we're here, we better own the lock... */
		if (unlikely(prev_val.c.owner != curr)) BUG();
     915:	0f b7 45 f0          	movzwl -0x10(%ebp),%eax
     919:	0f b7 c0             	movzwl %ax,%eax
     91c:	3b 45 f4             	cmp    -0xc(%ebp),%eax
     91f:	0f 95 c0             	setne  %al
     922:	0f b6 c0             	movzbl %al,%eax
     925:	85 c0                	test   %eax,%eax
     927:	74 17                	je     940 <__lock_release+0x52>
     929:	c7 04 24 08 06 00 00 	movl   $0x608,(%esp)
     930:	e8 fc ff ff ff       	call   931 <__lock_release+0x43>
     935:	b8 00 00 00 00       	mov    $0x0,%eax
     93a:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
		if (unlikely(prev_val.c.contested)) {
     940:	0f b7 45 f2          	movzwl -0xe(%ebp),%eax
     944:	66 85 c0             	test   %ax,%ax
     947:	0f 95 c0             	setne  %al
     94a:	0f b6 c0             	movzbl %al,%eax
     94d:	85 c0                	test   %eax,%eax
     94f:	74 14                	je     965 <__lock_release+0x77>
			return lock_release_contention(l, &prev_val);
     951:	8d 45 f0             	lea    -0x10(%ebp),%eax
     954:	89 44 24 04          	mov    %eax,0x4(%esp)
     958:	8b 45 08             	mov    0x8(%ebp),%eax
     95b:	89 04 24             	mov    %eax,(%esp)
     95e:	e8 fc ff ff ff       	call   95f <__lock_release+0x71>
     963:	eb 68                	jmp    9cd <__lock_release+0xdf>
		}

		/* The loop is necessary as when read, the lock might
		 * not be contested, but by the time we get here,
		 * another thread might have tried to take it. */
	} while (unlikely(!__cos_cas((unsigned long *)&l->atom, prev_val.v, 0, smp)));
     965:	8b 55 f0             	mov    -0x10(%ebp),%edx
     968:	8b 45 08             	mov    0x8(%ebp),%eax
     96b:	8b 4d 0c             	mov    0xc(%ebp),%ecx
     96e:	89 4c 24 0c          	mov    %ecx,0xc(%esp)
     972:	c7 44 24 08 00 00 00 	movl   $0x0,0x8(%esp)
     979:	00 
     97a:	89 54 24 04          	mov    %edx,0x4(%esp)
     97e:	89 04 24             	mov    %eax,(%esp)
     981:	e8 d4 fd ff ff       	call   75a <__cos_cas>
     986:	85 c0                	test   %eax,%eax
     988:	0f 94 c0             	sete   %al
     98b:	0f b6 c0             	movzbl %al,%eax
     98e:	85 c0                	test   %eax,%eax
     990:	0f 85 77 ff ff ff    	jne    90d <__lock_release+0x1f>
	assert(l->atom.c.owner != curr);
     996:	8b 45 08             	mov    0x8(%ebp),%eax
     999:	0f b7 00             	movzwl (%eax),%eax
     99c:	0f b7 c0             	movzwl %ax,%eax
     99f:	3b 45 f4             	cmp    -0xc(%ebp),%eax
     9a2:	0f 94 c0             	sete   %al
     9a5:	0f b6 c0             	movzbl %al,%eax
     9a8:	85 c0                	test   %eax,%eax
     9aa:	74 1c                	je     9c8 <__lock_release+0xda>
     9ac:	c7 04 24 64 06 00 00 	movl   $0x664,(%esp)
     9b3:	e8 fc ff ff ff       	call   9b4 <__lock_release+0xc6>
     9b8:	b8 00 00 00 00       	mov    $0x0,%eax
     9bd:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     9c3:	e8 83 f7 ff ff       	call   14b <__cos_noret>

	return 0;
     9c8:	b8 00 00 00 00       	mov    $0x0,%eax
}
     9cd:	c9                   	leave  
     9ce:	c3                   	ret    

000009cf <lock_release>:
/* 
 * The hard-coding of the smp values here, with function inlining and
 * constant propagation should remove any costs of having smp
 * conditionals in the code.
 */
static inline int lock_release(cos_lock_t *l)    { return __lock_release(l, 1); }
     9cf:	55                   	push   %ebp
     9d0:	89 e5                	mov    %esp,%ebp
     9d2:	83 ec 18             	sub    $0x18,%esp
     9d5:	c7 44 24 04 01 00 00 	movl   $0x1,0x4(%esp)
     9dc:	00 
     9dd:	8b 45 08             	mov    0x8(%ebp),%eax
     9e0:	89 04 24             	mov    %eax,(%esp)
     9e3:	e8 06 ff ff ff       	call   8ee <__lock_release>
     9e8:	c9                   	leave  
     9e9:	c3                   	ret    

000009ea <lock_release_up>:
/* uni-processor variant for partitioned data-structures */
static inline int lock_release_up(cos_lock_t *l) { return __lock_release(l, 0); }
     9ea:	55                   	push   %ebp
     9eb:	89 e5                	mov    %esp,%ebp
     9ed:	83 ec 18             	sub    $0x18,%esp
     9f0:	c7 44 24 04 00 00 00 	movl   $0x0,0x4(%esp)
     9f7:	00 
     9f8:	8b 45 08             	mov    0x8(%ebp),%eax
     9fb:	89 04 24             	mov    %eax,(%esp)
     9fe:	e8 eb fe ff ff       	call   8ee <__lock_release>
     a03:	c9                   	leave  
     a04:	c3                   	ret    

00000a05 <lock_take>:
static inline int lock_take(cos_lock_t *l)       { return __lock_take(l, 1); }
     a05:	55                   	push   %ebp
     a06:	89 e5                	mov    %esp,%ebp
     a08:	83 ec 18             	sub    $0x18,%esp
     a0b:	c7 44 24 04 01 00 00 	movl   $0x1,0x4(%esp)
     a12:	00 
     a13:	8b 45 08             	mov    0x8(%ebp),%eax
     a16:	89 04 24             	mov    %eax,(%esp)
     a19:	e8 7e fd ff ff       	call   79c <__lock_take>
     a1e:	c9                   	leave  
     a1f:	c3                   	ret    

00000a20 <lock_take_up>:
/* uni-processor variant for partitioned data-structures */
static inline int lock_take_up(cos_lock_t *l)    { return __lock_take(l, 0); }
     a20:	55                   	push   %ebp
     a21:	89 e5                	mov    %esp,%ebp
     a23:	83 ec 18             	sub    $0x18,%esp
     a26:	c7 44 24 04 00 00 00 	movl   $0x0,0x4(%esp)
     a2d:	00 
     a2e:	8b 45 08             	mov    0x8(%ebp),%eax
     a31:	89 04 24             	mov    %eax,(%esp)
     a34:	e8 63 fd ff ff       	call   79c <__lock_take>
     a39:	c9                   	leave  
     a3a:	c3                   	ret    

00000a3b <lock_contested>:

static unsigned int
lock_contested(cos_lock_t *l) { return l->atom.c.owner; }
     a3b:	55                   	push   %ebp
     a3c:	89 e5                	mov    %esp,%ebp
     a3e:	8b 45 08             	mov    0x8(%ebp),%eax
     a41:	0f b7 00             	movzwl (%eax),%eax
     a44:	0f b7 c0             	movzwl %ax,%eax
     a47:	5d                   	pop    %ebp
     a48:	c3                   	ret    

00000a49 <lock_id_alloc>:

static inline unsigned long 
lock_id_alloc(void)
{
     a49:	55                   	push   %ebp
     a4a:	89 e5                	mov    %esp,%ebp
     a4c:	83 ec 18             	sub    $0x18,%esp
	return lock_component_alloc(cos_spd_id());
     a4f:	e8 eb f5 ff ff       	call   3f <cos_spd_id>
     a54:	0f b7 c0             	movzwl %ax,%eax
     a57:	89 04 24             	mov    %eax,(%esp)
     a5a:	e8 fc ff ff ff       	call   a5b <lock_id_alloc+0x12>
}
     a5f:	c9                   	leave  
     a60:	c3                   	ret    

00000a61 <lock_id_get>:

}

static inline u32_t 
lock_id_get(void)
{
     a61:	55                   	push   %ebp
     a62:	89 e5                	mov    %esp,%ebp
     a64:	83 ec 08             	sub    $0x8,%esp
	if (__lid_top == 0) return lock_id_alloc();
     a67:	a1 00 00 00 00       	mov    0x0,%eax
     a6c:	85 c0                	test   %eax,%eax
     a6e:	75 07                	jne    a77 <lock_id_get+0x16>
     a70:	e8 d4 ff ff ff       	call   a49 <lock_id_alloc>
     a75:	eb 19                	jmp    a90 <lock_id_get+0x2f>
	else                return __lid_cache[--__lid_top];
     a77:	a1 00 00 00 00       	mov    0x0,%eax
     a7c:	83 e8 01             	sub    $0x1,%eax
     a7f:	a3 00 00 00 00       	mov    %eax,0x0
     a84:	a1 00 00 00 00       	mov    0x0,%eax
     a89:	8b 04 85 00 00 00 00 	mov    0x0(,%eax,4),%eax
}
     a90:	c9                   	leave  
     a91:	c3                   	ret    

00000a92 <lock_init>:

static inline int 
lock_init(cos_lock_t *l)
{
     a92:	55                   	push   %ebp
     a93:	89 e5                	mov    %esp,%ebp
	l->lock_id = 0;
     a95:	8b 45 08             	mov    0x8(%ebp),%eax
     a98:	c7 40 04 00 00 00 00 	movl   $0x0,0x4(%eax)
	l->atom.v  = 0;
     a9f:	8b 45 08             	mov    0x8(%ebp),%eax
     aa2:	c7 00 00 00 00 00    	movl   $0x0,(%eax)

	return 0;
     aa8:	b8 00 00 00 00       	mov    $0x0,%eax
}
     aad:	5d                   	pop    %ebp
     aae:	c3                   	ret    

00000aaf <lock_static_init>:

static inline unsigned long 
lock_static_init(cos_lock_t *l)
{
     aaf:	55                   	push   %ebp
     ab0:	89 e5                	mov    %esp,%ebp
     ab2:	83 ec 18             	sub    $0x18,%esp
	lock_init(l);
     ab5:	8b 45 08             	mov    0x8(%ebp),%eax
     ab8:	89 04 24             	mov    %eax,(%esp)
     abb:	e8 d2 ff ff ff       	call   a92 <lock_init>
	l->lock_id = lock_id_get();
     ac0:	e8 9c ff ff ff       	call   a61 <lock_id_get>
     ac5:	8b 55 08             	mov    0x8(%ebp),%edx
     ac8:	89 42 04             	mov    %eax,0x4(%edx)

	return l->lock_id;
     acb:	8b 45 08             	mov    0x8(%ebp),%eax
     ace:	8b 40 04             	mov    0x4(%eax),%eax
}
     ad1:	c9                   	leave  
     ad2:	c3                   	ret    

00000ad3 <cbuf_unpack>:
	struct cbuf_agg_elem elem[0];
};

static inline void 
cbuf_unpack(cbuf_t cb, u32_t *cbid) 
{
     ad3:	55                   	push   %ebp
     ad4:	89 e5                	mov    %esp,%ebp
     ad6:	83 ec 28             	sub    $0x28,%esp
	cbuf_unpacked_t cu = {0};
     ad9:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
	
	cu.v  = cb;
     ae0:	8b 45 08             	mov    0x8(%ebp),%eax
     ae3:	89 45 f4             	mov    %eax,-0xc(%ebp)
	*cbid = cu.c.id;
     ae6:	8b 45 f4             	mov    -0xc(%ebp),%eax
     ae9:	d1 e8                	shr    %eax
     aeb:	89 c2                	mov    %eax,%edx
     aed:	8b 45 0c             	mov    0xc(%ebp),%eax
     af0:	89 10                	mov    %edx,(%eax)
	assert(!cu.c.aggregate);
     af2:	0f b6 45 f4          	movzbl -0xc(%ebp),%eax
     af6:	0f b6 c0             	movzbl %al,%eax
     af9:	83 e0 01             	and    $0x1,%eax
     afc:	85 c0                	test   %eax,%eax
     afe:	74 1c                	je     b1c <cbuf_unpack+0x49>
     b00:	c7 04 24 cc 06 00 00 	movl   $0x6cc,(%esp)
     b07:	e8 fc ff ff ff       	call   b08 <cbuf_unpack+0x35>
     b0c:	b8 00 00 00 00       	mov    $0x0,%eax
     b11:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     b17:	e8 2f f6 ff ff       	call   14b <__cos_noret>
	return;
}
     b1c:	c9                   	leave  
     b1d:	c3                   	ret    

00000b1e <cbuf_vect_lookup_addr>:
extern void __cbuf_desc_free(struct cbuf_alloc_desc *d);
extern cvect_t meta_cbuf, meta_cbufp;

static inline struct cbuf_meta *
cbuf_vect_lookup_addr(long idx, int tmem)
{
     b1e:	55                   	push   %ebp
     b1f:	89 e5                	mov    %esp,%ebp
     b21:	57                   	push   %edi
     b22:	56                   	push   %esi
     b23:	53                   	push   %ebx
     b24:	81 ec 2c 10 00 00    	sub    $0x102c,%esp
	printc("ryx: spdid %d meta_cbufp %x meta %x\n", cos_spd_id(),&meta_cbufp, meta_cbufp);
     b2a:	e8 10 f5 ff ff       	call   3f <cos_spd_id>
     b2f:	89 45 e4             	mov    %eax,-0x1c(%ebp)
     b32:	8d 54 24 0c          	lea    0xc(%esp),%edx
     b36:	bb 00 00 00 00       	mov    $0x0,%ebx
     b3b:	b8 00 04 00 00       	mov    $0x400,%eax
     b40:	89 d7                	mov    %edx,%edi
     b42:	89 de                	mov    %ebx,%esi
     b44:	89 c1                	mov    %eax,%ecx
     b46:	f3 a5                	rep movsl %ds:(%esi),%es:(%edi)
     b48:	c7 44 24 08 00 00 00 	movl   $0x0,0x8(%esp)
     b4f:	00 
     b50:	8b 45 e4             	mov    -0x1c(%ebp),%eax
     b53:	89 44 24 04          	mov    %eax,0x4(%esp)
     b57:	c7 04 24 24 07 00 00 	movl   $0x724,(%esp)
     b5e:	e8 fc ff ff ff       	call   b5f <cbuf_vect_lookup_addr+0x41>
	if (tmem) return cvect_lookup_addr(&meta_cbuf,  idx);
     b63:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
     b67:	74 15                	je     b7e <cbuf_vect_lookup_addr+0x60>
     b69:	8b 45 08             	mov    0x8(%ebp),%eax
     b6c:	89 44 24 04          	mov    %eax,0x4(%esp)
     b70:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
     b77:	e8 bc f8 ff ff       	call   438 <cvect_lookup_addr>
     b7c:	eb 13                	jmp    b91 <cbuf_vect_lookup_addr+0x73>
	else      return cvect_lookup_addr(&meta_cbufp, idx);
     b7e:	8b 45 08             	mov    0x8(%ebp),%eax
     b81:	89 44 24 04          	mov    %eax,0x4(%esp)
     b85:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
     b8c:	e8 a7 f8 ff ff       	call   438 <cvect_lookup_addr>
}
     b91:	81 c4 2c 10 00 00    	add    $0x102c,%esp
     b97:	5b                   	pop    %ebx
     b98:	5e                   	pop    %esi
     b99:	5f                   	pop    %edi
     b9a:	5d                   	pop    %ebp
     b9b:	c3                   	ret    

00000b9c <__cbuf2buf>:
 * that wishes to access a cbuf created by another component must use
 * this function to map the cbuf_t to the actual buffer.
 */
static inline void * 
__cbuf2buf(cbuf_t cb, int len, int tmem)
{
     b9c:	55                   	push   %ebp
     b9d:	89 e5                	mov    %esp,%ebp
     b9f:	53                   	push   %ebx
     ba0:	83 ec 34             	sub    $0x34,%esp
	u32_t id;
	struct cbuf_meta *cm;
	union cbufm_info ci;//, ci_new;
	void *ret = NULL;
     ba3:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
	long cbidx;
	if (unlikely(!len)) return NULL;
     baa:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
     bae:	0f 94 c0             	sete   %al
     bb1:	0f b6 c0             	movzbl %al,%eax
     bb4:	85 c0                	test   %eax,%eax
     bb6:	74 0a                	je     bc2 <__cbuf2buf+0x26>
     bb8:	b8 00 00 00 00       	mov    $0x0,%eax
     bbd:	e9 26 02 00 00       	jmp    de8 <__cbuf2buf+0x24c>
	cbuf_unpack(cb, &id);
     bc2:	8d 45 e8             	lea    -0x18(%ebp),%eax
     bc5:	89 44 24 04          	mov    %eax,0x4(%esp)
     bc9:	8b 45 08             	mov    0x8(%ebp),%eax
     bcc:	89 04 24             	mov    %eax,(%esp)
     bcf:	e8 ff fe ff ff       	call   ad3 <cbuf_unpack>

	CBUF_TAKE();
     bd4:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
     bdb:	e8 40 fe ff ff       	call   a20 <lock_take_up>
     be0:	85 c0                	test   %eax,%eax
     be2:	0f 95 c0             	setne  %al
     be5:	0f b6 c0             	movzbl %al,%eax
     be8:	85 c0                	test   %eax,%eax
     bea:	74 17                	je     c03 <__cbuf2buf+0x67>
     bec:	c7 04 24 4c 07 00 00 	movl   $0x74c,(%esp)
     bf3:	e8 fc ff ff ff       	call   bf4 <__cbuf2buf+0x58>
     bf8:	b8 00 00 00 00       	mov    $0x0,%eax
     bfd:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	cbidx = cbid_to_meta_idx(id);
     c03:	8b 45 e8             	mov    -0x18(%ebp),%eax
     c06:	01 c0                	add    %eax,%eax
     c08:	89 45 f4             	mov    %eax,-0xc(%ebp)
     c0b:	eb 01                	jmp    c0e <__cbuf2buf+0x72>
again:
	do {
		cm = cbuf_vect_lookup_addr(cbidx, tmem);
		if (unlikely(!cm || cm->nfo.v == 0)) {
			if (__cbuf_2buf_miss(id, len, tmem)) goto done;
			goto again;
     c0d:	90                   	nop

	CBUF_TAKE();
	cbidx = cbid_to_meta_idx(id);
again:
	do {
		cm = cbuf_vect_lookup_addr(cbidx, tmem);
     c0e:	8b 45 10             	mov    0x10(%ebp),%eax
     c11:	89 44 24 04          	mov    %eax,0x4(%esp)
     c15:	8b 45 f4             	mov    -0xc(%ebp),%eax
     c18:	89 04 24             	mov    %eax,(%esp)
     c1b:	e8 fe fe ff ff       	call   b1e <cbuf_vect_lookup_addr>
     c20:	89 45 ec             	mov    %eax,-0x14(%ebp)
		if (unlikely(!cm || cm->nfo.v == 0)) {
     c23:	83 7d ec 00          	cmpl   $0x0,-0x14(%ebp)
     c27:	0f 94 c0             	sete   %al
     c2a:	0f b6 c0             	movzbl %al,%eax
     c2d:	85 c0                	test   %eax,%eax
     c2f:	75 11                	jne    c42 <__cbuf2buf+0xa6>
     c31:	8b 45 ec             	mov    -0x14(%ebp),%eax
     c34:	8b 00                	mov    (%eax),%eax
     c36:	85 c0                	test   %eax,%eax
     c38:	0f 94 c0             	sete   %al
     c3b:	0f b6 c0             	movzbl %al,%eax
     c3e:	85 c0                	test   %eax,%eax
     c40:	74 22                	je     c64 <__cbuf2buf+0xc8>
			if (__cbuf_2buf_miss(id, len, tmem)) goto done;
     c42:	8b 45 e8             	mov    -0x18(%ebp),%eax
     c45:	8b 55 10             	mov    0x10(%ebp),%edx
     c48:	89 54 24 08          	mov    %edx,0x8(%esp)
     c4c:	8b 55 0c             	mov    0xc(%ebp),%edx
     c4f:	89 54 24 04          	mov    %edx,0x4(%esp)
     c53:	89 04 24             	mov    %eax,(%esp)
     c56:	e8 fc ff ff ff       	call   c57 <__cbuf2buf+0xbb>
     c5b:	85 c0                	test   %eax,%eax
     c5d:	74 ae                	je     c0d <__cbuf2buf+0x71>
     c5f:	e9 14 01 00 00       	jmp    d78 <__cbuf2buf+0x1dc>
			goto again;
		}
	} while (unlikely(!cm->nfo.v));
     c64:	8b 45 ec             	mov    -0x14(%ebp),%eax
     c67:	8b 00                	mov    (%eax),%eax
     c69:	85 c0                	test   %eax,%eax
     c6b:	0f 94 c0             	sete   %al
     c6e:	0f b6 c0             	movzbl %al,%eax
     c71:	85 c0                	test   %eax,%eax
     c73:	75 99                	jne    c0e <__cbuf2buf+0x72>
	ci.v = cm->nfo.v;
     c75:	8b 45 ec             	mov    -0x14(%ebp),%eax
     c78:	8b 00                	mov    (%eax),%eax
     c7a:	89 45 e4             	mov    %eax,-0x1c(%ebp)

	if (!tmem) {
     c7d:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
     c81:	0f 85 a7 00 00 00    	jne    d2e <__cbuf2buf+0x192>
		if (unlikely(cm->nfo.c.flags & CBUFM_TMEM)) goto done;
     c87:	8b 45 ec             	mov    -0x14(%ebp),%eax
     c8a:	0f b7 40 02          	movzwl 0x2(%eax),%eax
     c8e:	66 c1 e8 04          	shr    $0x4,%ax
     c92:	0f b7 c0             	movzwl %ax,%eax
     c95:	83 e0 10             	and    $0x10,%eax
     c98:	85 c0                	test   %eax,%eax
     c9a:	0f 95 c0             	setne  %al
     c9d:	0f b6 c0             	movzbl %al,%eax
     ca0:	85 c0                	test   %eax,%eax
     ca2:	0f 85 c6 00 00 00    	jne    d6e <__cbuf2buf+0x1d2>
		if (unlikely(len > cm->sz)) goto done;
     ca8:	8b 45 ec             	mov    -0x14(%ebp),%eax
     cab:	0f b7 40 04          	movzwl 0x4(%eax),%eax
     caf:	0f b7 c0             	movzwl %ax,%eax
     cb2:	3b 45 0c             	cmp    0xc(%ebp),%eax
     cb5:	0f 9c c0             	setl   %al
     cb8:	0f b6 c0             	movzbl %al,%eax
     cbb:	85 c0                	test   %eax,%eax
     cbd:	0f 85 ae 00 00 00    	jne    d71 <__cbuf2buf+0x1d5>
		/*cm->nfo.c.flags |= CBUFM_IN_USE;*/
     cc3:	8b 45 ec             	mov    -0x14(%ebp),%eax
     cc6:	0f b7 40 02          	movzwl 0x2(%eax),%eax
     cca:	66 c1 e8 04          	shr    $0x4,%ax
     cce:	83 c8 04             	or     $0x4,%eax
     cd1:	89 c2                	mov    %eax,%edx
     cd3:	66 81 e2 ff 0f       	and    $0xfff,%dx
     cd8:	8b 45 ec             	mov    -0x14(%ebp),%eax
     cdb:	89 d1                	mov    %edx,%ecx
     cdd:	c1 e1 04             	shl    $0x4,%ecx
     ce0:	0f b7 50 02          	movzwl 0x2(%eax),%edx
     ce4:	83 e2 0f             	and    $0xf,%edx
     ce7:	09 ca                	or     %ecx,%edx
     ce9:	66 89 50 02          	mov    %dx,0x2(%eax)
		cm->nfo.c.refcnt++;
     ced:	8b 45 ec             	mov    -0x14(%ebp),%eax
     cf0:	0f b6 40 07          	movzbl 0x7(%eax),%eax
     cf4:	3c ff                	cmp    $0xff,%al
     cf6:	0f 94 c0             	sete   %al
     cf9:	0f b6 c0             	movzbl %al,%eax
     cfc:	85 c0                	test   %eax,%eax
     cfe:	74 1c                	je     d1c <__cbuf2buf+0x180>
     d00:	c7 04 24 98 07 00 00 	movl   $0x798,(%esp)
     d07:	e8 fc ff ff ff       	call   d08 <__cbuf2buf+0x16c>
     d0c:	b8 00 00 00 00       	mov    $0x0,%eax
     d11:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     d17:	e8 2f f4 ff ff       	call   14b <__cos_noret>
		assert(cm->owner_nfo.c.nrecvd < TMEM_SENDRECV_MAX);
     d1c:	8b 45 ec             	mov    -0x14(%ebp),%eax
     d1f:	0f b6 40 07          	movzbl 0x7(%eax),%eax
     d23:	8d 50 01             	lea    0x1(%eax),%edx
     d26:	8b 45 ec             	mov    -0x14(%ebp),%eax
     d29:	88 50 07             	mov    %dl,0x7(%eax)
     d2c:	eb 2e                	jmp    d5c <__cbuf2buf+0x1c0>
		cm->owner_nfo.c.nrecvd++;
	} else {
     d2e:	8b 45 ec             	mov    -0x14(%ebp),%eax
     d31:	0f b7 40 02          	movzwl 0x2(%eax),%eax
     d35:	66 c1 e8 04          	shr    $0x4,%ax
     d39:	0f b7 c0             	movzwl %ax,%eax
     d3c:	83 e0 10             	and    $0x10,%eax
     d3f:	85 c0                	test   %eax,%eax
     d41:	0f 94 c0             	sete   %al
     d44:	0f b6 c0             	movzbl %al,%eax
     d47:	85 c0                	test   %eax,%eax
     d49:	75 29                	jne    d74 <__cbuf2buf+0x1d8>
		if (unlikely(!(cm->nfo.c.flags & CBUFM_TMEM))) goto done;
     d4b:	81 7d 0c 00 10 00 00 	cmpl   $0x1000,0xc(%ebp)
     d52:	0f 9f c0             	setg   %al
     d55:	0f b6 c0             	movzbl %al,%eax
     d58:	85 c0                	test   %eax,%eax
     d5a:	75 1b                	jne    d77 <__cbuf2buf+0x1db>
	/* if (unlikely(cm->sz && (((int)cm->sz)<<PAGE_ORDER) < len)) goto done; */
	/* ci_new.v       = ci.v; */
	/* ci_new.c.flags = ci.c.flags | CBUFM_RECVED | CBUFM_IN_USE; */
	/* if (unlikely(!cos_cas((unsigned long *)&cm->nfo.v,  */
	/* 		      (unsigned long)   ci.v,  */
	/* 		      (unsigned long)   ci_new.v))) goto again; */
     d5c:	8b 45 ec             	mov    -0x14(%ebp),%eax
     d5f:	8b 00                	mov    (%eax),%eax
     d61:	25 ff ff 0f 00       	and    $0xfffff,%eax
     d66:	c1 e0 0c             	shl    $0xc,%eax
     d69:	89 45 f0             	mov    %eax,-0x10(%ebp)
     d6c:	eb 0a                	jmp    d78 <__cbuf2buf+0x1dc>
		}
	} while (unlikely(!cm->nfo.v));
	ci.v = cm->nfo.v;

	if (!tmem) {
		if (unlikely(cm->nfo.c.flags & CBUFM_TMEM)) goto done;
     d6e:	90                   	nop
     d6f:	eb 07                	jmp    d78 <__cbuf2buf+0x1dc>
		if (unlikely(len > cm->sz)) goto done;
     d71:	90                   	nop
     d72:	eb 04                	jmp    d78 <__cbuf2buf+0x1dc>
		/*cm->nfo.c.flags |= CBUFM_IN_USE;*/
		cm->nfo.c.refcnt++;
		assert(cm->owner_nfo.c.nrecvd < TMEM_SENDRECV_MAX);
		cm->owner_nfo.c.nrecvd++;
	} else {
     d74:	90                   	nop
     d75:	eb 01                	jmp    d78 <__cbuf2buf+0x1dc>
		if (unlikely(!(cm->nfo.c.flags & CBUFM_TMEM))) goto done;
     d77:	90                   	nop
	/* ci_new.c.flags = ci.c.flags | CBUFM_RECVED | CBUFM_IN_USE; */
	/* if (unlikely(!cos_cas((unsigned long *)&cm->nfo.v,  */
	/* 		      (unsigned long)   ci.v,  */
	/* 		      (unsigned long)   ci_new.v))) goto again; */
	ret = ((void*)(cm->nfo.c.ptr << PAGE_ORDER));
done:	
     d78:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
     d7f:	e8 66 fc ff ff       	call   9ea <lock_release_up>
     d84:	85 c0                	test   %eax,%eax
     d86:	0f 95 c0             	setne  %al
     d89:	0f b6 c0             	movzbl %al,%eax
     d8c:	85 c0                	test   %eax,%eax
     d8e:	74 17                	je     da7 <__cbuf2buf+0x20b>
     d90:	c7 04 24 f0 07 00 00 	movl   $0x7f0,(%esp)
     d97:	e8 fc ff ff ff       	call   d98 <__cbuf2buf+0x1fc>
     d9c:	b8 00 00 00 00       	mov    $0x0,%eax
     da1:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	CBUF_RELEASE();
     da7:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
     dae:	e8 88 fc ff ff       	call   a3b <lock_contested>
     db3:	89 c3                	mov    %eax,%ebx
     db5:	e8 71 f2 ff ff       	call   2b <cos_get_thd_id>
     dba:	0f b7 c0             	movzwl %ax,%eax
     dbd:	39 c3                	cmp    %eax,%ebx
     dbf:	0f 94 c0             	sete   %al
     dc2:	0f b6 c0             	movzbl %al,%eax
     dc5:	85 c0                	test   %eax,%eax
     dc7:	74 1c                	je     de5 <__cbuf2buf+0x249>
     dc9:	c7 04 24 3c 08 00 00 	movl   $0x83c,(%esp)
     dd0:	e8 fc ff ff ff       	call   dd1 <__cbuf2buf+0x235>
     dd5:	b8 00 00 00 00       	mov    $0x0,%eax
     dda:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     de0:	e8 66 f3 ff ff       	call   14b <__cos_noret>
	assert(lock_contested(&cbuf_lock) != cos_get_thd_id());
     de5:	8b 45 f0             	mov    -0x10(%ebp),%eax
	return ret;
     de8:	83 c4 34             	add    $0x34,%esp
     deb:	5b                   	pop    %ebx
     dec:	5d                   	pop    %ebp
     ded:	c3                   	ret    

00000dee <cbuf2buf>:
}

static inline void *
     dee:	55                   	push   %ebp
     def:	89 e5                	mov    %esp,%ebp
     df1:	83 ec 18             	sub    $0x18,%esp
     df4:	c7 44 24 08 01 00 00 	movl   $0x1,0x8(%esp)
     dfb:	00 
     dfc:	8b 45 0c             	mov    0xc(%ebp),%eax
     dff:	89 44 24 04          	mov    %eax,0x4(%esp)
     e03:	8b 45 08             	mov    0x8(%ebp),%eax
     e06:	89 04 24             	mov    %eax,(%esp)
     e09:	e8 8e fd ff ff       	call   b9c <__cbuf2buf>
     e0e:	c9                   	leave  
     e0f:	c3                   	ret    

00000e10 <cmap_to_vect_id>:
	CVECT_CREATE_STATIC(__##name##_vect);				\
	cmap_t name = {.data = &__##name##_vect,			\
		       .free_list = -1, .id_boundary = 0}

static inline long cvect_to_map_id(long vid) { return vid/2; }
static inline long cmap_to_vect_id(long mid) { return mid*2; }
     e10:	55                   	push   %ebp
     e11:	89 e5                	mov    %esp,%ebp
     e13:	8b 45 08             	mov    0x8(%ebp),%eax
     e16:	01 c0                	add    %eax,%eax
     e18:	5d                   	pop    %ebp
     e19:	c3                   	ret    

00000e1a <cmap_to_vect_freeid>:
static inline long cmap_to_vect_freeid(long mid) { return cmap_to_vect_id(mid)+1; }
     e1a:	55                   	push   %ebp
     e1b:	89 e5                	mov    %esp,%ebp
     e1d:	83 ec 04             	sub    $0x4,%esp
     e20:	8b 45 08             	mov    0x8(%ebp),%eax
     e23:	89 04 24             	mov    %eax,(%esp)
     e26:	e8 e5 ff ff ff       	call   e10 <cmap_to_vect_id>
     e2b:	83 c0 01             	add    $0x1,%eax
     e2e:	c9                   	leave  
     e2f:	c3                   	ret    

00000e30 <cos_val_to_free>:
static inline struct cvect_intern *cos_val_to_free(struct cvect_intern *val) { return val+1; }
     e30:	55                   	push   %ebp
     e31:	89 e5                	mov    %esp,%ebp
     e33:	8b 45 08             	mov    0x8(%ebp),%eax
     e36:	83 c0 04             	add    $0x4,%eax
     e39:	5d                   	pop    %ebp
     e3a:	c3                   	ret    

00000e3b <__cmap_init>:
static inline struct cvect_intern *cos_free_to_val(struct cvect_intern *f) { return f-1; }

static inline void 
__cmap_init(cmap_t *m)
{
     e3b:	55                   	push   %ebp
     e3c:	89 e5                	mov    %esp,%ebp
     e3e:	83 ec 18             	sub    $0x18,%esp
	assert(m);
     e41:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     e45:	0f 94 c0             	sete   %al
     e48:	0f b6 c0             	movzbl %al,%eax
     e4b:	85 c0                	test   %eax,%eax
     e4d:	74 1c                	je     e6b <__cmap_init+0x30>
     e4f:	c7 04 24 94 08 00 00 	movl   $0x894,(%esp)
     e56:	e8 fc ff ff ff       	call   e57 <__cmap_init+0x1c>
     e5b:	b8 00 00 00 00       	mov    $0x0,%eax
     e60:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     e66:	e8 e0 f2 ff ff       	call   14b <__cos_noret>
	m->data        = NULL;
     e6b:	8b 45 08             	mov    0x8(%ebp),%eax
     e6e:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	m->free_list   = -1;
     e74:	8b 45 08             	mov    0x8(%ebp),%eax
     e77:	c7 40 04 ff ff ff ff 	movl   $0xffffffff,0x4(%eax)
	m->id_boundary = 0;
     e7e:	8b 45 08             	mov    0x8(%ebp),%eax
     e81:	c7 40 08 00 00 00 00 	movl   $0x0,0x8(%eax)
}
     e88:	c9                   	leave  
     e89:	c3                   	ret    

00000e8a <cmap_init_static>:

static inline void 
cmap_init_static(cmap_t *m) { return; }
     e8a:	55                   	push   %ebp
     e8b:	89 e5                	mov    %esp,%ebp
     e8d:	5d                   	pop    %ebp
     e8e:	c3                   	ret    

00000e8f <cmap_init>:
static inline void 
cmap_init(cmap_t *m) { __cmap_init(m); }
     e8f:	55                   	push   %ebp
     e90:	89 e5                	mov    %esp,%ebp
     e92:	83 ec 18             	sub    $0x18,%esp
     e95:	8b 45 08             	mov    0x8(%ebp),%eax
     e98:	89 04 24             	mov    %eax,(%esp)
     e9b:	e8 9b ff ff ff       	call   e3b <__cmap_init>
     ea0:	c9                   	leave  
     ea1:	c3                   	ret    

00000ea2 <cmap_alloc>:
#include <cos_alloc.h>
#endif /* COS_LINUX_ENV */

static cmap_t *
cmap_alloc(void)
{
     ea2:	55                   	push   %ebp
     ea3:	89 e5                	mov    %esp,%ebp
     ea5:	83 ec 28             	sub    $0x28,%esp
	cmap_t *m;

	m = malloc(sizeof(cmap_t));
     ea8:	c7 04 24 0c 00 00 00 	movl   $0xc,(%esp)
     eaf:	e8 fc ff ff ff       	call   eb0 <cmap_alloc+0xe>
     eb4:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (NULL == m) goto err;
     eb7:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
     ebb:	74 31                	je     eee <cmap_alloc+0x4c>
	cmap_init(m);
     ebd:	8b 45 f4             	mov    -0xc(%ebp),%eax
     ec0:	89 04 24             	mov    %eax,(%esp)
     ec3:	e8 c7 ff ff ff       	call   e8f <cmap_init>
	
	m->data = cvect_alloc();
     ec8:	e8 51 f3 ff ff       	call   21e <cvect_alloc>
     ecd:	8b 55 f4             	mov    -0xc(%ebp),%edx
     ed0:	89 02                	mov    %eax,(%edx)
	if (!m->data) goto err_free_map;
     ed2:	8b 45 f4             	mov    -0xc(%ebp),%eax
     ed5:	8b 00                	mov    (%eax),%eax
     ed7:	85 c0                	test   %eax,%eax
     ed9:	74 05                	je     ee0 <cmap_alloc+0x3e>

	return m;
     edb:	8b 45 f4             	mov    -0xc(%ebp),%eax
     ede:	eb 14                	jmp    ef4 <cmap_alloc+0x52>
	m = malloc(sizeof(cmap_t));
	if (NULL == m) goto err;
	cmap_init(m);
	
	m->data = cvect_alloc();
	if (!m->data) goto err_free_map;
     ee0:	90                   	nop

	return m;
err_free_map:
	free(m);
     ee1:	8b 45 f4             	mov    -0xc(%ebp),%eax
     ee4:	89 04 24             	mov    %eax,(%esp)
     ee7:	e8 fc ff ff ff       	call   ee8 <cmap_alloc+0x46>
     eec:	eb 01                	jmp    eef <cmap_alloc+0x4d>
cmap_alloc(void)
{
	cmap_t *m;

	m = malloc(sizeof(cmap_t));
	if (NULL == m) goto err;
     eee:	90                   	nop

	return m;
err_free_map:
	free(m);
err:
	return NULL;
     eef:	b8 00 00 00 00       	mov    $0x0,%eax
}
     ef4:	c9                   	leave  
     ef5:	c3                   	ret    

00000ef6 <cmap_free>:

static void 
cmap_free(cmap_t *m)
{
     ef6:	55                   	push   %ebp
     ef7:	89 e5                	mov    %esp,%ebp
     ef9:	83 ec 18             	sub    $0x18,%esp
	assert(m);
     efc:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     f00:	0f 94 c0             	sete   %al
     f03:	0f b6 c0             	movzbl %al,%eax
     f06:	85 c0                	test   %eax,%eax
     f08:	74 1c                	je     f26 <cmap_free+0x30>
     f0a:	c7 04 24 ec 08 00 00 	movl   $0x8ec,(%esp)
     f11:	e8 fc ff ff ff       	call   f12 <cmap_free+0x1c>
     f16:	b8 00 00 00 00       	mov    $0x0,%eax
     f1b:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     f21:	e8 25 f2 ff ff       	call   14b <__cos_noret>
	cvect_free(m->data);
     f26:	8b 45 08             	mov    0x8(%ebp),%eax
     f29:	8b 00                	mov    (%eax),%eax
     f2b:	89 04 24             	mov    %eax,(%esp)
     f2e:	e8 9b f3 ff ff       	call   2ce <cvect_free>
	free(m);
     f33:	8b 45 08             	mov    0x8(%ebp),%eax
     f36:	89 04 24             	mov    %eax,(%esp)
     f39:	e8 fc ff ff ff       	call   f3a <cmap_free+0x44>
}
     f3e:	c9                   	leave  
     f3f:	c3                   	ret    

00000f40 <cmap_lookup>:

#endif /* CMAP_DYNAMIC */

static inline void *
cmap_lookup(cmap_t *m, long mid)
{
     f40:	55                   	push   %ebp
     f41:	89 e5                	mov    %esp,%ebp
     f43:	83 ec 18             	sub    $0x18,%esp
	return cvect_lookup(m->data, cmap_to_vect_id(mid));
     f46:	8b 45 0c             	mov    0xc(%ebp),%eax
     f49:	89 04 24             	mov    %eax,(%esp)
     f4c:	e8 bf fe ff ff       	call   e10 <cmap_to_vect_id>
     f51:	8b 55 08             	mov    0x8(%ebp),%edx
     f54:	8b 12                	mov    (%edx),%edx
     f56:	89 44 24 04          	mov    %eax,0x4(%esp)
     f5a:	89 14 24             	mov    %edx,(%esp)
     f5d:	e8 57 f4 ff ff       	call   3b9 <cvect_lookup>
}
     f62:	c9                   	leave  
     f63:	c3                   	ret    

00000f64 <cmap_add>:

/* return the id of the value */
static inline long 
cmap_add(cmap_t *m, void *val)
{
     f64:	55                   	push   %ebp
     f65:	89 e5                	mov    %esp,%ebp
     f67:	83 ec 38             	sub    $0x38,%esp
	long free;
	struct cvect_intern *is_free, *is;

	assert(m);
     f6a:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
     f6e:	0f 94 c0             	sete   %al
     f71:	0f b6 c0             	movzbl %al,%eax
     f74:	85 c0                	test   %eax,%eax
     f76:	74 1c                	je     f94 <cmap_add+0x30>
     f78:	c7 04 24 44 09 00 00 	movl   $0x944,(%esp)
     f7f:	e8 fc ff ff ff       	call   f80 <cmap_add+0x1c>
     f84:	b8 00 00 00 00       	mov    $0x0,%eax
     f89:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
     f8f:	e8 b7 f1 ff ff       	call   14b <__cos_noret>
	free = m->free_list;
     f94:	8b 45 08             	mov    0x8(%ebp),%eax
     f97:	8b 40 04             	mov    0x4(%eax),%eax
     f9a:	89 45 e0             	mov    %eax,-0x20(%ebp)
	/* no free slots? Create more! */
	if (free == -1) {
     f9d:	83 7d e0 ff          	cmpl   $0xffffffff,-0x20(%ebp)
     fa1:	0f 85 00 01 00 00    	jne    10a7 <cmap_add+0x143>
		long lower, upper;
		if (val != CVECT_INIT_VAL && 
     fa7:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
     fab:	74 34                	je     fe1 <cmap_add+0x7d>
		    0 > cvect_add_id(m->data, val, cmap_to_vect_id(m->id_boundary))) return -1;	
     fad:	8b 45 08             	mov    0x8(%ebp),%eax
     fb0:	8b 40 08             	mov    0x8(%eax),%eax
     fb3:	89 04 24             	mov    %eax,(%esp)
     fb6:	e8 55 fe ff ff       	call   e10 <cmap_to_vect_id>
     fbb:	8b 55 08             	mov    0x8(%ebp),%edx
     fbe:	8b 12                	mov    (%edx),%edx
     fc0:	89 44 24 08          	mov    %eax,0x8(%esp)
     fc4:	8b 45 0c             	mov    0xc(%ebp),%eax
     fc7:	89 44 24 04          	mov    %eax,0x4(%esp)
     fcb:	89 14 24             	mov    %edx,(%esp)
     fce:	e8 05 f6 ff ff       	call   5d8 <cvect_add>
	assert(m);
	free = m->free_list;
	/* no free slots? Create more! */
	if (free == -1) {
		long lower, upper;
		if (val != CVECT_INIT_VAL && 
     fd3:	85 c0                	test   %eax,%eax
     fd5:	79 0a                	jns    fe1 <cmap_add+0x7d>
		    0 > cvect_add_id(m->data, val, cmap_to_vect_id(m->id_boundary))) return -1;	
     fd7:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
     fdc:	e9 3e 01 00 00       	jmp    111f <cmap_add+0x1bb>
		free = lower = m->id_boundary;
     fe1:	8b 45 08             	mov    0x8(%ebp),%eax
     fe4:	8b 40 08             	mov    0x8(%eax),%eax
     fe7:	89 45 ec             	mov    %eax,-0x14(%ebp)
     fea:	8b 45 ec             	mov    -0x14(%ebp),%eax
     fed:	89 45 e0             	mov    %eax,-0x20(%ebp)
		m->id_boundary = upper = lower + CVECT_BASE;
     ff0:	8b 45 ec             	mov    -0x14(%ebp),%eax
     ff3:	05 00 04 00 00       	add    $0x400,%eax
     ff8:	89 45 f0             	mov    %eax,-0x10(%ebp)
     ffb:	8b 45 08             	mov    0x8(%ebp),%eax
     ffe:	8b 55 f0             	mov    -0x10(%ebp),%edx
    1001:	89 50 08             	mov    %edx,0x8(%eax)
		/* Add the new values to the free list */
		while (lower != upper) {
    1004:	eb 52                	jmp    1058 <cmap_add+0xf4>
			int idx = cmap_to_vect_freeid(lower);
    1006:	8b 45 ec             	mov    -0x14(%ebp),%eax
    1009:	89 04 24             	mov    %eax,(%esp)
    100c:	e8 09 fe ff ff       	call   e1a <cmap_to_vect_freeid>
    1011:	89 45 f4             	mov    %eax,-0xc(%ebp)
			if (cvect_add(m->data, (void*)(lower+1), idx)) assert(0);
    1014:	8b 45 ec             	mov    -0x14(%ebp),%eax
    1017:	83 c0 01             	add    $0x1,%eax
    101a:	89 c2                	mov    %eax,%edx
    101c:	8b 45 08             	mov    0x8(%ebp),%eax
    101f:	8b 00                	mov    (%eax),%eax
    1021:	8b 4d f4             	mov    -0xc(%ebp),%ecx
    1024:	89 4c 24 08          	mov    %ecx,0x8(%esp)
    1028:	89 54 24 04          	mov    %edx,0x4(%esp)
    102c:	89 04 24             	mov    %eax,(%esp)
    102f:	e8 a4 f5 ff ff       	call   5d8 <cvect_add>
    1034:	85 c0                	test   %eax,%eax
    1036:	74 1c                	je     1054 <cmap_add+0xf0>
    1038:	c7 04 24 9c 09 00 00 	movl   $0x99c,(%esp)
    103f:	e8 fc ff ff ff       	call   1040 <cmap_add+0xdc>
    1044:	b8 00 00 00 00       	mov    $0x0,%eax
    1049:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    104f:	e8 f7 f0 ff ff       	call   14b <__cos_noret>
			lower++;
    1054:	83 45 ec 01          	addl   $0x1,-0x14(%ebp)
		if (val != CVECT_INIT_VAL && 
		    0 > cvect_add_id(m->data, val, cmap_to_vect_id(m->id_boundary))) return -1;	
		free = lower = m->id_boundary;
		m->id_boundary = upper = lower + CVECT_BASE;
		/* Add the new values to the free list */
		while (lower != upper) {
    1058:	8b 45 ec             	mov    -0x14(%ebp),%eax
    105b:	3b 45 f0             	cmp    -0x10(%ebp),%eax
    105e:	75 a6                	jne    1006 <cmap_add+0xa2>
			int idx = cmap_to_vect_freeid(lower);
			if (cvect_add(m->data, (void*)(lower+1), idx)) assert(0);
			lower++;
		}
		/* The end of the freelist */
		if (__cvect_set(m->data, cmap_to_vect_freeid(upper-1), (void*)-1)) assert(0);
    1060:	8b 45 f0             	mov    -0x10(%ebp),%eax
    1063:	83 e8 01             	sub    $0x1,%eax
    1066:	89 04 24             	mov    %eax,(%esp)
    1069:	e8 ac fd ff ff       	call   e1a <cmap_to_vect_freeid>
    106e:	8b 55 08             	mov    0x8(%ebp),%edx
    1071:	8b 12                	mov    (%edx),%edx
    1073:	c7 44 24 08 ff ff ff 	movl   $0xffffffff,0x8(%esp)
    107a:	ff 
    107b:	89 44 24 04          	mov    %eax,0x4(%esp)
    107f:	89 14 24             	mov    %edx,(%esp)
    1082:	e8 f0 f4 ff ff       	call   577 <__cvect_set>
    1087:	85 c0                	test   %eax,%eax
    1089:	74 1c                	je     10a7 <cmap_add+0x143>
    108b:	c7 04 24 f4 09 00 00 	movl   $0x9f4,(%esp)
    1092:	e8 fc ff ff ff       	call   1093 <cmap_add+0x12f>
    1097:	b8 00 00 00 00       	mov    $0x0,%eax
    109c:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    10a2:	e8 a4 f0 ff ff       	call   14b <__cos_noret>
	}

	is = __cvect_lookup(m->data, cmap_to_vect_id(free));
    10a7:	8b 45 e0             	mov    -0x20(%ebp),%eax
    10aa:	89 04 24             	mov    %eax,(%esp)
    10ad:	e8 5e fd ff ff       	call   e10 <cmap_to_vect_id>
    10b2:	8b 55 08             	mov    0x8(%ebp),%edx
    10b5:	8b 12                	mov    (%edx),%edx
    10b7:	89 44 24 04          	mov    %eax,0x4(%esp)
    10bb:	89 14 24             	mov    %edx,(%esp)
    10be:	e8 d4 f2 ff ff       	call   397 <__cvect_lookup>
    10c3:	89 45 e8             	mov    %eax,-0x18(%ebp)
	assert(NULL != is);
    10c6:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
    10ca:	0f 94 c0             	sete   %al
    10cd:	0f b6 c0             	movzbl %al,%eax
    10d0:	85 c0                	test   %eax,%eax
    10d2:	74 1c                	je     10f0 <cmap_add+0x18c>
    10d4:	c7 04 24 4c 0a 00 00 	movl   $0xa4c,(%esp)
    10db:	e8 fc ff ff ff       	call   10dc <cmap_add+0x178>
    10e0:	b8 00 00 00 00       	mov    $0x0,%eax
    10e5:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    10eb:	e8 5b f0 ff ff       	call   14b <__cos_noret>
	is->c.val = val;
    10f0:	8b 45 e8             	mov    -0x18(%ebp),%eax
    10f3:	8b 55 0c             	mov    0xc(%ebp),%edx
    10f6:	89 10                	mov    %edx,(%eax)

	is_free = cos_val_to_free(is);
    10f8:	8b 45 e8             	mov    -0x18(%ebp),%eax
    10fb:	89 04 24             	mov    %eax,(%esp)
    10fe:	e8 2d fd ff ff       	call   e30 <cos_val_to_free>
    1103:	89 45 e4             	mov    %eax,-0x1c(%ebp)
	m->free_list = (long)is_free->c.val;
    1106:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    1109:	8b 00                	mov    (%eax),%eax
    110b:	89 c2                	mov    %eax,%edx
    110d:	8b 45 08             	mov    0x8(%ebp),%eax
    1110:	89 50 04             	mov    %edx,0x4(%eax)
	is_free->c.val = (void*)-1;
    1113:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    1116:	c7 00 ff ff ff ff    	movl   $0xffffffff,(%eax)

	return free;
    111c:	8b 45 e0             	mov    -0x20(%ebp),%eax
}
    111f:	c9                   	leave  
    1120:	c3                   	ret    

00001121 <cmap_del>:
	return -1;
}

static inline int 
cmap_del(cmap_t *m, long mid)
{
    1121:	55                   	push   %ebp
    1122:	89 e5                	mov    %esp,%ebp
    1124:	83 ec 28             	sub    $0x28,%esp
	struct cvect_intern *is;

	assert(m && m->data);
    1127:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
    112b:	0f 94 c0             	sete   %al
    112e:	0f b6 c0             	movzbl %al,%eax
    1131:	85 c0                	test   %eax,%eax
    1133:	75 11                	jne    1146 <cmap_del+0x25>
    1135:	8b 45 08             	mov    0x8(%ebp),%eax
    1138:	8b 00                	mov    (%eax),%eax
    113a:	85 c0                	test   %eax,%eax
    113c:	0f 94 c0             	sete   %al
    113f:	0f b6 c0             	movzbl %al,%eax
    1142:	85 c0                	test   %eax,%eax
    1144:	74 1c                	je     1162 <cmap_del+0x41>
    1146:	c7 04 24 a4 0a 00 00 	movl   $0xaa4,(%esp)
    114d:	e8 fc ff ff ff       	call   114e <cmap_del+0x2d>
    1152:	b8 00 00 00 00       	mov    $0x0,%eax
    1157:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    115d:	e8 e9 ef ff ff       	call   14b <__cos_noret>
	is = __cvect_lookup(m->data, cmap_to_vect_id(mid));
    1162:	8b 45 0c             	mov    0xc(%ebp),%eax
    1165:	89 04 24             	mov    %eax,(%esp)
    1168:	e8 a3 fc ff ff       	call   e10 <cmap_to_vect_id>
    116d:	8b 55 08             	mov    0x8(%ebp),%edx
    1170:	8b 12                	mov    (%edx),%edx
    1172:	89 44 24 04          	mov    %eax,0x4(%esp)
    1176:	89 14 24             	mov    %edx,(%esp)
    1179:	e8 19 f2 ff ff       	call   397 <__cvect_lookup>
    117e:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (NULL == is) return -1;
    1181:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    1185:	75 07                	jne    118e <cmap_del+0x6d>
    1187:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
    118c:	eb 5c                	jmp    11ea <cmap_del+0xc9>
	is->c.val = CVECT_INIT_VAL;
    118e:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1191:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	is = cos_val_to_free(is);
    1197:	8b 45 f4             	mov    -0xc(%ebp),%eax
    119a:	89 04 24             	mov    %eax,(%esp)
    119d:	e8 8e fc ff ff       	call   e30 <cos_val_to_free>
    11a2:	89 45 f4             	mov    %eax,-0xc(%ebp)
	assert(NULL != is);
    11a5:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    11a9:	0f 94 c0             	sete   %al
    11ac:	0f b6 c0             	movzbl %al,%eax
    11af:	85 c0                	test   %eax,%eax
    11b1:	74 1c                	je     11cf <cmap_del+0xae>
    11b3:	c7 04 24 fc 0a 00 00 	movl   $0xafc,(%esp)
    11ba:	e8 fc ff ff ff       	call   11bb <cmap_del+0x9a>
    11bf:	b8 00 00 00 00       	mov    $0x0,%eax
    11c4:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    11ca:	e8 7c ef ff ff       	call   14b <__cos_noret>
	is->c.val = (void*)m->free_list;
    11cf:	8b 45 08             	mov    0x8(%ebp),%eax
    11d2:	8b 40 04             	mov    0x4(%eax),%eax
    11d5:	89 c2                	mov    %eax,%edx
    11d7:	8b 45 f4             	mov    -0xc(%ebp),%eax
    11da:	89 10                	mov    %edx,(%eax)
	m->free_list = mid;
    11dc:	8b 45 08             	mov    0x8(%ebp),%eax
    11df:	8b 55 0c             	mov    0xc(%ebp),%edx
    11e2:	89 50 04             	mov    %edx,0x4(%eax)
	return 0;
    11e5:	b8 00 00 00 00       	mov    $0x0,%eax
}
    11ea:	c9                   	leave  
    11eb:	c3                   	ret    

000011ec <cbufp_meta_lookup_cmr>:
CVECT_CREATE_STATIC(components);
CMAP_CREATE_STATIC(cbufs);

static struct cbufp_meta_range *
cbufp_meta_lookup_cmr(struct cbufp_comp_info *comp, u32_t cbid)
{
    11ec:	55                   	push   %ebp
    11ed:	89 e5                	mov    %esp,%ebp
    11ef:	83 ec 28             	sub    $0x28,%esp
	struct cbufp_meta_range *cmr;
	assert(comp);
    11f2:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
    11f6:	0f 94 c0             	sete   %al
    11f9:	0f b6 c0             	movzbl %al,%eax
    11fc:	85 c0                	test   %eax,%eax
    11fe:	74 1c                	je     121c <cbufp_meta_lookup_cmr+0x30>
    1200:	c7 04 24 54 0b 00 00 	movl   $0xb54,(%esp)
    1207:	e8 fc ff ff ff       	call   1208 <cbufp_meta_lookup_cmr+0x1c>
    120c:	b8 00 00 00 00       	mov    $0x0,%eax
    1211:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    1217:	e8 2f ef ff ff       	call   14b <__cos_noret>

	cmr = comp->cbuf_metas;
    121c:	8b 45 08             	mov    0x8(%ebp),%eax
    121f:	8b 80 08 02 00 00    	mov    0x208(%eax),%eax
    1225:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (!cmr) return NULL;
    1228:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    122c:	75 07                	jne    1235 <cbufp_meta_lookup_cmr+0x49>
    122e:	b8 00 00 00 00       	mov    $0x0,%eax
    1233:	eb 3c                	jmp    1271 <cbufp_meta_lookup_cmr+0x85>
	do {
		if (cmr->low_id >= cbid || CBUFP_META_RANGE_HIGH(cmr) > cbid) {
    1235:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1238:	8b 40 08             	mov    0x8(%eax),%eax
    123b:	3b 45 0c             	cmp    0xc(%ebp),%eax
    123e:	73 10                	jae    1250 <cbufp_meta_lookup_cmr+0x64>
    1240:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1243:	8b 40 08             	mov    0x8(%eax),%eax
    1246:	05 00 02 00 00       	add    $0x200,%eax
    124b:	3b 45 0c             	cmp    0xc(%ebp),%eax
    124e:	76 05                	jbe    1255 <cbufp_meta_lookup_cmr+0x69>
			return cmr;
    1250:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1253:	eb 1c                	jmp    1271 <cbufp_meta_lookup_cmr+0x85>
		}
		cmr = FIRST_LIST(cmr, next, prev);
    1255:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1258:	8b 40 0c             	mov    0xc(%eax),%eax
    125b:	89 45 f4             	mov    %eax,-0xc(%ebp)
	} while (cmr != comp->cbuf_metas);
    125e:	8b 45 08             	mov    0x8(%ebp),%eax
    1261:	8b 80 08 02 00 00    	mov    0x208(%eax),%eax
    1267:	3b 45 f4             	cmp    -0xc(%ebp),%eax
    126a:	75 c9                	jne    1235 <cbufp_meta_lookup_cmr+0x49>

	return NULL;
    126c:	b8 00 00 00 00       	mov    $0x0,%eax
}
    1271:	c9                   	leave  
    1272:	c3                   	ret    

00001273 <cbufp_meta_lookup>:

static struct cbuf_meta *
cbufp_meta_lookup(struct cbufp_comp_info *comp, u32_t cbid)
{
    1273:	55                   	push   %ebp
    1274:	89 e5                	mov    %esp,%ebp
    1276:	53                   	push   %ebx
    1277:	83 ec 24             	sub    $0x24,%esp
	struct cbufp_meta_range *cmr;

	cmr = cbufp_meta_lookup_cmr(comp, cbid);
    127a:	8b 45 0c             	mov    0xc(%ebp),%eax
    127d:	89 44 24 04          	mov    %eax,0x4(%esp)
    1281:	8b 45 08             	mov    0x8(%ebp),%eax
    1284:	89 04 24             	mov    %eax,(%esp)
    1287:	e8 60 ff ff ff       	call   11ec <cbufp_meta_lookup_cmr>
    128c:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (!cmr) return NULL;
    128f:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    1293:	75 07                	jne    129c <cbufp_meta_lookup+0x29>
    1295:	b8 00 00 00 00       	mov    $0x0,%eax
    129a:	eb 1a                	jmp    12b6 <cbufp_meta_lookup+0x43>
	return &cmr->m[cbid - cmr->low_id];
    129c:	8b 45 f4             	mov    -0xc(%ebp),%eax
    129f:	8b 10                	mov    (%eax),%edx
    12a1:	8b 45 f4             	mov    -0xc(%ebp),%eax
    12a4:	8b 40 08             	mov    0x8(%eax),%eax
    12a7:	8b 4d 0c             	mov    0xc(%ebp),%ecx
    12aa:	89 cb                	mov    %ecx,%ebx
    12ac:	29 c3                	sub    %eax,%ebx
    12ae:	89 d8                	mov    %ebx,%eax
    12b0:	c1 e0 03             	shl    $0x3,%eax
    12b3:	8d 04 02             	lea    (%edx,%eax,1),%eax
}
    12b6:	83 c4 24             	add    $0x24,%esp
    12b9:	5b                   	pop    %ebx
    12ba:	5d                   	pop    %ebp
    12bb:	c3                   	ret    

000012bc <cbufp_meta_add>:

static struct cbufp_meta_range *
cbufp_meta_add(struct cbufp_comp_info *comp, u32_t cbid, struct cbuf_meta *m, vaddr_t dest)
{
    12bc:	55                   	push   %ebp
    12bd:	89 e5                	mov    %esp,%ebp
    12bf:	83 ec 28             	sub    $0x28,%esp
	struct cbufp_meta_range *cmr;

	if (cbufp_meta_lookup(comp, cbid)) return NULL;
    12c2:	8b 45 0c             	mov    0xc(%ebp),%eax
    12c5:	89 44 24 04          	mov    %eax,0x4(%esp)
    12c9:	8b 45 08             	mov    0x8(%ebp),%eax
    12cc:	89 04 24             	mov    %eax,(%esp)
    12cf:	e8 9f ff ff ff       	call   1273 <cbufp_meta_lookup>
    12d4:	85 c0                	test   %eax,%eax
    12d6:	74 0a                	je     12e2 <cbufp_meta_add+0x26>
    12d8:	b8 00 00 00 00       	mov    $0x0,%eax
    12dd:	e9 cd 00 00 00       	jmp    13af <cbufp_meta_add+0xf3>
	cmr = malloc(sizeof(struct cbufp_meta_range));
    12e2:	c7 04 24 14 00 00 00 	movl   $0x14,(%esp)
    12e9:	e8 fc ff ff ff       	call   12ea <cbufp_meta_add+0x2e>
    12ee:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (!cmr) return NULL;
    12f1:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    12f5:	75 0a                	jne    1301 <cbufp_meta_add+0x45>
    12f7:	b8 00 00 00 00       	mov    $0x0,%eax
    12fc:	e9 ae 00 00 00       	jmp    13af <cbufp_meta_add+0xf3>
	INIT_LIST(cmr, next, prev);
    1301:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1304:	8b 55 f4             	mov    -0xc(%ebp),%edx
    1307:	89 50 10             	mov    %edx,0x10(%eax)
    130a:	8b 45 f4             	mov    -0xc(%ebp),%eax
    130d:	8b 50 10             	mov    0x10(%eax),%edx
    1310:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1313:	89 50 0c             	mov    %edx,0xc(%eax)
	cmr->m      = m;
    1316:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1319:	8b 55 10             	mov    0x10(%ebp),%edx
    131c:	89 10                	mov    %edx,(%eax)
	cmr->dest   = dest;
    131e:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1321:	8b 55 14             	mov    0x14(%ebp),%edx
    1324:	89 50 04             	mov    %edx,0x4(%eax)
	/* must be power of 2: */
	cmr->low_id = (cbid & ~((PAGE_SIZE/sizeof(struct cbuf_meta))-1));
    1327:	8b 45 0c             	mov    0xc(%ebp),%eax
    132a:	89 c2                	mov    %eax,%edx
    132c:	81 e2 00 fe ff ff    	and    $0xfffffe00,%edx
    1332:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1335:	89 50 08             	mov    %edx,0x8(%eax)

	printc("ryx: cbufp_meta_add cbid %d lowid %d\n", cbid, cmr->low_id);
    1338:	8b 45 f4             	mov    -0xc(%ebp),%eax
    133b:	8b 40 08             	mov    0x8(%eax),%eax
    133e:	89 44 24 08          	mov    %eax,0x8(%esp)
    1342:	8b 45 0c             	mov    0xc(%ebp),%eax
    1345:	89 44 24 04          	mov    %eax,0x4(%esp)
    1349:	c7 04 24 74 0b 00 00 	movl   $0xb74,(%esp)
    1350:	e8 fc ff ff ff       	call   1351 <cbufp_meta_add+0x95>
	if (comp->cbuf_metas) ADD_LIST(comp->cbuf_metas, cmr, next, prev);
    1355:	8b 45 08             	mov    0x8(%ebp),%eax
    1358:	8b 80 08 02 00 00    	mov    0x208(%eax),%eax
    135e:	85 c0                	test   %eax,%eax
    1360:	74 3e                	je     13a0 <cbufp_meta_add+0xe4>
    1362:	8b 45 08             	mov    0x8(%ebp),%eax
    1365:	8b 80 08 02 00 00    	mov    0x208(%eax),%eax
    136b:	8b 50 0c             	mov    0xc(%eax),%edx
    136e:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1371:	89 50 0c             	mov    %edx,0xc(%eax)
    1374:	8b 45 08             	mov    0x8(%ebp),%eax
    1377:	8b 90 08 02 00 00    	mov    0x208(%eax),%edx
    137d:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1380:	89 50 10             	mov    %edx,0x10(%eax)
    1383:	8b 45 08             	mov    0x8(%ebp),%eax
    1386:	8b 80 08 02 00 00    	mov    0x208(%eax),%eax
    138c:	8b 55 f4             	mov    -0xc(%ebp),%edx
    138f:	89 50 0c             	mov    %edx,0xc(%eax)
    1392:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1395:	8b 40 0c             	mov    0xc(%eax),%eax
    1398:	8b 55 f4             	mov    -0xc(%ebp),%edx
    139b:	89 50 10             	mov    %edx,0x10(%eax)
    139e:	eb 0c                	jmp    13ac <cbufp_meta_add+0xf0>
	else                  comp->cbuf_metas = cmr;
    13a0:	8b 45 08             	mov    0x8(%ebp),%eax
    13a3:	8b 55 f4             	mov    -0xc(%ebp),%edx
    13a6:	89 90 08 02 00 00    	mov    %edx,0x208(%eax)

	return cmr;
    13ac:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
    13af:	c9                   	leave  
    13b0:	c3                   	ret    

000013b1 <cbufp_comp_info_get>:

static struct cbufp_comp_info *
cbufp_comp_info_get(spdid_t spdid)
{
    13b1:	55                   	push   %ebp
    13b2:	89 e5                	mov    %esp,%ebp
    13b4:	83 ec 38             	sub    $0x38,%esp
    13b7:	8b 45 08             	mov    0x8(%ebp),%eax
    13ba:	66 89 45 e4          	mov    %ax,-0x1c(%ebp)
	struct cbufp_comp_info *cci;

	cci = cvect_lookup(&components, spdid);
    13be:	0f b7 45 e4          	movzwl -0x1c(%ebp),%eax
    13c2:	89 44 24 04          	mov    %eax,0x4(%esp)
    13c6:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    13cd:	e8 e7 ef ff ff       	call   3b9 <cvect_lookup>
    13d2:	89 45 f4             	mov    %eax,-0xc(%ebp)
	if (!cci) {
    13d5:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    13d9:	75 5c                	jne    1437 <cbufp_comp_info_get+0x86>
		cci = malloc(sizeof(struct cbufp_comp_info));
    13db:	c7 04 24 0c 02 00 00 	movl   $0x20c,(%esp)
    13e2:	e8 fc ff ff ff       	call   13e3 <cbufp_comp_info_get+0x32>
    13e7:	89 45 f4             	mov    %eax,-0xc(%ebp)
		if (!cci) return NULL;
    13ea:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    13ee:	75 07                	jne    13f7 <cbufp_comp_info_get+0x46>
    13f0:	b8 00 00 00 00       	mov    $0x0,%eax
    13f5:	eb 43                	jmp    143a <cbufp_comp_info_get+0x89>
		memset(cci, 0, sizeof(struct cbufp_comp_info));
    13f7:	c7 44 24 08 0c 02 00 	movl   $0x20c,0x8(%esp)
    13fe:	00 
    13ff:	c7 44 24 04 00 00 00 	movl   $0x0,0x4(%esp)
    1406:	00 
    1407:	8b 45 f4             	mov    -0xc(%ebp),%eax
    140a:	89 04 24             	mov    %eax,(%esp)
    140d:	e8 fc ff ff ff       	call   140e <cbufp_comp_info_get+0x5d>
		cci->spdid = spdid;
    1412:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1415:	0f b7 55 e4          	movzwl -0x1c(%ebp),%edx
    1419:	66 89 10             	mov    %dx,(%eax)
		cvect_add(&components, cci, spdid);
    141c:	0f b7 45 e4          	movzwl -0x1c(%ebp),%eax
    1420:	89 44 24 08          	mov    %eax,0x8(%esp)
    1424:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1427:	89 44 24 04          	mov    %eax,0x4(%esp)
    142b:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1432:	e8 a1 f1 ff ff       	call   5d8 <cvect_add>
	}
	return cci;
    1437:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
    143a:	c9                   	leave  
    143b:	c3                   	ret    

0000143c <cbufp_comp_info_bin_get>:

static struct cbufp_bin *
cbufp_comp_info_bin_get(struct cbufp_comp_info *cci, int sz)
{
    143c:	55                   	push   %ebp
    143d:	89 e5                	mov    %esp,%ebp
    143f:	83 ec 28             	sub    $0x28,%esp
	int i;

	assert(sz);
    1442:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
    1446:	0f 94 c0             	sete   %al
    1449:	0f b6 c0             	movzbl %al,%eax
    144c:	85 c0                	test   %eax,%eax
    144e:	74 1c                	je     146c <cbufp_comp_info_bin_get+0x30>
    1450:	c7 04 24 9c 0b 00 00 	movl   $0xb9c,(%esp)
    1457:	e8 fc ff ff ff       	call   1458 <cbufp_comp_info_bin_get+0x1c>
    145c:	b8 00 00 00 00       	mov    $0x0,%eax
    1461:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    1467:	e8 df ec ff ff       	call   14b <__cos_noret>
	for (i = 0 ; i < cci->nbin ; i++) {
    146c:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
    1473:	eb 24                	jmp    1499 <cbufp_comp_info_bin_get+0x5d>
		if (sz == cci->cbufs[i].size) return &cci->cbufs[i];
    1475:	8b 55 f4             	mov    -0xc(%ebp),%edx
    1478:	8b 45 08             	mov    0x8(%ebp),%eax
    147b:	8b 44 d0 08          	mov    0x8(%eax,%edx,8),%eax
    147f:	3b 45 0c             	cmp    0xc(%ebp),%eax
    1482:	75 11                	jne    1495 <cbufp_comp_info_bin_get+0x59>
    1484:	8b 45 08             	mov    0x8(%ebp),%eax
    1487:	8d 50 08             	lea    0x8(%eax),%edx
    148a:	8b 45 f4             	mov    -0xc(%ebp),%eax
    148d:	c1 e0 03             	shl    $0x3,%eax
    1490:	8d 04 02             	lea    (%edx,%eax,1),%eax
    1493:	eb 14                	jmp    14a9 <cbufp_comp_info_bin_get+0x6d>
cbufp_comp_info_bin_get(struct cbufp_comp_info *cci, int sz)
{
	int i;

	assert(sz);
	for (i = 0 ; i < cci->nbin ; i++) {
    1495:	83 45 f4 01          	addl   $0x1,-0xc(%ebp)
    1499:	8b 45 08             	mov    0x8(%ebp),%eax
    149c:	8b 40 04             	mov    0x4(%eax),%eax
    149f:	3b 45 f4             	cmp    -0xc(%ebp),%eax
    14a2:	7f d1                	jg     1475 <cbufp_comp_info_bin_get+0x39>
		if (sz == cci->cbufs[i].size) return &cci->cbufs[i];
	}
	return NULL;
    14a4:	b8 00 00 00 00       	mov    $0x0,%eax
}
    14a9:	c9                   	leave  
    14aa:	c3                   	ret    

000014ab <cbufp_comp_info_bin_add>:

static struct cbufp_bin *
cbufp_comp_info_bin_add(struct cbufp_comp_info *cci, int sz)
{
    14ab:	55                   	push   %ebp
    14ac:	89 e5                	mov    %esp,%ebp
	if (sz == CBUFP_MAX_NSZ) return NULL;
    14ae:	83 7d 0c 40          	cmpl   $0x40,0xc(%ebp)
    14b2:	75 07                	jne    14bb <cbufp_comp_info_bin_add+0x10>
    14b4:	b8 00 00 00 00       	mov    $0x0,%eax
    14b9:	eb 34                	jmp    14ef <cbufp_comp_info_bin_add+0x44>
	cci->cbufs[cci->nbin].size = sz;
    14bb:	8b 45 08             	mov    0x8(%ebp),%eax
    14be:	8b 50 04             	mov    0x4(%eax),%edx
    14c1:	8b 45 08             	mov    0x8(%ebp),%eax
    14c4:	8b 4d 0c             	mov    0xc(%ebp),%ecx
    14c7:	89 4c d0 08          	mov    %ecx,0x8(%eax,%edx,8)
	cci->nbin++;
    14cb:	8b 45 08             	mov    0x8(%ebp),%eax
    14ce:	8b 40 04             	mov    0x4(%eax),%eax
    14d1:	8d 50 01             	lea    0x1(%eax),%edx
    14d4:	8b 45 08             	mov    0x8(%ebp),%eax
    14d7:	89 50 04             	mov    %edx,0x4(%eax)

	return &cci->cbufs[cci->nbin-1];
    14da:	8b 45 08             	mov    0x8(%ebp),%eax
    14dd:	8d 50 08             	lea    0x8(%eax),%edx
    14e0:	8b 45 08             	mov    0x8(%ebp),%eax
    14e3:	8b 40 04             	mov    0x4(%eax),%eax
    14e6:	83 e8 01             	sub    $0x1,%eax
    14e9:	c1 e0 03             	shl    $0x3,%eax
    14ec:	8d 04 02             	lea    (%edx,%eax,1),%eax
}
    14ef:	5d                   	pop    %ebp
    14f0:	c3                   	ret    

000014f1 <cbufp_alloc_map>:

static int
cbufp_alloc_map(spdid_t spdid, vaddr_t *daddr, void **page, int size)
{
    14f1:	55                   	push   %ebp
    14f2:	89 e5                	mov    %esp,%ebp
    14f4:	56                   	push   %esi
    14f5:	53                   	push   %ebx
    14f6:	83 ec 40             	sub    $0x40,%esp
    14f9:	8b 45 08             	mov    0x8(%ebp),%eax
    14fc:	66 89 45 e4          	mov    %ax,-0x1c(%ebp)
	void *p;
	vaddr_t dest;
	int off;

	assert(size == (int)round_to_page(size));
    1500:	8b 45 14             	mov    0x14(%ebp),%eax
    1503:	25 00 f0 ff ff       	and    $0xfffff000,%eax
    1508:	3b 45 14             	cmp    0x14(%ebp),%eax
    150b:	0f 95 c0             	setne  %al
    150e:	0f b6 c0             	movzbl %al,%eax
    1511:	85 c0                	test   %eax,%eax
    1513:	74 1c                	je     1531 <cbufp_alloc_map+0x40>
    1515:	c7 04 24 bc 0b 00 00 	movl   $0xbbc,(%esp)
    151c:	e8 fc ff ff ff       	call   151d <cbufp_alloc_map+0x2c>
    1521:	b8 00 00 00 00       	mov    $0x0,%eax
    1526:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    152c:	e8 1a ec ff ff       	call   14b <__cos_noret>
	p = page_alloc(size/PAGE_SIZE);
    1531:	8b 45 14             	mov    0x14(%ebp),%eax
    1534:	89 c2                	mov    %eax,%edx
    1536:	c1 fa 1f             	sar    $0x1f,%edx
    1539:	c1 ea 14             	shr    $0x14,%edx
    153c:	8d 04 02             	lea    (%edx,%eax,1),%eax
    153f:	c1 f8 0c             	sar    $0xc,%eax
    1542:	89 04 24             	mov    %eax,(%esp)
    1545:	e8 fc ff ff ff       	call   1546 <cbufp_alloc_map+0x55>
    154a:	89 45 e8             	mov    %eax,-0x18(%ebp)
	assert(p);
    154d:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
    1551:	0f 94 c0             	sete   %al
    1554:	0f b6 c0             	movzbl %al,%eax
    1557:	85 c0                	test   %eax,%eax
    1559:	74 1c                	je     1577 <cbufp_alloc_map+0x86>
    155b:	c7 04 24 dc 0b 00 00 	movl   $0xbdc,(%esp)
    1562:	e8 fc ff ff ff       	call   1563 <cbufp_alloc_map+0x72>
    1567:	b8 00 00 00 00       	mov    $0x0,%eax
    156c:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    1572:	e8 d4 eb ff ff       	call   14b <__cos_noret>
	memset(p, 0, size);
    1577:	8b 45 14             	mov    0x14(%ebp),%eax
    157a:	89 44 24 08          	mov    %eax,0x8(%esp)
    157e:	c7 44 24 04 00 00 00 	movl   $0x0,0x4(%esp)
    1585:	00 
    1586:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1589:	89 04 24             	mov    %eax,(%esp)
    158c:	e8 fc ff ff ff       	call   158d <cbufp_alloc_map+0x9c>

	dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
    1591:	8b 45 14             	mov    0x14(%ebp),%eax
    1594:	89 c2                	mov    %eax,%edx
    1596:	c1 fa 1f             	sar    $0x1f,%edx
    1599:	c1 ea 14             	shr    $0x14,%edx
    159c:	8d 04 02             	lea    (%edx,%eax,1),%eax
    159f:	c1 f8 0c             	sar    $0xc,%eax
    15a2:	89 c6                	mov    %eax,%esi
    15a4:	0f b7 5d e4          	movzwl -0x1c(%ebp),%ebx
    15a8:	e8 92 ea ff ff       	call   3f <cos_spd_id>
    15ad:	0f b7 c0             	movzwl %ax,%eax
    15b0:	89 74 24 08          	mov    %esi,0x8(%esp)
    15b4:	89 5c 24 04          	mov    %ebx,0x4(%esp)
    15b8:	89 04 24             	mov    %eax,(%esp)
    15bb:	e8 fc ff ff ff       	call   15bc <cbufp_alloc_map+0xcb>
    15c0:	89 45 ec             	mov    %eax,-0x14(%ebp)
	assert(dest);
    15c3:	83 7d ec 00          	cmpl   $0x0,-0x14(%ebp)
    15c7:	0f 94 c0             	sete   %al
    15ca:	0f b6 c0             	movzbl %al,%eax
    15cd:	85 c0                	test   %eax,%eax
    15cf:	74 1c                	je     15ed <cbufp_alloc_map+0xfc>
    15d1:	c7 04 24 fc 0b 00 00 	movl   $0xbfc,(%esp)
    15d8:	e8 fc ff ff ff       	call   15d9 <cbufp_alloc_map+0xe8>
    15dd:	b8 00 00 00 00       	mov    $0x0,%eax
    15e2:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    15e8:	e8 5e eb ff ff       	call   14b <__cos_noret>
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
    15ed:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
    15f4:	eb 65                	jmp    165b <cbufp_alloc_map+0x16a>
		vaddr_t d = dest + off;
    15f6:	8b 45 f0             	mov    -0x10(%ebp),%eax
    15f9:	03 45 ec             	add    -0x14(%ebp),%eax
    15fc:	89 45 f4             	mov    %eax,-0xc(%ebp)
		if (d != 
		    (mman_alias_page(cos_spd_id(), ((vaddr_t)p) + off, spdid, d, MAPPING_RW))) {
    15ff:	0f b7 5d e4          	movzwl -0x1c(%ebp),%ebx
    1603:	8b 55 e8             	mov    -0x18(%ebp),%edx
    1606:	8b 45 f0             	mov    -0x10(%ebp),%eax
    1609:	8d 34 02             	lea    (%edx,%eax,1),%esi
    160c:	e8 2e ea ff ff       	call   3f <cos_spd_id>
    1611:	0f b7 c0             	movzwl %ax,%eax
    1614:	c7 44 24 10 01 00 00 	movl   $0x1,0x10(%esp)
    161b:	00 
    161c:	8b 55 f4             	mov    -0xc(%ebp),%edx
    161f:	89 54 24 0c          	mov    %edx,0xc(%esp)
    1623:	89 5c 24 08          	mov    %ebx,0x8(%esp)
    1627:	89 74 24 04          	mov    %esi,0x4(%esp)
    162b:	89 04 24             	mov    %eax,(%esp)
    162e:	e8 d4 ea ff ff       	call   107 <mman_alias_page>

	dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	assert(dest);
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
		vaddr_t d = dest + off;
		if (d != 
    1633:	3b 45 f4             	cmp    -0xc(%ebp),%eax
    1636:	74 1c                	je     1654 <cbufp_alloc_map+0x163>
		    (mman_alias_page(cos_spd_id(), ((vaddr_t)p) + off, spdid, d, MAPPING_RW))) {
			assert(0);
    1638:	c7 04 24 1c 0c 00 00 	movl   $0xc1c,(%esp)
    163f:	e8 fc ff ff ff       	call   1640 <cbufp_alloc_map+0x14f>
    1644:	b8 00 00 00 00       	mov    $0x0,%eax
    1649:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    164f:	e8 f7 ea ff ff       	call   14b <__cos_noret>
	assert(p);
	memset(p, 0, size);

	dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	assert(dest);
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
    1654:	81 45 f0 00 10 00 00 	addl   $0x1000,-0x10(%ebp)
    165b:	8b 45 f0             	mov    -0x10(%ebp),%eax
    165e:	3b 45 14             	cmp    0x14(%ebp),%eax
    1661:	7c 93                	jl     15f6 <cbufp_alloc_map+0x105>
			assert(0);
			/* TODO: roll back the aliases, etc... */
			valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
		}
	}
	*page  = p;
    1663:	8b 45 10             	mov    0x10(%ebp),%eax
    1666:	8b 55 e8             	mov    -0x18(%ebp),%edx
    1669:	89 10                	mov    %edx,(%eax)
	*daddr = dest;
    166b:	8b 45 0c             	mov    0xc(%ebp),%eax
    166e:	8b 55 ec             	mov    -0x14(%ebp),%edx
    1671:	89 10                	mov    %edx,(%eax)

	return 0;
    1673:	b8 00 00 00 00       	mov    $0x0,%eax
}
    1678:	83 c4 40             	add    $0x40,%esp
    167b:	5b                   	pop    %ebx
    167c:	5e                   	pop    %esi
    167d:	5d                   	pop    %ebp
    167e:	c3                   	ret    

0000167f <cbufp_referenced>:

/* Do any components have a reference to the cbuf? */
static int
cbufp_referenced(struct cbufp_info *cbi)
{
    167f:	55                   	push   %ebp
    1680:	89 e5                	mov    %esp,%ebp
    1682:	83 ec 10             	sub    $0x10,%esp
	struct cbufp_maps *m = &cbi->owner;
    1685:	8b 45 08             	mov    0x8(%ebp),%eax
    1688:	83 c0 0c             	add    $0xc,%eax
    168b:	89 45 f0             	mov    %eax,-0x10(%ebp)
	int sent, recvd;

	sent = recvd = 0;
    168e:	c7 45 f8 00 00 00 00 	movl   $0x0,-0x8(%ebp)
    1695:	8b 45 f8             	mov    -0x8(%ebp),%eax
    1698:	89 45 f4             	mov    %eax,-0xc(%ebp)
	do {
		struct cbuf_meta *meta = m->m;
    169b:	8b 45 f0             	mov    -0x10(%ebp),%eax
    169e:	8b 40 08             	mov    0x8(%eax),%eax
    16a1:	89 45 fc             	mov    %eax,-0x4(%ebp)

		if (meta) {
    16a4:	83 7d fc 00          	cmpl   $0x0,-0x4(%ebp)
    16a8:	74 36                	je     16e0 <cbufp_referenced+0x61>
			/* if (meta->nfo.c.flags & CBUFM_IN_USE) return 1; */
    16aa:	8b 45 fc             	mov    -0x4(%ebp),%eax
    16ad:	0f b7 40 02          	movzwl 0x2(%eax),%eax
    16b1:	66 c1 e8 04          	shr    $0x4,%ax
    16b5:	0f b7 c0             	movzwl %ax,%eax
    16b8:	83 e0 04             	and    $0x4,%eax
    16bb:	85 c0                	test   %eax,%eax
    16bd:	74 07                	je     16c6 <cbufp_referenced+0x47>
    16bf:	b8 01 00 00 00       	mov    $0x1,%eax
    16c4:	eb 42                	jmp    1708 <cbufp_referenced+0x89>
			if (meta->nfo.c.refcnt) return1;
    16c6:	8b 45 fc             	mov    -0x4(%ebp),%eax
    16c9:	0f b6 40 06          	movzbl 0x6(%eax),%eax
    16cd:	0f b6 c0             	movzbl %al,%eax
    16d0:	01 45 f4             	add    %eax,-0xc(%ebp)
			sent  += meta->owner_nfo.c.nsent;
    16d3:	8b 45 fc             	mov    -0x4(%ebp),%eax
    16d6:	0f b6 40 07          	movzbl 0x7(%eax),%eax
    16da:	0f b6 c0             	movzbl %al,%eax
    16dd:	01 45 f8             	add    %eax,-0x8(%ebp)
			recvd += meta->owner_nfo.c.nrecvd;
		}

    16e0:	8b 45 f0             	mov    -0x10(%ebp),%eax
    16e3:	8b 40 0c             	mov    0xc(%eax),%eax
    16e6:	89 45 f0             	mov    %eax,-0x10(%ebp)
		m = FIRST_LIST(m, next, prev);
    16e9:	8b 45 08             	mov    0x8(%ebp),%eax
    16ec:	83 c0 0c             	add    $0xc,%eax
    16ef:	3b 45 f0             	cmp    -0x10(%ebp),%eax
    16f2:	75 a7                	jne    169b <cbufp_referenced+0x1c>
	} while (m != &cbi->owner);

    16f4:	8b 45 f4             	mov    -0xc(%ebp),%eax
    16f7:	3b 45 f8             	cmp    -0x8(%ebp),%eax
    16fa:	74 07                	je     1703 <cbufp_referenced+0x84>
    16fc:	b8 01 00 00 00       	mov    $0x1,%eax
    1701:	eb 05                	jmp    1708 <cbufp_referenced+0x89>
	if (sent != recvd) return 1;
	
    1703:	b8 00 00 00 00       	mov    $0x0,%eax
	return 0;
    1708:	c9                   	leave  
    1709:	c3                   	ret    

0000170a <cbufp_references_clear>:
}

static void
cbufp_references_clear(struct cbufp_info *cbi)
    170a:	55                   	push   %ebp
    170b:	89 e5                	mov    %esp,%ebp
    170d:	83 ec 10             	sub    $0x10,%esp
{
    1710:	8b 45 08             	mov    0x8(%ebp),%eax
    1713:	83 c0 0c             	add    $0xc,%eax
    1716:	89 45 f8             	mov    %eax,-0x8(%ebp)
	struct cbufp_maps *m = &cbi->owner;

	do {
    1719:	8b 45 f8             	mov    -0x8(%ebp),%eax
    171c:	8b 40 08             	mov    0x8(%eax),%eax
    171f:	89 45 fc             	mov    %eax,-0x4(%ebp)
		struct cbuf_meta *meta = m->m;

    1722:	83 7d fc 00          	cmpl   $0x0,-0x4(%ebp)
    1726:	74 14                	je     173c <cbufp_references_clear+0x32>
		if (meta) {
    1728:	8b 45 fc             	mov    -0x4(%ebp),%eax
    172b:	c6 40 07 00          	movb   $0x0,0x7(%eax)
    172f:	8b 45 fc             	mov    -0x4(%ebp),%eax
    1732:	0f b6 50 07          	movzbl 0x7(%eax),%edx
    1736:	8b 45 fc             	mov    -0x4(%ebp),%eax
    1739:	88 50 06             	mov    %dl,0x6(%eax)
			meta->owner_nfo.c.nsent = meta->owner_nfo.c.nrecvd = 0;
		}
    173c:	8b 45 f8             	mov    -0x8(%ebp),%eax
    173f:	8b 40 0c             	mov    0xc(%eax),%eax
    1742:	89 45 f8             	mov    %eax,-0x8(%ebp)
		m = FIRST_LIST(m, next, prev);
    1745:	8b 45 08             	mov    0x8(%ebp),%eax
    1748:	83 c0 0c             	add    $0xc,%eax
    174b:	3b 45 f8             	cmp    -0x8(%ebp),%eax
    174e:	75 c9                	jne    1719 <cbufp_references_clear+0xf>
	} while (m != &cbi->owner);

	return;
    1750:	c9                   	leave  
    1751:	c3                   	ret    

00001752 <cbufp_free_unmap>:
}

static void
cbufp_free_unmap(spdid_t spdid, struct cbufp_info *cbi)
    1752:	55                   	push   %ebp
    1753:	89 e5                	mov    %esp,%ebp
    1755:	57                   	push   %edi
    1756:	56                   	push   %esi
    1757:	53                   	push   %ebx
    1758:	83 ec 3c             	sub    $0x3c,%esp
    175b:	8b 45 08             	mov    0x8(%ebp),%eax
    175e:	66 89 45 d4          	mov    %ax,-0x2c(%ebp)
{
    1762:	8b 45 0c             	mov    0xc(%ebp),%eax
    1765:	83 c0 0c             	add    $0xc,%eax
    1768:	89 45 d8             	mov    %eax,-0x28(%ebp)
	struct cbufp_maps *m = &cbi->owner;
    176b:	8b 45 0c             	mov    0xc(%ebp),%eax
    176e:	8b 40 08             	mov    0x8(%eax),%eax
    1771:	89 45 dc             	mov    %eax,-0x24(%ebp)
	void *ptr = cbi->mem;
	int off;

    1774:	8b 45 0c             	mov    0xc(%ebp),%eax
    1777:	89 04 24             	mov    %eax,(%esp)
    177a:	e8 00 ff ff ff       	call   167f <cbufp_referenced>
    177f:	85 c0                	test   %eax,%eax
    1781:	0f 85 e3 01 00 00    	jne    196a <cbufp_free_unmap+0x218>
	if (cbufp_referenced(cbi)) return;
    1787:	8b 45 0c             	mov    0xc(%ebp),%eax
    178a:	89 04 24             	mov    %eax,(%esp)
    178d:	e8 78 ff ff ff       	call   170a <cbufp_references_clear>
	cbufp_references_clear(cbi);
	do {
    1792:	8b 45 d8             	mov    -0x28(%ebp),%eax
    1795:	8b 40 08             	mov    0x8(%eax),%eax
    1798:	85 c0                	test   %eax,%eax
    179a:	0f 94 c0             	sete   %al
    179d:	0f b6 c0             	movzbl %al,%eax
    17a0:	85 c0                	test   %eax,%eax
    17a2:	74 1c                	je     17c0 <cbufp_free_unmap+0x6e>
    17a4:	c7 04 24 3c 0c 00 00 	movl   $0xc3c,(%esp)
    17ab:	e8 fc ff ff ff       	call   17ac <cbufp_free_unmap+0x5a>
    17b0:	b8 00 00 00 00       	mov    $0x0,%eax
    17b5:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    17bb:	e8 8b e9 ff ff       	call   14b <__cos_noret>
		assert(m->m);
    17c0:	8b 45 d8             	mov    -0x28(%ebp),%eax
    17c3:	8b 40 08             	mov    0x8(%eax),%eax
    17c6:	0f b7 40 02          	movzwl 0x2(%eax),%eax
    17ca:	66 c1 e8 04          	shr    $0x4,%ax
    17ce:	0f b7 c0             	movzwl %ax,%eax
    17d1:	83 e0 04             	and    $0x4,%eax
    17d4:	85 c0                	test   %eax,%eax
    17d6:	0f 95 c0             	setne  %al
    17d9:	0f b6 c0             	movzbl %al,%eax
    17dc:	85 c0                	test   %eax,%eax
    17de:	74 1c                	je     17fc <cbufp_free_unmap+0xaa>
    17e0:	c7 04 24 5c 0c 00 00 	movl   $0xc5c,(%esp)
    17e7:	e8 fc ff ff ff       	call   17e8 <cbufp_free_unmap+0x96>
    17ec:	b8 00 00 00 00       	mov    $0x0,%eax
    17f1:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    17f7:	e8 4f e9 ff ff       	call   14b <__cos_noret>
		/* assert(!(m->m->nfo.c.flags & CBUFM_IN_USE)); */
		assert(!m->m->nfo.c.refcnt);
    17fc:	8b 45 d8             	mov    -0x28(%ebp),%eax
    17ff:	8b 40 08             	mov    0x8(%eax),%eax
    1802:	c7 44 24 08 08 00 00 	movl   $0x8,0x8(%esp)
    1809:	00 
    180a:	c7 44 24 04 00 00 00 	movl   $0x0,0x4(%esp)
    1811:	00 
    1812:	89 04 24             	mov    %eax,(%esp)
    1815:	e8 fc ff ff ff       	call   1816 <cbufp_free_unmap+0xc4>
		/* TODO: fix race here with atomic instruction */
		memset(m->m, 0, sizeof(struct cbuf_meta));
    181a:	8b 45 d8             	mov    -0x28(%ebp),%eax
    181d:	8b 40 0c             	mov    0xc(%eax),%eax
    1820:	89 45 d8             	mov    %eax,-0x28(%ebp)

    1823:	8b 45 0c             	mov    0xc(%ebp),%eax
    1826:	83 c0 0c             	add    $0xc,%eax
    1829:	3b 45 d8             	cmp    -0x28(%ebp),%eax
    182c:	0f 85 60 ff ff ff    	jne    1792 <cbufp_free_unmap+0x40>
		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);
	
    1832:	c7 45 e0 00 00 00 00 	movl   $0x0,-0x20(%ebp)
    1839:	eb 2c                	jmp    1867 <cbufp_free_unmap+0x115>
	/* Unmap all of the pages from the clients */
    183b:	8b 55 dc             	mov    -0x24(%ebp),%edx
    183e:	8b 45 e0             	mov    -0x20(%ebp),%eax
    1841:	8d 1c 02             	lea    (%edx,%eax,1),%ebx
    1844:	e8 f6 e7 ff ff       	call   3f <cos_spd_id>
    1849:	0f b7 c0             	movzwl %ax,%eax
    184c:	c7 44 24 08 00 00 00 	movl   $0x0,0x8(%esp)
    1853:	00 
    1854:	89 5c 24 04          	mov    %ebx,0x4(%esp)
    1858:	89 04 24             	mov    %eax,(%esp)
    185b:	e8 fc ff ff ff       	call   185c <cbufp_free_unmap+0x10a>
		/* TODO: fix race here with atomic instruction */
		memset(m->m, 0, sizeof(struct cbuf_meta));

		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);
	
    1860:	81 45 e0 00 10 00 00 	addl   $0x1000,-0x20(%ebp)
    1867:	8b 45 0c             	mov    0xc(%ebp),%eax
    186a:	8b 40 04             	mov    0x4(%eax),%eax
    186d:	3b 45 e0             	cmp    -0x20(%ebp),%eax
    1870:	7f c9                	jg     183b <cbufp_free_unmap+0xe9>
		mman_revoke_page(cos_spd_id(), (vaddr_t)ptr + off, 0);
	}

	/* 
	 * Deallocate the virtual address in the client, and cleanup
	 * the memory in this component
    1872:	8b 45 0c             	mov    0xc(%ebp),%eax
    1875:	83 c0 0c             	add    $0xc,%eax
    1878:	89 45 d8             	mov    %eax,-0x28(%ebp)
	 */
	m = &cbi->owner;
	do {
		struct cbufp_maps *next;
    187b:	8b 45 d8             	mov    -0x28(%ebp),%eax
    187e:	8b 40 0c             	mov    0xc(%eax),%eax
    1881:	89 45 e4             	mov    %eax,-0x1c(%ebp)

    1884:	8b 45 d8             	mov    -0x28(%ebp),%eax
    1887:	8b 40 0c             	mov    0xc(%eax),%eax
    188a:	8b 55 d8             	mov    -0x28(%ebp),%edx
    188d:	8b 52 10             	mov    0x10(%edx),%edx
    1890:	89 50 10             	mov    %edx,0x10(%eax)
    1893:	8b 45 d8             	mov    -0x28(%ebp),%eax
    1896:	8b 40 10             	mov    0x10(%eax),%eax
    1899:	8b 55 d8             	mov    -0x28(%ebp),%edx
    189c:	8b 52 0c             	mov    0xc(%edx),%edx
    189f:	89 50 0c             	mov    %edx,0xc(%eax)
    18a2:	8b 45 d8             	mov    -0x28(%ebp),%eax
    18a5:	8b 55 d8             	mov    -0x28(%ebp),%edx
    18a8:	89 50 10             	mov    %edx,0x10(%eax)
    18ab:	8b 45 d8             	mov    -0x28(%ebp),%eax
    18ae:	8b 50 10             	mov    0x10(%eax),%edx
    18b1:	8b 45 d8             	mov    -0x28(%ebp),%eax
    18b4:	89 50 0c             	mov    %edx,0xc(%eax)
		next = FIRST_LIST(m, next, prev);
    18b7:	8b 45 0c             	mov    0xc(%ebp),%eax
    18ba:	8b 40 04             	mov    0x4(%eax),%eax
    18bd:	89 c2                	mov    %eax,%edx
    18bf:	c1 fa 1f             	sar    $0x1f,%edx
    18c2:	c1 ea 14             	shr    $0x14,%edx
    18c5:	8d 04 02             	lea    (%edx,%eax,1),%eax
    18c8:	c1 f8 0c             	sar    $0xc,%eax
    18cb:	89 c7                	mov    %eax,%edi
    18cd:	8b 45 d8             	mov    -0x28(%ebp),%eax
    18d0:	8b 40 04             	mov    0x4(%eax),%eax
    18d3:	89 c6                	mov    %eax,%esi
    18d5:	8b 45 d8             	mov    -0x28(%ebp),%eax
    18d8:	0f b7 00             	movzwl (%eax),%eax
    18db:	0f b7 d8             	movzwl %ax,%ebx
    18de:	e8 5c e7 ff ff       	call   3f <cos_spd_id>
    18e3:	0f b7 c0             	movzwl %ax,%eax
    18e6:	89 7c 24 0c          	mov    %edi,0xc(%esp)
    18ea:	89 74 24 08          	mov    %esi,0x8(%esp)
    18ee:	89 5c 24 04          	mov    %ebx,0x4(%esp)
    18f2:	89 04 24             	mov    %eax,(%esp)
    18f5:	e8 fc ff ff ff       	call   18f6 <cbufp_free_unmap+0x1a4>
		REM_LIST(m, next, prev);
    18fa:	8b 45 0c             	mov    0xc(%ebp),%eax
    18fd:	83 c0 0c             	add    $0xc,%eax
    1900:	3b 45 d8             	cmp    -0x28(%ebp),%eax
    1903:	74 0b                	je     1910 <cbufp_free_unmap+0x1be>
    1905:	8b 45 d8             	mov    -0x28(%ebp),%eax
    1908:	89 04 24             	mov    %eax,(%esp)
    190b:	e8 fc ff ff ff       	call   190c <cbufp_free_unmap+0x1ba>
		valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, cbi->size/PAGE_SIZE);
    1910:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    1913:	89 45 d8             	mov    %eax,-0x28(%ebp)
		if (m != &cbi->owner) free(m);
    1916:	8b 45 0c             	mov    0xc(%ebp),%eax
    1919:	83 c0 0c             	add    $0xc,%eax
    191c:	3b 45 d8             	cmp    -0x28(%ebp),%eax
    191f:	0f 85 56 ff ff ff    	jne    187b <cbufp_free_unmap+0x129>
		m = next;
	} while (m != &cbi->owner);

    1925:	8b 45 0c             	mov    0xc(%ebp),%eax
    1928:	8b 40 04             	mov    0x4(%eax),%eax
    192b:	89 c2                	mov    %eax,%edx
    192d:	c1 fa 1f             	sar    $0x1f,%edx
    1930:	c1 ea 14             	shr    $0x14,%edx
    1933:	8d 04 02             	lea    (%edx,%eax,1),%eax
    1936:	c1 f8 0c             	sar    $0xc,%eax
    1939:	89 44 24 04          	mov    %eax,0x4(%esp)
    193d:	8b 45 dc             	mov    -0x24(%ebp),%eax
    1940:	89 04 24             	mov    %eax,(%esp)
    1943:	e8 fc ff ff ff       	call   1944 <cbufp_free_unmap+0x1f2>
	/* deallocate/unlink our data-structures */
    1948:	8b 45 0c             	mov    0xc(%ebp),%eax
    194b:	8b 00                	mov    (%eax),%eax
    194d:	89 44 24 04          	mov    %eax,0x4(%esp)
    1951:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1958:	e8 c4 f7 ff ff       	call   1121 <cmap_del>
	page_free(ptr, cbi->size/PAGE_SIZE);
    195d:	8b 45 0c             	mov    0xc(%ebp),%eax
    1960:	89 04 24             	mov    %eax,(%esp)
    1963:	e8 fc ff ff ff       	call   1964 <cbufp_free_unmap+0x212>
    1968:	eb 01                	jmp    196b <cbufp_free_unmap+0x219>
cbufp_free_unmap(spdid_t spdid, struct cbufp_info *cbi)
{
	struct cbufp_maps *m = &cbi->owner;
	void *ptr = cbi->mem;
	int off;

    196a:	90                   	nop
		m = next;
	} while (m != &cbi->owner);

	/* deallocate/unlink our data-structures */
	page_free(ptr, cbi->size/PAGE_SIZE);
	cmap_del(&cbufs, cbi->cbid);
    196b:	83 c4 3c             	add    $0x3c,%esp
    196e:	5b                   	pop    %ebx
    196f:	5e                   	pop    %esi
    1970:	5f                   	pop    %edi
    1971:	5d                   	pop    %ebp
    1972:	c3                   	ret    

00001973 <cbufp_create>:
	free(cbi);
}

int
    1973:	55                   	push   %ebp
    1974:	89 e5                	mov    %esp,%ebp
    1976:	53                   	push   %ebx
    1977:	83 ec 44             	sub    $0x44,%esp
    197a:	8b 45 08             	mov    0x8(%ebp),%eax
    197d:	66 89 45 d4          	mov    %ax,-0x2c(%ebp)
cbufp_create(spdid_t spdid, int size, long cbid)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
    1981:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
	struct cbuf_meta *meta;
	int ret = 0;

    1988:	8b 45 10             	mov    0x10(%ebp),%eax
    198b:	c1 e8 1f             	shr    $0x1f,%eax
    198e:	85 c0                	test   %eax,%eax
    1990:	74 0a                	je     199c <cbufp_create+0x29>
    1992:	b8 00 00 00 00       	mov    $0x0,%eax
    1997:	e9 d5 02 00 00       	jmp    1c71 <cbufp_create+0x2fe>
	printl("cbufp_create\n");
    199c:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    19a3:	e8 5d f0 ff ff       	call   a05 <lock_take>
    19a8:	85 c0                	test   %eax,%eax
    19aa:	74 17                	je     19c3 <cbufp_create+0x50>
    19ac:	c7 04 24 7c 0c 00 00 	movl   $0xc7c,(%esp)
    19b3:	e8 fc ff ff ff       	call   19b4 <cbufp_create+0x41>
    19b8:	b8 00 00 00 00       	mov    $0x0,%eax
    19bd:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	if (unlikely(cbid < 0)) return 0;
    19c3:	0f b7 45 d4          	movzwl -0x2c(%ebp),%eax
    19c7:	89 04 24             	mov    %eax,(%esp)
    19ca:	e8 e2 f9 ff ff       	call   13b1 <cbufp_comp_info_get>
    19cf:	89 45 e4             	mov    %eax,-0x1c(%ebp)
	CBUFP_TAKE();
    19d2:	83 7d e4 00          	cmpl   $0x0,-0x1c(%ebp)
    19d6:	0f 84 28 02 00 00    	je     1c04 <cbufp_create+0x291>
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;

	/* 
	 * Client wants to allocate a new cbuf, but the meta might not
	 * be mapped in.
    19dc:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
    19e0:	0f 85 47 01 00 00    	jne    1b2d <cbufp_create+0x1ba>
	 */
	if (!cbid) {
		struct cbufp_bin *bin;
    19e6:	c7 04 24 28 00 00 00 	movl   $0x28,(%esp)
    19ed:	e8 fc ff ff ff       	call   19ee <cbufp_create+0x7b>
    19f2:	89 45 e8             	mov    %eax,-0x18(%ebp)

    19f5:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
    19f9:	0f 84 08 02 00 00    	je     1c07 <cbufp_create+0x294>
 		cbi = malloc(sizeof(struct cbufp_info));
		if (!cbi) goto done;

    19ff:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a02:	89 44 24 04          	mov    %eax,0x4(%esp)
    1a06:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1a0d:	e8 52 f5 ff ff       	call   f64 <cmap_add>
    1a12:	89 45 10             	mov    %eax,0x10(%ebp)
		/* Allocate and map in the cbuf. */
    1a15:	8b 55 10             	mov    0x10(%ebp),%edx
    1a18:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a1b:	89 10                	mov    %edx,(%eax)
		cbid = cmap_add(&cbufs, cbi);
    1a1d:	8b 45 0c             	mov    0xc(%ebp),%eax
    1a20:	05 ff 0f 00 00       	add    $0xfff,%eax
    1a25:	25 00 f0 ff ff       	and    $0xfffff000,%eax
    1a2a:	89 45 0c             	mov    %eax,0xc(%ebp)
		cbi->cbid        = cbid;
    1a2d:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a30:	8b 55 0c             	mov    0xc(%ebp),%edx
    1a33:	89 50 04             	mov    %edx,0x4(%eax)
		size             = round_up_to_page(size);
    1a36:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a39:	c7 40 14 00 00 00 00 	movl   $0x0,0x14(%eax)
		cbi->size        = size;
    1a40:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a43:	0f b7 55 d4          	movzwl -0x2c(%ebp),%edx
    1a47:	66 89 50 0c          	mov    %dx,0xc(%eax)
		cbi->owner.m     = NULL;
    1a4b:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a4e:	8d 50 0c             	lea    0xc(%eax),%edx
    1a51:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a54:	89 50 1c             	mov    %edx,0x1c(%eax)
    1a57:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a5a:	8b 50 1c             	mov    0x1c(%eax),%edx
    1a5d:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a60:	89 50 18             	mov    %edx,0x18(%eax)
		cbi->owner.spdid = spdid;
    1a63:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a66:	8b 55 e8             	mov    -0x18(%ebp),%edx
    1a69:	89 50 24             	mov    %edx,0x24(%eax)
    1a6c:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a6f:	8b 50 24             	mov    0x24(%eax),%edx
    1a72:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1a75:	89 50 20             	mov    %edx,0x20(%eax)
		INIT_LIST(&cbi->owner, next, prev);
		INIT_LIST(cbi, next, prev);
    1a78:	8b 45 0c             	mov    0xc(%ebp),%eax
    1a7b:	89 44 24 04          	mov    %eax,0x4(%esp)
    1a7f:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    1a82:	89 04 24             	mov    %eax,(%esp)
    1a85:	e8 b2 f9 ff ff       	call   143c <cbufp_comp_info_bin_get>
    1a8a:	89 45 f4             	mov    %eax,-0xc(%ebp)

    1a8d:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    1a91:	75 15                	jne    1aa8 <cbufp_create+0x135>
    1a93:	8b 45 0c             	mov    0xc(%ebp),%eax
    1a96:	89 44 24 04          	mov    %eax,0x4(%esp)
    1a9a:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    1a9d:	89 04 24             	mov    %eax,(%esp)
    1aa0:	e8 06 fa ff ff       	call   14ab <cbufp_comp_info_bin_add>
    1aa5:	89 45 f4             	mov    %eax,-0xc(%ebp)
		bin = cbufp_comp_info_bin_get(cci, size);
    1aa8:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    1aac:	0f 84 9b 01 00 00    	je     1c4d <cbufp_create+0x2da>
		if (!bin) bin = cbufp_comp_info_bin_add(cci, size);
		if (!bin) goto free;

    1ab2:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1ab5:	83 c0 08             	add    $0x8,%eax
		INIT_LIST(&cbi->owner, next, prev);
		INIT_LIST(cbi, next, prev);

		bin = cbufp_comp_info_bin_get(cci, size);
		if (!bin) bin = cbufp_comp_info_bin_add(cci, size);
		if (!bin) goto free;
    1ab8:	89 c2                	mov    %eax,%edx
    1aba:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1abd:	8d 58 10             	lea    0x10(%eax),%ebx
    1ac0:	0f b7 45 d4          	movzwl -0x2c(%ebp),%eax
    1ac4:	8b 4d 0c             	mov    0xc(%ebp),%ecx
    1ac7:	89 4c 24 0c          	mov    %ecx,0xc(%esp)
    1acb:	89 54 24 08          	mov    %edx,0x8(%esp)
    1acf:	89 5c 24 04          	mov    %ebx,0x4(%esp)
    1ad3:	89 04 24             	mov    %eax,(%esp)
    1ad6:	e8 16 fa ff ff       	call   14f1 <cbufp_alloc_map>
    1adb:	85 c0                	test   %eax,%eax
    1add:	0f 85 6d 01 00 00    	jne    1c50 <cbufp_create+0x2dd>

		if (cbufp_alloc_map(spdid, &(cbi->owner.addr), 
    1ae3:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1ae6:	8b 40 04             	mov    0x4(%eax),%eax
    1ae9:	85 c0                	test   %eax,%eax
    1aeb:	74 35                	je     1b22 <cbufp_create+0x1af>
    1aed:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1af0:	8b 40 04             	mov    0x4(%eax),%eax
    1af3:	8b 50 20             	mov    0x20(%eax),%edx
    1af6:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1af9:	89 50 20             	mov    %edx,0x20(%eax)
    1afc:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1aff:	8b 50 04             	mov    0x4(%eax),%edx
    1b02:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1b05:	89 50 24             	mov    %edx,0x24(%eax)
    1b08:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1b0b:	8b 40 04             	mov    0x4(%eax),%eax
    1b0e:	8b 55 e8             	mov    -0x18(%ebp),%edx
    1b11:	89 50 20             	mov    %edx,0x20(%eax)
    1b14:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1b17:	8b 40 20             	mov    0x20(%eax),%eax
    1b1a:	8b 55 e8             	mov    -0x18(%ebp),%edx
    1b1d:	89 50 24             	mov    %edx,0x24(%eax)
				    (void**)&(cbi->mem), size)) goto free;
    1b20:	eb 3c                	jmp    1b5e <cbufp_create+0x1eb>
    1b22:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1b25:	8b 55 e8             	mov    -0x18(%ebp),%edx
    1b28:	89 50 04             	mov    %edx,0x4(%eax)
    1b2b:	eb 31                	jmp    1b5e <cbufp_create+0x1eb>
		if (bin->c) ADD_LIST(bin->c, cbi, next, prev);
		else        bin->c = cbi;
	} 
	/* If the client has a cbid, then make sure we agree! */
    1b2d:	8b 45 10             	mov    0x10(%ebp),%eax
    1b30:	89 44 24 04          	mov    %eax,0x4(%esp)
    1b34:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1b3b:	e8 00 f4 ff ff       	call   f40 <cmap_lookup>
    1b40:	89 45 e8             	mov    %eax,-0x18(%ebp)
	else {
    1b43:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
    1b47:	0f 84 bd 00 00 00    	je     1c0a <cbufp_create+0x297>
		cbi = cmap_lookup(&cbufs, cbid);
    1b4d:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1b50:	0f b7 40 0c          	movzwl 0xc(%eax),%eax
    1b54:	66 3b 45 d4          	cmp    -0x2c(%ebp),%ax
    1b58:	0f 85 af 00 00 00    	jne    1c0d <cbufp_create+0x29a>
		if (!cbi) goto done;
		if (cbi->owner.spdid != spdid) goto done;
    1b5e:	8b 45 10             	mov    0x10(%ebp),%eax
    1b61:	89 44 24 04          	mov    %eax,0x4(%esp)
    1b65:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    1b68:	89 04 24             	mov    %eax,(%esp)
    1b6b:	e8 03 f7 ff ff       	call   1273 <cbufp_meta_lookup>
    1b70:	89 45 ec             	mov    %eax,-0x14(%ebp)
	}
	meta = cbufp_meta_lookup(cci, cbid);
    1b73:	83 7d ec 00          	cmpl   $0x0,-0x14(%ebp)
    1b77:	75 0d                	jne    1b86 <cbufp_create+0x213>
	/* We need to map in the meta for this cbid.  Tell the client. */
    1b79:	8b 45 10             	mov    0x10(%ebp),%eax
    1b7c:	f7 d8                	neg    %eax
    1b7e:	89 45 f0             	mov    %eax,-0x10(%ebp)
	if (!meta) {
    1b81:	e9 88 00 00 00       	jmp    1c0e <cbufp_create+0x29b>
		ret = cbid * -1;
		goto done;
    1b86:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1b89:	8b 55 ec             	mov    -0x14(%ebp),%edx
    1b8c:	89 50 14             	mov    %edx,0x14(%eax)
	cbi->owner.m = meta;

	/* 
	 * Now we know we have a cbid, a backing structure for it, a
	 * component structure, and the meta mapped in for the cbuf.
	 * Update the meta with the correct addresses and flags!
    1b8f:	c7 44 24 08 08 00 00 	movl   $0x8,0x8(%esp)
    1b96:	00 
    1b97:	c7 44 24 04 00 00 00 	movl   $0x0,0x4(%esp)
    1b9e:	00 
    1b9f:	8b 45 ec             	mov    -0x14(%ebp),%eax
    1ba2:	89 04 24             	mov    %eax,(%esp)
    1ba5:	e8 fc ff ff ff       	call   1ba6 <cbufp_create+0x233>
	 */
    1baa:	8b 45 ec             	mov    -0x14(%ebp),%eax
    1bad:	0f b7 40 02          	movzwl 0x2(%eax),%eax
    1bb1:	66 c1 e8 04          	shr    $0x4,%ax
    1bb5:	83 c8 0f             	or     $0xf,%eax
    1bb8:	89 c2                	mov    %eax,%edx
    1bba:	66 81 e2 ff 0f       	and    $0xfff,%dx
    1bbf:	8b 45 ec             	mov    -0x14(%ebp),%eax
    1bc2:	89 d1                	mov    %edx,%ecx
    1bc4:	c1 e1 04             	shl    $0x4,%ecx
    1bc7:	0f b7 50 02          	movzwl 0x2(%eax),%edx
    1bcb:	83 e2 0f             	and    $0xf,%edx
    1bce:	09 ca                	or     %ecx,%edx
    1bd0:	66 89 50 02          	mov    %dx,0x2(%eax)
	memset(meta, 0, sizeof(struct cbuf_meta));
	/* meta->nfo.c.flags |= CBUFM_IN_USE | CBUFM_TOUCHED |  */
    1bd4:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1bd7:	8b 40 10             	mov    0x10(%eax),%eax
    1bda:	c1 e8 0c             	shr    $0xc,%eax
    1bdd:	89 c2                	mov    %eax,%edx
    1bdf:	81 e2 ff ff 0f 00    	and    $0xfffff,%edx
    1be5:	8b 45 ec             	mov    -0x14(%ebp),%eax
    1be8:	89 d1                	mov    %edx,%ecx
    1bea:	81 e1 ff ff 0f 00    	and    $0xfffff,%ecx
    1bf0:	8b 10                	mov    (%eax),%edx
    1bf2:	81 e2 00 00 f0 ff    	and    $0xfff00000,%edx
    1bf8:	09 ca                	or     %ecx,%edx
    1bfa:	89 10                	mov    %edx,(%eax)
	/* 	             CBUFM_OWNER  | CBUFM_WRITABLE; */
    1bfc:	8b 45 10             	mov    0x10(%ebp),%eax
    1bff:	89 45 f0             	mov    %eax,-0x10(%ebp)
    1c02:	eb 0a                	jmp    1c0e <cbufp_create+0x29b>
	struct cbuf_meta *meta;
	int ret = 0;

	printl("cbufp_create\n");
	if (unlikely(cbid < 0)) return 0;
	CBUFP_TAKE();
    1c04:	90                   	nop
    1c05:	eb 07                	jmp    1c0e <cbufp_create+0x29b>
	 * Client wants to allocate a new cbuf, but the meta might not
	 * be mapped in.
	 */
	if (!cbid) {
		struct cbufp_bin *bin;

    1c07:	90                   	nop
    1c08:	eb 04                	jmp    1c0e <cbufp_create+0x29b>
				    (void**)&(cbi->mem), size)) goto free;
		if (bin->c) ADD_LIST(bin->c, cbi, next, prev);
		else        bin->c = cbi;
	} 
	/* If the client has a cbid, then make sure we agree! */
	else {
    1c0a:	90                   	nop
    1c0b:	eb 01                	jmp    1c0e <cbufp_create+0x29b>
		cbi = cmap_lookup(&cbufs, cbid);
    1c0d:	90                   	nop
	 */
	memset(meta, 0, sizeof(struct cbuf_meta));
	/* meta->nfo.c.flags |= CBUFM_IN_USE | CBUFM_TOUCHED |  */
	/* 	             CBUFM_OWNER  | CBUFM_WRITABLE; */
	meta->nfo.c.flags |= CBUFM_TOUCHED | 
		             CBUFM_OWNER  | CBUFM_WRITABLE;
    1c0e:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1c15:	e8 b5 ed ff ff       	call   9cf <lock_release>
    1c1a:	85 c0                	test   %eax,%eax
    1c1c:	74 17                	je     1c35 <cbufp_create+0x2c2>
    1c1e:	c7 04 24 90 0c 00 00 	movl   $0xc90,(%esp)
    1c25:	e8 fc ff ff ff       	call   1c26 <cbufp_create+0x2b3>
    1c2a:	b8 00 00 00 00       	mov    $0x0,%eax
    1c2f:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	meta->nfo.c.refcnt++;
	meta->nfo.c.ptr    = cbi->owner.addr >> PAGE_ORDER;
    1c35:	8b 45 f0             	mov    -0x10(%ebp),%eax
    1c38:	89 44 24 04          	mov    %eax,0x4(%esp)
    1c3c:	c7 04 24 a4 0c 00 00 	movl   $0xca4,(%esp)
    1c43:	e8 fc ff ff ff       	call   1c44 <cbufp_create+0x2d1>
	ret = cbid;
    1c48:	8b 45 f0             	mov    -0x10(%ebp),%eax
    1c4b:	eb 24                	jmp    1c71 <cbufp_create+0x2fe>
		cbi->owner.m     = NULL;
		cbi->owner.spdid = spdid;
		INIT_LIST(&cbi->owner, next, prev);
		INIT_LIST(cbi, next, prev);

		bin = cbufp_comp_info_bin_get(cci, size);
    1c4d:	90                   	nop
    1c4e:	eb 01                	jmp    1c51 <cbufp_create+0x2de>
		if (!bin) bin = cbufp_comp_info_bin_add(cci, size);
		if (!bin) goto free;

    1c50:	90                   	nop
		             CBUFM_OWNER  | CBUFM_WRITABLE;
	meta->nfo.c.refcnt++;
	meta->nfo.c.ptr    = cbi->owner.addr >> PAGE_ORDER;
	ret = cbid;
done:
	CBUFP_RELEASE();
    1c51:	8b 45 10             	mov    0x10(%ebp),%eax
    1c54:	89 44 24 04          	mov    %eax,0x4(%esp)
    1c58:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1c5f:	e8 bd f4 ff ff       	call   1121 <cmap_del>

    1c64:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1c67:	89 04 24             	mov    %eax,(%esp)
    1c6a:	e8 fc ff ff ff       	call   1c6b <cbufp_create+0x2f8>
	printc("ryx: cbufp cbufp_create ret %d\n", ret);
    1c6f:	eb 9d                	jmp    1c0e <cbufp_create+0x29b>
	return ret;
    1c71:	83 c4 44             	add    $0x44,%esp
    1c74:	5b                   	pop    %ebx
    1c75:	5d                   	pop    %ebp
    1c76:	c3                   	ret    

00001c77 <cbufp_collect>:
 *
 * This function is semantically complicated.  It can block if no
 * cbufps are available, and the component is not supposed to allocate
 * any more.  It can return no cbufps even if they are available to
 * force the pool of cbufps to be expanded (the client will call
 * cbufp_create in this case).  Or, the common case: it can return a
    1c77:	55                   	push   %ebp
    1c78:	89 e5                	mov    %esp,%ebp
    1c7a:	83 ec 48             	sub    $0x48,%esp
    1c7d:	8b 45 08             	mov    0x8(%ebp),%eax
    1c80:	66 89 45 d4          	mov    %ax,-0x2c(%ebp)
 * number of available cbufs.
    1c84:	c7 04 24 c4 0c 00 00 	movl   $0xcc4,(%esp)
    1c8b:	e8 fc ff ff ff       	call   1c8c <cbufp_collect+0x15>
 */
int
    1c90:	c7 45 e4 00 00 00 00 	movl   $0x0,-0x1c(%ebp)
cbufp_collect(spdid_t spdid, int size, long cbid)
{
	printc("ryx: go into cbufp cbufp_collect\n");
	long *buf;
    1c97:	c7 45 f4 ea ff ff ff 	movl   $0xffffffea,-0xc(%ebp)
	int off = 0;
	struct cbufp_info *cbi;
	struct cbufp_comp_info *cci;
	struct cbufp_bin *bin;
    1c9e:	8b 45 10             	mov    0x10(%ebp),%eax
    1ca1:	c7 44 24 04 00 10 00 	movl   $0x1000,0x4(%esp)
    1ca8:	00 
    1ca9:	89 04 24             	mov    %eax,(%esp)
    1cac:	e8 3d f1 ff ff       	call   dee <cbuf2buf>
    1cb1:	89 45 e0             	mov    %eax,-0x20(%ebp)
	int ret = -EINVAL;
    1cb4:	83 7d e0 00          	cmpl   $0x0,-0x20(%ebp)
    1cb8:	75 0a                	jne    1cc4 <cbufp_collect+0x4d>
    1cba:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
    1cbf:	e9 19 01 00 00       	jmp    1ddd <cbufp_collect+0x166>

	printl("cbufp_collect\n");
    1cc4:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1ccb:	e8 35 ed ff ff       	call   a05 <lock_take>
    1cd0:	85 c0                	test   %eax,%eax
    1cd2:	74 17                	je     1ceb <cbufp_collect+0x74>
    1cd4:	c7 04 24 e6 0c 00 00 	movl   $0xce6,(%esp)
    1cdb:	e8 fc ff ff ff       	call   1cdc <cbufp_collect+0x65>
    1ce0:	b8 00 00 00 00       	mov    $0x0,%eax
    1ce5:	c7 00 00 00 00 00    	movl   $0x0,(%eax)

    1ceb:	0f b7 45 d4          	movzwl -0x2c(%ebp),%eax
    1cef:	89 04 24             	mov    %eax,(%esp)
    1cf2:	e8 ba f6 ff ff       	call   13b1 <cbufp_comp_info_get>
    1cf7:	89 45 ec             	mov    %eax,-0x14(%ebp)
	buf = cbuf2buf(cbid, PAGE_SIZE);
    1cfa:	83 7d ec 00          	cmpl   $0x0,-0x14(%ebp)
    1cfe:	75 0c                	jne    1d0c <cbufp_collect+0x95>
    1d00:	c7 45 f4 f4 ff ff ff 	movl   $0xfffffff4,-0xc(%ebp)
    1d07:	e9 94 00 00 00       	jmp    1da0 <cbufp_collect+0x129>
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) ERR_THROW(-ENOMEM, done);

	/* 
	 * Go through all cbufs we own, and report all of them that
    1d0c:	8b 45 0c             	mov    0xc(%ebp),%eax
    1d0f:	05 ff 0f 00 00       	add    $0xfff,%eax
    1d14:	25 00 f0 ff ff       	and    $0xfffff000,%eax
    1d19:	89 44 24 04          	mov    %eax,0x4(%esp)
    1d1d:	8b 45 ec             	mov    -0x14(%ebp),%eax
    1d20:	89 04 24             	mov    %eax,(%esp)
    1d23:	e8 14 f7 ff ff       	call   143c <cbufp_comp_info_bin_get>
    1d28:	89 45 f0             	mov    %eax,-0x10(%ebp)
	 * have no current references to them.  Unfortunately, this is
    1d2b:	83 7d f0 00          	cmpl   $0x0,-0x10(%ebp)
    1d2f:	75 09                	jne    1d3a <cbufp_collect+0xc3>
    1d31:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
    1d38:	eb 66                	jmp    1da0 <cbufp_collect+0x129>
	 * O(N*M), N = min(num cbufs, PAGE_SIZE/sizeof(int)), and M =
    1d3a:	8b 45 f0             	mov    -0x10(%ebp),%eax
    1d3d:	8b 40 04             	mov    0x4(%eax),%eax
    1d40:	89 45 e8             	mov    %eax,-0x18(%ebp)
	 * num components.
	 */
    1d43:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
    1d47:	74 4d                	je     1d96 <cbufp_collect+0x11f>
	bin = cbufp_comp_info_bin_get(cci, round_up_to_page(size));
    1d49:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1d4c:	89 04 24             	mov    %eax,(%esp)
    1d4f:	e8 2b f9 ff ff       	call   167f <cbufp_referenced>
    1d54:	85 c0                	test   %eax,%eax
    1d56:	75 28                	jne    1d80 <cbufp_collect+0x109>
	if (!bin) ERR_THROW(0, done);
    1d58:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1d5b:	89 04 24             	mov    %eax,(%esp)
    1d5e:	e8 a7 f9 ff ff       	call   170a <cbufp_references_clear>
	cbi = bin->c;
    1d63:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    1d66:	c1 e0 02             	shl    $0x2,%eax
    1d69:	03 45 e0             	add    -0x20(%ebp),%eax
    1d6c:	8b 55 e8             	mov    -0x18(%ebp),%edx
    1d6f:	8b 12                	mov    (%edx),%edx
    1d71:	89 10                	mov    %edx,(%eax)
    1d73:	83 45 e4 01          	addl   $0x1,-0x1c(%ebp)
	do {
    1d77:	81 7d e4 00 04 00 00 	cmpl   $0x400,-0x1c(%ebp)
    1d7e:	74 19                	je     1d99 <cbufp_collect+0x122>
		if (!cbi) break;
		if (!cbufp_referenced(cbi)) {
    1d80:	8b 45 e8             	mov    -0x18(%ebp),%eax
    1d83:	8b 40 20             	mov    0x20(%eax),%eax
    1d86:	89 45 e8             	mov    %eax,-0x18(%ebp)
			cbufp_references_clear(cbi);
    1d89:	8b 45 f0             	mov    -0x10(%ebp),%eax
    1d8c:	8b 40 04             	mov    0x4(%eax),%eax
    1d8f:	3b 45 e8             	cmp    -0x18(%ebp),%eax
    1d92:	75 af                	jne    1d43 <cbufp_collect+0xcc>
    1d94:	eb 04                	jmp    1d9a <cbufp_collect+0x123>
	/* 
	 * Go through all cbufs we own, and report all of them that
	 * have no current references to them.  Unfortunately, this is
	 * O(N*M), N = min(num cbufs, PAGE_SIZE/sizeof(int)), and M =
	 * num components.
	 */
    1d96:	90                   	nop
    1d97:	eb 01                	jmp    1d9a <cbufp_collect+0x123>
	bin = cbufp_comp_info_bin_get(cci, round_up_to_page(size));
	if (!bin) ERR_THROW(0, done);
	cbi = bin->c;
	do {
    1d99:	90                   	nop
		if (!cbi) break;
		if (!cbufp_referenced(cbi)) {
			cbufp_references_clear(cbi);
			buf[off++] = cbi->cbid;
    1d9a:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    1d9d:	89 45 f4             	mov    %eax,-0xc(%ebp)
			if (off == PAGE_SIZE/sizeof(int)) break;
		}
    1da0:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1da7:	e8 23 ec ff ff       	call   9cf <lock_release>
    1dac:	85 c0                	test   %eax,%eax
    1dae:	74 17                	je     1dc7 <cbufp_collect+0x150>
    1db0:	c7 04 24 fa 0c 00 00 	movl   $0xcfa,(%esp)
    1db7:	e8 fc ff ff ff       	call   1db8 <cbufp_collect+0x141>
    1dbc:	b8 00 00 00 00       	mov    $0x0,%eax
    1dc1:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
		cbi = FIRST_LIST(cbi, next, prev);
    1dc7:	8b 45 f4             	mov    -0xc(%ebp),%eax
    1dca:	89 44 24 04          	mov    %eax,0x4(%esp)
    1dce:	c7 04 24 10 0d 00 00 	movl   $0xd10,(%esp)
    1dd5:	e8 fc ff ff ff       	call   1dd6 <cbufp_collect+0x15f>
	} while (cbi != bin->c);
    1dda:	8b 45 f4             	mov    -0xc(%ebp),%eax
	ret = off;
    1ddd:	c9                   	leave  
    1dde:	c3                   	ret    

00001ddf <cbufp_delete>:
	CBUFP_RELEASE();
	printc("ryx: cbufp.c cbuf_collect ret %d\n", ret);
	return ret;
}

/* 
    1ddf:	55                   	push   %ebp
    1de0:	89 e5                	mov    %esp,%ebp
    1de2:	83 ec 38             	sub    $0x38,%esp
    1de5:	8b 45 08             	mov    0x8(%ebp),%eax
    1de8:	66 89 45 e4          	mov    %ax,-0x1c(%ebp)
 * Called by cbufp_deref.
 */
int
    1dec:	c7 45 f4 ea ff ff ff 	movl   $0xffffffea,-0xc(%ebp)
cbufp_delete(spdid_t spdid, int cbid)
{
	struct cbufp_comp_info *cci;
    1df3:	c7 04 24 34 0d 00 00 	movl   $0xd34,(%esp)
    1dfa:	e8 fc ff ff ff       	call   1dfb <cbufp_delete+0x1c>
    1dff:	b8 00 00 00 00       	mov    $0x0,%eax
    1e04:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    1e0a:	e8 3c e3 ff ff       	call   14b <__cos_noret>

00001e0f <cbufp_retrieve>:
done:
	CBUFP_RELEASE();
	return ret;
}

/* 
    1e0f:	55                   	push   %ebp
    1e10:	89 e5                	mov    %esp,%ebp
    1e12:	57                   	push   %edi
    1e13:	56                   	push   %esi
    1e14:	53                   	push   %ebx
    1e15:	83 ec 5c             	sub    $0x5c,%esp
    1e18:	8b 45 08             	mov    0x8(%ebp),%eax
    1e1b:	66 89 45 c4          	mov    %ax,-0x3c(%ebp)
 */
int
cbufp_retrieve(spdid_t spdid, int cbid, int size)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
    1e1f:	c7 45 e0 ea ff ff ff 	movl   $0xffffffea,-0x20(%ebp)
	struct cbuf_meta *meta;
	struct cbufp_maps *map;
	vaddr_t dest;
	void *page;
    1e26:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1e2d:	e8 d3 eb ff ff       	call   a05 <lock_take>
    1e32:	85 c0                	test   %eax,%eax
    1e34:	74 17                	je     1e4d <cbufp_retrieve+0x3e>
    1e36:	c7 04 24 54 0d 00 00 	movl   $0xd54,(%esp)
    1e3d:	e8 fc ff ff ff       	call   1e3e <cbufp_retrieve+0x2f>
    1e42:	b8 00 00 00 00       	mov    $0x0,%eax
    1e47:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	int ret = -EINVAL, off;
    1e4d:	0f b7 45 c4          	movzwl -0x3c(%ebp),%eax
    1e51:	89 04 24             	mov    %eax,(%esp)
    1e54:	e8 58 f5 ff ff       	call   13b1 <cbufp_comp_info_get>
    1e59:	89 45 c8             	mov    %eax,-0x38(%ebp)

    1e5c:	83 7d c8 00          	cmpl   $0x0,-0x38(%ebp)
    1e60:	0f 84 7a 02 00 00    	je     20e0 <cbufp_retrieve+0x2d1>
	printl("cbufp_retrieve\n");
    1e66:	8b 45 0c             	mov    0xc(%ebp),%eax
    1e69:	89 44 24 04          	mov    %eax,0x4(%esp)
    1e6d:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    1e74:	e8 c7 f0 ff ff       	call   f40 <cmap_lookup>
    1e79:	89 45 cc             	mov    %eax,-0x34(%ebp)

    1e7c:	83 7d cc 00          	cmpl   $0x0,-0x34(%ebp)
    1e80:	0f 84 5d 02 00 00    	je     20e3 <cbufp_retrieve+0x2d4>
	CBUFP_TAKE();
	cci        = cbufp_comp_info_get(spdid);
    1e86:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1e89:	0f b7 40 0c          	movzwl 0xc(%eax),%eax
    1e8d:	66 3b 45 c4          	cmp    -0x3c(%ebp),%ax
    1e91:	0f 84 4f 02 00 00    	je     20e6 <cbufp_retrieve+0x2d7>
	if (!cci) goto done;
    1e97:	8b 45 0c             	mov    0xc(%ebp),%eax
    1e9a:	89 44 24 04          	mov    %eax,0x4(%esp)
    1e9e:	8b 45 c8             	mov    -0x38(%ebp),%eax
    1ea1:	89 04 24             	mov    %eax,(%esp)
    1ea4:	e8 ca f3 ff ff       	call   1273 <cbufp_meta_lookup>
    1ea9:	89 45 d0             	mov    %eax,-0x30(%ebp)
	cbi        = cmap_lookup(&cbufs, cbid);
    1eac:	83 7d d0 00          	cmpl   $0x0,-0x30(%ebp)
    1eb0:	0f 84 33 02 00 00    	je     20e9 <cbufp_retrieve+0x2da>
	if (!cbi) goto done;
	/* shouldn't cbuf2buf your own buffer! */
    1eb6:	c7 04 24 14 00 00 00 	movl   $0x14,(%esp)
    1ebd:	e8 fc ff ff ff       	call   1ebe <cbufp_retrieve+0xaf>
    1ec2:	89 45 d4             	mov    %eax,-0x2c(%ebp)
	if (cbi->owner.spdid == spdid) goto done;
    1ec5:	83 7d d4 00          	cmpl   $0x0,-0x2c(%ebp)
    1ec9:	75 0c                	jne    1ed7 <cbufp_retrieve+0xc8>
    1ecb:	c7 45 e0 f4 ff ff ff 	movl   $0xfffffff4,-0x20(%ebp)
    1ed2:	e9 16 02 00 00       	jmp    20ed <cbufp_retrieve+0x2de>
	meta       = cbufp_meta_lookup(cci, cbid);
    1ed7:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1eda:	8b 40 04             	mov    0x4(%eax),%eax
    1edd:	3b 45 10             	cmp    0x10(%ebp),%eax
    1ee0:	0f 8c 06 02 00 00    	jl     20ec <cbufp_retrieve+0x2dd>
	if (!meta) goto done;
    1ee6:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1ee9:	8b 40 04             	mov    0x4(%eax),%eax
    1eec:	89 c2                	mov    %eax,%edx
    1eee:	81 e2 00 f0 ff ff    	and    $0xfffff000,%edx
    1ef4:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1ef7:	8b 40 04             	mov    0x4(%eax),%eax
    1efa:	39 c2                	cmp    %eax,%edx
    1efc:	0f 95 c0             	setne  %al
    1eff:	0f b6 c0             	movzbl %al,%eax
    1f02:	85 c0                	test   %eax,%eax
    1f04:	74 1c                	je     1f22 <cbufp_retrieve+0x113>
    1f06:	c7 04 24 68 0d 00 00 	movl   $0xd68,(%esp)
    1f0d:	e8 fc ff ff ff       	call   1f0e <cbufp_retrieve+0xff>
    1f12:	b8 00 00 00 00       	mov    $0x0,%eax
    1f17:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    1f1d:	e8 29 e2 ff ff       	call   14b <__cos_noret>

    1f22:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1f25:	8b 40 04             	mov    0x4(%eax),%eax
    1f28:	89 45 10             	mov    %eax,0x10(%ebp)
	map        = malloc(sizeof(struct cbufp_maps));
    1f2b:	8b 45 10             	mov    0x10(%ebp),%eax
    1f2e:	89 c2                	mov    %eax,%edx
    1f30:	c1 fa 1f             	sar    $0x1f,%edx
    1f33:	c1 ea 14             	shr    $0x14,%edx
    1f36:	8d 04 02             	lea    (%edx,%eax,1),%eax
    1f39:	c1 f8 0c             	sar    $0xc,%eax
    1f3c:	89 c6                	mov    %eax,%esi
    1f3e:	0f b7 5d c4          	movzwl -0x3c(%ebp),%ebx
    1f42:	e8 f8 e0 ff ff       	call   3f <cos_spd_id>
    1f47:	0f b7 c0             	movzwl %ax,%eax
    1f4a:	89 74 24 08          	mov    %esi,0x8(%esp)
    1f4e:	89 5c 24 04          	mov    %ebx,0x4(%esp)
    1f52:	89 04 24             	mov    %eax,(%esp)
    1f55:	e8 fc ff ff ff       	call   1f56 <cbufp_retrieve+0x147>
    1f5a:	89 45 d8             	mov    %eax,-0x28(%ebp)
	if (!map) ERR_THROW(-ENOMEM, done);
    1f5d:	83 7d d8 00          	cmpl   $0x0,-0x28(%ebp)
    1f61:	0f 84 b8 01 00 00    	je     211f <cbufp_retrieve+0x310>
	if (size > cbi->size) goto done;
	assert((int)round_to_page(cbi->size) == cbi->size);
    1f67:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1f6a:	0f b7 55 c4          	movzwl -0x3c(%ebp),%edx
    1f6e:	66 89 10             	mov    %dx,(%eax)
	size       = cbi->size;
    1f71:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1f74:	8b 55 d0             	mov    -0x30(%ebp),%edx
    1f77:	89 50 08             	mov    %edx,0x8(%eax)
	dest       = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
    1f7a:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1f7d:	8b 55 d8             	mov    -0x28(%ebp),%edx
    1f80:	89 50 04             	mov    %edx,0x4(%eax)
	if (!dest) goto free;
    1f83:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1f86:	8b 55 d4             	mov    -0x2c(%ebp),%edx
    1f89:	89 50 10             	mov    %edx,0x10(%eax)
    1f8c:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1f8f:	8b 50 10             	mov    0x10(%eax),%edx
    1f92:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1f95:	89 50 0c             	mov    %edx,0xc(%eax)

    1f98:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1f9b:	8b 50 18             	mov    0x18(%eax),%edx
    1f9e:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1fa1:	89 50 0c             	mov    %edx,0xc(%eax)
    1fa4:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1fa7:	8d 50 0c             	lea    0xc(%eax),%edx
    1faa:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1fad:	89 50 10             	mov    %edx,0x10(%eax)
    1fb0:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1fb3:	8b 55 d4             	mov    -0x2c(%ebp),%edx
    1fb6:	89 50 18             	mov    %edx,0x18(%eax)
    1fb9:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    1fbc:	8b 40 0c             	mov    0xc(%eax),%eax
    1fbf:	8b 55 d4             	mov    -0x2c(%ebp),%edx
    1fc2:	89 50 10             	mov    %edx,0x10(%eax)
	map->spdid = spdid;
	map->m     = meta;
    1fc5:	8b 45 cc             	mov    -0x34(%ebp),%eax
    1fc8:	8b 40 08             	mov    0x8(%eax),%eax
    1fcb:	89 45 dc             	mov    %eax,-0x24(%ebp)
	map->addr  = dest;
    1fce:	83 7d dc 00          	cmpl   $0x0,-0x24(%ebp)
    1fd2:	0f 94 c0             	sete   %al
    1fd5:	0f b6 c0             	movzbl %al,%eax
    1fd8:	85 c0                	test   %eax,%eax
    1fda:	74 1c                	je     1ff8 <cbufp_retrieve+0x1e9>
    1fdc:	c7 04 24 88 0d 00 00 	movl   $0xd88,(%esp)
    1fe3:	e8 fc ff ff ff       	call   1fe4 <cbufp_retrieve+0x1d5>
    1fe8:	b8 00 00 00 00       	mov    $0x0,%eax
    1fed:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    1ff3:	e8 53 e1 ff ff       	call   14b <__cos_noret>
	INIT_LIST(map, next, prev);
    1ff8:	c7 45 e4 00 00 00 00 	movl   $0x0,-0x1c(%ebp)
    1fff:	eb 6d                	jmp    206e <cbufp_retrieve+0x25f>
	ADD_LIST(&cbi->owner, map, next, prev);
    2001:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    2004:	89 c7                	mov    %eax,%edi
    2006:	03 7d d8             	add    -0x28(%ebp),%edi

    2009:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    200c:	89 c6                	mov    %eax,%esi
    200e:	03 75 d8             	add    -0x28(%ebp),%esi
    2011:	0f b7 5d c4          	movzwl -0x3c(%ebp),%ebx
    2015:	8b 55 dc             	mov    -0x24(%ebp),%edx
    2018:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    201b:	01 c2                	add    %eax,%edx
    201d:	89 55 c0             	mov    %edx,-0x40(%ebp)
    2020:	e8 1a e0 ff ff       	call   3f <cos_spd_id>
    2025:	0f b7 c0             	movzwl %ax,%eax
    2028:	c7 44 24 10 00 00 00 	movl   $0x0,0x10(%esp)
    202f:	00 
    2030:	89 74 24 0c          	mov    %esi,0xc(%esp)
    2034:	89 5c 24 08          	mov    %ebx,0x8(%esp)
    2038:	8b 55 c0             	mov    -0x40(%ebp),%edx
    203b:	89 54 24 04          	mov    %edx,0x4(%esp)
    203f:	89 04 24             	mov    %eax,(%esp)
    2042:	e8 c0 e0 ff ff       	call   107 <mman_alias_page>

	map->spdid = spdid;
	map->m     = meta;
	map->addr  = dest;
	INIT_LIST(map, next, prev);
	ADD_LIST(&cbi->owner, map, next, prev);
    2047:	39 c7                	cmp    %eax,%edi
    2049:	74 1c                	je     2067 <cbufp_retrieve+0x258>

	page = cbi->mem;
    204b:	c7 04 24 a8 0d 00 00 	movl   $0xda8,(%esp)
    2052:	e8 fc ff ff ff       	call   2053 <cbufp_retrieve+0x244>
    2057:	b8 00 00 00 00       	mov    $0x0,%eax
    205c:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    2062:	e8 e4 e0 ff ff       	call   14b <__cos_noret>
	if (!dest) goto free;

	map->spdid = spdid;
	map->m     = meta;
	map->addr  = dest;
	INIT_LIST(map, next, prev);
    2067:	81 45 e4 00 10 00 00 	addl   $0x1000,-0x1c(%ebp)
    206e:	8b 45 e4             	mov    -0x1c(%ebp),%eax
    2071:	3b 45 10             	cmp    0x10(%ebp),%eax
    2074:	7c 8b                	jl     2001 <cbufp_retrieve+0x1f2>
	page = cbi->mem;
	assert(page);
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
		if (dest+off != 
		    (mman_alias_page(cos_spd_id(), ((vaddr_t)page)+off, spdid, dest+off, MAPPING_READ))) {
			assert(0);
    2076:	8b 45 d0             	mov    -0x30(%ebp),%eax
    2079:	0f b7 40 02          	movzwl 0x2(%eax),%eax
    207d:	66 c1 e8 04          	shr    $0x4,%ax
    2081:	83 c8 08             	or     $0x8,%eax
    2084:	89 c2                	mov    %eax,%edx
    2086:	66 81 e2 ff 0f       	and    $0xfff,%dx
    208b:	8b 45 d0             	mov    -0x30(%ebp),%eax
    208e:	89 d1                	mov    %edx,%ecx
    2090:	c1 e1 04             	shl    $0x4,%ecx
    2093:	0f b7 50 02          	movzwl 0x2(%eax),%edx
    2097:	83 e2 0f             	and    $0xf,%edx
    209a:	09 ca                	or     %ecx,%edx
    209c:	66 89 50 02          	mov    %dx,0x2(%eax)
			valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
    20a0:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    20a3:	8b 40 04             	mov    0x4(%eax),%eax
    20a6:	c1 e8 0c             	shr    $0xc,%eax
    20a9:	89 c2                	mov    %eax,%edx
    20ab:	81 e2 ff ff 0f 00    	and    $0xfffff,%edx
    20b1:	8b 45 d0             	mov    -0x30(%ebp),%eax
    20b4:	89 d1                	mov    %edx,%ecx
    20b6:	81 e1 ff ff 0f 00    	and    $0xfffff,%ecx
    20bc:	8b 10                	mov    (%eax),%edx
    20be:	81 e2 00 00 f0 ff    	and    $0xfff00000,%edx
    20c4:	09 ca                	or     %ecx,%edx
    20c6:	89 10                	mov    %edx,(%eax)
		}
    20c8:	8b 45 cc             	mov    -0x34(%ebp),%eax
    20cb:	8b 40 04             	mov    0x4(%eax),%eax
    20ce:	89 c2                	mov    %eax,%edx
    20d0:	8b 45 d0             	mov    -0x30(%ebp),%eax
    20d3:	66 89 50 04          	mov    %dx,0x4(%eax)
	}
    20d7:	c7 45 e0 00 00 00 00 	movl   $0x0,-0x20(%ebp)
    20de:	eb 0d                	jmp    20ed <cbufp_retrieve+0x2de>
	struct cbuf_meta *meta;
	struct cbufp_maps *map;
	vaddr_t dest;
	void *page;
	int ret = -EINVAL, off;

    20e0:	90                   	nop
    20e1:	eb 0a                	jmp    20ed <cbufp_retrieve+0x2de>
	printl("cbufp_retrieve\n");

    20e3:	90                   	nop
    20e4:	eb 07                	jmp    20ed <cbufp_retrieve+0x2de>
	CBUFP_TAKE();
	cci        = cbufp_comp_info_get(spdid);
    20e6:	90                   	nop
    20e7:	eb 04                	jmp    20ed <cbufp_retrieve+0x2de>
	if (!cci) goto done;
	cbi        = cmap_lookup(&cbufs, cbid);
    20e9:	90                   	nop
    20ea:	eb 01                	jmp    20ed <cbufp_retrieve+0x2de>
	if (!cbi) goto done;
	/* shouldn't cbuf2buf your own buffer! */
	if (cbi->owner.spdid == spdid) goto done;
	meta       = cbufp_meta_lookup(cci, cbid);
    20ec:	90                   	nop
			assert(0);
			valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
		}
	}

	meta->nfo.c.flags |= CBUFM_TOUCHED;
    20ed:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    20f4:	e8 d6 e8 ff ff       	call   9cf <lock_release>
    20f9:	85 c0                	test   %eax,%eax
    20fb:	74 17                	je     2114 <cbufp_retrieve+0x305>
    20fd:	c7 04 24 c8 0d 00 00 	movl   $0xdc8,(%esp)
    2104:	e8 fc ff ff ff       	call   2105 <cbufp_retrieve+0x2f6>
    2109:	b8 00 00 00 00       	mov    $0x0,%eax
    210e:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	meta->nfo.c.ptr    = map->addr >> PAGE_ORDER;
	meta->sz           = cbi->size;
    2114:	8b 45 e0             	mov    -0x20(%ebp),%eax
	ret                = 0;
done:
	CBUFP_RELEASE();

    2117:	83 c4 5c             	add    $0x5c,%esp
    211a:	5b                   	pop    %ebx
    211b:	5e                   	pop    %esi
    211c:	5f                   	pop    %edi
    211d:	5d                   	pop    %ebp
    211e:	c3                   	ret    
	if (cbi->owner.spdid == spdid) goto done;
	meta       = cbufp_meta_lookup(cci, cbid);
	if (!meta) goto done;

	map        = malloc(sizeof(struct cbufp_maps));
	if (!map) ERR_THROW(-ENOMEM, done);
    211f:	90                   	nop

	meta->nfo.c.flags |= CBUFM_TOUCHED;
	meta->nfo.c.ptr    = map->addr >> PAGE_ORDER;
	meta->sz           = cbi->size;
	ret                = 0;
done:
    2120:	8b 45 d4             	mov    -0x2c(%ebp),%eax
    2123:	89 04 24             	mov    %eax,(%esp)
    2126:	e8 fc ff ff ff       	call   2127 <cbufp_retrieve+0x318>
	CBUFP_RELEASE();
    212b:	eb c0                	jmp    20ed <cbufp_retrieve+0x2de>

0000212d <cbufp_register>:

	return ret;
free:
	free(map);
	goto done;
    212d:	55                   	push   %ebp
    212e:	89 e5                	mov    %esp,%ebp
    2130:	83 ec 48             	sub    $0x48,%esp
    2133:	8b 45 08             	mov    0x8(%ebp),%eax
    2136:	66 89 45 d4          	mov    %ax,-0x2c(%ebp)
}

vaddr_t
cbufp_register(spdid_t spdid, long cbid)
    213a:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
{
	struct cbufp_comp_info  *cci;
    2141:	c7 04 24 dc 0d 00 00 	movl   $0xddc,(%esp)
    2148:	e8 fc ff ff ff       	call   2149 <cbufp_register+0x1c>
	struct cbufp_meta_range *cmr;
	void *p;
    214d:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    2154:	e8 ac e8 ff ff       	call   a05 <lock_take>
    2159:	85 c0                	test   %eax,%eax
    215b:	74 17                	je     2174 <cbufp_register+0x47>
    215d:	c7 04 24 ff 0d 00 00 	movl   $0xdff,(%esp)
    2164:	e8 fc ff ff ff       	call   2165 <cbufp_register+0x38>
    2169:	b8 00 00 00 00       	mov    $0x0,%eax
    216e:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	vaddr_t dest, ret = 0;
    2174:	0f b7 45 d4          	movzwl -0x2c(%ebp),%eax
    2178:	89 04 24             	mov    %eax,(%esp)
    217b:	e8 31 f2 ff ff       	call   13b1 <cbufp_comp_info_get>
    2180:	89 45 ec             	mov    %eax,-0x14(%ebp)

    2183:	83 7d ec 00          	cmpl   $0x0,-0x14(%ebp)
    2187:	0f 84 e1 00 00 00    	je     226e <cbufp_register+0x141>
	printc("ryx: go into cbufp cbufp_register\n");
    218d:	8b 45 0c             	mov    0xc(%ebp),%eax
    2190:	89 44 24 04          	mov    %eax,0x4(%esp)
    2194:	8b 45 ec             	mov    -0x14(%ebp),%eax
    2197:	89 04 24             	mov    %eax,(%esp)
    219a:	e8 4d f0 ff ff       	call   11ec <cbufp_meta_lookup_cmr>
    219f:	89 45 f0             	mov    %eax,-0x10(%ebp)
	printl("cbufp_register\n");
    21a2:	83 7d f0 00          	cmpl   $0x0,-0x10(%ebp)
    21a6:	74 0e                	je     21b6 <cbufp_register+0x89>
    21a8:	8b 45 f0             	mov    -0x10(%ebp),%eax
    21ab:	8b 40 04             	mov    0x4(%eax),%eax
    21ae:	89 45 f4             	mov    %eax,-0xc(%ebp)
    21b1:	e9 bc 00 00 00       	jmp    2272 <cbufp_register+0x145>
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
    21b6:	0f b7 45 d4          	movzwl -0x2c(%ebp),%eax
    21ba:	c7 44 24 0c 00 10 00 	movl   $0x1000,0xc(%esp)
    21c1:	00 
    21c2:	8d 55 e8             	lea    -0x18(%ebp),%edx
    21c5:	89 54 24 08          	mov    %edx,0x8(%esp)
    21c9:	8d 55 e4             	lea    -0x1c(%ebp),%edx
    21cc:	89 54 24 04          	mov    %edx,0x4(%esp)
    21d0:	89 04 24             	mov    %eax,(%esp)
    21d3:	e8 19 f3 ff ff       	call   14f1 <cbufp_alloc_map>
    21d8:	85 c0                	test   %eax,%eax
    21da:	0f 85 91 00 00 00    	jne    2271 <cbufp_register+0x144>
	cmr = cbufp_meta_lookup_cmr(cci, cbid);
    21e0:	8b 45 e8             	mov    -0x18(%ebp),%eax
    21e3:	8b 55 e8             	mov    -0x18(%ebp),%edx
    21e6:	81 e2 00 f0 ff ff    	and    $0xfffff000,%edx
    21ec:	39 d0                	cmp    %edx,%eax
    21ee:	0f 95 c0             	setne  %al
    21f1:	0f b6 c0             	movzbl %al,%eax
    21f4:	85 c0                	test   %eax,%eax
    21f6:	74 1c                	je     2214 <cbufp_register+0xe7>
    21f8:	c7 04 24 14 0e 00 00 	movl   $0xe14,(%esp)
    21ff:	e8 fc ff ff ff       	call   2200 <cbufp_register+0xd3>
    2204:	b8 00 00 00 00       	mov    $0x0,%eax
    2209:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    220f:	e8 37 df ff ff       	call   14b <__cos_noret>
	if (cmr) ERR_THROW(cmr->dest, done);
    2214:	8b 4d e4             	mov    -0x1c(%ebp),%ecx
    2217:	8b 45 e8             	mov    -0x18(%ebp),%eax
    221a:	89 c2                	mov    %eax,%edx
    221c:	8b 45 0c             	mov    0xc(%ebp),%eax
    221f:	89 4c 24 0c          	mov    %ecx,0xc(%esp)
    2223:	89 54 24 08          	mov    %edx,0x8(%esp)
    2227:	89 44 24 04          	mov    %eax,0x4(%esp)
    222b:	8b 45 ec             	mov    -0x14(%ebp),%eax
    222e:	89 04 24             	mov    %eax,(%esp)
    2231:	e8 86 f0 ff ff       	call   12bc <cbufp_meta_add>
    2236:	89 45 f0             	mov    %eax,-0x10(%ebp)

    2239:	83 7d f0 00          	cmpl   $0x0,-0x10(%ebp)
    223d:	0f 94 c0             	sete   %al
    2240:	0f b6 c0             	movzbl %al,%eax
    2243:	85 c0                	test   %eax,%eax
    2245:	74 1c                	je     2263 <cbufp_register+0x136>
    2247:	c7 04 24 34 0e 00 00 	movl   $0xe34,(%esp)
    224e:	e8 fc ff ff ff       	call   224f <cbufp_register+0x122>
    2253:	b8 00 00 00 00       	mov    $0x0,%eax
    2258:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
    225e:	e8 e8 de ff ff       	call   14b <__cos_noret>
	/* Create the mapping into the client */
    2263:	8b 45 f0             	mov    -0x10(%ebp),%eax
    2266:	8b 40 04             	mov    0x4(%eax),%eax
    2269:	89 45 f4             	mov    %eax,-0xc(%ebp)
    226c:	eb 04                	jmp    2272 <cbufp_register+0x145>
{
	struct cbufp_comp_info  *cci;
	struct cbufp_meta_range *cmr;
	void *p;
	vaddr_t dest, ret = 0;

    226e:	90                   	nop
    226f:	eb 01                	jmp    2272 <cbufp_register+0x145>
	printc("ryx: go into cbufp cbufp_register\n");
	printl("cbufp_register\n");
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
    2271:	90                   	nop
	cmr = cbufp_meta_lookup_cmr(cci, cbid);
	if (cmr) ERR_THROW(cmr->dest, done);

	/* Create the mapping into the client */
	if (cbufp_alloc_map(spdid, &dest, &p, PAGE_SIZE)) goto done;
	assert((u32_t)p == round_to_page(p));
    2272:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    2279:	e8 51 e7 ff ff       	call   9cf <lock_release>
    227e:	85 c0                	test   %eax,%eax
    2280:	74 17                	je     2299 <cbufp_register+0x16c>
    2282:	c7 04 24 54 0e 00 00 	movl   $0xe54,(%esp)
    2289:	e8 fc ff ff ff       	call   228a <cbufp_register+0x15d>
    228e:	b8 00 00 00 00       	mov    $0x0,%eax
    2293:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
	cmr = cbufp_meta_add(cci, cbid, p, dest);
    2299:	8b 45 f4             	mov    -0xc(%ebp),%eax
	assert(cmr);
    229c:	c9                   	leave  
    229d:	c3                   	ret    

0000229e <cos_init>:
	ret = cmr->dest;
done:
	CBUFP_RELEASE();
	return ret;
    229e:	55                   	push   %ebp
    229f:	89 e5                	mov    %esp,%ebp
    22a1:	83 ec 28             	sub    $0x28,%esp
}

    22a4:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    22ab:	e8 ff e7 ff ff       	call   aaf <lock_static_init>
void
    22b0:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    22b7:	e8 ce eb ff ff       	call   e8a <cmap_init_static>
cos_init(void)
    22bc:	c7 44 24 04 00 00 00 	movl   $0x0,0x4(%esp)
    22c3:	00 
    22c4:	c7 04 24 00 00 00 00 	movl   $0x0,(%esp)
    22cb:	e8 94 ec ff ff       	call   f64 <cmap_add>
    22d0:	89 45 f4             	mov    %eax,-0xc(%ebp)
{
    22d3:	c9                   	leave  
    22d4:	c3                   	ret    
