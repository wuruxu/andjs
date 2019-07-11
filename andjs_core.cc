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
#include "crypto/aead.h"
#include "crypto/sha2.h"
#include "base/base64.h"

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

AndJSCore::AndJSCore() : message_loop_(new base::MessageLoopForIO()) {
  isolate_ = NULL;
}

std::unique_ptr<gin::IsolateHolder> AndJSCore::CreateIsolateHolder() const {
  return std::make_unique<gin::IsolateHolder>(base::ThreadTaskRunnerHandle::Get(),
         gin::IsolateHolder::IsolateType::kBlinkMainThread);
}

v8::Local<v8::Value> AndJSCore::GetV8Version(gin::Arguments* args) {
  base::Value version(v8::V8::GetVersion());
  return g_converter_->ToV8Value(&version, args->isolate()->GetCurrentContext());
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

  g_converter_->SetDateAllowed(false);
  g_converter_->SetRegExpAllowed(false);
  g_converter_->SetFunctionAllowed(true);

  v8::Local<v8::FunctionTemplate> get_v8_version_func =
    gin::CreateFunctionTemplate(isolate_, base::BindRepeating(&AndJSCore::GetV8Version));
  v8::Local<v8::ObjectTemplate> global_templ =
    gin::ObjectTemplateBuilder(isolate_).Build();
  global_templ->Set(gin::StringToSymbol(isolate_, "get_v8_version"), get_v8_version_func);

  context_.Reset(isolate_, Context::New(isolate_, NULL, global_templ));
  Local<Context>::New(isolate_, context_)->Enter();

  InjectNativeObject();
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

bool AndJSCore::InjectNativeObject() {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = context_.Get(isolate_);

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();

  gin::Handle<AdbLog> adb = AdbLog::Create(isolate_);
  v8::Maybe<bool> result = global->Set(context, gin::StringToV8(isolate_, "adb"), adb.ToV8());

  gin::Handle<JSCrypto> jscrypto = JSCrypto::Create(isolate_);
  result = global->Set(context, gin::StringToV8(isolate_, "jscrypto"), jscrypto.ToV8());
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
