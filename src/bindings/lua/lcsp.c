#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp/csp_debug.h>
#include <csp/drivers/usart.h>

#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_udp.h>

#include "csp_conn.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define lua_stack_dump(L) _lua_stack_dump(L, __func__, __LINE__)
static void _lua_stack_dump(lua_State *L, const char *func, int line)
{
    int top = lua_gettop(L);

    printf("%s:%d: stack: ", func, line);
    for (int i = 1; i <= top; i++) { /* repeat for each level */
        int t = lua_type(L, i);

        switch (t) {
        case LUA_TSTRING: { /* strings */
            printf("%d:'%s'", i, lua_tostring(L, i));
            break;
            }
        case LUA_TBOOLEAN: { /* booleans */
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;
            }
        case LUA_TNUMBER: { /* numbers */
            printf("%d:%g", i, lua_tonumber(L, i));
            break;
        }
        default: { /* other values */
            printf("%d:%s", i, lua_typename(L, t));
            break;
            }
        }
        printf(" "); /* put a separator */
    }

    printf("\n"); /* end the listing */
}

static int lcsp_getmetafield(lua_State *L, int obj, int field)
{
	if (!lua_getmetatable(L, obj)) {   /* no metatable? */
		return LUA_TNIL;
	} else {
		lua_pushvalue(L, field);
		int tt = lua_rawget(L, -2);
		if (tt == LUA_TNIL)  /* is metafield nil? */
			lua_pop(L, 2);  /* remove metatable and metafield */
		else
			lua_remove(L, -2);  /* remove only metatable */
		return tt;  /* return metafield type */
	}
}

static const char *csp_error_to_str(int err)
{
	switch (err) {
		case CSP_ERR_NONE:     return "No error";
		case CSP_ERR_NOMEM:    return "Not enough memory";
		case CSP_ERR_INVAL:    return "Invalid argument";
		case CSP_ERR_TIMEDOUT: return "Operation timed out";
		case CSP_ERR_USED:     return "Resource already in use";
		case CSP_ERR_NOTSUP:   return "Operation not supported";
		case CSP_ERR_BUSY:     return "Device or resource busy";
		case CSP_ERR_ALREADY:  return "Connection already in progress";
		case CSP_ERR_RESET:    return "Connection reset";
		case CSP_ERR_NOBUFS:   return "No more buffer space available";
		case CSP_ERR_TX:       return "Transmission failed";
		case CSP_ERR_DRIVER:   return "Error in driver layer";
		case CSP_ERR_AGAIN:    return "Resource temporarily unavailable";
		case CSP_ERR_NOSYS:    return "Function not implemented";
		case CSP_ERR_HMAC:     return "HMAC failed";
		case CSP_ERR_CRC32:    return "CRC32 failed";
		case CSP_ERR_SFP:      return "SFP protocol error or inconsistency";
		case CSP_ERR_MTU:      return "Invalid MTU";
		default:
			static char sbuf[32];
			snprintf(sbuf, sizeof(sbuf)-1, "Error #%d", err);
			return sbuf;
	}
}

static int lcsp_error(lua_State *L, const char *prefix, int error)
{
	luaL_pushfail(L);
	lua_pushfstring(L, "%s, result/error: %d", prefix, error);
	lua_pushinteger(L, error);
	lua_pushinteger(L, csp_dbg_errno);
	return 4;
}


#ifdef __linux__

#include <math.h> // modf
#include <time.h> // clock_nanosleep
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME (0)
#endif
#define NSEC_PER_SEC (1000000000ULL)

static int lcsp_sleep(lua_State *L)
{
	lua_Number delay = luaL_checknumber(L, 1);
	clockid_t clockid = luaL_optinteger(L, 2, CLOCK_REALTIME); // REALTIME=0, MONOTONIC=1, TAI=11
	double sec, frac = modf(delay, &sec);

	struct timespec tm = {
		.tv_sec = sec,
		.tv_nsec = frac * NSEC_PER_SEC
	};

	int r;
	do {
		r = clock_nanosleep(clockid, 0, &tm, &tm);
	} while (r == EINTR);
	if (r != 0) {
		luaL_pushfail(L);
		lua_pushfstring(L, "clock_nanosleep: %s", strerror(r));
		lua_pushinteger(L, r);
		return 3;
	}

	lua_pushboolean(L, true);
	return 1;
}

#endif /* __linux__ */

