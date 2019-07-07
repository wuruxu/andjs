#ifndef __ANDJS_CORE_H__
#define __ANDJS_CORE_H__
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/jni_android.h"
#include "base/message_loop/message_loop.h"
#include "v8/include/v8.h"
#include "gin/public/isolate_holder.h"

namespace andjs {

//class gin::IsolateHolder;

class AndJSCore : public base::SupportsWeakPtr<AndJSCore> {
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

    v8::Local<v8::Script> CompileJS(std::string& jsbuf);
    void RunScript(v8::Local<v8::Script> script);

  private:
    bool InjectObject(std::string& name,
                      const base::android::JavaRef<jobject>& object,
                      const base::android::JavaRef<jclass>& annotation_clazz);
    void Run(std::string& jsbuf);
    void Shutdown();

    virtual std::unique_ptr<gin::IsolateHolder> CreateIsolateHolder() const;
    bool InjectJSLog();
    v8::Isolate* isolate_;
    std::unique_ptr<gin::IsolateHolder> instance_;
    v8::Persistent<v8::Context> context_;
    std::unique_ptr<base::MessageLoop> message_loop_;
};

}
#endif
