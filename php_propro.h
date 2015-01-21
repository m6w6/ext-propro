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

#ifndef DOXYGEN

extern zend_module_entry propro_module_entry;
#define phpext_propro_ptr &propro_module_entry

#define PHP_PROPRO_VERSION "2.0.0dev"

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

#define PHP_PROPRO_PTR(zo) (void*)(((char*)(zo))-(zo)->handlers->offset)

#endif /* DOXYGEN */

/**
 * The internal property proxy.
 *
 * Container for the object/array holding the proxied property.
 */
struct php_property_proxy {
	/** The container holding the property */
	zval container;
	/** The name of the proxied property */
	zend_string *member;
};
typedef struct php_property_proxy php_property_proxy_t;

/**
 * The userland object.
 *
 * Return an object instance of php\\PropertyProxy to make your C-struct
 * member accessible by reference from PHP userland.
 *
 * Example:
 * ~~~~~~~~~~{.c}
 * static zval *my_read_prop(zval *object, zval *member, int type, void **cache_slot, zval *tmp)
 * {
 *    zval *return_value;
 *    zend_string *member_name = zval_get_string(member);
 *    my_prophandler_t *handler = my_get_prophandler(member_name);
 *
 *    if (!handler || type == BP_VAR_R || type == BP_VAR_IS) {
 *    	return_value = zend_get_std_object_handlers()->read_property(object, member, type, cache_slot, tmp);
 *
 *    	if (handler) {
 *    		handler->read(object, tmp);
 *
 *    		zval_ptr_dtor(return_value);
 *    		ZVAL_COPY_VALUE(return_value, tmp);
 *    	}
 *    } else {
 *    	return_value = php_property_proxy_zval(object, member_name);
 *    }
 *
 *    zend_string_release(member_name);
 *
 *    return return_value;
 * }
 * ~~~~~~~~~~
 */
struct php_property_proxy_object {
	/** The actual property proxy */
	php_property_proxy_t *proxy;
	/** Any parent property proxy object */
	zval parent;
	/** Bond, James Bond */
	zval myself;
	/** The std zend_object */
	zend_object zo;
};
typedef struct php_property_proxy_object php_property_proxy_object_t;

PHP_PROPRO_API php_property_proxy_object_t *php_property_proxy_object_new_ex(
		zend_class_entry *ce, php_property_proxy_t *proxy);

PHP_PROPRO_API zend_object *php_property_proxy_object_new(zend_class_entry *ce);

/**
 * Create a property proxy as zval suitable to return from the property handler.
 *
 * Wrapper for php_property_proxy_init() and php_property_proxy_object_new_ex()
 * for use within a custom property handler.
 *
 * @param container the container holding the property
 * @param member the name of the proxied property
 * @return the new property proxy as zval
 */
PHP_PROPRO_API zval *php_property_proxy_zval(zval *container, zend_string *member);

/**
 * Create a property proxy
 *
 * The property proxy will forward reads and writes to itself to the
 * proxied property with name \a member_str of \a container.
 *
 * @param container the container holding the property
 * @param member the name of the proxied property
 * @return a new property proxy
 */
PHP_PROPRO_API php_property_proxy_t *php_property_proxy_init(zval *container,
		zend_string *member);

/**
 * Destroy and free a property proxy.
 *
 * The destruction of the property proxy object calls this.
 *
 * @param proxy a pointer to the allocated property proxy
 */
PHP_PROPRO_API void php_property_proxy_free(php_property_proxy_t **proxy);

/**
 * Get the zend_class_entry of php\\PropertyProxy
 * @return the class entry pointer
 */
PHP_PROPRO_API zend_class_entry *php_property_proxy_get_class_entry(void);

#endif	/* PHP_PROPRO_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
