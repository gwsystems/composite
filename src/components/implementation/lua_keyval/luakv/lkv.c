//#include <cos_component.h>
//#include <print.h>
//#include <lua.h>
//#include <lualib.h>
//#include <lauxlib.h>
//#include <stdlib.h>
#include <lua_keyval.h>

char* default_tablespace = "DEFAULT";

// Exported function calls in the interface
//char* luakv_getstring_bytable (lua_State *L, char *key, char *table_name);
//char* luakv_getstring (lua_State *L, char *key);
//int  luakv_getnumber_bytable (lua_State *L, char *key, char *table_name);
//int  luakv_getnumber (lua_State *L, char *key);
//void luakv_putstring_bytable (lua_State *L, char *key, char *value, char *table_name);
//void luakv_putstring (lua_State *L, char *key, char *value);
//void luakv_putnumber_bytable (lua_State *L, char *key, int intvalue, char *table_name);
//void luakv_putnumber (lua_State *L, char *key, int intvalue);

// Non exported function declaration
void create_tablespace(char *table_name);

lua_State *L;

//  Table specific string value lookup from the lua_State
char* luakv_getstring_bytable (char *key, char *table_name)
{
	char *result;
	lua_getglobal(L, table_name);
	if(!lua_istable(L, -1))
	{
		printc("==== tablespace didn't exist so try to create\n");
		create_tablespace(table_name);
	}
	lua_pushstring(L, key);
	lua_gettable(L, -2);
	result = (char*)lua_tostring(L, -1);
	lua_pop(L, 1);
	return result;
}

//  Search for a string value from the default table
char* luakv_getstring (char *key)
{
	return luakv_getstring_bytable (key, default_tablespace);
}

// Table specific lookup for an int value
int luakv_getnumber_bytable (char *key, char *table_name)
{
	lua_getglobal(L, table_name);
	if(!lua_istable(L, -1))
	{
		create_tablespace(table_name);
	}
	lua_pushstring(L, key);
	lua_gettable(L, -2);
	int result = (int)lua_tonumber(L, -1);
	lua_pop(L, 1);
	return result;

}

// Search for a number value in the default table
int luakv_getnumber (char *key)
{
	return luakv_getnumber_bytable(key, default_tablespace);
}

// Store a string value in the specified table
void luakv_putstring_bytable (char *key, char *value, char *table_name)
{
	lua_getglobal(L, table_name);
	if(!lua_istable(L, -1))
        {
                create_tablespace(table_name);
        }
	lua_pushstring(L, key);
	lua_pushstring(L, value);
	lua_settable(L, -3);
}

// Store a string value in the default table
void luakv_putstring (char *key, char *value)
{
	luakv_putstring_bytable(key, value, default_tablespace);
}

// Store an int value in the specified table
void luakv_putnumber_bytable (char *key, int intvalue, char *table_name)
{
	lua_getglobal(L, table_name);
	if(!lua_istable(L, -1))
        {
                create_tablespace(table_name);
        }
	lua_pushstring(L, key);
	lua_pushnumber(L, intvalue);
	lua_settable(L, -3);

}

// Store an int value in the default table
void luakv_putnumber (char *key, int intvalue)
{
	luakv_putnumber_bytable(key, intvalue, default_tablespace);
}


/*
*   Creates a table on the lua_State object for general use.
*/
void create_tablespace (char *table_name)
{
	printc("=== Attempting to create new lua table with table_name=%s\n",table_name);
	// Create the new table and assign it to global name *table_name
	lua_newtable(L);
	lua_setglobal(L, table_name);
	// Put the newly created table on top of the stack
	lua_getglobal(L, table_name);
}

void cos_init(void)
{
	printc("=== Trying out some keyval operations\n");
	// Creates the lua state
	L = luaL_newstate();
	printc("=== Starting lua keyvalue storage test. New Lua state has been set.\n");
	// Test for an empty value
	char *nilField = luakv_getstring("EMPTY");
	printc("=== trying to getString before fields are entered. received %s\n", nilField);
	// Test putting a string value and retrieving it
	luakv_putstring("key1","value1");
	printc("=== Put string of value1 into the default table at key1\n");
	char *get1 = luakv_getstring("key1");
	printc("=== Retrieved the value at key1, return was: %s\n", get1);
	// Test putting a number value and retrieving it
	luakv_putnumber("key2",(int)2);
	printc("=== Put number value of 2 into the default table at key2\n");
	int get2 = luakv_getnumber("key2");
	printc("=== pull from the table key2: %d\n", get2);
	// Try storing values into another tablespace
	luakv_putstring_bytable("key3","value3","TABLE2");
	luakv_putnumber_bytable("key4",(int)4,"TABLE2");
	printc("=== Placed key3=value3 and key4=4 into TABLE2\n");
	// Try to retrieve the other tables valus from the default table
	printc("=== attempting to retrieve TABLE2 values from DEFAULT table\n");
	char *get3_fail = luakv_getstring("key3");
	int get4_fail = luakv_getnumber("key4");
	printc("=== should have no values returned, retrieved key3=%s and key4=%d\n",get3_fail,get4_fail);
	// Now get TABLE 2 values from table2
	char *get3 = luakv_getstring_bytable("key3", "TABLE2");
	int get4 = luakv_getnumber_bytable("key4", "TABLE2");
	printc("=== should now have returned values from table 2 key3=%s and key4=%d\n",get3, get4);
	// Test duplicate key but in different table
	luakv_putstring("key3","default_value3");
	printc("=== stored key3=default_value3 in default table\n");
	char *default3 = luakv_getstring("key3");
	char *bytable3 = luakv_getstring_bytable("key3", "TABLE2");
	printc("=== Pulled key3 from both tables. DEFAULT=%s and TABLE2=%s\n", default3, bytable3);

	// Closes lua
	lua_close(L);
	printc("=== lua_state has been closed\n");
	return;
}



