
#include "lua_adapter.h"
#include "lstate.h"

bool luaadapter_issame_luavm(lua_State* L1, lua_State* L2)
{
    return (L1 == L2) || (G(L1)->mainthread == G(L2)->mainthread);
}

luaadapter_objref::luaadapter_objref()
{
    m_pLuaState = NULL;
    m_nLuaRef = LUA_NOREF;
}

// 如果出现这个断言,表明这个对象具有lua对象引用但是未进行释放
// 请在合适的地方(如删除对象的时候)加上"LUA_CLEAR_REF(L);"
luaadapter_objref::~luaadapter_objref()
{
    int nTop = 0;

    if (m_pLuaState)
    {
        nTop = lua_gettop(m_pLuaState);
    }

    LUAADAPTER_FAILED_JUMP(m_nLuaRef != LUA_NOREF);

    lua_rawgeti(m_pLuaState, LUA_REGISTRYINDEX, m_nLuaRef);
    LUAADAPTER_FAILED_JUMP(lua_istable(m_pLuaState, -1));

    lua_pushstring(m_pLuaState, LUA_OBJECT_POINTER);
    lua_pushnil(m_pLuaState);
    lua_rawset(m_pLuaState, -3);

    luaL_unref(m_pLuaState, LUA_REGISTRYINDEX, m_nLuaRef);

Exit0:
    if (m_pLuaState)
    {
        lua_settop(m_pLuaState, nTop);
    }

    m_pLuaState = NULL;
    m_nLuaRef = LUA_NOREF;
}

int _luaadapter_getvalue(lua_State* L, LuaObjectMemberType eType, void* pvAddr, size_t uSize)
{
    std::string* pStdStr = NULL;

    switch (eType)
    {
    case eLuaObjectMemberType_int:
    case eLuaObjectMemberType_enum:
        assert(uSize == sizeof(int));
        lua_pushinteger(L, *(int*)pvAddr);
        break;

    case eLuaObjectMemberType_int32:
        assert(uSize == sizeof(int32_t));
        lua_pushinteger(L, *(int32_t*)pvAddr);
        break;        
            
    case eLuaObjectMemberType_int64:
        assert(uSize == sizeof(int64_t));
        lua_pushinteger(L, *(int64_t*)pvAddr);
        break;

    case eLuaObjectMemberType_uint32:
        assert(uSize == sizeof(uint32_t));
        lua_pushinteger(L, *(uint32_t*)pvAddr);
        break;         

    case eLuaObjectMemberType_BYTE:
        assert(uSize == sizeof(unsigned char));
        lua_pushinteger(L, *(unsigned char*)pvAddr);
        break;

    case eLuaObjectMemberType_time:
        assert(uSize == sizeof(time_t));
        lua_pushnumber(L, (double)*(time_t*)pvAddr);
        break;

    case eLuaObjectMemberType_bool:
        assert(uSize == sizeof(bool));
        lua_pushboolean(L, *(bool*)pvAddr);
        break;

    case eLuaObjectMemberType_float:
        assert(uSize == sizeof(float));
        lua_pushnumber(L, *(float*)pvAddr);
        break;

    case eLuaObjectMemberType_double:
        assert(uSize == sizeof(double));
        lua_pushnumber(L, *(double*)pvAddr);
        break;

    case eLuaObjectMemberType_string:
        lua_pushstring(L, (const char*)pvAddr);
        break;

    case eLuaObjectMemberType_std_str:
        pStdStr = (std::string*)pvAddr;
        lua_pushstring(L, pStdStr->c_str());
        break;

    default:
        assert(false);
        lua_pushnil(L);
    }

    return 1;
}

