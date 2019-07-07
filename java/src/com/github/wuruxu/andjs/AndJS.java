package com.github.wuruxu.andjs;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.library_loader.LibraryProcessType;
import java.lang.annotation.Annotation;
import org.chromium.base.annotations.JNINamespace;
import android.util.Log;
import android.content.Context;

@JNINamespace("andjs")
public class AndJS {
	private long mNativeJSCore;

	public AndJS(Context context) {
		ContextUtils.initApplicationContext(context);
		try {
        	LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_CHILD);
		} catch( ProcessInitException pie) {
        	Log.e("AndJS" , "Unable to load native libraries.", pie);
		}
		mNativeJSCore = nativeInitAndJS();
	}

	public void loadJSBuf(String jsbuf) {
		nativeLoadJSBuf(mNativeJSCore, jsbuf);
	}

	public void loadJSFile(String jsfile) {
		nativeLoadJSFile(mNativeJSCore, jsfile);
	}

	public void injectObject(Object obj, String name, Class<? extends Annotation> requiredAnnotation) {
		nativeInjectObject(mNativeJSCore, obj, name, requiredAnnotation);
	}

	private native long nativeInitAndJS();
	private native boolean nativeInjectObject(long nativeAndJSCore, Object obj, String name, Class requiredAnnotation);
	private native void nativeLoadJSBuf(long nativeAndJSCore, String jsbuf);
	private native void nativeLoadJSFile(long nativeAndJSCore, String jsfile);
}
