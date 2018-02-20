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

#include "php_propro_api.h"

#define DEBUG_PROPRO 0

static inline php_property_proxy_object_t *get_propro(zval *object);
static inline zval *get_proxied_value(zval *object, zval *return_value);
static inline void set_proxied_value(zval *object, zval *value);

static zval *read_dimension(zval *object, zval *offset, int type, zval *return_value);
static ZEND_RESULT_CODE cast_obj(zval *object, zval *return_value, int type);
static void write_dimension(zval *object, zval *offset, zval *input_value);

#if DEBUG_PROPRO
/* we do not really care about TS when debugging */
static int level = 1;
static const char *inoutstr[] = {"< return","=       "," > enter "};
static const char *types[] = {
		"UNDEF",
		"NULL",
		"FALSE",
		"TRUE",
		"int",
		"float",
		"string",
		"Array",
		"Object",
		"resource",
		"reference",
		"constant",
		"const AST",
		"_BOOL",
		"callable",
		"indirect",
		"---",
		"pointer"
};

static const char *_type(zval *zv)
{
	switch (Z_TYPE_P(zv)) {
	case IS_OBJECT:
		if (zv->value.obj->ce == php_property_proxy_get_class_entry()) {
			return "PROPRO";
		}
		/* no break */
	default:
		return types[Z_TYPE_P(zv)];
	}
}

static int _walk(php_property_proxy_object_t *obj)
{
	int p = 0;

	if (obj) {
		if (!Z_ISUNDEF(obj->parent)) {
			p += _walk(get_propro(&obj->parent));
		}
		if (obj->proxy) {
			p += fprintf(stderr, ".%s", obj->proxy->member->val);
		}
	}

	return p;
}

static inline zval *get_container(zval *object, zval *tmp);
static inline zval *get_container_value(zval *container, zend_string *member, zval *return_value);

static void debug_propro(int inout, const char *f,
		php_property_proxy_object_t *obj,
		php_property_proxy_t *proxy,
		zval *offset, zval *value)
{
	int width;
	zval tmp, *container = &tmp;

	if (!proxy && obj) {
		proxy = obj->proxy;
	}

	fprintf(stderr, "#PP %14p %*c %s %s\t", proxy, level, ' ', inoutstr[inout + 1], f);

	level += inout;

	if (proxy) {
		ZVAL_UNDEF(&tmp);
		if (obj) {
			if (Z_ISUNDEF(obj->parent)) {
				container = &obj->proxy->container;
			} else {
				zval parent_tmp, *parent_container;
				php_property_proxy_object_t *parent_obj = get_propro(&obj->parent);

				ZVAL_UNDEF(&parent_tmp);
				parent_container = get_container(&obj->parent, &parent_tmp);
				container = get_container_value(parent_container, parent_obj->proxy->member, &tmp);
			}
		}
		fprintf(stderr, " container= %-14p < %-10s rc=%-11d ",
				Z_REFCOUNTED_P(container) ? Z_COUNTED_P(container) : NULL,
				_type(container),
				Z_REFCOUNTED_P(container) ? Z_REFCOUNT_P(container) : 0);
		if (Z_ISREF_P(container)) {
			zval *ref = Z_REFVAL_P(container);
			fprintf(stderr, " %-12s %p rc=% 2d",
					_type(ref),
					ref->value.counted,
					Z_REFCOUNTED_P(ref) ? Z_REFCOUNT_P(ref) : -1);
		}
		fprintf(stderr, "> ");
	}

	width = _walk(obj);

	if (*f++=='d'
	&&	*f++=='i'
	&&	*f++=='m'
	) {
		char *offset_str = "[]";
		zval *o = offset;

		if (o) {
			convert_to_string_ex(o);
			offset_str = Z_STRVAL_P(o);
		}

		width += fprintf(stderr, ".%s", offset_str);

		if (o && o != offset) {
			zval_ptr_dtor(o);
		}
	}
	if (value) {
		fprintf(stderr, "%.*s", 32-width, "                                ");
		if (Z_ISUNDEF_P(value)) {
			fprintf(stderr, " = UNDEF");
		} else {
			fprintf(stderr, " = %-14p < %-10s rc=%-11d %3s> ",
					Z_REFCOUNTED_P(value) ? Z_COUNTED_P(value) : NULL,
					_type(value),
					Z_REFCOUNTED_P(value) ? Z_REFCOUNT_P(value) : 0,
					Z_ISREF_P(value) ? "ref" : "");
			if (!Z_ISUNDEF_P(value) && Z_TYPE_P(value) != IS_INDIRECT) {
				zend_print_flat_zval_r(value TSRMLS_CC);
			}
		}
	}

	fprintf(stderr, "\n");
}
#else
#define debug_propro(l, f, obj, proxy, off, val)
#endif

