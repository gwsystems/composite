#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

extern int __dns_decodename(unsigned char *packet,unsigned int ofs,unsigned char *dest,
			    unsigned int maxlen,unsigned char* behindpacket);

int dn_expand(unsigned char *msg, unsigned char *eomorig, unsigned char *comp_dn, unsigned char *exp_dn, int length) {
  return __dns_decodename(msg,comp_dn-msg,exp_dn,length,eomorig)-(comp_dn-msg);
}

