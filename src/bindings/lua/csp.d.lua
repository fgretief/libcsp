---@meta csp

-- Place the path to this file in a workspace directory configured in .vscode/settings.json under
-- "Lua.workspace.userThirdParty" so the Lua Language Server can parse the type annotations.

---@class csplib
local csp = {}

---@type string
csp._NAME = "libcsp"

---@type string
csp._VERSION = ""

-- Reserved Ports
---@type integer
csp.CSP_CMP = 0
---@type integer
csp.CSP_PING = 0
---@type integer
csp.CSP_PS = 0
---@type integer
csp.CSP_MEMFREE = 0
---@type integer
csp.CSP_REBOOT = 0
---@type integer
csp.CSP_BUF_FREE = 0
---@type integer
csp.CSP_UPTIME = 0
---@type integer
csp.CSP_ANY = 0

-- Priorities
---@type integer
csp.CSP_PRIO_CRITICAL = 0
---@type integer
csp.CSP_PRIO_HIGH = 0
---@type integer
csp.CSP_PRIO_NORM = 0
---@type integer
csp.CSP_PRIO_LOW = 0

-- Flags
---@type integer
csp.CSP_FFRAG = 0
---@type integer
csp.CSP_FHMAC = 0
---@type integer
csp.CSP_FRDP = 0
---@type integer
csp.CSP_FCRC32 = 0

-- Socket Options
---@type integer
csp.CSP_SO_NONE = 0
---@type integer
csp.CSP_SO_RDPREQ = 0
---@type integer
csp.CSP_SO_RDPPROHIB = 0
---@type integer
csp.CSP_SO_HMACREQ = 0
---@type integer
csp.CSP_SO_HMACPROHIB = 0
---@type integer
csp.CSP_SO_CRC32REQ = 0
---@type integer
csp.CSP_SO_CRC32PROHIB = 0
---@type integer
csp.CSP_SO_CONN_LESS = 0
---@type integer
csp.CSP_SO_SAME = 0

-- Connect Options
---@type integer
csp.CSP_O_NONE = 0
---@type integer
csp.CSP_O_RDP = 0
---@type integer
csp.CSP_O_NORDP = 0
---@type integer
csp.CSP_O_HMAC = 0
---@type integer
csp.CSP_O_NOHMAC = 0
---@type integer
csp.CSP_O_CRC32 = 0
---@type integer
csp.CSP_O_NOCRC32 = 0

-- Error Codes
---@type integer
csp.CSP_ERR_NONE = 0
---@type integer
csp.CSP_ERR_NOMEM = 0
---@type integer
csp.CSP_ERR_INVAL = 0
---@type integer
csp.CSP_ERR_TIMEDOUT = 0
---@type integer
csp.CSP_ERR_USED = 0
---@type integer
csp.CSP_ERR_NOTSUP = 0
---@type integer
csp.CSP_ERR_BUSY = 0
---@type integer
csp.CSP_ERR_ALREADY = 0
---@type integer
csp.CSP_ERR_RESET = 0
---@type integer
csp.CSP_ERR_NOBUFS = 0
---@type integer
csp.CSP_ERR_TX = 0
---@type integer
csp.CSP_ERR_DRIVER = 0
---@type integer
csp.CSP_ERR_AGAIN = 0
---@type integer
csp.CSP_ERR_NOSYS = 0
---@type integer
csp.CSP_ERR_HMAC = 0
---@type integer
csp.CSP_ERR_CRC32 = 0
---@type integer
csp.CSP_ERR_SFP = 0
---@type integer
csp.CSP_ERR_MTU = 0

-- Misc Constants
---@type integer
csp.CSP_NO_VIA_ADDRESS = 0
---@type integer
csp.CSP_MAX_TIMEOUT = 0

---@class csp_socket_t

---@class csp_conn_t
---@field dport integer Destination port
---@field sport integer Source port
---@field dst integer Destination node address
---@field src integer Source node address
---@field flags integer Connection flags
---@field state "open"|"closed" Connection state