php_property_proxy_t *php_property_proxy_init(zval *container, zend_string *member)
{
	php_property_proxy_t *proxy = ecalloc(1, sizeof(*proxy));
#if DEBUG_PROPRO
	zval offset;

	ZVAL_STR_COPY(&offset, member);
#endif

	debug_propro(1, "init", NULL, proxy, &offset, NULL);

	if (container) {
		ZVAL_COPY(&proxy->container, container);
	}
	proxy->member = zend_string_copy(member);

	debug_propro(-1, "init", NULL, proxy, &offset, NULL);

#if DEBUG_PROPRO
	zval_dtor(&offset);
#endif

	return proxy;
}

void php_property_proxy_free(php_property_proxy_t **proxy)
{
#if DEBUG_PROPRO
	zval offset;

	ZVAL_STR_COPY(&offset, (*proxy)->member);
	debug_propro(1, "dtor", NULL, *proxy, &offset, NULL);
#endif

	if (*proxy) {
		zval_ptr_dtor(&(*proxy)->container);

		ZVAL_UNDEF(&(*proxy)->container);
		zend_string_release((*proxy)->member);
		(*proxy)->member = NULL;
		efree(*proxy);
		*proxy = NULL;
	}

#if DEBUG_PROPRO
	debug_propro(-1, "dtor", NULL, NULL, &offset, NULL);

	zval_dtor(&offset);
#endif
}

static zend_class_entry *php_property_proxy_class_entry;
static zend_object_handlers php_property_proxy_object_handlers;

zend_class_entry *php_property_proxy_get_class_entry(void)
{
	return php_property_proxy_class_entry;
}

php_property_proxy_object_t *php_property_proxy_object_new_ex(
		zend_class_entry *ce, php_property_proxy_t *proxy)
{
	php_property_proxy_object_t *o;

	if (!ce) {
		ce = php_property_proxy_class_entry;
	}

	o = ecalloc(1, sizeof(*o) + sizeof(zval) * (ce->default_properties_count - 1));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);

	o->proxy = proxy;
	o->zo.handlers = &php_property_proxy_object_handlers;

	return o;
}

zend_object *php_property_proxy_object_new(zend_class_entry *ce)
{
	return &php_property_proxy_object_new_ex(ce, NULL)->zo;
}

static void destroy_obj(zend_object *object)
{
	php_property_proxy_object_t *o = PHP_PROPRO_PTR(object);

	if (o->proxy) {
		php_property_proxy_free(&o->proxy);
	}
	if (!Z_ISUNDEF(o->parent)) {
		zval_ptr_dtor(&o->parent);
		ZVAL_UNDEF(&o->parent);
	}
	zend_object_std_dtor(object);
}

static ZEND_RESULT_CODE cast_obj(zval *object, zval *return_value, int type)
{
	zval tmp;

	ZVAL_UNDEF(&tmp);
	RETVAL_ZVAL(get_proxied_value(object, &tmp), 1, 0);

	debug_propro(0, "cast", get_propro(object), NULL, NULL, return_value);

	if (!Z_ISUNDEF_P(return_value)) {
		ZVAL_DEREF(return_value);
		convert_to_explicit_type_ex(return_value, type);
		return SUCCESS;
	}

	return FAILURE;
}

static zval *get_obj(zval *object, zval *return_value)
{
	zval tmp;

	ZVAL_UNDEF(&tmp);
	RETVAL_ZVAL(get_proxied_value(object, &tmp), 1, 0);
	return return_value;
}

static void set_obj(zval *object, zval *value) {
	set_proxied_value(object, value);
}

static inline php_property_proxy_object_t *get_propro(zval *object)
{
	ZEND_ASSERT(Z_TYPE_P(object) == IS_OBJECT);
	return PHP_PROPRO_PTR(Z_OBJ_P(object));
}

static inline zval *get_container(zval *object, zval *tmp)
{
	php_property_proxy_object_t *obj = get_propro(object);
	zval *container;

	if (Z_ISUNDEF(obj->parent)) {
		container = &obj->proxy->container;
	} else {
		container = get_proxied_value(&obj->parent, tmp);
	}

	return container;
}

static inline void set_container(zval *object, zval *container)
{
	php_property_proxy_object_t *obj = get_propro(object);

	if (Z_ISUNDEF(obj->parent)) {
		zval tmp;

		ZVAL_COPY_VALUE(&tmp, &obj->proxy->container);
		ZVAL_COPY(&obj->proxy->container, container);
		zval_ptr_dtor(&tmp);
	} else {
		set_proxied_value(&obj->parent, container);
	}
}

