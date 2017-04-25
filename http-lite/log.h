#ifndef _LOG_H_

#ifdef _DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

typedef enum { dbg_level_critical, dbg_level_error, dbg_level_debug, dbg_level_info, dbg_level_trace } log_level;
#define DEBUG_LEVEL dbg_level_trace

#define debug_print(level, ...) \
            do { if (DEBUG && level <= DEBUG_LEVEL) {fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\r\n"); } } while (0)

#endif // !_LOG_H_

