PHP_ARG_ENABLE(propro, whether to enable property proxy support,
[  --enable-propro           Enable property proxy support])

if test "$PHP_PROPRO" != "no"; then
	PHP_INSTALL_HEADERS(ext/propro, php_propro.h)
	PHP_NEW_EXTENSION(propro, php_propro.c, $ext_shared)
fi
