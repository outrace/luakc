#include "kc.h"

//gcc kc.c -fPIC -shared -lkyotocabinet -o kc.so

typedef struct {
  KCDB* db;
  int open;
} LuaKc;

static LuaKc *check_kc (lua_State *L, int check_open) {
  void *tmp = luaL_checkudata(L, 1, "now.kc" ); 
  luaL_argcheck(L, tmp != NULL, 1, "need kc db" ); 
  
  if (check_open == 1){
	LuaKc* kc = (LuaKc *)tmp;
	if(kc->open == 0){//检查打开
		luaL_error(L,  "db is not open");
	}
  }
  return (LuaKc *)tmp; 
};

static int kc_new(lua_State *L){  
	size_t nbytes = sizeof(LuaKc);
	LuaKc* kc = (LuaKc *) lua_newuserdata(L, nbytes);
	kc->open = 0;
	luaL_getmetatable(L, "now.kc" ); 
	lua_setmetatable(L, -2); 
	
	return 1;	
};

/**
 * 此方法来源于原作
 */
static int kc_split(lua_State *lua) {
  int32_t argc = lua_gettop(lua);
  size_t isiz;
  const char* ibuf = lua_tolstring(lua, 1, &isiz); 
  const char* delims = argc > 1 ? lua_tostring(lua, 2) : NULL;
  lua_newtable(lua);
  int32_t lnum = 1;
  if (delims) {
    const char* str = ibuf;
    while (1) {
      const char* sp = str;
      while (*str != '\0' && !strchr(delims, *str) ) {
        str++;
      }
      lua_pushlstring(lua, sp, str - sp);
      lua_rawseti(lua, -2, lnum++);
      if (*str == '\0') break;
      str++;
    }
  } else {
    const char* ptr = ibuf;
    int32_t size = isiz;
    while (size >= 0) {
      const char* rp = ptr;
      const char* ep = ptr + size;
      while (rp < ep) {
        if (*rp == '\0') break;
        rp++;
      }
      lua_pushlstring(lua, ptr, rp - ptr);
      lua_rawseti(lua, -2, lnum++);
      rp++;
      size -= rp - ptr;
      ptr = rp;
    }
  }
  return 1;
}

static int kc_open(lua_State *L){
	LuaKc *kc = check_kc(L,0);
	const char* path;
	path = lua_tostring(L,2);
	
	kc->db = kcdbnew();
	if (!kcdbopen(kc->db, path, KCOWRITER | KCOCREATE)) {
	  return luaL_error(L,  "open error: %s\n", kcecodename(kcdbecode(kc->db)));
	}
	kc->open = 1;
	return 1;
};

static int kc_close(lua_State *L){
	LuaKc *kc = check_kc(L,1);
	
	kcdbclose(kc->db);
	kcdbdel(kc->db);
	
	kc->open = 0;
	return 1;
};

static int kc_get(lua_State *L){
	LuaKc *kc = check_kc(L,1);
	
	const char* kbuf;
	const char* vbuf;
	size_t klen, vlen;
	
	kbuf = lua_tostring(L,2);
	klen = lua_strlen(L,2);
	
	vbuf = kcdbget(kc->db, kbuf, klen, &vlen);
	if (vbuf) {
		lua_pushlstring(L,vbuf, vlen);
	} else {
		lua_pushnil(L);
	}
	return 1;
};

static int kc_set(lua_State *L){
	LuaKc *kc = check_kc(L,1);
	const char* kbuf;
	const char* vbuf;
    size_t klen, vlen;
	
    kbuf = lua_tostring(L,2);
	klen = lua_strlen(L,2);
	
    vbuf = lua_tostring(L,3);
	vlen = lua_strlen(L,3);
	
	kcdbset(kc->db, kbuf, klen, vbuf, vlen);

	return 1;
};

void match(lua_State *L,int kind){
	int32_t argc = lua_gettop(L);
	LuaKc *kc = check_kc(L,1);
	const char* str;
    int64_t max;
	int64_t  ret;
	char *keys[999999];
	
    str = lua_tostring(L,2);
	if (argc > 2){
		max = lua_tonumber(L,3);
	} else {
		max = -1;
	}
	
	if (kind == 0){
		ret = kcdbmatchprefix(kc->db, str, keys, max);
	} else {
		ret = kcdbmatchregex(kc->db, str, keys, max);
	}

	lua_newtable(L);
	int begin = 0;
	for(begin=0; begin < ret; begin++){
		char *key = keys[begin];
		lua_pushstring(L, key);
		lua_rawseti(L, -2, begin+1);
	}
};

