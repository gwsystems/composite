#ifndef ACAP_SHARED_H
#define ACAP_SHARED_H

#define SERVER_ACTIVE(curr)        (*curr->server_active == 1)
#define SET_SERVER_ACTIVE(curr)    (*curr->server_active = 1)
#define CLEAR_SERVER_ACTIVE(curr)  (*curr->server_active = 0)

static inline int 
exec_fn(int (*fn)(), int nparams, int *params)
{
	int ret;

	assert(fn);

	switch (nparams)
	{		
	case 0:
		ret = fn();
		break;
	case 1:
		ret = fn(params[0]);
		break;
	case 2:
		ret = fn(params[0], params[1]);
		break;
	case 3:
		ret = fn(params[0], params[1], params[2]);
		break;
	case 4:
		ret = fn(params[0], params[1], params[2], params[3]);
		break;
	}
	
	return ret;
}

#endif /* !ACAP_SHARED_H */
