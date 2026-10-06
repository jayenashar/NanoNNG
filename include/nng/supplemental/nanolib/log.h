#ifndef NNG_NANOLIB_LOG_H
#define NNG_NANOLIB_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "nng/nng.h"

#define LOG_VERSION "0.2.1"

/**
 * 2023-12-08 move conf_log to log.h
*/

// log type
#define LOG_TO_FILE (1 << 0)
#define LOG_TO_CONSOLE (1 << 1)
#define LOG_TO_SYSLOG (1 << 2)
#define LOG_TO_UDS (1 << 3)

typedef struct conf_log conf_log;
struct conf_log {
	uint8_t  type;
	int      level;
	char    *dir;
	char    *file;
	char    *uds_addr;
	FILE    *fp;
	char    *abs_path;        // absolut path of log file
	char    *rotation_sz_str; // 1000KB, 100MB, 10GB
	uint64_t rotation_sz;     // unit: byte
	size_t   rotation_count;  // rotation count
};

// MQTT protocol trace categories. Each one is switched on by itself so an
// operator can trace a single packet class on a loaded broker without paying
// for the others, and so payload bodies stay off when metadata is on.
#define NMQ_TRACE_CONNECT (1u << 0)
#define NMQ_TRACE_AUTH (1u << 1)
#define NMQ_TRACE_SUB (1u << 2)
#define NMQ_TRACE_PUB (1u << 3)
#define NMQ_TRACE_PAYLOAD (1u << 4)
#define NMQ_TRACE_SESSION (1u << 5)
#define NMQ_TRACE_TLS (1u << 6)
// Separate from AUTH because an ACL decision is taken per PUBLISH and per
// topic filter, not once per connection, so its volume is a different order.
#define NMQ_TRACE_ACL (1u << 7)
#define NMQ_TRACE_ALL                                                     \
	(NMQ_TRACE_CONNECT | NMQ_TRACE_AUTH | NMQ_TRACE_SUB |             \
	    NMQ_TRACE_PUB | NMQ_TRACE_PAYLOAD | NMQ_TRACE_SESSION |       \
	    NMQ_TRACE_TLS | NMQ_TRACE_ACL)

// NMQ_TRACE_PAYLOAD is a modifier on NMQ_TRACE_PUB rather than a line of its
// own: it adds the body to the PUBLISH line. Enabling it implies NMQ_TRACE_PUB.
#define NMQ_TRACE_PAYLOAD_LIMIT_DEFAULT 256
// Raw bytes of payload that may be rendered. Capped so the render buffer can
// live on the stack of a worker callback.
#define NMQ_TRACE_PAYLOAD_LIMIT_MAX 1024
// Worst case is four output characters per input byte plus the "..." marker.
#define NMQ_TRACE_PAYLOAD_RENDER_MAX (4 * NMQ_TRACE_PAYLOAD_LIMIT_MAX + 8)

// Renders an MQTT string that the protocol allows to be absent, so a trace
// line shows "-" instead of taking a NULL into printf.
#define NMQ_TRACE_STR(s) \
	((const char *) ((s) != NULL ? (const char *) (s) : "-"))

// Longest rendering of a full category mask as names, plus the NUL.
#define NMQ_TRACE_CATEGORIES_STR_MAX 64

// A trace sink can take a line per message, so it gets a bigger default
// rotation than conf_log's 10KB: at that size the rotation stat, rename and
// index write would fire every few dozen lines.
#define NMQ_TRACE_ROTATION_SZ_DEFAULT (100 * 1024 * 1024)
#define NMQ_TRACE_ROTATION_COUNT_DEFAULT 10

typedef struct conf_log_trace conf_log_trace;
struct conf_log_trace {
	conf_log sink;          // own sinks, so nanomq.log keeps its own level
	uint32_t categories;    // NMQ_TRACE_* bitmask; 0 means trace nothing
	size_t   payload_limit; // bytes of PUBLISH body rendered per message
};

typedef struct {
	va_list     ap;
	const char *fmt;
	const char *file;
	const char *func;
	struct tm   time;
	void *      udata;
	int         line;
	int         level;
	uint32_t    category; // NMQ_TRACE_*, or 0 for an ordinary log event
	conf_log *  config;
} log_event;

typedef void (*log_func)(log_event *ev);

