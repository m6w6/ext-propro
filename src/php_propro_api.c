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
		"constant AST",
		"_BOOL",
		"callable",
		"indirect",
		"---",
		"pointer"
};

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

static void debug_propro(int inout, const char *f,
		php_property_proxy_object_t *obj,
		php_property_proxy_t *proxy,
		zval *offset, zval *value)
{
	int width;

	if (!proxy && obj) {
		proxy = obj->proxy;
	}

	fprintf(stderr, "#PP %14p %*c %s %s\t", proxy, level, ' ', inoutstr[inout + 1], f);

	level += inout;

	if (proxy) {
		fprintf(stderr, " container= %-10p <%12s rc=%d ",
				Z_REFCOUNTED(proxy->container) ? Z_COUNTED(proxy->container) : NULL,
				types[Z_TYPE(proxy->container)],
				Z_REFCOUNTED(proxy->container) ? Z_REFCOUNT(proxy->container) : 0);
		if (Z_ISREF(proxy->container)) {
			zval *ref = Z_REFVAL(proxy->container);
			fprintf(stderr, " %-12s %p rc=% 2d",
					types[Z_TYPE_P(ref)],
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
			fprintf(stderr, " =UNDEF                              ");
		} else {
			fprintf(stderr, " = %-10p <%12s rc=%d %3s> ",
					Z_REFCOUNTED_P(value) ? Z_COUNTED_P(value) : NULL,
					types[Z_TYPE_P(value)&0xf],
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

	ZVAL_COPY(&proxy->container, container);
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

static inline zval *get_container(zval *object)
{
	php_property_proxy_object_t *obj = get_propro(object);

	if (!Z_ISUNDEF(obj->parent)) {
		zval *parent_value, tmp;

		ZVAL_UNDEF(&tmp);
		parent_value = get_proxied_value(&obj->parent, &tmp);
		ZVAL_DEREF(parent_value);
		switch (Z_TYPE_P(parent_value)) {
		case IS_OBJECT:
		case IS_ARRAY:
			zend_assign_to_variable(&obj->proxy->container, parent_value, IS_CV);
			break;
		default:
			break;
		}
	}

	return &obj->proxy->container;
}

static inline void set_container(zval *object, zval *container)
{
	php_property_proxy_object_t *obj = get_propro(object);
	zend_assign_to_variable(&obj->proxy->container, container, IS_CV);

	if (!Z_ISUNDEF(obj->parent)) {
		set_proxied_value(&obj->parent, &obj->proxy->container);
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

static inline void set_container_value(zval *container, zend_string *member, zval *value)
{
	ZVAL_DEREF(container);
	switch (Z_TYPE_P(container)) {
	case IS_OBJECT:
		zend_update_property(Z_OBJCE_P(container), container,
				member->val, member->len, value);
		break;

	case IS_UNDEF:
		array_init(container);
		/* no break */
	default:
		SEPARATE_ZVAL(container);
		convert_to_array(container);
		/* no break */
	case IS_ARRAY:
		SEPARATE_ARRAY(container);
		Z_TRY_ADDREF_P(value);
		if (member) {
			zend_symtable_update(Z_ARRVAL_P(container), member, value);
		} else {
			zend_hash_next_index_insert(Z_ARRVAL_P(container), value);
		}
		break;
	}
}


static zval *get_proxied_value(zval *object, zval *return_value)
{
	php_property_proxy_object_t *obj = get_propro(object);

	debug_propro(1, "get", obj, NULL, NULL, NULL);

	if (obj->proxy) {
		zval *container = get_container(object);

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
		zval *container = get_container(object);

		set_container_value(container, obj->proxy->member, value);
		set_container(object, container);

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

	if (type == BP_VAR_R) {
		ZEND_ASSERT(member);

		if (!Z_ISUNDEF_P(value)) {
			zval tmp;

			ZVAL_UNDEF(&tmp);
			RETVAL_ZVAL(get_container_value(value, member, &tmp), 1, 0);
		}
	} else {
		php_property_proxy_t *proxy;
		php_property_proxy_object_t *proxy_obj;
		zval *array = value;
		zend_bool created = Z_ISUNDEF_P(value);

		ZVAL_DEREF(array);
		if (Z_REFCOUNTED_P(array) && Z_REFCOUNT_P(array) > 1) {
			created = 1;
			SEPARATE_ZVAL_NOREF(array);
		}
		if (Z_TYPE_P(array) != IS_ARRAY) {
			created = 1;
			if (Z_ISUNDEF_P(array)) {
				array_init(array);
			} else {
				convert_to_array(array);
			}
		}

		if (!member) {
			member = zend_long_to_str(zend_hash_next_free_element(
					Z_ARRVAL_P(array)));
		}

		proxy = php_property_proxy_init(value, member);
		proxy_obj = php_property_proxy_object_new_ex(NULL, proxy);
		ZVAL_COPY(&proxy_obj->parent, object);
		RETVAL_OBJ(&proxy_obj->zo);

		if (created) {
			Z_DELREF_P(value);
		}

		debug_propro(0, created ? "dim_R pp C" : "dim_R pp", get_propro(object), NULL, offset, return_value);
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

	exists = 0;
	if (!Z_ISUNDEF_P(value)) {
		zend_string *zs = zval_get_string(offset);

		ZVAL_DEREF(value);
		if (Z_TYPE_P(value) == IS_ARRAY) {
			zval *zentry = zend_symtable_find(Z_ARRVAL_P(value), zs);

			if (!zentry) {
				exists = 0;
			} else {
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

	debug_propro(1, "dim_w", get_propro(object), NULL, offset, input_value);

	ZVAL_UNDEF(&tmp);
	array = get_proxied_value(object, &tmp);

	if (offset) {
		zs = zval_get_string(offset);
	}
	set_container_value(array, zs, input_value);
	if (zs) {
		zend_string_release(zs);
	}

	set_proxied_value(object, array);

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
	ZEND_ARG_INFO(1, object)
	ZEND_ARG_INFO(0, member)
	ZEND_ARG_OBJ_INFO(0, parent, php\\PropertyProxy, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(propro, __construct) {
	zend_error_handling zeh;
	zval *reference, *container, *parent = NULL;
	zend_string *member;

	zend_replace_error_handling(EH_THROW, NULL, &zeh);
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "zS|O!",
			&reference, &member, &parent,
			php_property_proxy_class_entry)) {
		php_property_proxy_object_t *obj;

		container = reference;
		ZVAL_DEREF(container);
		switch (Z_TYPE_P(container)) {
		case IS_ARRAY:
			SEPARATE_ARRAY(container);
			break;
		case IS_OBJECT:
			break;
		default:
			SEPARATE_ZVAL(container);
			convert_to_array(container);
			break;
		}

		obj = get_propro(getThis());
		obj->proxy = php_property_proxy_init(reference, member);

		if (parent) {
			ZVAL_COPY(&obj->parent, parent);
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
