--TEST--
property proxy
--SKIPIF--
<?php if (!extension_loaded("propro")) print "skip"; ?>
--FILE--
<?php

echo "Test\n";

class c {
	private $prop;
	private $anon;
	function __get($p) {
		return new php\PropertyProxy($this, $p);
	}
}

$c = new c;

$p = $c->prop;
$a = $c->anon;

var_dump($c);

echo "set\n";
$a = 123; 
echo "get\n";
echo $a,"\n";

//echo "foo\n";
$p["foo"] = 123;
//echo "bar.baz.a.b\n";
$p["bar"]["baz"]["a"]["b"]=987;

var_dump($c);

?>
DONE
--EXPECTF--
Test
object(c)#%d (2) {
  ["prop":"c":private]=>
  NULL
  ["anon":"c":private]=>
  NULL
}
set
get
123
object(c)#%d (2) {
  ["prop":"c":private]=>
  array(2) {
    ["foo"]=>
    int(123)
    ["bar"]=>
    array(1) {
      ["baz"]=>
      array(1) {
        ["a"]=>
        array(1) {
          ["b"]=>
          int(987)
        }
      }
    }
  }
  ["anon":"c":private]=>
  int(123)
}
DONE