#ifdef CSP_ENABLE_CSP_PRINT

static int lcsp_print_connections(lua_State *L)
{
	csp_conn_print_table();
	return 0;
}

static int lcsp_print_interfaces(lua_State *L)
{
	csp_iflist_print();
	return 0;
}
#endif /* CSP_ENABLE_CSP_PRINT */

static csp_conn_t *lcsp_check_conn(lua_State *L, int ud);
static csp_packet_t *lcsp_check_packet(lua_State *L, int ud);

static int lcsp_init(lua_State *L)
{
	csp_conf_t *conf = &csp_conf;

	/* mandatory arguments */
	conf->hostname = luaL_checkstring(L, 1);
	conf->model    = luaL_checkstring(L, 2);
	conf->revision = luaL_checkstring(L, 3);

	/* optional arguments */
	conf->version = luaL_optinteger(L, 4, 2);
	conf->conn_dfl_so = luaL_optinteger(L, 5, CSP_O_NONE);
	conf->dedup = luaL_optinteger(L, 6, CSP_DEDUP_OFF);

	csp_init();
	return 0;
}

static int lcsp_get_hostname(lua_State *L)
{
	lua_pushstring(L, csp_get_conf()->hostname);
	return 1;
}

static int lcsp_get_model(lua_State *L)
{
	lua_pushstring(L, csp_get_conf()->model);
	return 1;
}

static int lcsp_get_revision(lua_State *L)
{
	lua_pushstring(L, csp_get_conf()->revision);
	return 1;
}

static int lcsp_service_handler(lua_State *L) {
    csp_packet_t *packet = luaL_checkudata(L, 1, "csp_packet");
    csp_service_handler(packet);
    return 0;
}

static int lcsp_socket_gc(lua_State *L)
{
	csp_socket_t *socket = luaL_checkudata(L, 1, "csp_socket_t");
	int rc = csp_socket_close(socket);
	if (rc != CSP_ERR_NONE) {
		// TODO: error handling
	}
	return 0;
}

static int lcsp_socket(lua_State *L)
{
	int opts = luaL_optinteger(L, 1, CSP_SO_NONE);

	csp_socket_t *socket = lua_newuserdatauv(L, sizeof(*socket), 0);

	memset(socket, 0, sizeof(*socket));
	socket->opts = opts;

	luaL_setmetatable(L, "csp_socket_t");
	return 1;
}

static int lcsp_bind(lua_State *L)
{
	csp_socket_t *socket = luaL_checkudata(L, 1, "csp_socket_t");
	int port = luaL_checkinteger(L, 2);

	int res = csp_bind(socket, port);
	if (res != CSP_ERR_NONE) {
		return lcsp_error(L, "csp_bind()", res);
	}

	lua_pushboolean(L, true);
	return 1;
}

static void lcsp_push_packet(lua_State *L, csp_packet_t *packet)
{
	csp_packet_t **ptr = lua_newuserdatauv(L, sizeof(*ptr), 0);
	*ptr = packet;
	luaL_setmetatable(L, "csp_packet_t");
}

static csp_packet_t *lcsp_check_packet(lua_State *L, int ud)
{
	csp_packet_t **ptr = luaL_checkudata(L, ud, "csp_packet_t");
	if (*ptr == NULL)
		luaL_error(L, "invalid packet object (already freed)");
	return *ptr;
}

static int lcsp_packet_index(lua_State *L)
{
	/* first, look in metatable */
	if (lcsp_getmetafield(L, 1, 2))
		return 1;

	/* second, look for members */
	csp_packet_t *packet = lcsp_check_packet(L, 1);
	const char *member = luaL_checkstring(L, 2);

	if (strcmp(member, "length") == 0) {
		lua_pushinteger(L, packet->length);
		return 1;
	}

	if (strcmp(member, "data") == 0) {
		lua_pushlstring(L, (const char *)packet->data, packet->length);
		return 1;
	}

	return 0;
}

static int lcsp_packet_gc(lua_State *L)
{
	csp_packet_t **ptr = (csp_packet_t **)luaL_checkudata(L, 1, "csp_packet_t");

	if (*ptr) {
		csp_buffer_free(*ptr);
		*ptr = NULL; // prevent double-free errors
	}

	return 0;
}

