#ifndef   	LUA_KEYVAL_H
#define   	LUA_KEYVAL_H

#include <cos_component.h>
#include <print.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdlib.h>

char* luakv_getstring_bytable (char *key, char *table_name);
char* luakv_getstring (char *key);
int  luakv_getnumber_bytable (char *key, char *table_name);
int  luakv_getnumber (char *key);
void luakv_putstring_bytable (char *key, char *value, char *table_name);
void luakv_putstring (char *key, char *value);
void luakv_putnumber_bytable (char *key, int intvalue, char *table_name);
void luakv_putnumber (char *key, int intvalue);

#endif 	    /* !LUA_KEYVAL_H */
