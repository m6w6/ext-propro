# provide headers in builddir, so they do not end up in /usr/include/ext/propro/src

PHP_PROPRO_HEADERS := $(addprefix $(PHP_PROPRO_BUILDDIR)/,$(PHP_PROPRO_HEADERS))

$(PHP_PROPRO_BUILDDIR)/%.h: $(PHP_PROPRO_SRCDIR)/src/%.h
	@cat >$@ <$<

all: propro-build-headers
clean: propro-clean-headers

.PHONY: propro-build-headers
propro-build-headers: $(PHP_PROPRO_HEADERS)

.PHONY: propro-clean-headers
propro-clean-headers:
	-rm -f $(PHP_PROPRO_HEADERS)
