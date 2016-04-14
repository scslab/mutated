#ifndef MUTATED_MEMCACHE_HH
#define MUTATED_MEMCACHE_HH

/**
 * memcache.hh - representation of the memcache binary protocol.
 */

// TODO: use scoped enums for constants
// TODO: use subtyping for extras field?
// TODO: add to/from network store order for header

#include <cstdint>
#include <iostream>
#include <string>

using MemcType   = uint8_t;
using MemcCmd    = uint8_t;
using MemcStatus = uint16_t;

// MemcHeader represents the header of a memcache binary protocl request or
// response.
struct MemcHeader {
    MemcType   type;     // (aka 'magic) request or response?
    MemcCmd    cmd;      // memcache operation
    uint16_t   keylen;   // length of key
    uint8_t    extralen; // length of extras field
    uint8_t    datatype; // unused / reserved
    MemcStatus status;   // success or failure of operation (response only)
    uint32_t   bodylen;  // total body length (key + extra + value)
    uint32_t   opaque;   // field associated with key-value controlable by user
    uint64_t   version;  // version of key-value pair
};

// Memcache header size
constexpr std::size_t MEMC_HEADER_SIZE = sizeof(MemcHeader);

// MemcPacket represents (as best as possible in C++) a memcache
// request/response packet. This is contiguous on the wire but we can't
// represent that in C++ without a layer of indirection to capture the varaible
// sizing.
struct MemcPacket {
    MemcHeader* header;
    const char* extras;
    const char* key;
    const char* value;
};

// MemcExtrasSet represents the extra field for a set request.
struct MemcExtrasSet {
    uint32_t flags;
    uint32_t expiration;
};

// MemcExtrasIncr represents the extra field for a increment/decrement request.
struct MemcExtrasIncr {
    uint64_t initial;
    uint64_t delta;
    uint32_t expiration;
};

// MemcExtrasTouch represents the extra field for a touch request.
struct MemcExtrasTouch {
    uint32_t expiration;
};

// Magic codes / packet types
constexpr MemcType MEMC_REQUEST = 0x80;
constexpr MemcType MEMC_RESPONE = 0x81;

// Memcache commands
constexpr MemcCmd MEMC_CMD_GET             = 0x00;
constexpr MemcCmd MEMC_CMD_SET             = 0x01;
constexpr MemcCmd MEMC_CMD_ADD             = 0x02;
constexpr MemcCmd MEMC_CMD_REPLACE         = 0x03;
constexpr MemcCmd MEMC_CMD_DELETE          = 0x04;
constexpr MemcCmd MEMC_CMD_INCREMENT       = 0x05;
constexpr MemcCmd MEMC_CMD_DECREMENT       = 0x06;
constexpr MemcCmd MEMC_CMD_QUIT            = 0x07;
constexpr MemcCmd MEMC_CMD_FLUSH           = 0x08;
constexpr MemcCmd MEMC_CMD_GETQ            = 0x09;
constexpr MemcCmd MEMC_CMD_NOOP            = 0x0a;
constexpr MemcCmd MEMC_CMD_VERSION         = 0x0b;
constexpr MemcCmd MEMC_CMD_GETK            = 0x0c;
constexpr MemcCmd MEMC_CMD_GETKQ           = 0x0d;
constexpr MemcCmd MEMC_CMD_APPEND          = 0x0e;
constexpr MemcCmd MEMC_CMD_PREPEND         = 0x0f;
constexpr MemcCmd MEMC_CMD_STAT            = 0x10;
constexpr MemcCmd MEMC_CMD_SETQ            = 0x11;
constexpr MemcCmd MEMC_CMD_ADDQ            = 0x12;
constexpr MemcCmd MEMC_CMD_REPLACEQ        = 0x13;
constexpr MemcCmd MEMC_CMD_DELETEQ         = 0x14;
constexpr MemcCmd MEMC_CMD_INCREMENTQ      = 0x15;
constexpr MemcCmd MEMC_CMD_DECREMENTQ      = 0x16;
constexpr MemcCmd MEMC_CMD_QUITQ           = 0x17;
constexpr MemcCmd MEMC_CMD_FLUSHQ          = 0x18;
constexpr MemcCmd MEMC_CMD_APPENDQ         = 0x19;
constexpr MemcCmd MEMC_CMD_PREPENDQ        = 0x1a;
constexpr MemcCmd MEMC_CMD_VERBOSITY       = 0x1b; // memcache doesn't imp. but in spec;
constexpr MemcCmd MEMC_CMD_TOUCH           = 0x1c;
constexpr MemcCmd MEMC_CMD_GAT             = 0x1d;
constexpr MemcCmd MEMC_CMD_GATQ            = 0x1e;
constexpr MemcCmd MEMC_CMD_GATK            = 0x23;
constexpr MemcCmd MEMC_CMD_GATKQ           = 0x24;
constexpr MemcCmd MEMC_CMD_SASL_LIST_MECHS = 0x20;
constexpr MemcCmd MEMC_CMD_SASL_AUTH       = 0x21;
constexpr MemcCmd MEMC_CMD_SASL_STEP       = 0x22;

