package com.wuruxu.andjs.sample;

import android.util.Log;
import com.wuruxu.andjs.CalledByJavascript;

public class MyObject extends Object {
	private static final String TAG = "MyObject";

	public MyObject() {}

	@CalledByJavascript
	public void doLog(String msg) {
		Log.i(TAG, " * " + msg);
	}
}
