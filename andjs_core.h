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

#ifndef __ANDJS_CORE_H__
#define __ANDJS_CORE_H__
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/jni_android.h"
#include "base/message_loop/message_loop.h"
#include "base/files/file_path.h"
#include "base/threading/thread.h"
#include "gin/public/isolate_holder.h"
#include "gin/arguments.h"
#include "gin/runner.h"
#include "content/public/renderer/v8_value_converter.h"
#include "gin/public/context_holder.h"

#include "content/browser/android/java/gin_java_bound_object_delegate.h"
#include "content/browser/android/java/gin_java_bound_object.h"

namespace andjs {

class AndJSCore : public gin::Runner {
  public:
    AndJSCore();
    ~AndJSCore() override;

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

    scoped_refptr<content::GinJavaBoundObject> GetObject(content::GinJavaBoundObject::ObjectID object_id);
    gin::ContextHolder* GetContextHolder() override;
    void Run(const std::string& jsbuf, const std::string& resource_name) override;
    v8::Local<v8::Value> InjectObject(const base::android::JavaRef<jobject>& jobject,
                                      const base::android::JavaRef<jclass>&  annotation_clazz);
  private:
    bool InjectObject(std::string& name,
                      const base::android::JavaRef<jobject>& object,
                      const base::android::JavaRef<jclass>& annotation_clazz);
    void loadJSFileTask(const std::string& jspath); //'const'
    void Shutdown();
    void doV8Test(const std::string& jsbuf);

    static v8::Local<v8::Value> GetV8Version(gin::Arguments* args);

    typedef std::map<content::GinJavaBoundObject::ObjectID, scoped_refptr<content::GinJavaBoundObject>> ObjectMap;
    ObjectMap objects_ GUARDED_BY(objects_lock_);
    base::Lock objects_lock_;
    content::GinJavaBoundObject::ObjectID next_object_id_;

    bool InjectNativeObject();
    std::unique_ptr<gin::IsolateHolder> instance_;
    std::unique_ptr<gin::ContextHolder> context_holder_;
    std::unique_ptr<base::MessageLoop> message_loop_;
    std::unique_ptr<base::Thread> thread_;
    v8::Persistent<v8::External> v8_this_;
};

}
#endif
