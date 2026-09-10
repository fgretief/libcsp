-- cd libcsp
-- cmake -B build -S . -DCSP_ENABLE_LUA_BINDINGS -DCSP_USE_RTABLE
-- cmake --build build
-- export LUA_CPATH="$(pwd)/build/?.so;;"
-- lua src/bindings/lua/csp.tests.lua

---@module 'csp'
local csp = require 'csp'

assert(csp._NAME == 'libcsp')
assert(csp._VERSION == '2.2')

-- Reserved ports for CSP services.
assert(csp.CSP_CMP      == 0)
assert(csp.CSP_PING     == 1)
assert(csp.CSP_PS       == 2)
assert(csp.CSP_MEMFREE  == 3)
assert(csp.CSP_REBOOT   == 4)
assert(csp.CSP_BUF_FREE == 5)
assert(csp.CSP_UPTIME   == 6)
assert(csp.CSP_ANY      == 255)

assert(csp.CSP_PRIO_CRITICAL == 0)
assert(csp.CSP_PRIO_HIGH == 1)
assert(csp.CSP_PRIO_NORM == 2)
assert(csp.CSP_PRIO_LOW == 3)

assert(csp.CSP_FFRAG  == 0x10)
assert(csp.CSP_FHMAC  == 0x08)
assert(csp.CSP_FRDP   == 0x02)
assert(csp.CSP_FCRC32 == 0x01)

assert(csp.CSP_SO_NONE == 0x0000)
assert(csp.CSP_SO_RDPREQ == 0x0001)
assert(csp.CSP_SO_RDPPROHIB == 0x0002)
assert(csp.CSP_SO_HMACREQ == 0x0004)
assert(csp.CSP_SO_HMACPROHIB == 0x0008)
assert(csp.CSP_SO_CRC32REQ == 0x0040)
assert(csp.CSP_SO_CRC32PROHIB == 0x0080)
assert(csp.CSP_SO_CONN_LESS == 0x0100)
assert(csp.CSP_SO_SAME == 0x8000)

assert(csp.CSP_ERR_NONE    ==  0)
assert(csp.CSP_ERR_NOMEM   == -1)
assert(csp.CSP_ERR_INVAL   == -2)
assert(csp.CSP_ERR_TIMEDOUT== -3)
assert(csp.CSP_ERR_USED    == -4)
assert(csp.CSP_ERR_NOTSUP  == -5)
assert(csp.CSP_ERR_BUSY    == -6)
assert(csp.CSP_ERR_ALREADY == -7)
assert(csp.CSP_ERR_RESET   == -8)
assert(csp.CSP_ERR_NOBUFS  == -9)
assert(csp.CSP_ERR_TX      == -10)
assert(csp.CSP_ERR_DRIVER  == -11)
assert(csp.CSP_ERR_AGAIN   == -12)
assert(csp.CSP_ERR_NOSYS   == -38)
assert(csp.CSP_ERR_HMAC    == -100)
assert(csp.CSP_ERR_CRC32   == -102)
assert(csp.CSP_ERR_SFP     == -103)
assert(csp.CSP_ERR_MTU     == -104)

assert(csp.CSP_NO_VIA_ADDRESS == 0xFFFF)
assert(csp.CSP_MAX_TIMEOUT == 4294967295)

assert(csp.init("test123", "foobar", "v123") == nil)
assert(csp.get_hostname() == "test123")
assert(csp.get_model() == "foobar")
assert(csp.get_revision() == "v123")

local socket = csp.socket(csp.CSP_SO_NONE);
do
    local csp_socket_t = 'userdata'
    assert(type(socket) == csp_socket_t)
    local mt = getmetatable(socket)
    assert(type(mt) == 'table')
    assert(mt.__name == 'csp_socket_t')
    assert(type(mt.__gc) == 'function')

	--print(mt.__name); for k,v in pairs(mt) do print("| " .. tostring(k),v) end
end

assert(csp.bind(socket, csp.CSP_SO_NONE) == true);

print("--> Interfaces:")
csp.print_interfaces()
print("--> Connections:")
csp.print_connections()

local conn = csp.connect(csp.CSP_PRIO_NORM, 123, 5, 500, 0)
do
    local csp_conn_t = 'userdata'
    assert(type(conn) == csp_conn_t)
    local mt = getmetatable(conn)
    assert(type(mt) == 'table')
    assert(mt.__name == 'csp_conn_t')
    assert(type(mt.__gc) == 'function')
    --print(mt.__name); for k,v in pairs(mt) do print("| ".. tostring(k),v) end
    assert(conn.state == 'open')
end

local packet = csp.buffer_get()
do
    local csp_packet_t = 'userdata'
    assert(type(packet) == csp_packet_t)
    local mt = getmetatable(packet)
    assert(type(mt) == 'table')
    assert(mt.__name == 'csp_packet_t')
    assert(type(mt.__gc) == 'function')
    --print(mt.__name); for k,v in pairs(mt) do print("| ".. tostring(k),v) end
end

--packet:set_data("hello world")
--assert(packet:get_data() == "hello world")
--assert(packet:get_length() == #"hello world")
--
--assert(packet.data == "hello world")
--assert(packet.length == #"hello world")
--
--csp.send(conn, packet, 1000)
--
csp.close(conn)
--assert(conn.state == 'closed')


--local conn = csp.accept(socket, 500)
--if conn == nil then
--    print("FIXME: connection was nil")
--else
--    local csp_conn_t = 'userdata'
--    assert(type(conn) == csp_conn_t)
--    local mt = getmetatable(conn)
--    assert(type(mt) == 'table')
--    assert(mt.__name == 'csp_conn_t')
--    assert(type(mt.__gc) == 'function')
--end

--local packet = csp.read(conn, 500);



csp.ping(0, 1000, 0)

print("TEST: SUCCESS")
