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
#include "andjs/andjs_core.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/strings/string_util.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/threading/thread.h"
#include "base/i18n/icu_util.h"
#include "gin/array_buffer.h"
#include "gin/try_catch.h"
#include "gin/v8_initializer.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "gin/per_context_data.h"
#include "crypto/aead.h"
#include "crypto/sha2.h"
#include "base/base64.h"
#include "v8/include/libplatform/libplatform.h"

#include "andjs/gin_java_bridge_object.h"

using v8::Context;
using v8::Local;
using v8::HandleScope;
using v8::Isolate;
using v8::Locker;

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;

namespace andjs {

static std::unique_ptr<content::V8ValueConverter> g_converter_ = content::V8ValueConverter::Create();

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

class JSCrypto: public gin::Wrappable<JSCrypto> {
  private:
    std::string aead_nonce_;
    std::string aead_key_;
	std::unique_ptr<crypto::Aead> aead_;

  public:
    static gin::WrapperInfo kWrapperInfo;

    static gin::Handle<JSCrypto> Create(v8::Isolate* isolate) {
      return CreateHandle(isolate, new JSCrypto());
    }

    void SetKey(v8::Isolate* isolate, const std::string& key) {
      std::string hash256_ = crypto::SHA256HashString(key);
      aead_nonce_.assign(hash256_, 0, aead_->NonceLength());
      aead_key_ = crypto::SHA256HashString(hash256_+aead_nonce_);
      aead_key_.append(hash256_, 0, 16);
      aead_->Init(&aead_key_);
    }

    v8::Local<v8::Value> Seal(v8::Isolate* isolate, const std::string& plaintext) {
      std::string ciphertext, output;
      if(aead_->Seal(plaintext, aead_nonce_, "jscrypto", &ciphertext)) {
        base::Base64Encode(ciphertext, &output);
        base::Value val(output);
        return g_converter_->ToV8Value(&val, isolate->GetCurrentContext());
      }
      return v8::Undefined(isolate);
    }

    v8::Local<v8::Value> Open(v8::Isolate* isolate, const std::string& ciphertext) {
      std::string plaintext, output;
      base::Base64Decode(ciphertext, &output);
      if(aead_->Open(output, aead_nonce_, "jscrypto", &plaintext)) {
        base::Value val(plaintext);
        return g_converter_->ToV8Value(&val, isolate->GetCurrentContext());
      }
      return v8::Undefined(isolate);
    }

  protected:
    JSCrypto() {
      aead_.reset(new crypto::Aead(crypto::Aead::AES_128_CTR_HMAC_SHA256));
    }

    gin::ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate) final {
      return gin::Wrappable<JSCrypto>::GetObjectTemplateBuilder(isolate)
             .SetMethod("setkey", &JSCrypto::SetKey)
             .SetMethod("seal", &JSCrypto::Seal)
             .SetMethod("open", &JSCrypto::Open);
    }
    const char* GetTypeName() final { return "JSCrypto"; }
    ~JSCrypto() override = default;

  private:
    DISALLOW_COPY_AND_ASSIGN(JSCrypto);
};
gin::WrapperInfo JSCrypto::kWrapperInfo = { gin::kEmbedderNativeGin };

AndJSCore::AndJSCore() : next_object_id_(1), message_loop_(new base::MessageLoopForIO()) {
  thread_.reset(new base::Thread("JSTask"));
  thread_->Start();

  g_converter_->SetDateAllowed(false);
  g_converter_->SetRegExpAllowed(false);
  g_converter_->SetFunctionAllowed(true);
  //thread_->task_runner()->PostTask(FROM_HERE, base::BindRepeating(&AndJSCore::Init, base::Unretained(this)));
}

v8::Local<v8::Value> AndJSCore::GetV8Version(gin::Arguments* args) {
  base::Value version(v8::V8::GetVersion());
  return g_converter_->ToV8Value(&version, args->isolate()->GetCurrentContext());
}

void AndJSCore::Init() {
  base::i18n::InitializeICU();
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif

  static const char kOptimizeForSize[] = "--optimize_for_size";
  v8::V8::SetFlagsFromString(kOptimizeForSize, strlen(kOptimizeForSize));
  static const char kNoOpt[] = "--noopt";
  v8::V8::SetFlagsFromString(kNoOpt, strlen(kNoOpt));

  // WebAssembly isn't encountered during resolution, so reduce the
  // potential attack surface.
  static const char kNoExposeWasm[] = "--no-expose-wasm";
  v8::V8::SetFlagsFromString(kNoExposeWasm, strlen(kNoExposeWasm));

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());

  instance_.reset(new gin::IsolateHolder(base::ThreadTaskRunnerHandle::Get(),
    #if ENABLE_V8_LOCKER
    gin::IsolateHolder::AccessMode::kUseLocker,
    #endif
    gin::IsolateHolder::IsolateType::kUtility));
  LOG(INFO) << " CreateIsolateHolder instance " << instance_;
  Isolate* isolate_ = instance_->isolate();

