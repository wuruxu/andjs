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
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "crypto/aead.h"
#include "crypto/sha2.h"
#include "base/base64.h"
#include "content/browser/android/java/gin_java_bound_object.h"
#include "content/browser/android/java/jni_reflect.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaObjectArrayReader;

namespace andjs {

static JSClassID jsdata_class_id = 0;
static JSClassID jscrypto_class_id = 0;
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

static JSValue jscrypto_seal_open(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
  JSCrypto* crypto = (JSCrypto *)JS_GetOpaque2(ctx, this_val, jscrypto_class_id);
  if(crypto == NULL) return JS_EXCEPTION;
  const char* str = JS_ToCString(ctx, argv[0]);
  std::string output;
  if((magic == 0 && crypto->Seal(str, output)) ||
     (magic == 1 && crypto->Open(str, output))) {
    LOG(INFO) << "jscrypto_seal_open " << str << " ==> " << output;
    return JS_NewString(ctx, output.c_str());
  }
  return JS_UNDEFINED;
}

static const JSCFunctionListEntry jscrypto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("seal", 1, jscrypto_seal_open, 0),
    JS_CFUNC_MAGIC_DEF("open", 1, jscrypto_seal_open, 1),
};

AndJSCore::AndJSCore() : next_object_id_(1), message_loop_(new base::MessageLoopForIO()) {
}

