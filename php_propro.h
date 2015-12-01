/*
    +--------------------------------------------------------------------+
    | PECL :: propro                                                     |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2013 Michael Wallner <mike@php.net>                  |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_PROPRO_H
#define PHP_PROPRO_H

extern zend_module_entry propro_module_entry;
#define phpext_propro_ptr &propro_module_entry

#define PHP_PROPRO_VERSION "1.0.2RC1"

#ifdef PHP_WIN32
#	define PHP_PROPRO_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_PROPRO_API extern __attribute__ ((visibility("default")))
#else
#	define PHP_PROPRO_API extern
#endif

#ifdef ZTS
#	include <TSRM/TSRM.h>
#endif

#include "php_propro_api.h"

#endif	/* PHP_PROPRO_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
