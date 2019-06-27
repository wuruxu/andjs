package com.wuruxu.andjs;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.library_loader.LibraryProcessType;
import java.lang.annotation.Annotation;
import android.util.Log;
import android.content.Context;

public class AndJS {
	public static void Init(Context context) {
		ContextUtils.initApplicationContext(context);
		try {
        	LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_CHILD);
		} catch( ProcessInitException pie) {
        	Log.e("AndJS" , "Unable to load native libraries.", pie);
		}
		nativeInitAndJS();
	}

	public static void loadJSBuf(String jsbuf) {
		nativeLoadJSBuf(jsbuf);
	}

	public static void loadJSFile(String jsfile) {
		nativeLoadJSFile(jsfile);
	}

	public static void injectObject(Object obj, String name, Class<? extends Annotation> requiredAnnotation) {
		nativeInjectObject(obj, name, requiredAnnotation);
	}

	private static native void nativeInjectObject(Object obj, String name, Class requiredAnnotation);
	private static native void nativeInitAndJS();
	private static native void nativeLoadJSBuf(String jsbuf);
	private static native void nativeLoadJSFile(String jsfile);
}
