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
				AndJS.loadJSBuf("This is a test jsbuf");
			}
		});
		obj = new MyObject();
		AndJS.injectObject(obj, "myobject", null);
    }
}
