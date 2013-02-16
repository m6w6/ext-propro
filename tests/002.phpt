--TEST--
property proxy
--SKIPIF--
<?php if (!extension_loaded("propro")) print "skip"; ?>
--FILE--
<?php

echo "Test\n";

class c {
	private $storage = array();
	function __get($p) {
		return new php\PropertyProxy($this->storage, $p);
	}
	function __set($p, $v) {
		$this->storage[$p] = $v;
	}
}

$c = new c;
$c->data["foo"] = 1;
var_dump(
	isset($c->data["foo"]),
	isset($c->data["bar"])
);

var_dump($c);

$c->data[] = 1;
$c->data[] = 2;
$c->data[] = 3;
$c->data["bar"][] = 123;
$c->data["bar"][] = 456;

var_dump($c);
unset($c->data["bar"][0]);

var_dump($c);

?>
DONE
--EXPECTF--
Test
bool(true)
bool(false)
object(c)#%d (1) {
  ["storage":"c":private]=>
  array(1) {
    ["data"]=>
    array(1) {
      ["foo"]=>
      int(1)
    }
  }
}
object(c)#%d (1) {
  ["storage":"c":private]=>
  array(1) {
    ["data"]=>
    array(5) {
      ["foo"]=>
      int(1)
      [0]=>
      int(1)
      [1]=>
      int(2)
      [2]=>
      int(3)
      ["bar"]=>
      array(2) {
        [0]=>
        int(123)
        [1]=>
        int(456)
      }
    }
  }
}
object(c)#%d (1) {
  ["storage":"c":private]=>
  array(1) {
    ["data"]=>
    array(5) {
      ["foo"]=>
      int(1)
      [0]=>
      int(1)
      [1]=>
      int(2)
      [2]=>
      int(3)
      ["bar"]=>
      array(1) {
        [1]=>
        int(456)
      }
    }
  }
}
DONE
