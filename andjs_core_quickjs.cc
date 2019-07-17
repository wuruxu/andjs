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
#include "andjs/andjs_core_quickjs.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/strings/string_util.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "crypto/aead.h"
#include "crypto/sha2.h"
#include "base/base64.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;

namespace andjs {

static JSClassID jscrypto_class_id;
class JSCrypto {
  public:
    JSCrypto(const std::string& key) {
      aead_.reset(new crypto::Aead(crypto::Aead::AES_128_CTR_HMAC_SHA256));
      std::string hash256_ = crypto::SHA256HashString(key);
      aead_nonce_.assign(hash256_, 0, aead_->NonceLength());
      aead_key_ = crypto::SHA256HashString(hash256_+aead_nonce_);
      aead_key_.append(hash256_, 0, 16);
      aead_->Init(&aead_key_);
    }

    bool Seal(const std::string& plaintext, std::string& output) {
      std::string ciphertext;
      if(aead_->Seal(plaintext, aead_nonce_, "jscrypto", &ciphertext)) {
        base::Base64Encode(ciphertext, &output);
        return true;
      }
      return false;
    }

    bool Open(const std::string& ciphertext, std::string& output) {
      base::Base64Decode(ciphertext, &output);
      if(aead_->Open(output, aead_nonce_, "jscrypto", &output)) {
        return true;
      }
      return false;
    }

    ~JSCrypto() {}

  private:
    std::string aead_nonce_;
    std::string aead_key_;
	std::unique_ptr<crypto::Aead> aead_;
    DISALLOW_COPY_AND_ASSIGN(JSCrypto);
};

static JSValue get_jscrypto_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  const char* str = JS_ToCString(ctx, argv[0]);
  if(!str) return JS_EXCEPTION;

  JSValue obj = JS_NewObjectClass(ctx, jscrypto_class_id);
  if(JS_IsException(obj)) return obj;
  
  JSCrypto* crypto = new JSCrypto(str);
  if(crypto == NULL) return JS_EXCEPTION;

  JS_SetOpaque(obj, crypto);
  return obj;
}

static void jscrypto_finalizer(JSRuntime *rt, JSValue val) {
    JSCrypto *crypto = (JSCrypto* )JS_GetOpaque(val, jscrypto_class_id);
    if (crypto) {
      delete crypto;
    }
}

//static void jscrypto_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
//    JSCrypto *crypto = JS_GetOpaque(val, jscrypto_class_id);
//    if (crypto) {
//        JS_MarkValue(rt, th->func, mark_func);
//    }
//}

static JSClassDef jscrypto_class = {
    "JSCrypto",
    .finalizer = jscrypto_finalizer,
    //.gc_mark = jscrypto_mark,
};

static JSValue jscrypto_seal(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSCrypto* crypto = (JSCrypto* )JS_GetOpaque2(ctx, this_val, jscrypto_class_id);
  if(crypto == NULL) return JS_EXCEPTION;
  const char* str = JS_ToCString(ctx, argv[0]);
  std::string output;
  if(crypto->Seal(str, output)) {
    LOG(INFO) << "jscrypto_seal " << str << " ==> " << output;
    return JS_NewString(ctx, output.c_str());
  }
  return JS_UNDEFINED;
}

static JSValue jscrypto_open(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSCrypto* crypto = (JSCrypto *)JS_GetOpaque2(ctx, this_val, jscrypto_class_id);
  if(crypto == NULL) return JS_EXCEPTION;
  const char* str = JS_ToCString(ctx, argv[0]);
  std::string output;
  if(crypto->Open(str, output)) {
    LOG(INFO) << "jscrypto_open " << str << " ==> " << output;
    return JS_NewString(ctx, output.c_str());
  }
  return JS_UNDEFINED;
}

static const JSCFunctionListEntry jscrypto_funcs[] = {
    JS_CFUNC_DEF("seal", 1, jscrypto_seal),
    JS_CFUNC_DEF("open", 1, jscrypto_open),
};

AndJSCore::AndJSCore() : message_loop_(new base::MessageLoopForIO()) {
}

void AndJSCore::Init() {
  rt_ = JS_NewRuntime();
  ctx_ = JS_NewContext(rt_);

  JS_SetModuleLoaderFunc(rt_, NULL, js_module_loader, NULL);
  js_init_module_std(ctx_, "std");
  js_init_module_os(ctx_, "os");

  InjectNativeObject();
  LOG(INFO) << " InjectNativeObject DONE";

  thread_.reset(new base::Thread("JSTask"));
  thread_->Start();
}

