#pragma once

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include "lua.hpp"

// 为什么统一用table来表示对象,而且统一都建引用?
// userdata,很难解决一个问题: 
// 脚本中把对象"句柄"保存起来,而C层面的对象已经释放了,再次用这个"句柄"回调回C的时候,就会导致非法内存访问
// 当然,userdata也可以建引用,嗯,但是,无论如何,这个"引用"看来都是必须建的.
// 既然这样,也就没必要同时采用table + userdata两种机制了,省得复杂化.
// 当然,建立了引用不见得就一定要支持动态成员扩展,动态成员扩展为差错带来了麻烦:)
// 所以,可以考虑是否提供一个标志位用来控制动态成员扩展,当然,作用有多大呢?值得怀疑

enum LuaObjectMemberType
{
    eLuaObjectMemberType_Invalid,
    eLuaObjectMemberType_int,
    eLuaObjectMemberType_int32,
    eLuaObjectMemberType_int64,
    eLuaObjectMemberType_uint32,
    eLuaObjectMemberType_enum,
    eLuaObjectMemberType_BYTE,
    eLuaObjectMemberType_time,
    eLuaObjectMemberType_bool,
    eLuaObjectMemberType_float,
    eLuaObjectMemberType_double,
    eLuaObjectMemberType_string,
    eLuaObjectMemberType_std_str,
    eLuaObjectMemberType_function
};


#define LUAADAPTER_FAILED_JUMP(Condition)       \
    do							                \
    {							                \
        if (!(Condition))		                \
            goto Exit0;			                \
    } while (false)

#define LUA_OBJECT_POINTER "__obj_pointer__"

bool luaadapter_issame_luavm(lua_State* L1, lua_State* L2);
int _luaadapter_setvalue(lua_State* L, LuaObjectMemberType eType, void* pvAddr, size_t uSize);
int _luaadapter_getvalue(lua_State* L, LuaObjectMemberType eType, void* pvAddr, size_t uSize);
void _luaadapter_registermember(lua_State* L, const char* pszName, size_t uNameLen, LuaObjectMemberType eType, void* pMember);

template <typename T>
int luaadapter_function(lua_State* L)
{
    T*                              pObj       = (T*)lua_touserdata(L, lua_upvalueindex(1));
    typename T::_ObjectMemberInfo*  pMember    = (typename T::_ObjectMemberInfo*)lua_touserdata(L, lua_upvalueindex(2));

    if (pObj == NULL || pMember == NULL || pMember->Function == NULL)
        return 0;

    return (pObj->*pMember->Function)(L);
}

template <typename T> 
int luaadapter_index(lua_State* L)
{
    int                             nResult         = 0;
    int                             nRetCode        = 0;
    int                             nTopIndex       = 0;
    T*                              pObj            = NULL;
    const char*                     pszKey          = NULL;
    typename T::_ObjectMemberInfo*  pMemberInfo     = NULL;
    void*                           pvAddr          = NULL;   

    nTopIndex = lua_gettop(L);
    LUAADAPTER_FAILED_JUMP(nTopIndex == 2);
    
    lua_pushstring(L, LUA_OBJECT_POINTER);
    lua_rawget(L, 1);

    LUAADAPTER_FAILED_JUMP(lua_isuserdata(L, -1));
    pObj    = (T*)lua_touserdata(L, -1);
    LUAADAPTER_FAILED_JUMP(pObj);

    lua_pop(L, 1);  // pop userdata

    pszKey      = lua_tostring(L, 2);
    LUAADAPTER_FAILED_JUMP(pszKey);

    luaL_getmetatable(L, T::_ms_pszMetaTableName);
    LUAADAPTER_FAILED_JUMP(lua_istable(L, -1));

    lua_pushstring(L, pszKey);
    lua_rawget(L, -2);

    LUAADAPTER_FAILED_JUMP(lua_isuserdata(L, -1));
    pMemberInfo = (typename T::_ObjectMemberInfo*)lua_touserdata(L, -1);
    LUAADAPTER_FAILED_JUMP(pMemberInfo);

    lua_settop(L, 2);

    pvAddr = ((unsigned char*)pObj) + pMemberInfo->nOffset;
    if (pMemberInfo->nType == eLuaObjectMemberType_function)
    {
        lua_pushlightuserdata(L, pObj);
        lua_pushlightuserdata(L, pMemberInfo);
        lua_pushcclosure(L, luaadapter_function<T>, 2);
    }
    else
    {
        nRetCode = _luaadapter_getvalue(L, (LuaObjectMemberType)pMemberInfo->nType, pvAddr, pMemberInfo->uSize);
        LUAADAPTER_FAILED_JUMP(nRetCode == 1);
    }

    nResult = 1;    
Exit0:    
    return nResult;
}

