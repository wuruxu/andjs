# andjs
v8 js engine for android
* How to integrate andjs         
  *  1: add andjs aar into your project
  *  2: **new AndJS(context)** to create AndJS Instance
  *  3: **@CalledByJavascript** to annotation your java method, which will be called in javascript
  *  4: **mJSInstance.injectObject** to inject java object
  *  5: **mJSInstance.loadJSBuf(String jsbuf)** to Run javascript in V8
* Sample code 
```java
import com.github.wuruxu.andjs.AndJS;
public class MainActivity extends AppCompatActivity {
    private AndJS mJSInstance;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
      mJSInstance = new AndJS(this); //create AndJS Instance
      
      obj = new MyObject();
      //Inject java object into v8 engine as 'myobject'
      mJSInstance.injectObject(obj, "myobject", null);
      
      //Inject this Activity object into v8 engine as 'myactivity'
      mJSInstance.injectObject(this, "myactivity", null);
      
      String jsbuf = "myobject.doLog('This is a JS string');";
			jsbuf += "var msg = myobject.getMessage(); adb.info(msg);";
			jsbuf += "var home = myobject.getMyHome(); home.printRect(0, 0, 512, 512);";
			jsbuf += "var homemsg = home.getMessage(); adb.info(homemsg);";
			jsbuf += "var version = get_v8_version(); myactivity.updateTitle(version);";
        
      mJSInstance.loadJSBuf(jsbuf); // Run Javascript in V8 engine
    }
    
    @CalledByJavascript
    void updateTitle(String version) {
    	String str = getTitle().toString();
	str += "@v8(" + version+")";
	setTitle(str);
    }
}

//sample code of MyObject
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

//sample code of MyHome
import com.github.wuruxu.andjs.CalledByJavascript;

public class MyHome extends Object {
	private static final String TAG = "MyHome";

	public MyHome() {}

	@CalledByJavascript
	public void printRect(int x0, int y0, int x1, int y1) {
		Log.i(TAG, " * x0 " + x0 + " y0 " + y0 + " x1 " + x1 + " y1 " + y1);
	}

	@CalledByJavascript
	public String getMessage() {
		return "This is a java string from MyHome";
	}
}
```
