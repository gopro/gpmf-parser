#pragma once

#if defined(_WIN32)
#	if defined(GPMFPARSER_EXPORT)
#		define GPMFPARSER_API __declspec(dllexport)
#	elif defined(GPMFPARSER_STATIC)
#		define GPMFPARSER_API
#	else
#		define GPMFPARSER_API __declspec(dllimport)
#	endif
#else
#	if defined(GPMFPARSER_EXPORT)
#		define GPMFPARSER_API __attribute__((visibility("default")))
#	elif defined(GPMFPARSER_STATIC)
#		define GPMFPARSER_API
#	else
#		define GPMFPARSER_API __attribute__((visibility("default")))
#	endif
#endif