static int kc_pre(lua_State *L){
	match(L,0);
	return 1;
};


static int kc_reg(lua_State *L){
	match(L,1);
	return 1;
};

static int kc_copy(lua_State *L){
	LuaKc *kc = check_kc(L,1);
	const char* path;
	path = lua_tostring(L,2);
	kcdbcopy(kc->db, path);
	return 1;
};

static int kc_begin(lua_State *L){
	LuaKc *kc = check_kc(L,1);
	int hard = lua_toboolean(L,2);
	kcdbbegintran(kc->db, hard);
	return 1;
};

static int kc_end(lua_State *L){
	LuaKc *kc = check_kc(L,1);
	
    int commit = lua_toboolean(L,2);
	kcdbendtran (kc->db,commit);
	return 1;
};

void setfield (lua_State *L, char *key, size_t ksiz, const char *value, size_t vsiz) { 
	lua_pushlstring(L, key, ksiz); 
	lua_pushlstring(L, value, vsiz); 
	lua_rawset(L, -3); 
}

static int kc_qry(lua_State *L){
	LuaKc *kc = check_kc(L,1);
	char *kbuf;
	const char *vbuf;
	size_t ksiz, vsiz;
	int max,now,flag;
	
    max = lua_tonumber(L,2);  //如果是0，或者0以下。那么我们取全部
	
	int n = lua_gettop(L);
	if (n > 2 && !lua_isfunction(L,3)) {
	  return luaL_error(L, "the tree arg must a lua function");
	}
	
	KCCUR* cur = kcdbcursor(kc->db);
	kccurjump(cur);
	
	now = 0;
	lua_newtable(L);
	while ((kbuf = kccurget(cur, &ksiz, &vbuf, &vsiz, 1)) != NULL) {
		if (n > 2){
			lua_pushvalue(L, -2);
			lua_pushlstring(L, kbuf, ksiz);
			lua_pushlstring(L, vbuf, vsiz); 
			//执行传递进来的函数。2个参数，一个返回值。返回值为boolean类型
			if (lua_pcall(L, 2, 1, 0) != 0){
				return luaL_error(L, "error to run function: %s",lua_tostring(L, -1)); 
			}
			flag = lua_toboolean(L, -1);
			lua_pop(L, 1);
			
			if (flag != 0){
				now = now + 1;
				setfield(L, kbuf,ksiz,vbuf,vsiz);
			}
		} else {
			now = now + 1;
			setfield(L, kbuf,ksiz,vbuf,vsiz);
		}
	
		kcfree(kbuf);
		if (max > 0){
			if (now >= max) break;
		}
	}
	kccurdel(cur);
	
	return 1;
};

static int kc_murmur(lua_State *L){
	const char* kbuf;
	size_t klen;
	int ret;
	
	kbuf = lua_tostring(L,1);
	klen = lua_strlen(L,1);
	
	ret = kchashmurmur(kbuf, klen);
	lua_pushnumber(L,ret);
	return 1;
};

static int kc_fnv(lua_State *L){
	const char* kbuf;
	size_t klen;
	int ret;
	
	kbuf = lua_tostring(L,1);
	klen = lua_strlen(L,1);
	
	ret = kchashfnv(kbuf, klen);
	lua_pushnumber(L,ret);
	return 1;
};

static const struct luaL_reg kclib_f [] = {
	{"new" , kc_new}, 
	{"split" , kc_split}, 
	{"murmur" , kc_murmur}, 
	{"fnv" , kc_fnv}, 
	{NULL, NULL} 
}; 
 
static const struct luaL_reg kclib_m [] = {
	{"open" , kc_open}, 
	{"close" , kc_close}, 
	{"copy",kc_copy},
	{"set" , kc_set}, 
	{"get" , kc_get}, 
	{"qry", kc_qry}, 
	{"pre", kc_pre}, 
	{"reg", kc_reg}, 
	{"begin_tran",kc_begin},
	{"end_tran",kc_end},
	{NULL, NULL} 
};

int luaopen_kc(lua_State *L) { 
	luaL_newmetatable(L, "now.kc" ); 

	lua_pushstring(L, "__index"); 
	lua_pushvalue(L, -2); /* pushes the metatable */ 
	lua_settable(L, -3); /* metatable.__index = metatable */    

	luaL_openlib(L, NULL, kclib_m, 0); 

	luaL_openlib(L, "kc", kclib_f, 0); 
	return 1; 
};