static inline zval *get_container_value(zval *container, zend_string *member, zval *return_value)
{
	zval *found_value = NULL, prop_tmp;

	ZVAL_DEREF(container);
	switch (Z_TYPE_P(container)) {
	case IS_OBJECT:
		found_value = zend_read_property(Z_OBJCE_P(container), container,
				member->val, member->len, 0, &prop_tmp);

		break;

	case IS_ARRAY:
		found_value = zend_symtable_find(Z_ARRVAL_P(container), member);
		break;
	}

	if (found_value) {
		RETVAL_ZVAL(found_value, 0, 0);
	}

	return return_value;
}

static inline void cleanup_container(zval *object, zval *container, zend_bool separated)
{
	if (separated) {
		zval_ptr_dtor(container);
	}
}

static inline zend_bool separate_container(zval *object, zval *container)
{
	switch (Z_TYPE_P(container)) {
	case IS_OBJECT:
		return 0;

	case IS_ARRAY:
		/* always duplicate for PHP-7.0 and 7.1 on travis */
		ZVAL_ARR(container, zend_array_dup(Z_ARRVAL_P(container)));
		break;

	case IS_UNDEF:
		array_init(container);
		break;

	default:
		SEPARATE_ZVAL(container);
		Z_TRY_ADDREF_P(container);
		convert_to_array(container);
		break;
	}

	return 1;
}

static inline zval *set_container_value(zval *container, zend_string *member, zval *value)
{
	ZVAL_DEREF(container);
	switch (Z_TYPE_P(container)) {
	case IS_OBJECT:
		zend_update_property(Z_OBJCE_P(container), container,
				member->val, member->len, value);
		break;

	case IS_ARRAY:
		Z_TRY_ADDREF_P(value);
		if (member) {
			value = zend_symtable_update(Z_ARRVAL_P(container), member, value);
		} else {
			value = zend_hash_next_index_insert(Z_ARRVAL_P(container), value);
		}
		break;

	default:
		ZEND_ASSERT(0);
		break;
	}

	return value;
}


static zval *get_proxied_value(zval *object, zval *return_value)
{
	php_property_proxy_object_t *obj = get_propro(object);

	debug_propro(1, "get", obj, NULL, NULL, NULL);

	if (obj->proxy) {
		zval tmp, *container;

		ZVAL_UNDEF(&tmp);
		container = get_container(object, &tmp);

		return_value = get_container_value(container, obj->proxy->member, return_value);
	}

	debug_propro(-1, "get", obj, NULL, NULL, return_value);

	return return_value;
}

static void set_proxied_value(zval *object, zval *value)
{
	php_property_proxy_object_t *obj = get_propro(object);

	debug_propro(1, "set", obj, NULL, NULL, value);

	if (obj->proxy) {
		zval tmp, *container;
		zend_bool separated;

		Z_TRY_ADDREF_P(value);

		ZVAL_UNDEF(&tmp);
		container = get_container(object, &tmp);
		separated = separate_container(object, container);
		set_container_value(container, obj->proxy->member, value);
		set_container(object, container);
		cleanup_container(object, container, separated);

		Z_TRY_DELREF_P(value);

		debug_propro(0, "set", obj, NULL, NULL, value);
	}

	debug_propro(-1, "set", obj, NULL, NULL, value);
}

static zval *read_dimension(zval *object, zval *offset, int type, zval *return_value)
{
	zval *value, tmp;
	zend_string *member = offset ? zval_get_string(offset) : NULL;

	debug_propro(1, type == BP_VAR_R ? "dim_r" : "dim_R",
			get_propro(object), NULL, offset, NULL);

	ZVAL_UNDEF(&tmp);
	value = get_proxied_value(object, &tmp);

	if (type == BP_VAR_R || type == BP_VAR_IS) {
		ZEND_ASSERT(member);

		if (!Z_ISUNDEF_P(value)) {
			zval tmp;

			ZVAL_UNDEF(&tmp);
			RETVAL_ZVAL(get_container_value(value, member, &tmp), 1, 0);
		}
	} else {
		php_property_proxy_t *proxy;
		php_property_proxy_object_t *proxy_obj;

		if (Z_ISUNDEF_P(value)) {
			ZVAL_NULL(value);
		}

		if (!member) {
			if (Z_TYPE_P(value) == IS_ARRAY) {
				member = zend_long_to_str(zend_hash_next_free_element(
					Z_ARRVAL_P(value)));
			} else if (Z_TYPE_P(value) != IS_OBJECT){
				member = zend_long_to_str(0);
			}
		}

		proxy = php_property_proxy_init(NULL, member);
		proxy_obj = php_property_proxy_object_new_ex(NULL, proxy);
		ZVAL_COPY(&proxy_obj->parent, object);
		RETVAL_OBJ(&proxy_obj->zo);

		debug_propro(0, "dim_R pp", get_propro(object), NULL, offset, return_value);
	}

	if (member) {
		zend_string_release(member);
	}

	debug_propro(-1, type == BP_VAR_R ? "dim_r" : "dim_R",
			get_propro(object), NULL, offset, return_value);

	return return_value;
}

