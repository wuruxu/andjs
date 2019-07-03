package com.github.wuruxu.andjs.sample;

import android.util.Log;
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
