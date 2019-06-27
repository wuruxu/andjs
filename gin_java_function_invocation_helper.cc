// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "andjs/gin_java_function_invocation_helper.h"

#include <utility>

#include "base/values.h"
#include "base/android/jni_android.h"
#include "content/common/android/gin_java_bridge_errors.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/public/renderer/v8_value_converter.h"
#include "andjs/gin_java_bridge_object.h"

namespace andjs {

namespace {

const char kMethodInvocationAsConstructorDisallowed[] =
    "Java bridge method can't be invoked as a constructor";
const char kMethodInvocationOnNonInjectedObjectDisallowed[] =
    "Java bridge method can't be invoked on a non-injected object";

}  // namespace

GinJavaFunctionInvocationHelper::GinJavaFunctionInvocationHelper(const std::string& method_name, scoped_refptr<content::GinJavaBoundObject> bound_object) 
  : method_name_(method_name),
    bound_object_(bound_object),
    converter_(content::V8ValueConverter::Create()) {
  converter_->SetDateAllowed(false);
  converter_->SetRegExpAllowed(false);
  converter_->SetFunctionAllowed(true);
}

GinJavaFunctionInvocationHelper::~GinJavaFunctionInvocationHelper() {}

JavaObjectWeakGlobalRef GinJavaFunctionInvocationHelper::GetObjectWeakRef(content::GinJavaBoundObject::ObjectID object_id) {
  LOG(INFO) << " GinJavaFunctionInvocationHelper GetObjectWeakRef " << object_id;
  //if(object_->bound_object().get()) {
  //  return object_->bound_object()->GetWeakRef();
  //}
  if(bound_object_.get()) {
    return bound_object_->GetWeakRef();
  }
  return JavaObjectWeakGlobalRef();
}

v8::Local<v8::Value> GinJavaFunctionInvocationHelper::Invoke(gin::Arguments* args) {
  if (args->IsConstructCall()) {
    args->isolate()->ThrowException(v8::Exception::Error(gin::StringToV8(
        args->isolate(), kMethodInvocationAsConstructorDisallowed)));
    return v8::Undefined(args->isolate());
  }

  GinJavaBridgeObject* object = NULL;
  if (!args->GetHolder(&object) || !object) {
    args->isolate()->ThrowException(v8::Exception::Error(gin::StringToV8(
        args->isolate(), kMethodInvocationOnNonInjectedObjectDisallowed)));
    return v8::Undefined(args->isolate());
  }

  base::ListValue arguments;
  {
    v8::HandleScope handle_scope(args->isolate());
    v8::Local<v8::Context> context = args->isolate()->GetCurrentContext();
    v8::Local<v8::Value> val;
    while (args->GetNext(&val)) {
      std::unique_ptr<base::Value> arg(converter_->FromV8Value(val, context));
      if (arg.get())
        arguments.Append(std::move(arg));
      else
        arguments.Append(std::make_unique<base::Value>());
    }
  }

  content::GinJavaBridgeError error;
  scoped_refptr<content::GinJavaMethodInvocationHelper> result =
    new content::GinJavaMethodInvocationHelper(
        std::make_unique<content::GinJavaBoundObjectDelegate>(bound_object_),
        method_name_, arguments);

  result->Init(this);
  result->Invoke();
  error = result->GetInvocationError();

  if (result->HoldsPrimitiveResult()) {
    base::Value* v8_result;
    std::unique_ptr<base::ListValue> result_copy(result->GetPrimitiveResult().DeepCopy());
    if(result_copy->Get(0, &v8_result)) {
      return converter_->ToV8Value(v8_result, args->isolate()->GetCurrentContext());
    }
  } else if (!result->GetObjectResult().is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    JavaObjectWeakGlobalRef ref(env, result->GetObjectResult().obj());
    scoped_refptr<content::GinJavaBoundObject> bound_object = content::GinJavaBoundObject::CreateNamed(ref, result->GetSafeAnnotationClass());
    gin::Handle<GinJavaBridgeObject> bridge_object = gin::CreateHandle(args->isolate(), new GinJavaBridgeObject(args->isolate(), bound_object));
    return bridge_object.ToV8();
  }
  return v8::Undefined(args->isolate());
}

}  // namespace andjs
