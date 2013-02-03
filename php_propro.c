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


#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <php.h>
#include <ext/standard/info.h>

#include "php_propro.h"

typedef int STATUS;

PHP_PROPRO_API php_property_proxy_t *php_property_proxy_init(zval *container, const char *member_str, size_t member_len TSRMLS_DC)
{
	php_property_proxy_t *proxy = ecalloc(1, sizeof(*proxy));

	Z_ADDREF_P(container);
	proxy->container = container;
	proxy->member_str = estrndup(member_str, member_len);
	proxy->member_len = member_len;

	return proxy;
}

PHP_PROPRO_API void php_property_proxy_free(php_property_proxy_t **proxy)
{
	if (*proxy) {
		zval_ptr_dtor(&(*proxy)->container);
		efree((*proxy)->member_str);
		efree(*proxy);
		*proxy = NULL;
	}
}

static zend_class_entry *php_property_proxy_class_entry;
static zend_object_handlers php_property_proxy_object_handlers;

PHP_PROPRO_API zend_class_entry *php_property_proxy_get_class_entry(void)
{
	return php_property_proxy_class_entry;
}

PHP_PROPRO_API zend_object_value php_property_proxy_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_property_proxy_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

static void php_property_proxy_object_free(void *object TSRMLS_DC)
{
	php_property_proxy_object_t *o = object;

	if (o->proxy) {
		php_property_proxy_free(&o->proxy);
	}
	if (o->parent) {
		zend_objects_store_del_ref_by_handle_ex(o->parent->zv.handle, o->parent->zv.handlers TSRMLS_CC);
		o->parent = NULL;
	}
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

PHP_PROPRO_API zend_object_value php_property_proxy_object_new_ex(zend_class_entry *ce, php_property_proxy_t *proxy, php_property_proxy_object_t **ptr TSRMLS_DC)
{
	php_property_proxy_object_t *o;

	o = ecalloc(1, sizeof(*o));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (ptr) {
		*ptr = o;
	}
	o->proxy = proxy;

	o->zv.handle = zend_objects_store_put(o, NULL, php_property_proxy_object_free, NULL TSRMLS_CC);
	o->zv.handlers = &php_property_proxy_object_handlers;

	return o->zv;
}

static zval *get_proxied_value(zval *object TSRMLS_DC);
static zval *read_dimension(zval *object, zval *offset, int type TSRMLS_DC);
static STATUS cast_proxied_value(zval *object, zval *return_value, int type TSRMLS_DC);
static void write_dimension(zval *object, zval *offset, zval *value TSRMLS_DC);
static void set_proxied_value(zval **object, zval *value TSRMLS_DC);

static zval *get_proxied_value(zval *object TSRMLS_DC)
{
	zval **hash_value, *value = NULL;
	php_property_proxy_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);

	if (obj->proxy) {
		switch (Z_TYPE_P(obj->proxy->container)) {
		case IS_OBJECT:
			value = zend_read_property(Z_OBJCE_P(obj->proxy->container), obj->proxy->container, obj->proxy->member_str, obj->proxy->member_len, 0 TSRMLS_CC);
			break;

		case IS_ARRAY:
			if (SUCCESS == zend_hash_find(Z_ARRVAL_P(obj->proxy->container), obj->proxy->member_str, obj->proxy->member_len + 1, (void *) &hash_value)) {
				value = *hash_value;
			}
			break;
		}
	}

	return value;
}

static STATUS cast_proxied_value(zval *object, zval *return_value, int type TSRMLS_DC)
{
	zval *proxied_value;

	if ((proxied_value = get_proxied_value(object TSRMLS_CC))) {
		RETVAL_ZVAL(proxied_value, 1, 0);
		if (Z_TYPE_P(proxied_value) != type) {
			convert_to_explicit_type(return_value, type);
		}
		return SUCCESS;
	}

	return FAILURE;
}

static void set_proxied_value(zval **object, zval *value TSRMLS_DC)
{
	php_property_proxy_object_t *obj = zend_object_store_get_object(*object TSRMLS_CC);

	if (obj->proxy) {
		switch (Z_TYPE_P(obj->proxy->container)) {
		case IS_OBJECT:
			zend_update_property(Z_OBJCE_P(obj->proxy->container), obj->proxy->container, obj->proxy->member_str, obj->proxy->member_len, value TSRMLS_CC);
			break;

		case IS_ARRAY:
			Z_ADDREF_P(value);
			zend_symtable_update(Z_ARRVAL_P(obj->proxy->container), obj->proxy->member_str, obj->proxy->member_len + 1, (void *) &value, sizeof(zval *), NULL);
			break;
		}

		if (obj->parent) {
			zval *zparent;
			MAKE_STD_ZVAL(zparent);
			zparent->type = IS_OBJECT;
			zparent->value.obj = obj->parent->zv;
			set_proxied_value(&zparent, obj->proxy->container TSRMLS_CC);
		}
	}
}

