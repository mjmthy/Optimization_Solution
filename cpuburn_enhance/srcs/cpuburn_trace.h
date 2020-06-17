#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpuburn

#if !defined(__CPUBURN_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __CPUBURN_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(periodic_burn,
	    TP_PROTO(u64 start, u64 end, u64 duration),
	    TP_ARGS(start, end, duration),
	    TP_STRUCT__entry(
		__field(u64, start)
		__field(u64, end)
		__field(u64, duration)
	    ),
	    TP_fast_assign(
		__entry->start = start;
		__entry->end = end;
		__entry->duration = duration;
	    ),
	    TP_printk("start=%llu, end=%llu, duration=%llu", __entry->start, __entry->end, __entry->duration)
);

#endif

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE cpuburn_trace
#include <trace/define_trace.h>