template <typename T>
int luaadapter_newindex(lua_State* L)
{
    int                             nResult         = 0;
    int                             nRetCode        = 0;
    int                             nTopIndex       = 0;
    T*                              pObj            = NULL;
    const char*                     pszKey          = NULL;
    typename T::_ObjectMemberInfo*  pMemberInfo     = NULL;
    void*                           pvAddr          = NULL;

    nTopIndex = lua_gettop(L);
    LUAADAPTER_FAILED_JUMP(nTopIndex == 3);
    
    lua_pushstring(L, LUA_OBJECT_POINTER);
    lua_rawget(L, 1);

    pObj    = (T*)lua_touserdata(L, -1);
    LUAADAPTER_FAILED_JUMP(pObj);

    lua_pop(L, 1);

    pszKey      = lua_tostring(L, 2);
    LUAADAPTER_FAILED_JUMP(pszKey);

    luaL_getmetatable(L, T::_ms_pszMetaTableName);
    LUAADAPTER_FAILED_JUMP(lua_istable(L, -1));

    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    pMemberInfo = (typename T::_ObjectMemberInfo*)lua_touserdata(L, -1);
    
    lua_pop(L, 2);

    if (pMemberInfo == NULL)
    {
        lua_rawset(L, -3);
        goto Exit1;
    }

    if(pMemberInfo->bReadOnly)
    {
        printf("[Lua] Try to write member readonly: %s::%s\n", T::_ms_pszClassName, pMemberInfo->pszName);
        goto Exit0;
    }

    pvAddr = ((unsigned char*)pObj) + pMemberInfo->nOffset;
    if (pMemberInfo->nType == eLuaObjectMemberType_function)
    {
        printf("lua object can not change function.\n");
    }
    else
    {
        nRetCode = _luaadapter_setvalue(L, (LuaObjectMemberType)pMemberInfo->nType, pvAddr, pMemberInfo->uSize);
        LUAADAPTER_FAILED_JUMP(nRetCode == 1);
    }

Exit1:
    nResult = 1;    
Exit0:    
    return nResult;
}

template <typename T>
void luaadapter_registerclass(lua_State* L)
{
    int         nTopIndex               = lua_gettop(L);

    luaL_newmetatable(L, T::_ms_pszMetaTableName);
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, (&luaadapter_index<T>));
    lua_rawset(L, -3);

    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, (&luaadapter_newindex<T>));
    lua_rawset(L, -3);

    typename T::_ObjectMemberInfo* pMember = T::GetMemberList();

    while (pMember->nType != eLuaObjectMemberType_Invalid)
    {
        _luaadapter_registermember(L, pMember->pszName, strlen(pMember->pszName), (LuaObjectMemberType)pMember->nType, pMember);
        pMember++;
    }

    lua_settop(L, nTopIndex);
}

struct luaadapter_objref
{
    luaadapter_objref();
    ~luaadapter_objref();

    lua_State*  m_pLuaState;
    int         m_nLuaRef;
};

#define DECLARE_LUA_CLASS(ClassName)    \
    typedef int (ClassName::*LUA_MEMBER_FUNCTION)(lua_State*);    \
    struct _ObjectMemberInfo    \
    {   \
        int                     nType;      \
        const char*             pszName;    \
        int                     nOffset;    \
        size_t                  uSize;      \
        bool                    bReadOnly;  \
        LUA_MEMBER_FUNCTION     Function;   \
    };  \
    luaadapter_objref  _m_LuaRef;  \
    static const char* _ms_pszClassName;    \
    static const char* _ms_pszMetaTableName;    \
    static ClassName::_ObjectMemberInfo* GetMemberList();   \
    \
    virtual void LuaPushThis(lua_State* L)  \
    {   \
        if (_m_LuaRef.m_nLuaRef != LUA_NOREF)   \
        {   \
            assert(luaadapter_issame_luavm(_m_LuaRef.m_pLuaState, L)); \
            lua_rawgeti(L, LUA_REGISTRYINDEX, _m_LuaRef.m_nLuaRef); \
            return; \
        }   \
        \
        lua_newtable(L);    \
        lua_pushstring(L, LUA_OBJECT_POINTER);  \
        lua_pushlightuserdata(L, this); \
        lua_settable(L, -3);    \
        \
        luaL_getmetatable(L, _ms_pszMetaTableName); \
        if (lua_isnil(L, -1)) \
        {   \
            lua_remove(L, -1);  \
            luaadapter_registerclass<ClassName>(L); \
            luaL_getmetatable(L, _ms_pszMetaTableName); \
        }   \
        lua_setmetatable(L, -2);    \
        \
        lua_pushvalue(L, -1);   \
        _m_LuaRef.m_pLuaState = L;  \
        _m_LuaRef.m_nLuaRef = luaL_ref(L, LUA_REGISTRYINDEX);  \
    }

