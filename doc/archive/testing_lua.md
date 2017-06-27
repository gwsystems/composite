# Lua

## Building Lua
The Lua library will be built as part of a normal Composite build. To test Lua functionality, there is a `lua_keyval` directory that contains two components, `luakv` and `luatests`. `luakv` demonstrates a key-value store using the Lua stack accessed entirely from the C api. `luatests` shows various ways of using Lua from C, including reading from a configuration file, reading Lua code from a Lua script file, etc.

Both test components have associated run scripts.

* `lua_test.sh` will run `luatests`
* `lua_keyval_test.sh` will run the `luakv`

Currently, the Lua component does not export an interface. If you want to try Lua in your own components, you'll need to include the Lua header files.

	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>

You'll also need to build against the lua lib object in your Makefile. Something like

	first:
		@echo PRE
		$(info |     [CP]  Copying lua library)
		@cp $(LUAOBJ) .

	IF_LIB=$(LUAOBJ)

should do the trick. See the `luatests` component for more information.

## Using Lua
If you actually want to play with Lua, again, take a look at the `luatests` component. At the very least, you'll need to initialize the Lua state 

	lua_State *L = luaL_newstate();
 
and close the state when you are done 
	
	lua_close(L);

One note - the current Lua version included is `5.2`. Lua syntax changes somewhat frequently from version to version, so if you are Googling for Lua tutorials/documentation, make sure you are looking for info on version 5.2, as some functions that are common in tutorials (such as `luaL_openlibs`) were removed from 5.2


