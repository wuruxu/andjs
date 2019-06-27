#ifndef __ANDJS_CORE_H__
#define __ANDJS_CORE_H__
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/android/jni_weak_ref.h"
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
    void Shutdown();

    void InjectObject(std::string& name, const base::android::JavaRef<jobject>& object,
                      const base::android::JavaRef<jclass>& safe_annotation_clazz);
    void Run(std::string& jsbuf);

    v8::Local<v8::Script> CompileJS(std::string& jsbuf);
    void RunScript(v8::Local<v8::Script> script);

  private:
    virtual std::unique_ptr<gin::IsolateHolder> CreateIsolateHolder() const;
    void InjectJSLog();
    v8::Isolate* isolate_;
    std::unique_ptr<gin::IsolateHolder> instance_;
    v8::Persistent<v8::Context> context_;
    std::unique_ptr<base::MessageLoop> message_loop_;
};

}
#endif
