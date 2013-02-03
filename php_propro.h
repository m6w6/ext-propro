
#ifndef PHP_PROPRO_H
#define PHP_PROPRO_H

extern zend_module_entry propro_module_entry;
#define phpext_propro_ptr &propro_module_entry

#define PHP_PROPRO_VERSION "1.0.0"

#ifdef PHP_WIN32
#	define PHP_PROPRO_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_PROPRO_API __attribute__ ((visibility("default")))
#else
#	define PHP_PROPRO_API
#endif

#ifdef ZTS
#	include <TSRM/TSRM.h>
#endif

typedef struct php_property_proxy {
	zval *container;
	char *member_str;
	size_t member_len;
} php_property_proxy_t;

typedef struct php_property_proxy_object {
	zend_object zo;
	zend_object_value zv;
	php_property_proxy_t *proxy;
	struct php_property_proxy_object *parent;
} php_property_proxy_object_t;

PHP_PROPRO_API php_property_proxy_t *php_property_proxy_init(zval *container, const char *member_str, size_t member_len TSRMLS_DC);
PHP_PROPRO_API void php_property_proxy_free(php_property_proxy_t **proxy);

PHP_PROPRO_API zend_class_entry *php_property_proxy_get_class_entry(void);

PHP_PROPRO_API zend_object_value php_property_proxy_object_new(zend_class_entry *ce TSRMLS_DC);
PHP_PROPRO_API zend_object_value php_property_proxy_object_new_ex(zend_class_entry *ce, php_property_proxy_t *proxy, php_property_proxy_object_t **ptr TSRMLS_DC);

#endif	/* PHP_PROPRO_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
