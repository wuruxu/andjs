adb.info("this is a ADB message", " QuickJS, It's great");

var c1 = JSCrypto.key("mykey");
//var c1 = JSCrypto("myKey");
var msg = c1.seal("This is a JS Message");
adb.info("after seal:", msg);
var text = c1.open(msg);
adb.info("after open:", text);

var c2 = getJSCrypto("myTestKey");
var msg = c2.seal("myTestKey: This a JS Message ");
var text = c2.open(msg);
adb.info("after open:", text);

myobject.doLog("This is a JS String");
var msg = myobject.getMessage();
adb.info(msg);
var home = myobject.getMyHome();
home.printRect(0, 0, 512, 512);
