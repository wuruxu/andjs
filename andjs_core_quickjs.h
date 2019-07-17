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

#ifndef __ANDJS_CORE_QUICKJS_H__
#define __ANDJS_CORE_QUICKJS_H__
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/jni_android.h"
#include "base/message_loop/message_loop.h"
#include "base/files/file_path.h"
#include "base/threading/thread.h"

extern "C" {
#include "cutils.h"
#include "quickjs-libc.h"
}

namespace andjs {

class AndJSCore {
  public:
    AndJSCore();
    ~AndJSCore();

    void Init();

    bool InjectObject(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jcaller,
                      const base::android::JavaParamRef<jobject>& jobject,
                      const base::android::JavaParamRef<jstring>& jname,
                      const base::android::JavaParamRef<jclass>&  annotation_clazz);

    void LoadJSBuf(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jcaller,
                   const base::android::JavaParamRef<jstring>& jsbuf);

    void LoadJSFile(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jcaller,
                    const base::android::JavaParamRef<jstring>& jsfile);

    void Shutdown(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller);

    void Run(const std::string& jsbuf, const std::string& resource_name);
  private:
    bool InjectObject(std::string& name,
                      const base::android::JavaRef<jobject>& object,
                      const base::android::JavaRef<jclass>& annotation_clazz);
    void Shutdown();
    bool InjectNativeObject();

    JSRuntime* rt_;
    JSContext* ctx_;
    std::unique_ptr<base::Thread> thread_;
    std::unique_ptr<base::MessageLoop> message_loop_;
};

}
#endif
