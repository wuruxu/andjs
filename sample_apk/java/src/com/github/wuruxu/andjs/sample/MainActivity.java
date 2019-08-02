package com.github.wuruxu.andjs.sample;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

import java.io.File;
import com.github.wuruxu.andjs.AndJS;
import com.github.wuruxu.andjs.CalledByJavascript;
import com.google.firebase.FirebaseOptions;
import com.google.firebase.FirebaseApp;
import com.google.firebase.analytics.FirebaseAnalytics;

public class MainActivity extends AppCompatActivity {
	private static final String TAG = "AndJS";
	private MyObject obj;
	private FirebaseAnalytics mFirebaseAnalytics;
	private AndJS mJSInstance;
	private boolean mTitleIsUpdated;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_andjs_sample);

		mTitleIsUpdated = false;
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

		findViewById(R.id.loadjs_button).setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View view) {
				Log.i(TAG, "LoadJS onClick"); 
				boolean is_samplejs = false;
				String jsbuf = "myobject.doLog('This is a JS string');";
				jsbuf += "var msg = myobject.getMessage(); adb.info(msg);";
				jsbuf += "var home = myobject.getMyHome(); home.printRect(0, 0, 512, 512);";
				jsbuf += "var homemsg = home.getMessage(); adb.info(homemsg);";
				jsbuf += "var version = get_v8_version(); myactivity.updateTitle(version);";
				jsbuf += "var demostring = 'This is jscrypto demo string'; jscrypto.setkey('mykey');";
				jsbuf += "var encrypt = jscrypto.seal(demostring); var output = jscrypto.open(encrypt); adb.info(output);";

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
		mJSInstance = new AndJS(this);
		mJSInstance.injectObject(obj, "myobject");
		mJSInstance.injectObject(this, "myactivity");
    }

	@Override
	public void onDestroy() {
		super.onDestroy();
		mJSInstance.shutdown();
	}

	@CalledByJavascript
	void updateTitle(String version) {
        if(!mTitleIsUpdated) {
            this.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String str = getTitle().toString();
                    str += "@v8(" + version+")";
                    setTitle(str);
                }
            });
            mTitleIsUpdated = true;
        }
	}
}
