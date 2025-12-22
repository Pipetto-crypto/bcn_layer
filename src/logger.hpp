#ifndef __LOGGER_HPP
#define __LOGGER_HPP

#include <sstream>
#include <string>
#include <iostream>
#include <stdarg.h>

namespace Logger {
	#define BCN_LAYER_LOG_INFO (1ull << 0)
	#define BCN_LAYER_LOG_ERROR (1ull << 1)
	
	struct bcn_layer_log {
	    std::string name;
	    unsigned long long value;
	};
	
	void log (const std::string& log_level, const char *format, ...);
}

#endif
