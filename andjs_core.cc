#include "andjs/andjs_core.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/strings/string_util.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/jni_string.h"
#include "base/files/file_util.h"
#include "gin/array_buffer.h"
#include "gin/try_catch.h"
#include "gin/v8_initializer.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "gin/handle.h"
#include "gin/wrappable.h"

#include "andjs/gin_java_bridge_object.h"

using v8::Context;
using v8::Local;
using v8::HandleScope;

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;

namespace andjs {

class AdbLog: public gin::Wrappable<AdbLog> {
  public:
    static gin::WrapperInfo kWrapperInfo;

    static gin::Handle<AdbLog> Create(v8::Isolate* isolate) {
      return CreateHandle(isolate, new AdbLog());
    }

    void Info(v8::Isolate* isolate, const std::string info) {
      LOG(INFO) << info;
    }

    void Error(v8::Isolate* isolate, const std::string error) {
      LOG(ERROR) << error;
    }

  protected:
    AdbLog() = default;
    gin::ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate) final {
      return gin::Wrappable<AdbLog>::GetObjectTemplateBuilder(isolate)
             .SetMethod("error", &AdbLog::Error)
             .SetMethod("info", &AdbLog::Info);
    }
    const char* GetTypeName() final { return "AdbLog"; }
    ~AdbLog() override = default;

  private:
    DISALLOW_COPY_AND_ASSIGN(AdbLog);
};

gin::WrapperInfo AdbLog::kWrapperInfo = { gin::kEmbedderNativeGin };

AndJSCore::AndJSCore() : message_loop_(new base::MessageLoopForIO()) {
  isolate_ = NULL;
}

std::unique_ptr<gin::IsolateHolder> AndJSCore::CreateIsolateHolder() const {
  return std::make_unique<gin::IsolateHolder>(base::ThreadTaskRunnerHandle::Get(),
         gin::IsolateHolder::IsolateType::kBlinkMainThread);
}

void AndJSCore::Init() {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());

  instance_ = CreateIsolateHolder();
  isolate_ = instance_->isolate();
  isolate_->Enter();
  HandleScope handle_scope(isolate_);
  context_.Reset(instance_->isolate(), Context::New(isolate_));
  Local<Context>::New(isolate_, context_)->Enter();

  InjectJSLog();
}

void AndJSCore::Shutdown() {
  {
    HandleScope handle_scope(instance_->isolate());
    Local<Context>::New(instance_->isolate(), context_)->Exit();
    context_.Reset();
  }
  instance_->isolate()->Exit();
  isolate_ = NULL;
  instance_.reset();
}

bool AndJSCore::InjectJSLog() {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = context_.Get(isolate_);

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();
  gin::Handle<AdbLog> obj = AdbLog::Create(isolate_);
  std::string object_name("adb");
  v8::Maybe<bool> result = global->Set(context, gin::StringToV8(isolate_, object_name), obj.ToV8());
  return !result.IsNothing() && result.FromJust();
}

bool AndJSCore::InjectObject(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& jcaller,
                             const base::android::JavaParamRef<jobject>& jobject,
                             const base::android::JavaParamRef<jstring>& jname,
                             const base::android::JavaParamRef<jclass>&  annotation_clazz) {
  std::string name(ConvertJavaStringToUTF8(env, jname));
  return InjectObject(name, jobject, annotation_clazz);
}

void AndJSCore::LoadJSBuf(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jcaller,
                          const base::android::JavaParamRef<jstring>& jsbuf) {
  std::string buf(ConvertJavaStringToUTF8(env, jsbuf));
  Run(buf);
}

void AndJSCore::LoadJSFile(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller,
                           const base::android::JavaParamRef<jstring>& jsfile) {
    std::string jspath (ConvertJavaStringToUTF8(env, jsfile));
    std::string buf;

    base::FilePath filepath(jspath);
    if(base::ReadFileToString(filepath, &buf)) {
      Run(buf);
    }
}

void AndJSCore::Shutdown(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jcaller) {
  Shutdown();
}

bool AndJSCore::InjectObject(std::string& name,
                             const base::android::JavaRef<jobject>& jobject,
                             const base::android::JavaRef<jclass>&  annotation_clazz) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = context_.Get(isolate_);

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();

  JNIEnv* env = base::android::AttachCurrentThread();
  JavaObjectWeakGlobalRef ref(env, jobject.obj());

  scoped_refptr<content::GinJavaBoundObject> bound_object = content::GinJavaBoundObject::CreateNamed(ref, annotation_clazz);
  GinJavaBridgeObject* object = new GinJavaBridgeObject(isolate_, bound_object);
  gin::Handle<GinJavaBridgeObject> bridge_object = gin::CreateHandle(isolate_, object);
  if(!bridge_object.IsEmpty()) {
    v8::Maybe<bool> result = global->Set(context, gin::StringToV8(isolate_, name), bridge_object.ToV8());
    return !result.IsNothing() && result.FromJust();
  }
  return false;
}

void AndJSCore::Run(std::string& jsbuf) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = context_.Get(isolate_);
  v8::Local<v8::String> source = gin::StringToV8(isolate_, jsbuf);

  v8::TryCatch try_catch(isolate_);
  v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();
  //v8::Local<v8::Script> script = v8::Script::Compile(context, source).FromMaybe(v8::Local<v8::Value>());
  //if(try_catch.HasCaught()) {
  //  LOG(INFO) << " Compile " << gin::V8ToString(isolate, try_catch.Message()->Get());
  //}
  //script->Run(context).ToLocalChecked();
  script->Run(context).FromMaybe(v8::Local<v8::Value>());
  if(try_catch.HasCaught()) {
    LOG(ERROR) << gin::V8ToString(isolate_, try_catch.Message()->Get());
  }
}

v8::Local<v8::Script> AndJSCore::CompileJS(std::string& jsbuf) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = context_.Get(isolate_);
  v8::Local<v8::String> source = gin::StringToV8(isolate_, jsbuf);

  v8::TryCatch try_catch(isolate_);
  v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();
  return script;
}

void AndJSCore::RunScript(v8::Local<v8::Script> script) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = context_.Get(isolate_);
  //v8::Local<v8::Value> obj = script->Run(context).ToLocalChecked();
  script->Run(context).ToLocalChecked();
}

AndJSCore::~AndJSCore() {}
}