static zval *read_dimension(zval *object, zval *offset, int type TSRMLS_DC)
{
	zval *value = NULL;
	zval *proxied_value;

	if ((proxied_value = get_proxied_value(object TSRMLS_CC))) {
		zval *o = offset;

		convert_to_string_ex(&o);

		if (Z_TYPE_P(proxied_value) == IS_ARRAY) {
			zval **hash_value;
			if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(proxied_value), Z_STRVAL_P(o), Z_STRLEN_P(o), (void *) &hash_value)) {
				Z_ADDREF_PP(hash_value);
				value = *hash_value;
			}
		}

		if (!value) {
			zval **hash_value;

			SEPARATE_ZVAL(&proxied_value);
			convert_to_array(proxied_value);

			MAKE_STD_ZVAL(value);
			ZVAL_NULL(value);
			zend_symtable_update(Z_ARRVAL_P(proxied_value), Z_STRVAL_P(o), Z_STRLEN_P(o) + 1, (void *) &value, sizeof(zval *), (void *) &hash_value);
			value = *hash_value;
			Z_SET_ISREF_P(value);

			set_proxied_value(&object, proxied_value TSRMLS_CC);
		}

		if (o != offset) {
			zval_ptr_dtor(&o);
		}
	}

	return value;
}

static void write_dimension(zval *object, zval *offset, zval *value TSRMLS_DC)
{
	zval *proxied_value;

	if ((proxied_value = get_proxied_value(object TSRMLS_CC))) {
		zval *o = offset;

		SEPARATE_ZVAL(&proxied_value);
		convert_to_array(proxied_value);

		if (Z_REFCOUNT_P(value) > 1) {
			SEPARATE_ZVAL(&value);
		}
		Z_ADDREF_P(value);

		if (o) {
			convert_to_string_ex(&o);
			zend_symtable_update(Z_ARRVAL_P(proxied_value), Z_STRVAL_P(o), Z_STRLEN_P(o) + 1, (void *) &value, sizeof(zval *), NULL);
		} else {
			zend_hash_next_index_insert(Z_ARRVAL_P(proxied_value), (void *) &value, sizeof(zval *), NULL);
		}

		if (o != offset) {
			zval_ptr_dtor(&o);
		}

		set_proxied_value(&object, proxied_value TSRMLS_CC);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_propro_construct, 0, 0, 2)
	ZEND_ARG_INFO(0, object)
	ZEND_ARG_INFO(0, member)
	ZEND_ARG_OBJ_INFO(0, parent, php\\PropertyProxy, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(propro, __construct) {
	zend_error_handling zeh;
	zval *container, *parent = NULL;
	char *member_str;
	int member_len;

	zend_replace_error_handling(EH_THROW, NULL, &zeh TSRMLS_CC);
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zs|O!", &container, &member_str, &member_len, &parent, php_property_proxy_class_entry)) {
		php_property_proxy_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		obj->proxy = php_property_proxy_init(container, member_str, member_len TSRMLS_CC);
		if (parent) {
			zend_objects_store_add_ref(parent TSRMLS_CC);
			obj->parent = zend_object_store_get_object(parent TSRMLS_CC);
		}
	}
	zend_restore_error_handling(&zeh TSRMLS_CC);
}

static const zend_function_entry php_property_proxy_method_entry[] = {
	PHP_ME(propro, __construct, ai_propro_construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	{0}
};

static PHP_MINIT_FUNCTION(propro)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "php", "PropertyProxy", php_property_proxy_method_entry);
	php_property_proxy_class_entry = zend_register_internal_class_ex(&ce, NULL, NULL TSRMLS_CC);
	php_property_proxy_class_entry->create_object = php_property_proxy_object_new;
	php_property_proxy_class_entry->ce_flags |= ZEND_ACC_FINAL_CLASS;

	memcpy(&php_property_proxy_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_property_proxy_object_handlers.set = set_proxied_value;
	php_property_proxy_object_handlers.get = get_proxied_value;
	php_property_proxy_object_handlers.cast_object = cast_proxied_value;
	php_property_proxy_object_handlers.read_dimension = read_dimension;
	php_property_proxy_object_handlers.write_dimension = write_dimension;

	return SUCCESS;
}

PHP_MINFO_FUNCTION(propro)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Property proxy support", "enabled");
	php_info_print_table_row(2, "Extension version", PHP_PROPRO_VERSION);
	php_info_print_table_end();
}

static const zend_function_entry propro_functions[] = {
	{0}
};

zend_module_entry propro_module_entry = {
	STANDARD_MODULE_HEADER,
	"propro",
	propro_functions,
	PHP_MINIT(propro),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(propro),
	PHP_PROPRO_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PROPRO
ZEND_GET_MODULE(propro)
#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