enum {
	NNG_LOG_FATAL = 0,
	NNG_LOG_ERROR,
	NNG_LOG_WARN,
	NNG_LOG_INFO,
	NNG_LOG_DEBUG,
	NNG_LOG_TRACE,
};

NNG_DECL const char *log_level_string(int level);
NNG_DECL int         log_level_num(const char *level);
NNG_DECL void        log_set_level(int level);
NNG_DECL void        log_update_level(int new_level);
NNG_DECL int         log_add_callback(
            log_func fn, void *udata, int level, void *mtx, conf_log *config);
NNG_DECL void log_add_console(int level, void *mtx);
NNG_DECL int  log_add_fp(FILE *fp, int level, void *mtx, conf_log *config);
NNG_DECL void log_add_syslog(const char *log_name, uint8_t level, void *mtx);
NNG_DECL void log_add_uds(const char *uds_path, const char *log_name, uint8_t level, void *mtx);
NNG_DECL void uds_closelog(void);
NNG_DECL void log_log(int level, const char *file, int line, const char *func,
    const char *fmt, ...);
NNG_DECL void log_clear_callback();

// log_trace_get_categories() returns the enabled categories. The nmq_trace()
// macro calls it before formatting anything, so a disabled category costs one
// call, one load and one branch. Written only by log_trace_set_categories();
// raced deliberately, as log_update_level() already races L.level.
NNG_DECL void     log_trace_set_categories(uint32_t categories);
NNG_DECL uint32_t log_trace_get_categories(void);
NNG_DECL void     log_trace_set_payload_limit(size_t limit);
NNG_DECL size_t   log_trace_get_payload_limit(void);
// Returns the NMQ_TRACE_* bit for a category name, or 0 when the name is not
// one. "all" returns every bit.
NNG_DECL uint32_t log_trace_category_num(const char *name);
NNG_DECL const char *log_trace_category_string(uint32_t category);
// Renders a category mask as a comma separated name list, for reporting what
// tracing is armed. Writes "none" for an empty mask.
NNG_DECL void log_trace_categories_string(
    uint32_t categories, char *out, size_t out_sz);
// Parses a comma separated category list, e.g. "connect,pub,payload".
NNG_DECL uint32_t log_trace_categories_parse(const char *list);
NNG_DECL int      log_add_trace_fp(FILE *fp, void *mtx, conf_log *config);
NNG_DECL void     log_add_trace_console(void *mtx);
NNG_DECL void     log_log_trace(uint32_t category, const char *file, int line,
        const char *func, const char *fmt, ...);
// Renders len bytes of data into out as printable ASCII, escaping everything
// else as \xNN, and appends "..." when the input was truncated. One format
// serves JSON payloads and binary alike, so there is no mode to choose.
NNG_DECL void log_trace_escape(
    const void *data, size_t len, size_t limit, char *out, size_t out_sz);
#ifdef ENABLE_LOG

#define log_trace(...) \
    log_log(NNG_LOG_TRACE, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_debug(...) \
    log_log(NNG_LOG_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_info(...) \
    log_log(NNG_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_warn(...) \
    log_log(NNG_LOG_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_error(...) \
    log_log(NNG_LOG_ERROR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_fatal(...) \
    log_log(NNG_LOG_FATAL, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define nmq_trace_on(cat) (((cat) & log_trace_get_categories()) != 0)
#define nmq_trace(cat, ...)                                               \
	do {                                                              \
		if (nmq_trace_on(cat))                                    \
			log_log_trace((cat), __FILE__, __LINE__,          \
			    __FUNCTION__, __VA_ARGS__);                   \
	} while (0)

#else

static inline void dummy_log_function(const char *fmt, ...) { (void)fmt;  }

#define log_trace(...) dummy_log_function(__VA_ARGS__)
#define log_debug(...) dummy_log_function(__VA_ARGS__)
#define log_info(...) dummy_log_function(__VA_ARGS__)
#define log_warn(...) dummy_log_function(__VA_ARGS__)
#define log_error(...) dummy_log_function(__VA_ARGS__)
#define log_fatal(...) dummy_log_function(__VA_ARGS__)

#define nmq_trace_on(cat) (false)
#define nmq_trace(cat, ...) dummy_log_function(__VA_ARGS__)

#endif


#ifdef __cplusplus
}
#endif

#endif
