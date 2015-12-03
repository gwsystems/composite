#include <stdlib.h>
#include <stdio.h>

#include <ps.h>

#define MAX_KEY_LEN 255

struct item {
	struct item *next;
	char        *key, *data;
	size_t       key_len, data_len;
} PS_ALIGNED;

#define BINORD (16)
#define NBINS (1<<BINORD)

struct bin {
	struct item *item;
	struct ps_lock lock;
	char padding[PS_CACHE_LINE - ((sizeof(struct ps_lock)+sizeof(struct item *)+sizeof(struct mheader)) % PS_CACHE_LINE];
} PS_ALIGNED PS_PACKED;

PS_NSSLAB_CREATE(htbl, sizeof(struct bin), 2, 9, 7)
PS_PARSLAB_CREATE(item300, sizeof(struct item) + 300, PS_PAGE_SIZE * 64)


/* lets start with something simple but stupid, djb2 hash @ http://www.cse.yorku.ca/~oz/hash.html */
static inline unsigned long
hash(unsigned char *str, size_t len)
{
	unsigned long hash = 5381;
	unsigned int  i;
	
	for (i = 0 ; i < len ; i++) {
		hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */
	}
	
	return hash;
}

struct parsec ps;

static struct ps_ns *
ht_create(struct ps_ns *ps)
{
	int i;
	struct ps_ns *ht;
	(void)ps;

	ht = ps_nsptr_create_slab_htbl();
	assert(ht);

	for (i = 0 ; i < NBINS ; i++) {
		ps_desc_t d = 0;
		struct bin *ret;

		ret = ps_nsptr_alloc_htbl(ht, &d);
		assert(ret);
		assert(i == (int)d);
		ret->l = locks[i];
	}

	return ht;
}

static void
slabs_init(void)
{
	ps_mem_init_item300(&ps);
}

static inline struct bin *
__ht_get_bin(struct ps_ns *ht, char *key, size_t key_len)
{
	ps_desc_t bin = hash(key, key_len) % NBINS;
	struct bin *b = ps_nsptr_lkup_htbl(ht, bin);
	assert(b);

	return b;
}

static inline struct item *
__ht_get_walk(struct bin *b, char *key, size_t key_len, struct item **prev)
{
	struct item *i;

	for (i = b->item, *prev = NULL ; i != NULL ; *prev = i, i = i->next) {
		if (key_len == i->key_len && !memcmp(i->key, key, key_len)) {
			return i;
		}
	}

	return NULL;
}

static inline struct item *
__ht_get(struct ps_ns *ht, char *key, size_t key_len, struct item **prev)
{
	struct bin *b = __ht_get_bin(ht, key, key_len);
	assert(b);

	return __ht_get_walk(b, key, key_len, prev);
}

static int
ht_get(struct ps_ns *ht, char *key, size_t key_len, char **data, size_t *data_len)
{
	struct item *item, *prev;

	item      = __ht_get(ht, key, key_len, &prev);
	if (!item) return -1;

	*data     = item->data;
	*data_len = item->data_len;

	return 0;
}

static int
ht_put(struct ps_ns *ht, char *key, size_t key_len, char *data, size_t data_len)
{
	struct item *item, *prev, *new;
	struct bin  *b;

	assert(sizeof(struct item) + key_len + data_len < 300);
	new = ps_mem_alloc_item300();
	assert(new);
	new->key = (char *)&new[1];
	memcpy(new->key, key, key_len);
	new->data = new->key + key_len;
	memcpy(new->data, data, data_len);

	b = __ht_get_bin(ht, key, key_len);
	assert(b);
	ps_lock_take(&b->lock);
	item = __ht_get_walk(b, key, key_len, &prev);

	if (item) {
		assert(prev);
		prev->next = item->next;
		ps_mem_free_item300(item);
	}

	new = b->item;
	b->item = new;
	ps_lock_release(&b->lock);

	return 0;
}

#define NITEMS (1<<(BINORD+3))
#define KEYSZ  128
#define DATASZ 128

void
ht_load(struct ps_ns *ht, unsigned long start, unsigned long end)
{
	unsigned long i, j;

	assert(KEYSZ % sizeof(unsigned long) == 0);
	assert(end > start);
	for (i = start ; i < end ; i++) {
		char key[KEYSZ], data[DATASZ], dataval;

		for (j = 0 ; j < KEYSZ ; j += sizeof(unsigned long)) {
			memcpy(&key[j], &i, sizeof(unsigned long));
		}
		
		dataval = (char)i % 256;
		for (j = 0 ; j < DATASZ ; j++) data[j] = dataval;
		
		ps_enter(&ps);
		ht_put(ht, key, KEYSZ, data, DATASZ);
		ps_exit(&ps);
	}
}

int
main(void)
{
	struct ps_ns *ht;

	ps_init(&ps);
	ht = ht_create(&ps);
	

	return 0;
}
