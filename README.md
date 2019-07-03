# andjs
v8 js engine for android
* How to integrate andjs         
  *  add andjs aar into your project
```java
import com.github.wuruxu.andjs.AndJS;
public class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
      AndJS.Init(this);  //AndJS Initialize
      
      obj = new MyObject();
      AndJS.injectObject(obj, "myobject", null);  //Inject Java Object into javascript
      
      String jsbuf = "myobject.doLog('This is a JS string');";
			jsbuf += "var msg = myobject.getMessage(); adb.info(msg);";
			jsbuf += "var home = myobject.getMyHome(); home.printRect(0, 0, 512, 512);";
			jsbuf += "var homemsg = home.getMessage(); adb.info(homemsg);";
        
      AndJS.loadJSBuf(jsbuf); // Run Javascript in V8 engine
    }
}

//sample code of MyObject
package com.github.wuruxu.andjs.sample;

import android.util.Log;
import com.github.wuruxu.andjs.CalledByJavascript;

public class MyObject extends Object {
	private static final String TAG = "MyObject";
	private MyHome home;

	public MyObject() {
		home = new MyHome();
	}

	@CalledByJavascript
	public void doLog(String msg) {
		Log.i(TAG, " * " + msg);
	}

	@CalledByJavascript
	public String getMessage() {
		return "This is a java string";
	}

	@CalledByJavascript
	public MyHome getMyHome() {
		return home;
	}
}
```
