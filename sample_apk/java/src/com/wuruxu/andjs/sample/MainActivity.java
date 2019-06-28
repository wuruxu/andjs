package com.wuruxu.andjs.sample;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import com.wuruxu.andjs.R;
import android.util.Log;
import android.view.View;
import org.chromium.base.ContextUtils;

import com.wuruxu.andjs.AndJS;

public class MainActivity extends AppCompatActivity {
	private static final String TAG = "AndJS";
	private MyObject obj;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_andjs_sample);

		AndJS.Init(this);
		findViewById(R.id.loadjs_button).setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View view) {
				Log.i(TAG, "LoadJS onClick"); 
				String jsbuf = "myobject.doLog('This is a JS string');";
				jsbuf += "var msg = myobject.getMessage(); adb.info(msg);";
				jsbuf += "var home = myobject.getMyHome(); home.printRect(0, 0, 512, 512);";
				jsbuf += "var homemsg = home.getMessage(); adb.info(homemsg);";
				AndJS.loadJSBuf(jsbuf);
			}
		});
		obj = new MyObject();
		AndJS.injectObject(obj, "myobject", null);
    }
}