#if ENABLE_V8_LOCKER
  v8::Locker locked(isolate_);
#endif
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  v8::Local<v8::FunctionTemplate> get_v8_version_templ =
    gin::CreateFunctionTemplate(isolate_, base::BindRepeating(&AndJSCore::GetV8Version));
  v8::Local<v8::ObjectTemplate> global_templ =
    gin::ObjectTemplateBuilder(isolate_).Build();
  global_templ->Set(gin::StringToSymbol(isolate_, "get_v8_version"), get_v8_version_templ);

  context_holder_.reset(new gin::ContextHolder(isolate_));
  context_holder_->SetContext(v8::Context::New(isolate_, nullptr, global_templ));

  v8::Context::Scope scope(context_holder_->context());
  isolate_->SetCaptureStackTraceForUncaughtExceptions(true);
  InjectNativeObject();
  LOG(INFO) << " InjectNativeObject DONE ";
}

void AndJSCore::doV8Test(const std::string& jsbuf) {
  for(int i = 0; i < 1; i ++)
  {
    v8::Isolate* isolate_ = context_holder_->isolate();
#if ENABLE_V8_LOCKER
    LOG(INFO) << " AndJSCore.Run isolate.IsLocked = " << v8::Locker::IsLocked(isolate_) << " IsActive " << v8::Locker::IsActive();
    v8::Locker locked(isolate_);
#endif
    //gin::Runner::Scope scope(this);
    v8::ScriptOrigin origin(gin::StringToV8(isolate_, "v8test"));
    gin::TryCatch try_catch(isolate_);

    for(int j = 0; j < 30000; j++) {
    LOG(INFO) << " JNI_AndJS_V8Test STEP 02 Loop " << j;
    auto maybe_script = v8::Script::Compile(context_holder_->context(), gin::StringToV8(isolate_, jsbuf), &origin);
    v8::Local<v8::Script> script;
    if (!maybe_script.ToLocal(&script)) {
      LOG(ERROR) << try_catch.GetStackTrace();
      return;
    }

    auto maybe = script->Run(context_holder_->context());
    v8::Local<v8::Value> result;
    if (!maybe.ToLocal(&result)) {
      LOG(ERROR) << try_catch.GetStackTrace();
    }
    v8::String::Utf8Value utf8(isolate_, result);
    LOG(INFO) << " JNI_AndJS_V8Test STEP 03 " << *utf8 << " Loop " << j;
    }
  }
}

void AndJSCore::Shutdown() {
  v8::Isolate* isolate_ = instance_->isolate();
#if ENABLE_V8_LOCKER
  v8::Locker locked(isolate_);
#endif
  instance_.reset();
  LOG(INFO) << " AndJSCore Shutdown instance " << instance_;
}

bool AndJSCore::InjectNativeObject() {
  v8::Isolate* isolate_ = context_holder_->isolate();
#if ENABLE_V8_LOCKER
  LOG(INFO) << " InjectNativeObject IsLocked = " << v8::Locker::IsLocked(isolate_);
  v8::Locker locked(isolate_);
#endif
  v8::HandleScope handle_scope(isolate_);
  gin::Handle<AdbLog> adb = AdbLog::Create(isolate_);
  v8::Maybe<bool> result = global()->Set(context_holder_->context(), gin::StringToV8(isolate_, "adb"), adb.ToV8());

  gin::Handle<JSCrypto> jscrypto = JSCrypto::Create(isolate_);
  result = global()->Set(context_holder_->context(), gin::StringToV8(isolate_, "jscrypto"), jscrypto.ToV8());
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
  thread_->task_runner()->PostTask(FROM_HERE, base::BindRepeating(&AndJSCore::Run, base::Unretained(this), buf, "_membuf.js_"));
}

void AndJSCore::loadJSFileTask(const std::string& jspath) {
  std::string buf;
  base::FilePath filepath(jspath);

  if(base::ReadFileToString(filepath, &buf)) {
    for(int i = 0; i < 10000; i ++) {
      Run(buf, filepath.BaseName().value());
      LOG(INFO) << " loadJSFileTask loop:" << i;
    }
  }
}

