# ext-propro

[![Build Status](https://travis-ci.org/m6w6/ext-propro.svg?branch=master)](https://travis-ci.org/m6w6/ext-propro)

The "Property Proxy" extension provides a fairly transparent proxy for internal
object properties hidden in custom non-zval implementations.

## Documentation

See the [online markdown reference](https://mdref.m6w6.name/propro).

Known issues are listed in [BUGS](./BUGS) and future ideas can be found in [TODO](./TODO).

## Installing

### PECL

	pecl install propro

### PHARext

Watch out for [PECL replicates](https://replicator.pharext.org?propro)
and pharext packages attached to [releases](https://github.com/m6w6/ext-propro/releases).

### Checkout

	git clone github.com:m6w6/ext-propro
	cd ext-propro
	/path/to/phpize
	./configure --with-php-config=/path/to/php-config
	make
	sudo make install

## ChangeLog

A comprehensive list of changes can be obtained from the
[PECL website](https://pecl.php.net/package-changelog.php?package=propro).

## License

ext-propro is licensed under the 2-Clause-BSD license, which can be found in
the accompanying [LICENSE](./LICENSE) file.

## Contributing

All forms of contribution are welcome! Please see the bundled
[CONTRIBUTING](./CONTRIBUTING.md) note for the general principles followed.

The list of past and current contributors is maintained in [THANKS](./THANKS).