// Memcache status/error codes
constexpr MemcStatus MEMC_OK                     = 0x0000;
constexpr MemcStatus MEMC_ERROR_KEY_NOT_FOUND    = 0x0001;
constexpr MemcStatus MEMC_ERROR_KEY_EXISTS       = 0x0002;
constexpr MemcStatus MEMC_ERROR_VALUE_TOO_LARGE  = 0x0003;
constexpr MemcStatus MEMC_ERROR_INVALID_ARGUMENT = 0x0004;
constexpr MemcStatus MEMC_ERROR_ITEM_NOT_STORED  = 0x0005;
constexpr MemcStatus MEMC_ERROR_NON_NUMERIC      = 0x0006;
constexpr MemcStatus MEMC_ERROR_AUTH_FAILED      = 0x0020;
constexpr MemcStatus MEMC_ERROR_UNKNOWN_COMMAND  = 0x0081;
constexpr MemcStatus MEMC_ERROR_OUT_OF_MEMORY    = 0x0082;
constexpr MemcStatus MEMC_ERROR_BUSY             = 0x0085;

// Memcache stat headers
const std::string MEMC_STATS_TIME              = "time";
const std::string MEMC_STATS_CURR_ITEMS        = "curr_items";
const std::string MEMC_STATS_BYTES             = "bytes";
const std::string MEMC_STATS_BYTES_READ        = "bytes_read";
const std::string MEMC_STATS_BYTES_WRITTEN     = "bytes_written";
const std::string MEMC_STATS_EVICTIONS         = "evictions";
const std::string MEMC_STATS_EXPIRATIONS       = "expired";
const std::string MEMC_STATS_TOTAL_ITEMS       = "total_items";
const std::string MEMC_STATS_CURR_CONNECTIONS  = "curr_connections";
const std::string MEMC_STATS_TOTAL_CONNECTIONS = "total_connections";
const std::string MEMC_STATS_AUTH_CMDS         = "auth_cmds";
const std::string MEMC_STATS_AUTH_ERRORS       = "auth_errors";
const std::string MEMC_STATS_GET               = "cmd_get";
const std::string MEMC_STATS_SET               = "cmd_set";
const std::string MEMC_STATS_DELETE            = "cmd_delete";
const std::string MEMC_STATS_TOUCH             = "cmd_touch";
const std::string MEMC_STATS_FLUSH             = "cmd_flush";
const std::string MEMC_STATS_GET_HITS          = "get_hits";
const std::string MEMC_STATS_GET_MISSES        = "get_misses";
const std::string MEMC_STATS_DELETE_HITS       = "delete_hits";
const std::string MEMC_STATS_DELETE_MISSES     = "delete_misses";
const std::string MEMC_STATS_INCR_HITS         = "incr_hits";
const std::string MEMC_STATS_INCR_MISSES       = "incr_misses";
const std::string MEMC_STATS_DECR_HITS         = "decr_hits";
const std::string MEMC_STATS_DECR_MISSES       = "decr_misses";
const std::string MEMC_STATS_TOUCH_HITS        = "touch_hits";
const std::string MEMC_STATS_TOUCH_MISSES      = "touch_misses";
const std::string MEMC_STATS_MEM_LIMIT         = "limit_maxbytes";
const std::string MEMC_STATS_CAS_HITS          = "cas_hits";
const std::string MEMC_STATS_CAS_MISS          = "cas_misses";
const std::string MEMC_STATS_CAS_BADVAL        = "cas_badval";

// Memcache 'stats settings' headers
const std::string MEMC_STATS_SET_MAXBYTES = "maxbytes";
const std::string MEMC_STATS_SET_TCPPORT  = "tcpport";
const std::string MEMC_STATS_SET_CAS      = "cas_enabled";
const std::string MEMC_STATS_SET_AUTH     = "auth_enabled_sasl";
const std::string MEMC_STATS_SET_MAXITEM  = "item_size_max";

// Memcache 'stats items' headers
const std::string MEMC_STATS_ITEMS_SIZE       = "chunk_size";
const std::string MEMC_STATS_ITEMS_NUM        = "number";
const std::string MEMC_STATS_ITEMS_EVICTED    = "evicted";
const std::string MEMC_STATS_ITEMS_EVICTED_NZ = "evicted_nonzero";
const std::string MEMC_STATS_ITEMS_OOM        = "outofmemory";

/**
 * Print to stdout the memcache packet header.
 * @header: the header to print.
 */
inline void print_memc_header(const MemcHeader & header)
{
    std::ios_base::fmtflags flags = std::cout.flags();
    std::cout << std::hex;

    std::cout << "Memcache Header" << std::endl;
    std::cout << "-   type: 0x" << (uint16_t) header.type << std::endl;
    std::cout << "-    cmd: 0x" << (uint16_t) header.cmd << std::endl;
    std::cout << "-   keyl: 0x" << header.keylen << std::endl;
    std::cout << "-   extl: 0x" << (uint16_t) header.extralen << std::endl;
    std::cout << "-   data: 0x" << (uint16_t) header.datatype << std::endl;
    std::cout << "- status: 0x" << header.status << std::endl;
    std::cout << "-  bodyl: 0x" << header.bodylen << std::endl;
    std::cout << "- opaque: 0x" << header.opaque << std::endl;
    std::cout << "-   vers: 0x" << header.version << std::endl;

    std::cout.flags(flags);
}

#endif /* MUTATED_MEMCACHE_HH */