static int has_dimension(zval *object, zval *offset, int check_empty)
{
	zval *value, tmp;
	int exists = 0;

	debug_propro(1, "dim_e", get_propro(object), NULL, offset, NULL);

	ZVAL_UNDEF(&tmp);
	value = get_proxied_value(object, &tmp);

	if (!Z_ISUNDEF_P(value)) {
		zend_string *zs = zval_get_string(offset);

		ZVAL_DEREF(value);
		if (Z_TYPE_P(value) == IS_ARRAY) {
			zval *zentry = zend_symtable_find(Z_ARRVAL_P(value), zs);

			if (zentry) {
				if (check_empty) {
					exists = !Z_ISNULL_P(zentry);
				} else {
					exists = 1;
				}
			}
		}

		zend_string_release(zs);
	}

	debug_propro(-1, "dim_e", get_propro(object), NULL, offset, NULL);

	return exists;
}

static void write_dimension(zval *object, zval *offset, zval *input_value)
{
	zval *array, tmp;
	zend_string *zs = NULL;
	zend_bool separated;

	debug_propro(1, "dim_w", get_propro(object), NULL, offset, input_value);

	if (offset) {
		zs = zval_get_string(offset);
	}

	ZVAL_UNDEF(&tmp);
	array = get_proxied_value(object, &tmp);
	separated = separate_container(object, array);
	set_container_value(array, zs, input_value);
	set_proxied_value(object, array);
	cleanup_container(object, array, separated);

	if (zs) {
		zend_string_release(zs);
	}

	debug_propro(-1, "dim_w", get_propro(object), NULL, offset, input_value);
}

static void unset_dimension(zval *object, zval *offset)
{
	zval *array, *value, tmp;

	debug_propro(1, "dim_u", get_propro(object), NULL, offset, NULL);

	ZVAL_UNDEF(&tmp);
	value = get_proxied_value(object, &tmp);
	array = value;
	ZVAL_DEREF(array);

	if (Z_TYPE_P(array) == IS_ARRAY) {
		zend_string *o = zval_get_string(offset);

		SEPARATE_ARRAY(array);
		zend_symtable_del(Z_ARRVAL_P(array), o);

		set_proxied_value(object, value);

		zend_string_release(o);
	}

	debug_propro(-1, "dim_u", get_propro(object), NULL, offset, NULL);
}

ZEND_BEGIN_ARG_INFO_EX(ai_propro_construct, 0, 0, 2)
	ZEND_ARG_INFO(0, object)
	ZEND_ARG_INFO(0, member)
	ZEND_ARG_OBJ_INFO(0, parent, php\\PropertyProxy, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(propro, __construct) {
	zend_error_handling zeh;
	zval *reference, *parent = NULL;
	zend_string *member;

	zend_replace_error_handling(EH_THROW, NULL, &zeh);
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "o!S|O!",
			&reference, &member, &parent,
			php_property_proxy_class_entry)) {
		php_property_proxy_object_t *obj;

		obj = get_propro(getThis());

		if (parent) {
			ZVAL_COPY(&obj->parent, parent);
			obj->proxy = php_property_proxy_init(NULL, member);
		} else if (reference) {
			zval *container = reference;

			obj->proxy = php_property_proxy_init(container, member);
		} else {
			php_error(E_WARNING, "Either object or parent must be set");
		}
	}
	zend_restore_error_handling(&zeh);
}

static const zend_function_entry php_property_proxy_method_entry[] = {
	PHP_ME(propro, __construct, ai_propro_construct, ZEND_ACC_PUBLIC)
	{0}
};

static PHP_MINIT_FUNCTION(propro)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "php", "PropertyProxy",
			php_property_proxy_method_entry);
	php_property_proxy_class_entry = zend_register_internal_class(&ce);
	php_property_proxy_class_entry->create_object =	php_property_proxy_object_new;
	php_property_proxy_class_entry->ce_flags |= ZEND_ACC_FINAL;

	memcpy(&php_property_proxy_object_handlers, zend_get_std_object_handlers(),
			sizeof(zend_object_handlers));
	php_property_proxy_object_handlers.offset = XtOffsetOf(php_property_proxy_object_t, zo);
	php_property_proxy_object_handlers.free_obj = destroy_obj;
	php_property_proxy_object_handlers.set = set_obj;
	php_property_proxy_object_handlers.get = get_obj;
	php_property_proxy_object_handlers.cast_object = cast_obj;
	php_property_proxy_object_handlers.read_dimension = read_dimension;
	php_property_proxy_object_handlers.write_dimension = write_dimension;
	php_property_proxy_object_handlers.has_dimension = has_dimension;
	php_property_proxy_object_handlers.unset_dimension = unset_dimension;

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
