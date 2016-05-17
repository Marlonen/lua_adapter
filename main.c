//
//  main.c
//  luna
//
//  Created by dpull on 16/2/18.
//  Copyright © 2016年 dpull. All rights reserved.
//

#include <stdio.h>
#include "lauxlib.h"
#include "lualib.h"

int luaopen_luna(lua_State * L)
{
    return 0;
}

int main(int argc, const char * argv[])
{
    lua_State* l = luaL_newstate();
    luaL_requiref(l, "luna", luaopen_luna, 0);
    luaL_openlibs(l);
    if (luaL_dofile(l, "test/main.lua"))
        printf("[Lua] %s\n", lua_tostring(l, -1));
    lua_close(l);
    return 0;
}
