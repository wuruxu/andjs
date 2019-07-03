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