int _luaadapter_setvalue(lua_State* L, LuaObjectMemberType eType, void* pvAddr, size_t uSize)
{
    int         nResult = 0;
    const char* pszStr  = NULL;
    size_t      uStrLen = 0;

    switch (eType)
    {
    case eLuaObjectMemberType_int:
    case eLuaObjectMemberType_enum:
        assert(uSize == sizeof(int));
        *(int*)pvAddr = (int)lua_tointeger(L, -1);
        break;
            
    case eLuaObjectMemberType_int32:
        assert(uSize == sizeof(int32_t));
        *(int32_t*)pvAddr = (int32_t)lua_tointeger(L, -1);
        break;

    case eLuaObjectMemberType_int64:
        assert(uSize == sizeof(int64_t));
        *(int64_t*)pvAddr = (int64_t)lua_tointeger(L, -1);
        break;

    case eLuaObjectMemberType_uint32:
        assert(uSize == sizeof(uint32_t));
        *(uint32_t*)pvAddr = (uint32_t)lua_tointeger(L, -1);
        break;    

    case eLuaObjectMemberType_BYTE:
        assert(uSize == sizeof(unsigned char));
        *(unsigned char*)pvAddr = (unsigned char)lua_tointeger(L, -1);
        break;

    case eLuaObjectMemberType_time:
        assert(uSize == sizeof(time_t));
        *(time_t*)pvAddr = (time_t)lua_tonumber(L, -1);
        break;

    case eLuaObjectMemberType_bool:
        assert(uSize == sizeof(bool));
        *(bool*)pvAddr = !!lua_toboolean(L, -1);
        break;

    case eLuaObjectMemberType_float:
        assert(uSize == sizeof(float));
        *(float*)pvAddr = (float)lua_tonumber(L, -1);
        break;

    case eLuaObjectMemberType_double:
        assert(uSize == sizeof(double));
        *(double*)pvAddr = lua_tonumber(L, -1);
        break;

    case eLuaObjectMemberType_string:
        LUAADAPTER_FAILED_JUMP(lua_isstring(L, -1));
        pszStr = (const char*)lua_tostring(L, -1);
        LUAADAPTER_FAILED_JUMP(pszStr);
        uStrLen = strlen(pszStr);
        LUAADAPTER_FAILED_JUMP(uStrLen < uSize);
        strcpy((char*)pvAddr, pszStr);
        break;

    case eLuaObjectMemberType_std_str:
        LUAADAPTER_FAILED_JUMP(lua_isstring(L, -1));
        pszStr = (const char*)lua_tostring(L, -1);
        LUAADAPTER_FAILED_JUMP(pszStr);
        *(std::string*)pvAddr = pszStr;
        break;

    default:
        assert(false);
        goto Exit0;
    }

    nResult = 1;    
Exit0:    
    return nResult;
}

void _luaadapter_registermember(lua_State* L, const char* pszName, size_t uNameLen, LuaObjectMemberType eType, void* pMember)
{
    if (eType == eLuaObjectMemberType_function)
    {
        const char* pszMemberFuncPrefix = "Lua";
        size_t uMemberFuncPrefixLen = strlen(pszMemberFuncPrefix);

        if (uNameLen > uMemberFuncPrefixLen)
        {
            // 去掉class成员函数的"Lua"前缀
            int nCmp = memcmp(pszName, pszMemberFuncPrefix, uMemberFuncPrefixLen);
            if (nCmp == 0)
                pszName += uMemberFuncPrefixLen;
        }
    }
    else
    {
        const char* pszMemberVarPrefix = "m_";
        size_t uMemberVarPrefixLen = strlen(pszMemberVarPrefix);

        if (uNameLen > uMemberVarPrefixLen)
        {
            // 去掉class成员变量的"m_"前缀
            int nCmp = memcmp(pszName, pszMemberVarPrefix, uMemberVarPrefixLen);
            if (nCmp == 0)
                pszName += uMemberVarPrefixLen;
        }
    }

    lua_pushstring(L, pszName);
    lua_pushlightuserdata(L, pMember);
    lua_rawset(L, -3);    
}