#define LUA_CLEAR_REF() _m_LuaRef.Clear()

#define EXPORT_CLASS_BEGIN(ClassName) \
    const char* ClassName::_ms_pszClassName = #ClassName;   \
    const char* ClassName::_ms_pszMetaTableName = #ClassName"_metatable_";  \
    ClassName::_ObjectMemberInfo* ClassName::GetMemberList()    \
    {   \
        typedef ClassName   __ThisClass__;  \
        static _ObjectMemberInfo _ms_MemberList[] =  \
        {

#define EXPORT_CLASS_END()    \
            {eLuaObjectMemberType_Invalid, NULL, 0, 0, false, NULL},    \
        };  \
        return _ms_MemberList;  \
    }

#define EXPORT_LUA_MEMBER(Type, Member, bReadOnly)  \
    {Type, #Member, offsetof(__ThisClass__, Member), sizeof(((__ThisClass__*)NULL)->Member), bReadOnly, NULL},

#define EXPORT_LUA_INT(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_int, Member, false)
#define EXPORT_LUA_INT_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_int, Member, true)

#define EXPORT_LUA_INT32(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_int32, Member, false)
#define EXPORT_LUA_INT32_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_int32, Member, true)

#define EXPORT_LUA_INT64(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_int64, Member, false)
#define EXPORT_LUA_INT64_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_int64, Member, true)

#define EXPORT_LUA_UINT32(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_uint32, Member, false)
#define EXPORT_LUA_UINT32_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_uint32, Member, true)    

#define EXPORT_LUA_ENUM(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_enum, Member, false)
#define EXPORT_LUA_ENUM_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_enum, Member, true)

#define EXPORT_LUA_BYTE(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_BYTE, Member, false)
#define EXPORT_LUA_BYTE_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_BYTE, Member, true)

#define EXPORT_LUA_TIME(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_time, Member, false)
#define EXPORT_LUA_TIME_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_time, Member, true)

#define EXPORT_LUA_BOOL(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_bool, Member, false)
#define EXPORT_LUA_BOOL_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_bool, Member, true)

#define EXPORT_LUA_FLOAT(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_float, Member, false)
#define EXPORT_LUA_FLOAT_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_float, Member, true)

#define EXPORT_LUA_DOUBLE(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_double, Member, false)
#define EXPORT_LUA_DOUBLE_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_double, Member, true)

#define EXPORT_LUA_STRING(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_string, Member, false)
#define EXPORT_LUA_STRING_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_string, Member, true)

#define EXPORT_LUA_STD_STR(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_std_str, Member, false)
#define EXPORT_LUA_STD_STR_R(Member)   EXPORT_LUA_MEMBER(eLuaObjectMemberType_std_str, Member, true)

#define EXPORT_LUA_FUNCTION(Member) \
    {eLuaObjectMemberType_function, #Member, 0, 0, true, &__ThisClass__::Member},

template <typename T>
void lua_pushobject(lua_State* L, T* pObj)
{
    if (pObj == NULL)
    {
        lua_pushnil(L);
        return;
    }
    pObj->LuaPushThis(L);
}

template <typename T>
T* lua_toobject(lua_State* L, int idx)
{
    T* pObj = NULL;
    if (lua_istable(L, idx))
    {
        lua_getfield(L, idx, LUA_OBJECT_POINTER);
        pObj = (T*)lua_touserdata(L, -1);
        lua_pop(L, 1);
    }
    return pObj;
}

template <typename T>
bool lua_getobjectfunction(lua_State* L, T* pObj, const char cszFunc[])
{
    Lua_PushObject(L, pObj);
    lua_pushstring(L, cszFunc);
    lua_gettable(L, -2);
    lua_remove(L, -2);

    return (lua_type(L, -1) == LUA_TFUNCTION);
}
