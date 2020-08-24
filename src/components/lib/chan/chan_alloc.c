#include <chan.h>
#include <malloc.h>

struct chan *
chan_alloc(unsigned int item_sz, unsigned int slots, chan_flags_t flags)
{
	struct chan *c = malloc(sizeof(struct chan));

	if (!c) return NULL;
	if (chan_init(c, item_sz, slots, flags)) {
		free(c);
		return NULL;
	}

	return c;
}

struct chan_snd *
chan_snd_alloc(struct chan *c)
{
	struct chan_snd *s = malloc(sizeof(struct chan_snd));

	if (!s) return NULL;
	if (chan_snd_init(s, c)) {
		free(s);
		return NULL;
	}

	return s;
}

struct chan_rcv *
chan_rcv_alloc(struct chan *c)
{
	struct chan_rcv *r = malloc(sizeof(struct chan_rcv));

	if (!r) return NULL;
	if (chan_rcv_init(r, c)) {
		free(r);
		return NULL;
	}

	return r;
}

struct chan_snd *
chan_snd_alloc_with(chan_id_t cap_id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags)
{
	struct chan_snd *s = malloc(sizeof(struct chan_snd));

	if (!s) return NULL;
	if (chan_snd_init_with(s, cap_id, item_sz, nslots, flags)) {
		free(s);
		return NULL;
	}

	return s;
}

struct chan_rcv *
chan_rcv_alloc_with(chan_id_t cap_id, unsigned int item_sz, unsigned int nslots, chan_flags_t flags)
{
	struct chan_rcv *r = malloc(sizeof(struct chan_rcv));

	if (!r) return NULL;
	if (chan_rcv_init_with(r, cap_id, item_sz, nslots, flags)) {
		free(r);
		return NULL;
	}

	return r;

}

void
chan_free(struct chan *c)
{
	if (chan_teardown(c)) return;
	free(c);
}

void
chan_snd_free(struct chan_snd *s)
{
	struct chan *c = s->c;

	chan_snd_teardown(s);
	chan_free(c);
}

void
chan_rcv_free(struct chan_rcv *r)
{
	struct chan *c = r->c;

	chan_rcv_teardown(r);
	chan_free(c);
}
