//
// Created by ken on 19. 11. 26..
//

#ifndef __LOG4CXX_H
#define __LOG4CXX_H

#include <log4cxx/logger.h>     /* log4cxx */

#define DECLARE_LOGGER(_LOGID_) static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger(_LOGID_))

#define _EXIT(...)				do { \
	LOG4CXX_FATAL(logger, std::format(__VA_ARGS__).c_str()); exit(0); \
} while (0)
#define _INFO(...)				LOG4CXX_INFO(logger, std::format(__VA_ARGS__).c_str())
#define _WARN(...)				LOG4CXX_WARN(logger, std::format(__VA_ARGS__).c_str())
#define _DEBUG(...)				LOG4CXX_DEBUG(logger, std::format(__VA_ARGS__).c_str())
#define _TRACE(...)				LOG4CXX_TRACE(logger, std::format(__VA_ARGS__).c_str())

#endif // __LOG4CXX_H