void AndJSCore::LoadJSFile(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller,
                           const base::android::JavaParamRef<jstring>& jsfile) {
  std::string jspath (ConvertJavaStringToUTF8(env, jsfile));
  thread_->task_runner()->PostTask(FROM_HERE, base::BindRepeating(&AndJSCore::loadJSFileTask, base::Unretained(this), jspath));
}

void AndJSCore::Shutdown(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jcaller) {
  Shutdown();
}

scoped_refptr<content::GinJavaBoundObject> AndJSCore::GetObject(content::GinJavaBoundObject::ObjectID object_id) {
  // Can be called on any thread.
  base::AutoLock locker(objects_lock_);
  auto iter = objects_.find(object_id);
  if (iter != objects_.end())
    return iter->second;
  LOG(ERROR) << "AndJSCore: Unknown object: " << object_id;
  return nullptr;
}

v8::Local<v8::Value> AndJSCore::InjectObject(const base::android::JavaRef<jobject>& jobject,
                                             const base::android::JavaRef<jclass>&  annotation_clazz) {

  JNIEnv* env = base::android::AttachCurrentThread();
  JavaObjectWeakGlobalRef ref(env, jobject.obj());

  scoped_refptr<content::GinJavaBoundObject> bound_object = content::GinJavaBoundObject::CreateNamed(ref, annotation_clazz);
  content::GinJavaBoundObject::ObjectID object_id;
  {
    base::AutoLock locker(objects_lock_);
    object_id = next_object_id_++;
    objects_[object_id] = bound_object;
  }
  GinJavaBridgeObject* object = new GinJavaBridgeObject(this, object_id);

  v8::Isolate* isolate_ = context_holder_->isolate();
#if ENABLE_V8_LOCKER
  v8::Locker locked(isolate_);
#endif
  v8::EscapableHandleScope handle_scope(isolate_);

  LOG(INFO) << " Inject Anonymous Object " << object;
  gin::Handle<GinJavaBridgeObject> bridge_object = gin::CreateHandle(isolate_, object);
  LOG(INFO) << " Inject Anonymous Object " << "  bridge_object " << object;
  if(!bridge_object.IsEmpty()) {
    return handle_scope.Escape(bridge_object.ToV8());
  }
  return handle_scope.Escape(v8::Undefined(isolate_));
}
 
bool AndJSCore::InjectObject(std::string& name,
                             const base::android::JavaRef<jobject>& jobject,
                             const base::android::JavaRef<jclass>&  annotation_clazz) {

  JNIEnv* env = base::android::AttachCurrentThread();
  JavaObjectWeakGlobalRef ref(env, jobject.obj());

  scoped_refptr<content::GinJavaBoundObject> bound_object = content::GinJavaBoundObject::CreateNamed(ref, annotation_clazz);
  content::GinJavaBoundObject::ObjectID object_id;
  {
    base::AutoLock locker(objects_lock_);
    object_id = next_object_id_++;
    objects_[object_id] = bound_object;
  }
  GinJavaBridgeObject* object = new GinJavaBridgeObject(this, object_id);

  v8::Isolate* isolate_ = context_holder_->isolate();
  #if ENABLE_V8_LOCKER
  v8::Locker locked(isolate_);
  #endif
  gin::Runner::Scope scope(this);

  gin::Handle<GinJavaBridgeObject> bridge_object = gin::CreateHandle(isolate_, object);
  LOG(INFO) << " InjectJavaObject " << name << "  bridge_object " << object;
  if(!bridge_object.IsEmpty()) {
    v8::Maybe<bool> result = global()->Set(context_holder_->context(), gin::StringToV8(isolate_, name), bridge_object.ToV8());
    return !result.IsNothing() && result.FromJust();
  }
  return false;
}

void AndJSCore::Run(const std::string& jsbuf, const std::string& resource_name) {
  v8::Isolate* isolate_ = context_holder_->isolate();
#if ENABLE_V8_LOCKER
    v8::Locker locked(isolate_);
#endif
  gin::Runner::Scope scope(this);
  gin::TryCatch try_catch(isolate_);
  v8::ScriptOrigin origin(gin::StringToV8(isolate_, resource_name));

  auto maybe_script = v8::Script::Compile(context_holder_->context(), gin::StringToV8(isolate_, jsbuf), &origin);
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script)) {
    LOG(ERROR) << try_catch.GetStackTrace();
    return;
  }

  auto maybe = script->Run(context_holder_->context());
  v8::Local<v8::Value> result;
  if (!maybe.ToLocal(&result)) {
    LOG(ERROR) << try_catch.GetStackTrace();
  }
}

gin::ContextHolder* AndJSCore::GetContextHolder() {
  return context_holder_.get();
}

AndJSCore::~AndJSCore() = default;
}