---@class csp_packet_t
---@field length integer Packet payload length in bytes
---@field data string Packet binary payload

---Initialize the CSP subsystem.
---@param hostname string Local hostname
---@param model string Device model string
---@param revision string Revision string
---@param version? integer Protocol version (default: 2)
---@param conn_dfl_so? integer Default socket options
---@param dedup? integer Deduplication setting
function csp.init(hostname, model, revision, version, conn_dfl_so, dedup) end

---Pass a packet to the default CSP service handler.
---@param packet csp_packet_t
function csp.service_handler(packet) end

---Pass a packet to the default CSP service handler.
---@param packet csp_packet_t
function csp.service_handler(packet) end

---Get local hostname.
---@return string
function csp.get_hostname() end

---Get local model name.
---@return string
function csp.get_model() end

---Get local revision string.
---@return string
function csp.get_revision() end

---Create a new CSP socket object.
---@param opts? integer Socket option flags (e.g., `csp.CSP_SO_NONE`)
---@return csp_socket_t
function csp.socket(opts) end

---Bind a socket to a port.
---@param socket csp_socket_t
---@param port integer
---@return boolean success
function csp.bind(socket, port) end

---Accept an incoming connection on a socket.
---@param socket csp_socket_t
---@param timeout? integer Timeout in ms (default: `CSP_MAX_TIMEOUT`)
---@return csp_conn_t? conn
function csp.accept(socket, timeout) end

---Establish an outgoing connection.
---@param prio integer Priority level
---@param dest integer Destination node address
---@param dport integer Destination port
---@param timeout? integer Timeout in ms
---@param opts? integer Connection options
---@return csp_conn_t conn
function csp.connect(prio, dest, dport, timeout, opts) end

---Read a packet from a connection.
---@param conn csp_conn_t
---@param timeout? integer Timeout in ms (default: 500)
---@return csp_packet_t? packet
function csp.read(conn, timeout) end

---Send a packet over a connection.
---@param conn csp_conn_t
---@param packet csp_packet_t
---@param timeout? integer Timeout in ms (default: 1000)
---@return boolean success
function csp.send(conn, packet, timeout) end

---Close a CSP connection manually.
---@param conn csp_conn_t
function csp.close(conn) end

---Send a CSP ping request to a node.
---@param node integer Node address
---@param timeout? integer Timeout in ms (default: 1000)
---@param size? integer Ping payload size (default: 10)
---@param conn_options? integer
---@return integer ping_time Ping duration in ms
function csp.ping(node, timeout, size, conn_options) end

---Send a reboot command to a node.
---@param node integer Node address
function csp.reboot(node) end

---Send a shutdown command to a node.
---@param node integer Node address
function csp.shutdown(node) end

---Allocate a CSP buffer for a packet.
---@return csp_packet_t packet
function csp.buffer_get() end

---Free a CSP packet buffer manually.
---@param packet csp_packet_t
function csp.buffer_free(packet) end

---Get the payload length of a packet.
---@param packet csp_packet_t
---@return integer length
function csp.packet_get_length(packet) end

---Get the binary data of a packet payload.
---@param packet csp_packet_t
---@return string data
function csp.packet_get_data(packet) end

---Set the binary data payload of a packet.
---@param packet csp_packet_t
---@param data string
function csp.packet_set_data(packet, data) end

---Query node identity via CMP.
---@param node integer Node address
---@param timeout? integer Timeout in ms (default: 1000)
---@return string hostname
---@return string model
---@return string revision
---@return string date
---@return string time
function csp.cmp_ident(node, timeout) end

---Print active CSP connections table.
function csp.print_connections() end

---Print interface table.
function csp.print_interfaces() end

---Sleep execution for a specified delay.
---@param delay number Sleep time in seconds
---@param clockid? integer Clock source ID
---@return boolean success
function csp.sleep(delay, clockid) end

---Close a CSP connection object.
function csp_conn_t:close() end

return csp
