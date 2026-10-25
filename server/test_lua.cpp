#include <iostream>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

int main() {
    lua_State* L = luaL_newstate();
    if (!L) {
        std::cerr << "Failed to create Lua state." << std::endl;
        return 1;
    }
    luaL_openlibs(L);

    const char* script = "function on_player_connect(id, name) return 'Player ' .. id .. ' (' .. name .. ') joined Wasteland' end";
    if (luaL_dostring(L, script) != LUA_OK) {
        std::cerr << "Lua error: " << lua_tostring(L, -1) << std::endl;
        lua_close(L);
        return 1;
    }

    lua_getglobal(L, "on_player_connect");
    lua_pushinteger(L, 42);
    lua_pushstring(L, "RoadWarrior");

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        std::cerr << "Lua call error: " << lua_tostring(L, -1) << std::endl;
        lua_close(L);
        return 1;
    }

    std::cout << "[Lua Test Result] " << lua_tostring(L, -1) << std::endl;
    lua_close(L);
    return 0;
}