static void lcsp_push_conn(lua_State *L, csp_conn_t *conn)
{
	csp_conn_t **ptr = lua_newuserdatauv(L, sizeof(*ptr), 0);
	*ptr = conn;
	luaL_setmetatable(L, "csp_conn_t");
}

static csp_conn_t *lcsp_check_conn(lua_State *L, int ud)
{
	csp_conn_t **ptr = luaL_checkudata(L, ud, "csp_conn_t");
	if (*ptr == NULL)
		luaL_error(L, "invalid connection object");
	return *ptr;
}

static int lcsp_conn_index(lua_State *L)
{
	/* first, look in metatable */
	if (lcsp_getmetafield(L, 1, 2))
		return 1;

	/* second, look for members */
	csp_conn_t *conn = lcsp_check_conn(L, 1);
	const char *member = luaL_checkstring(L, 2);

#define maybe_push_conn_member(M) \
	if (strcmp(member, #M) == 0) { \
		lua_pushinteger(L, csp_conn_##M(conn)); \
		return 1; \
	}

	maybe_push_conn_member(dport);
	maybe_push_conn_member(sport);
	maybe_push_conn_member(dst);
	maybe_push_conn_member(src);
	maybe_push_conn_member(flags);

#undef maybe_push_conn_member

	if (strcmp(member, "state") == 0) {
		lua_pushstring(L, conn->state == CONN_OPEN ? "open" : "closed");
		return 1;
	}

	return 0;

}

static int lcsp_conn_gc(lua_State *L)
{
	csp_conn_t **conn_ptr = luaL_checkudata(L, 1, "csp_conn_t");

	if (*conn_ptr) {
		csp_close(*conn_ptr);

		*conn_ptr = NULL; // prevent double-close errors
	}

	return 0;
}

static int lcsp_close(lua_State *L)
{
	return lcsp_conn_gc(L);
}


static int lcsp_connect(lua_State *L)
{
	uint8_t prio = luaL_checkinteger(L, 1);
	uint16_t dest = luaL_checkinteger(L, 2);
	uint8_t dport = luaL_checkinteger(L, 3);
	uint32_t timeout = luaL_optinteger(L, 4, CSP_MAX_TIMEOUT);
	uint32_t opts = luaL_optinteger(L, 5, 0);

	csp_conn_t *conn = csp_connect(prio, dest, dport, timeout, opts);

	if (conn == NULL) {
		return lcsp_error(L, "csp_connect()", -ENOMEM);
	}

	lcsp_push_conn(L, conn);
	return 1;
}

static int lcsp_accept(lua_State *L)
{
	csp_socket_t *socket = luaL_checkudata(L, 1, "csp_socket_t");
	uint32_t timeout = luaL_optinteger(L, 2, CSP_MAX_TIMEOUT);

	csp_conn_t *conn = csp_accept(socket, timeout);

	if (conn == NULL) {
		luaL_pushfail(L);
		return 0;
	}

	lcsp_push_conn(L, conn);
	return 1;
}



static int lcsp_read(lua_State *L)
{
	csp_conn_t *conn = lcsp_check_conn(L, 1);
	uint32_t timeout = luaL_optinteger(L, 2, 500);

	csp_packet_t *packet = csp_read(conn, timeout);

	if (packet == NULL) {
		lua_pushnil(L);
		return 1;
	}

	lcsp_push_packet(L, packet);
	return 1;
}

static int lcsp_send(lua_State *L)
{
	csp_conn_t *conn = lcsp_check_conn(L, 1);
	csp_packet_t *packet = lcsp_check_packet(L, 2);
	uint32_t timeout = luaL_optinteger(L, 3, 1000);

	csp_send(conn, packet);

	lua_pushboolean(L, true);
	return 0;
}

int lcsp_conn_dport(lua_State *L)
{
	csp_conn_t *conn = lcsp_check_conn(L, 1);
	lua_pushinteger(L, csp_conn_dport(conn));
	return 1;
}

int lcsp_conn_sport(lua_State *L)
{
	csp_conn_t *conn = lcsp_check_conn(L, 1);
	lua_pushinteger(L, csp_conn_sport(conn));
	return 1;
}

int lcsp_conn_dst(lua_State *L)
{
	csp_conn_t *conn = lcsp_check_conn(L, 1);
	lua_pushinteger(L, csp_conn_dst(conn));
	return 1;
}

int lcsp_conn_src(lua_State *L)
{
	csp_conn_t *conn = lcsp_check_conn(L, 1);
	lua_pushinteger(L, csp_conn_src(conn));
	return 1;
}

int lcsp_conn_flags(lua_State *L)
{
	csp_conn_t *conn = lcsp_check_conn(L, 1);
	lua_pushinteger(L, csp_conn_flags(conn));
	return 1;
}

static int lcsp_sendto(lua_State *L)
{
    uint8_t prio = luaL_checkinteger(L, 1);
    uint16_t dest = luaL_checkinteger(L, 2);
    uint8_t dport = luaL_checkinteger(L, 3);
    uint8_t src_port = luaL_checkinteger(L, 4);
    uint32_t opts = luaL_checkinteger(L, 5);

    csp_packet_t **packet = luaL_checkudata(L, 6, "csp_packet_t");
    if (*packet == NULL) {
		return luaL_error(L, "Null packet");
	}

    csp_sendto(prio, dest, dport, src_port, opts, *packet);

    *packet = NULL; // Consumed
    return 0;
}

static int lcsp_sendto_reply(lua_State *L)
{
	csp_packet_t **request = luaL_checkudata(L, 1, "csp_packet_t");
	if (*request == NULL)
		return luaL_error(L, "request packet is nil");

	csp_packet_t **reply = (csp_packet_t **)luaL_checkudata(L, 2, "csp_packet_t");
	if (*reply == NULL)
		return luaL_error(L, "reply packet is nil");

	uint32_t opts = luaL_optinteger(L, 3, CSP_O_NONE);

	csp_sendto_reply(*request, *reply, opts);

	*reply = NULL; // Reply consumed
	return 0;
}

static int lcsp_recvfrom(lua_State *L)
{
    csp_socket_t *socket = luaL_checkudata(L, 1, "csp_socket_t");
    uint32_t timeout = luaL_optinteger(L, 2, 500);

    csp_packet_t *packet = csp_recvfrom(socket, timeout);

	if (packet == NULL) {
        lua_pushnil(L);
        return 1;
    }

	lcsp_push_packet(L, packet);
	return 1;
}

static int lcsp_packet_set_data(lua_State *L)
{
	csp_packet_t **packet = (csp_packet_t **)luaL_checkudata(L, 1, "csp_packet_t");
	if (*packet == NULL)
		return luaL_error(L, "Null packet");

	size_t len;
	const char *data = luaL_checklstring(L, 2, &len);

	if (len > sizeof((*packet)->data))
		return lcsp_error(L, "packet_set_data() exceeding max size", CSP_ERR_INVAL);

	memcpy((*packet)->data, data, len);
	(*packet)->length = len;

	return 0;
}

static int lcsp_packet_get_data(lua_State *L)
{
	csp_packet_t **packet = (csp_packet_t **)luaL_checkudata(L, 1, "csp_packet_t");
	if (*packet == NULL)
		return luaL_error(L, "Null packet");

	lua_pushlstring(L, (const char *)(*packet)->data, (*packet)->length);
	return 1;
}

static int lcsp_packet_get_length(lua_State *L)
{
    csp_packet_t **packet = (csp_packet_t **)luaL_checkudata(L, 1, "csp_packet_t");
    if (*packet == NULL)
		return luaL_error(L, "Null packet");

    lua_pushinteger(L, (*packet)->length);
    return 1;
}

static int lcsp_buffer_get(lua_State *L)
{
    csp_packet_t *packet = csp_buffer_get(0);

	if (packet) {
		lcsp_push_packet(L, packet);
		return 1;
	}

	return lcsp_error(L, "csp_buffer_get() - no free buffers", CSP_ERR_NOMEM);
}

static int lcsp_buffer_free(lua_State *L)
{
    return lcsp_packet_gc(L); // Re-use GC logic (handles NULL checks)
}


static int lcsp_ping(lua_State *L)
{
	uint16_t node = luaL_checkinteger(L, 1);
	uint32_t timeout = luaL_optinteger(L, 2, 1000);
	unsigned int size = luaL_optinteger(L, 3, 10);
	uint8_t conn_options = luaL_optinteger(L, 4, CSP_O_NONE);

	int res = csp_ping(node, timeout, size, conn_options);
	if (res < 0) {
		return lcsp_error(L, "csp_ping()", res);
	}

	lua_pushinteger(L, res);
	return 1;
}

static int lcsp_reboot(lua_State *L)
{
	uint16_t node = luaL_checkinteger(L, 1);
	csp_reboot(node);
	return 0;
}

static int lcsp_shutdown(lua_State *L)
{
	uint16_t node = luaL_checkinteger(L, 1);
	csp_shutdown(node);
	return 0;
}

static int lcsp_cmp_ident(lua_State *L)
{
	uint16_t node = luaL_checkinteger(L, 1);
	uint32_t timeout = luaL_optinteger(L, 2, 1000);

	struct csp_cmp_message msg;
	memset(&msg, 0, sizeof(msg));

	int res = csp_cmp_ident(node, timeout, &msg);
	if (res != CSP_ERR_NONE) {
		return luaL_error(L, "csp_cmp_ident(): %d %s", res, csp_error_to_str(res));
	}

	//lua_newtable(L);

	lua_pushstring(L, msg.ident.hostname);
	//lua_setfield(L, -2, "hostname");

	lua_pushstring(L, msg.ident.model);
	//lua_setfield(L, -2, "model");

	lua_pushstring(L, msg.ident.revision);
	//lua_setfield(L, -2, "revision");

	lua_pushstring(L, msg.ident.date);
	//lua_setfield(L, -2, "date");

	lua_pushstring(L, msg.ident.time);
	//lua_setfield(L, -2, "time");

	//return 1;
	return 5;
}

#ifdef CSP_USE_RTABLE

static int lcsp_rtable_set(lua_State *L)
{
	uint16_t node = luaL_checkinteger(L, 1);
	int mask = luaL_checkinteger(L, 2);
	const char * interface_name = luaL_checkstring(L, 3);
	uint16_t via = luaL_optinteger(L, 4, CSP_NO_VIA_ADDRESS);

	int res = csp_rtable_set(node, mask, csp_iflist_get_by_name(interface_name), via);
	if (res != CSP_ERR_NONE) {
		return luaL_error(L, "csp_rtable_set(): %d %s", res, csp_error_to_str(res));
	}

	lua_pushboolean(L, true);
	return 1;
}

static int lcsp_rtable_clear(lua_State *L)
{
	csp_rtable_clear();
	return 0;
}

static int lcsp_rtable_check(lua_State *L)
{
	const char * buffer = luaL_checkstring(L, 1);

	int res = csp_rtable_check(buffer);
	if (res <= 0) {
		return luaL_error(L, "csp_rtable_check(): %d %s", res, csp_error_to_str(res));
	}

	lua_pushinteger(L, res);
	return 1;
}

static int lcsp_rtable_load(lua_State *L) {
	const char * buffer = luaL_checkstring(L, 1);

	int res = csp_rtable_load(buffer);
	if (res <= CSP_ERR_NONE) {
		return luaL_error(L, "csp_rtable_load(): %d %s", res, csp_error_to_str(res));
	}

	lua_pushinteger(L, res);
	return 1;
}

static int lcsp_print_routes(lua_State *L)
{
	csp_rtable_print();
	return 0;
}

#endif /* CSP_USE_RTABLE */

static int lcsp_udp_init(lua_State *L)
{
	const char *udp_host = luaL_checkstring(L, 1);
	int remote_port = luaL_checkinteger(L, 2);
	int listen_port = luaL_checkinteger(L, 3);

	csp_iface_t *iface;
	csp_if_udp_conf_t *udp_conf;

	iface = calloc(1, sizeof(*iface) + sizeof(*udp_conf));
	udp_conf = (void *)&iface[1];

	udp_conf->host = (char *)udp_host;
	udp_conf->lport = listen_port;
	udp_conf->rport = remote_port;

	csp_if_udp_init(iface, udp_conf);
	return 0;
}

static int lcsp_kiss_init(lua_State *L)
{
	const char *device = luaL_checkstring(L, 1);
	uint16_t addr = luaL_checkinteger(L, 2);
	uint32_t baudrate = luaL_optinteger(L, 3, 500000);
	uint32_t mtu = luaL_optinteger(L, 4, 512);
	const char *if_name = luaL_optstring(L, 5, CSP_IF_KISS_DEFAULT_NAME);
	int is_default = luaL_optinteger(L, 6, 0);
	uint16_t mask = luaL_optinteger(L, 7, 8);

	csp_usart_conf_t conf = {
		.device = device,
		.baudrate = baudrate
	};
	csp_iface_t *iface;
	int res = csp_usart_open_and_add_kiss_interface(&conf, if_name, addr, &iface);
	if (res != CSP_ERR_NONE) {
		return luaL_error(L, "csp_usart_open_and_add_kiss_interface(): %d %s", res, csp_error_to_str(res));
	}

	if (iface) {
		iface->is_default = is_default;
		iface->addr = addr;
		iface->netmask = mask;
	}

	lua_pushboolean(L, true);
	return 1;
}


static const struct luaL_Reg lcsp_socket_methods[] = {
	{"__gc", lcsp_socket_gc},
	
	{ NULL, NULL } /* sentinel */
};

static const struct luaL_Reg lcsp_conn_methods[] = {
	/* fields */
	//{"dport", lcsp_conn_dport},
	//{"sport", lcsp_conn_sport},
	//{"dst", lcsp_conn_dst},
	//{"src", lcsp_conn_src},
	//{"flags", lcsp_conn_flags},

	{"close", lcsp_conn_gc},

	{"__index", lcsp_conn_index},
	{"__gc", lcsp_conn_gc},
	
	{ NULL, NULL } /* sentinel */
};

static const struct luaL_Reg lcsp_packet_methods[] = {
//	{"get_length", lcsp_packet_get_length},
//	{"get_data", lcsp_packet_get_data},
//	{"set_data", lcsp_packet_set_data},
//
	{"__index", lcsp_packet_index},
	{"__gc", lcsp_packet_gc},

	{ NULL, NULL } /* sentinel */
};


static const luaL_Reg lcsp_functions[] = {
    {"init", lcsp_init},
    {"service_handler", lcsp_service_handler},

	{"get_hostname", lcsp_get_hostname},
	{"get_model", lcsp_get_model},
	{"get_revision", lcsp_get_revision},

    {"socket", lcsp_socket},
    {"bind", lcsp_bind},
    {"accept", lcsp_accept},
    {"connect", lcsp_connect},
    {"read", lcsp_read},
    {"send", lcsp_send},
    {"close", lcsp_close},

	{"ping", lcsp_ping},
	{"reboot", lcsp_reboot},
	{"shutdown", lcsp_shutdown},

    {"buffer_free", lcsp_buffer_free},
    {"buffer_get", lcsp_buffer_get},

    {"packet_get_length", lcsp_packet_get_length},
    {"packet_get_data", lcsp_packet_get_data},
    {"packet_set_data", lcsp_packet_set_data},

	{"cmp_ident", lcsp_cmp_ident},

//#ifdef CSP_USE_RTABLE
//	{"rtable_set", lcsp_rtable_set},
//	{"rtable_clear", lcsp_rtable_clear},
//	{"rtable_check", lcsp_rtable_check},
//	{"rtable_load", lcsp_rtable_load},
//	{"print_routes", lcsp_print_routes},
//#endif

#ifdef CSP_ENABLE_CSP_PRINT
	{"print_connections", lcsp_print_connections},
	{"print_interfaces", lcsp_print_interfaces},
#endif

#ifdef __linux__
	{"sleep", lcsp_sleep},
#endif    
    
    {NULL, NULL}  /* sentinel */
};

int luaopen_csp(lua_State *L) {
	/* Register metatable for the 'csp_socket_t' objects */
	if (luaL_newmetatable(L, "csp_socket_t")) {
		luaL_setfuncs(L, lcsp_socket_methods, 0);   /* add 'csp_socket_t' methods to the new metatable */
		lua_setfield(L, -1, "__index");             /* metatable.__index = metatable */
	}

	/* Register metatable for the 'csp_conn_t' objects */
	if (luaL_newmetatable(L, "csp_conn_t")) {       /* create metatable to handle 'csp_conn_t' objects */
		luaL_setfuncs(L, lcsp_conn_methods, 0);     /* add 'csp_conn_t' methods to the new metatable */
		lua_pop(L, 1);                              /* pop new metatable off the stack */
		//lua_setfield(L, -1, "__index");             /* metatable.__index = metatable */
	}

	/* Register metatable for the 'csp_packet_t' objects */
	if (luaL_newmetatable(L, "csp_packet_t")) {     /* create metatable to handle 'csp_packet_t' objects */
		luaL_setfuncs(L, lcsp_packet_methods, 0);   /* add 'csp_packet_t' methods to the new metatable */
		lua_pop(L, 1);                              /* pop new metatable off the stack */
		//lua_setfield(L, -1, "__index");             /* metatable.__index = metatable */
	}

    luaL_newlib(L, lcsp_functions);

	lua_pushstring(L, "libcsp");
	lua_setfield(L, -2, "_NAME");

	lua_pushstring(L, "" CSP_LUA_VERSION);
	lua_setfield(L, -2, "_VERSION");

#define lua_AddIntConstant(L, name) \
	lua_pushinteger(L, name); \
	lua_setfield(L, -2, #name)

    /* RESERVED PORTS */
    lua_AddIntConstant(L, CSP_CMP);
    lua_AddIntConstant(L, CSP_PING);
    lua_AddIntConstant(L, CSP_PS);
    lua_AddIntConstant(L, CSP_MEMFREE);
    lua_AddIntConstant(L, CSP_REBOOT);
    lua_AddIntConstant(L, CSP_BUF_FREE);
    lua_AddIntConstant(L, CSP_UPTIME);
    lua_AddIntConstant(L, CSP_ANY);

    /* PRIORITIES */
    lua_AddIntConstant(L, CSP_PRIO_CRITICAL);
    lua_AddIntConstant(L, CSP_PRIO_HIGH);
    lua_AddIntConstant(L, CSP_PRIO_NORM);
    lua_AddIntConstant(L, CSP_PRIO_LOW);

    /* FLAGS */
    lua_AddIntConstant(L, CSP_FFRAG);
    lua_AddIntConstant(L, CSP_FHMAC);
    lua_AddIntConstant(L, CSP_FRDP);
    lua_AddIntConstant(L, CSP_FCRC32);

    /* SOCKET OPTIONS */
    lua_AddIntConstant(L, CSP_SO_NONE);
    lua_AddIntConstant(L, CSP_SO_RDPREQ);
    lua_AddIntConstant(L, CSP_SO_RDPPROHIB);
    lua_AddIntConstant(L, CSP_SO_HMACREQ);
    lua_AddIntConstant(L, CSP_SO_HMACPROHIB);
    lua_AddIntConstant(L, CSP_SO_CRC32REQ);
    lua_AddIntConstant(L, CSP_SO_CRC32PROHIB);
    lua_AddIntConstant(L, CSP_SO_CONN_LESS);
    lua_AddIntConstant(L, CSP_SO_SAME);

    /* CONNECT OPTIONS */
    lua_AddIntConstant(L, CSP_O_NONE);
    lua_AddIntConstant(L, CSP_O_RDP);
    lua_AddIntConstant(L, CSP_O_NORDP);
    lua_AddIntConstant(L, CSP_O_HMAC);
    lua_AddIntConstant(L, CSP_O_NOHMAC);
    lua_AddIntConstant(L, CSP_O_CRC32);
    lua_AddIntConstant(L, CSP_O_NOCRC32);

    /* csp/csp_error.h */
    lua_AddIntConstant(L, CSP_ERR_NONE);
    lua_AddIntConstant(L, CSP_ERR_NOMEM);
    lua_AddIntConstant(L, CSP_ERR_INVAL);
    lua_AddIntConstant(L, CSP_ERR_TIMEDOUT);
    lua_AddIntConstant(L, CSP_ERR_USED);
    lua_AddIntConstant(L, CSP_ERR_NOTSUP);
    lua_AddIntConstant(L, CSP_ERR_BUSY);
    lua_AddIntConstant(L, CSP_ERR_ALREADY);
    lua_AddIntConstant(L, CSP_ERR_RESET);
    lua_AddIntConstant(L, CSP_ERR_NOBUFS);
    lua_AddIntConstant(L, CSP_ERR_TX);
    lua_AddIntConstant(L, CSP_ERR_DRIVER);
    lua_AddIntConstant(L, CSP_ERR_AGAIN);
    lua_AddIntConstant(L, CSP_ERR_NOSYS);
    lua_AddIntConstant(L, CSP_ERR_HMAC);
    lua_AddIntConstant(L, CSP_ERR_CRC32);
    lua_AddIntConstant(L, CSP_ERR_SFP);
    lua_AddIntConstant(L, CSP_ERR_MTU);

    /* misc */
    lua_AddIntConstant(L, CSP_NO_VIA_ADDRESS);
    lua_AddIntConstant(L, CSP_MAX_TIMEOUT);

#undef lua_AddIntConstant

    return 1;
}
