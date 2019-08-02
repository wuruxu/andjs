/* Copyright (c) 2019 wuruxu <wrxzzj@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
package com.github.wuruxu.andjs;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.library_loader.LibraryProcessType;
import java.lang.annotation.Annotation;
import java.lang.Object;
import org.chromium.base.annotations.JNINamespace;
import android.util.Log;
import android.content.Context;

@JNINamespace("andjs")
public class AndJS extends Object {
	private long mNativeJSCore;
	private boolean mShutdown;
	private Object locker;

	public AndJS(Context context) {
		ContextUtils.initApplicationContext(context);
		try {
        	LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_CHILD);
		} catch( ProcessInitException pie) {
        	Log.e("AndJS" , "Unable to load native libraries.", pie);
		}
		mNativeJSCore = nativeInitAndJS();
		mShutdown = false;
		locker = new Object();
	}

	public void loadJSBuf(String jsbuf) {
		nativeLoadJSBuf(mNativeJSCore, jsbuf);
	}

	public void loadJSFile(String jsfile) {
		nativeLoadJSFile(mNativeJSCore, jsfile);
	}

	public void injectObject(Object obj, String name) {
		nativeInjectObject(mNativeJSCore, obj, name, CalledByJavascript.class);
	}

	public void shutdown() {
		synchronized(locker) {
			if(!mShutdown) {
				nativeShutdown(mNativeJSCore);
				mShutdown = true;
			}
		}
	}

	@Override
	public void finalize() {
		shutdown();
	}

	private native long nativeInitAndJS();
	private native boolean nativeInjectObject(long nativeAndJSCore, Object obj, String name, Class requiredAnnotation);
	private native void nativeLoadJSBuf(long nativeAndJSCore, String jsbuf);
	private native void nativeLoadJSFile(long nativeAndJSCore, String jsfile);
	private native void nativeShutdown(long nativeAndJSCore);
}
