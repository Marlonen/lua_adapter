//
//  main.c
//  luna
//
//  Created by dpull on 16/2/18.
//  Copyright © 2016年 dpull. All rights reserved.
//

#include <stdio.h>
#include "lua.hpp"
#include "lua_adapter.h"

class Test
{
public:
    DECLARE_LUA_CLASS(Test);
    
private:
    int m_nId;
    int LuaGetId(lua_State* L)
    {
        lua_pushinteger(L, m_nId);
        return 1;
    }
};

EXPORT_CLASS_BEGIN(Test)
EXPORT_LUA_INT(m_nId)
EXPORT_LUA_FUNCTION(LuaGetId)
EXPORT_CLASS_END()


static int lua_new(lua_State* L) {
    auto pTest = new Test();
    lua_pushobject<Test>(L, pTest);
    return 1;
}

static int lua_delete(lua_State* L) {
    auto pTest = lua_toobject<Test>(L, 1);
    delete(pTest);
    return 0;
}

static luaL_Reg funs[] = {
    { "new", lua_new },
    { "delete", lua_delete },
    { NULL, NULL }
};

int luaopen_luna(lua_State * L)
{
    luaL_checkversion(L);
    luaL_newlib(L, funs);
    return 1;
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