void AndJSCore::Shutdown() {
  JS_FreeContext(ctx_);
  JS_FreeRuntime(rt_);
  LOG(INFO) << " AndJSCore Shutdown instance " ;
}

static JSValue adb_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  std::string output("");

  for(int i = 0; i < argc; i++) {
    const char* str = JS_ToCString(ctx, argv[i]);
    if(!str) return JS_EXCEPTION;
    output.append(str);
    JS_FreeCString(ctx, str);
  }
  LOG(INFO) << " " << output;
  return JS_UNDEFINED;
}

static JSValue adb_error(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  std::string output("");
  for(int i = 0; i < argc; i++) {
    const char* str = JS_ToCString(ctx, argv[i]);
    if(!str) return JS_EXCEPTION;
    output.append(str);
    JS_FreeCString(ctx, str);
  }
  LOG(ERROR) << " " << output;
  return JS_UNDEFINED;
}

bool AndJSCore::InjectNativeObject() {
  JSValue global, adb, jscrypto;
  global = JS_GetGlobalObject(ctx_);

  adb = JS_NewObject(ctx_);
  JS_SetPropertyStr(ctx_, adb, "info", JS_NewCFunction(ctx_, adb_info, "info", 1));
  JS_SetPropertyStr(ctx_, adb, "error", JS_NewCFunction(ctx_, adb_error, "error", 1));
  JS_SetPropertyStr(ctx_, global, "adb", adb);

  /* JSCrypto class */
  JSValue proto;
  JS_NewClassID(&jscrypto_class_id);
  JS_NewClass(JS_GetRuntime(ctx_), jscrypto_class_id, &jscrypto_class);
  proto = JS_NewObject(ctx_);
  JS_SetPropertyFunctionList(ctx_, proto, jscrypto_funcs, countof(jscrypto_funcs));
  JS_SetClassProto(ctx_, jscrypto_class_id, proto);

  JS_SetPropertyStr(ctx_, global, "getJSCrypto", JS_NewCFunction(ctx_, get_jscrypto_object, "getJSCrypto", 1));

  JS_FreeValue(ctx_, global);
  return true;
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

void AndJSCore::LoadJSFile(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller,
                           const base::android::JavaParamRef<jstring>& jsfile) {
  std::string jspath (ConvertJavaStringToUTF8(env, jsfile));
  std::string buf;
  base::FilePath filepath(jspath);
  if(base::ReadFileToString(filepath, &buf)) {
    thread_->task_runner()->PostTask(FROM_HERE, base::BindRepeating(&AndJSCore::Run, base::Unretained(this), buf, filepath.BaseName().value()));
  }
}

void AndJSCore::Shutdown(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jcaller) {
  Shutdown();
}

bool AndJSCore::InjectObject(std::string& name,
                             const base::android::JavaRef<jobject>& jobject,
                             const base::android::JavaRef<jclass>&  annotation_clazz) {
  JSValue obj, global;

  global = JS_GetGlobalObject(ctx_);
  obj = JS_NewObject(ctx_);
  
  JS_SetPropertyStr(ctx_, global, name.c_str(), obj);
  JS_FreeValue(ctx_, global);

  return true;
}

void AndJSCore::Run(const std::string& jsbuf, const std::string& resource_name) {
  JSValue val;

  val = JS_Eval(ctx_, jsbuf.c_str(), jsbuf.length(), resource_name.c_str(), JS_EVAL_TYPE_MODULE);
  if(JS_IsException(val)) {
    JSValue exception_val = JS_GetException(ctx_);
    BOOL is_error = JS_IsError(ctx_, exception_val);
    const char* str = JS_ToCString(ctx_, exception_val);
    LOG(ERROR) << " is_error " << is_error << ": " << str;
    JS_FreeCString(ctx_, str);
    if (is_error) {
        JSValue stack = JS_GetPropertyStr(ctx_, exception_val, "stack");
        if (!JS_IsUndefined(stack)) {
          str = JS_ToCString(ctx_, stack);
          LOG(INFO) << "*" << str;
          JS_FreeCString(ctx_, str);
        }
        JS_FreeValue(ctx_, stack);
    }
    JS_FreeValue(ctx_, exception_val);
  }
  JS_FreeValue(ctx_, val);
}

AndJSCore::~AndJSCore() = default;
}
