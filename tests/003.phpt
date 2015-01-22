--TEST--
property proxy
--SKIPIF--
<?php 
extension_loaded("propro") || print "skip";
?>
--FILE--
<?php 
echo "Test\n";

class t {
	private $ref;
	function __get($v) {
		return new php\PropertyProxy($this, $v);
	} 
}

$t = new t;
$r = &$t->ref;
$r = 1;
var_dump($t);
$t->ref[] = 2;
var_dump($t);
?>
===DONE===
--EXPECTF--
Test
object(t)#%d (1) {
  ["ref":"t":private]=>
  int(1)
}
object(t)#%d (1) {
  ["ref":"t":private]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(2)
  }
}
===DONE===