void AndJSCore::Init() {
  rt_ = JS_NewRuntime();
  ctx_ = JS_NewContext(rt_);

  JS_SetMemoryLimit(rt_, 51200);
  JS_SetGCThreshold(rt_, 25600);
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
 
JavaObjectWeakGlobalRef AndJSCore::GetObjectWeakRef(content::GinJavaBoundObject::ObjectID object_id) {
  LOG(INFO) << " *AndJSCore::GetObjectWeakRef* " << object_id;
  return JavaObjectWeakGlobalRef();
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

  /* JSData class */
  JS_NewClassID(&jsdata_class_id);

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

//static void jscrypto_finalizer(JSRuntime *rt, JSValue val) {
//    JSCrypto *crypto = (JSCrypto* )JS_GetOpaque(val, jscrypto_class_id);
//    if (crypto) {
//      delete crypto;
//    }
//}

std::unique_ptr<base::Value> AndJSCore::FromJSValue(JSValue val) {
  uint32_t tag = JS_VALUE_GET_TAG(val);
  //LOG(INFO) << " FromJSValue jsvalue.type=" << tag;
  switch(tag) {
    case JS_TAG_BOOL: {
      int b = JS_ToBool(ctx_, val);
      if(b < 0) return std::make_unique<base::Value>();
      return std::make_unique<base::Value>(b);
    }
    case JS_TAG_NULL: {
      return std::make_unique<base::Value>();
    }
    case JS_TAG_UNDEFINED: {
      return std::make_unique<base::Value>();
    }
    case JS_TAG_STRING: {
      const char* str = JS_ToCString(ctx_, val);
      if(!str) return std::make_unique<base::Value>();
      std::unique_ptr<base::Value> ret = std::make_unique<base::Value>(str);
      JS_FreeCString(ctx_, str);
      return ret;
    }
    case JS_TAG_OBJECT: {
      return std::make_unique<base::Value>();
    }
    case JS_TAG_BIG_INT:
    case JS_TAG_BIG_FLOAT: {
      return std::make_unique<base::Value>();
    }
    case JS_TAG_INT: {
      int i;
      if(JS_ToInt32(ctx_, &i, val))
        return std::make_unique<base::Value>();
      return std::make_unique<base::Value>(i);
    }
    case JS_TAG_FLOAT64: {
      double d;
      if(JS_ToFloat64(ctx_, &d, val))
        return std::make_unique<base::Value>();
      return std::make_unique<base::Value>(d);
    }
  }
  return std::make_unique<base::Value>();
}

JSValue AndJSCore::ToJSValue(const base::Value* value) {
  LOG(INFO) << " ToJSValue value.type = " << value->type();
  switch(value->type()) {
    case base::Value::Type::NONE:
      return JS_NULL;
    case base::Value::Type::BOOLEAN: {
      bool val = false;
      value->GetAsBoolean(&val);
      return JS_NewBool(ctx_, val);
    }
    case base::Value::Type::INTEGER: {
      int val = 0;
      value->GetAsInteger(&val);
      return JS_NewInt32(ctx_, val);
    }
    case base::Value::Type::DOUBLE: {
      double val = 0.0;
      value->GetAsDouble(&val);
      return JS_NewFloat64(ctx_, val);
    }
    case base::Value::Type::STRING: {
      std::string val;
      value->GetAsString(&val);
      return JS_NewString(ctx_, val.c_str());
    }
    case base::Value::Type::LIST:
    break;
    case base::Value::Type::DICTIONARY:
    break;
    case base::Value::Type::BINARY: {
      return JS_NewArrayBufferCopy(ctx_, value->GetBlob().data(), value->GetBlob().size());
    }
    default: break;
  }

  LOG(ERROR) << "Unexpected value type: " << value->type();
  return JS_UNDEFINED;
}

static JSValue java_object_invoke(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue* data) {
  content::GinJavaBoundObject::ObjectID object_id = magic;
  AndJSCore* thiz = (AndJSCore* )JS_GetOpaque(data[0], jsdata_class_id);
  const char* method_name = JS_ToCString(ctx, data[1]);
  scoped_refptr<content::GinJavaBoundObject> bound_object = thiz->GetObject(object_id);
  LOG(INFO) << " java_object_invoke " << " object_id " << object_id << " method_name " << method_name;
  base::ListValue arguments;
  for(int i = 0; i < argc; i++) {
    arguments.Append(thiz->FromJSValue(argv[i]));
  }

  content::GinJavaBridgeError error;
  scoped_refptr<content::GinJavaMethodInvocationHelper> result =
    new content::GinJavaMethodInvocationHelper(std::make_unique<content::GinJavaBoundObjectDelegate>(bound_object), method_name, arguments);

  result->Init(NULL);
  result->Invoke();
  error = result->GetInvocationError();

  JS_FreeCString(ctx, method_name);
  if (result->HoldsPrimitiveResult()) {
    base::Value* v8_result;
    std::unique_ptr<base::ListValue> result_copy(result->GetPrimitiveResult().DeepCopy());
    if(result_copy->Get(0, &v8_result)) {
      return thiz->ToJSValue(v8_result);
    }
  } else if (!result->GetObjectResult().is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    JavaObjectWeakGlobalRef ref(env, result->GetObjectResult().obj());
    //scoped_refptr<content::GinJavaBoundObject> bound_object = content::GinJavaBoundObject::CreateNamed(ref, result->GetSafeAnnotationClass());
    return thiz->ToJSObject(result->GetObjectResult(), result->GetSafeAnnotationClass());
  }

  return JS_UNDEFINED;
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

JSValue AndJSCore::ToJSObject(const base::android::JavaRef<jobject>& java_object, const base::android::JavaRef<jclass>&  annotation_clazz) {
  JSValue jsobj = JS_NULL;

  JNIEnv* env = base::android::AttachCurrentThread();
  if(java_object.obj()) {
    jsobj = JS_NewObject(ctx_);
    JavaObjectWeakGlobalRef ref(env, java_object.obj());
    //content::GinJavaBoundObject* bound_object = content::GinJavaBoundObject::CreateNamed(ref, annotation_clazz);
    scoped_refptr<content::GinJavaBoundObject> new_object = content::GinJavaBoundObject::CreateNamed(ref, annotation_clazz);
    ScopedJavaLocalRef<jclass> clazz = new_object->GetLocalClassRef(env);
    content::GinJavaBoundObject::ObjectID object_id;
    {
      base::AutoLock locker(objects_lock_);
      object_id = next_object_id_++;
      objects_[object_id] = new_object;
    }
    std::string class_name = content::GetClassName(env, clazz);
    JSValue jsdata = JS_NewObjectClass(ctx_, jsdata_class_id);
    JS_SetOpaque(jsdata, (void *)this);
    LOG(INFO) << " java_object class_name " << class_name << " object_id " << object_id;

    JavaObjectArrayReader<jobject> methods(content::GetClassMethods(env, clazz));
    for (auto java_method : methods) {
      if (!annotation_clazz.is_null() && !content::IsAnnotationPresent(env, java_method, annotation_clazz)) {
        continue;
      }
      std::string method_name = content::GetMethodName(env, java_method);
      ScopedJavaLocalRef<jobjectArray> parameters(content::GetMethodParameterTypes(env, java_method));
      int num_parameters = env->GetArrayLength(parameters.obj());
      bool is_static = content::IsMethodStatic(env, java_method);
      JSValueConst method_data[2];

      method_data[0] = jsdata;
      method_data[1] = JS_NewString(ctx_, method_name.c_str());
      JS_SetPropertyStr(ctx_, jsobj, method_name.c_str(), JS_NewCFunctionData(ctx_, java_object_invoke, num_parameters, object_id, 2, method_data));
      //LOG(INFO) << " method : " << method_name << " is_static " << is_static << " num_parameters " << num_parameters;
    }
  }
  return jsobj;
}

bool AndJSCore::InjectObject(std::string& objname,
                             const base::android::JavaRef<jobject>& java_object,
                             const base::android::JavaRef<jclass>&  annotation_clazz) {
  bool ret = false;

  JSValue global = JS_GetGlobalObject(ctx_);
  JSValue jsobj = ToJSObject(java_object, annotation_clazz);

  if(!JS_IsNull(jsobj)) {
    JS_SetPropertyStr(ctx_, global, objname.c_str(), jsobj);
    JS_FreeValue(ctx_, global);
    ret = true;
  }
  return ret;
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
