package com.github.wuruxu.andjs.sample;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

import java.io.File;
import com.github.wuruxu.andjs.AndJS;
import com.google.firebase.FirebaseOptions;
import com.google.firebase.FirebaseApp;
import com.google.firebase.analytics.FirebaseAnalytics;

public class MainActivity extends AppCompatActivity {
	private static final String TAG = "AndJS";
	private MyObject obj;
	private FirebaseAnalytics mFirebaseAnalytics;
	private AndJS mJSInstance;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_andjs_sample);

		try {
			FirebaseOptions fOpt = FirebaseOptions.fromResource(this);
			FirebaseApp.initializeApp(this, fOpt, "[DEFAULT]");
		} catch(Exception e) {
			//e.printStackTrace();
		}
		mFirebaseAnalytics = FirebaseAnalytics.getInstance(this);
		Bundle bundle = new Bundle();
		bundle.putString("Object", "MainActivity");
		mFirebaseAnalytics.logEvent("onCreate", bundle);

		mJSInstance = new AndJS(this);
		findViewById(R.id.loadjs_button).setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View view) {
				Log.i(TAG, "LoadJS onClick"); 
				boolean is_samplejs = false;
				String jsbuf = "myobject.doLog('This is a JS string');";
				jsbuf += "var msg = myobject.getMessage(); adb.info(msg);";
				jsbuf += "var home = myobject.getMyHome(); home.printRect(0, 0, 512, 512);";
				jsbuf += "var homemsg = home.getMessage(); adb.info(homemsg);";

				File samplefile = new File("/data/local/tmp/sample.js");
				Bundle bundle = new Bundle();
				if(samplefile.exists() && samplefile.length() > 0) {
					is_samplejs = true;
					bundle.putString("LoadJS", "sample.js");
					mJSInstance.loadJSFile("/data/local/tmp/sample.js");
				} else {
					bundle.putString("LoadJS", "jsbuf");
					mJSInstance.loadJSBuf(jsbuf);
				}
				mFirebaseAnalytics.logEvent("onClick", bundle);
			}
		});
		obj = new MyObject();
		mJSInstance.injectObject(obj, "myobject", null);
    }
}
