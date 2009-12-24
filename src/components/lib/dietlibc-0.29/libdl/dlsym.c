#include "_dl_int.h"

#include "elf_hash.h"

#ifdef __DIET_LD_SO__
static
#endif
void *_dlsym(void* handle,const char* symbol) {
  unsigned long*sym=0;
  if (handle) {
    struct _dl_handle*dh=(struct _dl_handle*)handle;
    unsigned int hash =elf_hash(symbol);
    unsigned int bhash=hash%HASH_BUCKET_LEN(dh->hash_tab);
    unsigned int*chain=HASH_CHAIN(dh->hash_tab);
    unsigned int ind;
    char *name=dh->dyn_str_tab;

#ifdef DEBUG
//    pf(__FUNCTION__); pf(": bucket("); ph(bhash); pf(",\""); pf(symbol); pf("\")\n");
#endif

    ind=HASH_BUCKET(dh->hash_tab)[bhash];
#ifdef DEBUG
//    pf(__FUNCTION__); pf(": chain ("); ph(ind); pf(",\""); pf(symbol); pf("\")\n");
#endif

    while(ind) {
      int ptr=dh->dyn_sym_tab[ind].st_name;
#ifdef DEBUG
//      pf(__FUNCTION__); pf(": symbol(\""); pf(name+ptr); pf("\",\""); pf(symbol); pf("\")\n");
#endif
      if (_dl_lib_strcmp(name+ptr,symbol)==0 && dh->dyn_sym_tab[ind].st_value!=0) {
	if (dh->dyn_sym_tab[ind].st_shndx!=SHN_UNDEF) {
	  sym=(long*)(dh->mem_base+dh->dyn_sym_tab[ind].st_value);
	  break;	/* ok found ... */
	}
      }
      ind=chain[ind];
    }
#ifdef DEBUG
    pf(__FUNCTION__); pf(": symbol \""); pf(symbol); pf("\" @ "); ph((long)sym); pf("\n");
#endif
  }
  return sym;
}

#ifdef __DIET_LD_SO__
static
#endif
void*_dl_sym_search_str(struct _dl_handle*dh_begin,const char*name) {
  void *sym=0;
  struct _dl_handle*tmp;
#ifdef DEBUG
  pf(__FUNCTION__); pf(": search for: "); pf(name); pf("\n");
#endif
  for (tmp=dh_begin;tmp && (!sym);tmp=tmp->next) {
//    if (!(tmp->flags&RTLD_GLOBAL)) continue;
#ifdef DEBUG
    pf(__FUNCTION__); pf(": searching in "); pf(tmp->name); pf("\n");
#endif
    sym=_dlsym((void*)tmp,name);
#ifdef DEBUG
    if (sym) { pf(__FUNCTION__); pf(": found: "); pf(name); pf(" @ "); ph((long)sym); pf("\n"); }
#endif
  }
  return sym;
}

#ifdef __DIET_LD_SO__
static
#endif
void*_dl_sym(struct _dl_handle*dh,int symbol) {
  char *name=dh->dyn_str_tab+dh->dyn_sym_tab[symbol].st_name;
  void*sym=_dl_sym_search_str(_dl_root_handle,name);
#ifdef DEBUG
  pf(__FUNCTION__); pf(": "); ph(symbol); pf(" -> "); ph((long)sym); pf("\n");
#endif
  return sym;
}

#ifdef __DIET_LD_SO__
static
#endif
void*_dl_sym_next(struct _dl_handle*dh,int symbol) {
  char *name=dh->dyn_str_tab+dh->dyn_sym_tab[symbol].st_name;
  void*sym=_dl_sym_search_str(dh->next,name);
#ifdef DEBUG
  pf(__FUNCTION__); pf(": "); ph(symbol); pf(" -> "); ph((long)sym); pf("\n");
#endif
  return sym;
}

void* dlsym(void* handle,const char* symbol) {
  void*h;
  if (handle==RTLD_DEFAULT || !handle /* RTLD_DEFAULT is NULL on glibc */ )
    h=_dl_sym_search_str(0,symbol);
  else h=_dlsym(handle,symbol);
  if (h==0) {
    _dl_error_location="dlsym";
    _dl_error_data=symbol;
    _dl_error=5;
  }
  return h;
}
