// Copyright 2019 wuruxu <wrxzzj@gmail.com> modified
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "andjs/gin_java_bridge_object.h"

#include "base/values.h"
#include "content/browser/android/java/gin_java_bound_object_delegate.h"
#include "content/browser/android/java/gin_java_method_invocation_helper.h"
#include "gin/function_template.h"
#include "andjs/andjs_core.h"

namespace andjs {

namespace {

const char kMethodInvocationAsConstructorDisallowed[] =
    "Java bridge method can't be invoked as a constructor";
const char kMethodInvocationOnNonInjectedObjectDisallowed[] =
    "Java bridge method can't be invoked on a non-injected object";

}  // namespace

GinJavaBridgeObject::GinJavaBridgeObject(AndJSCore* jscore, content::GinJavaBoundObject::ObjectID object_id)
                                         : gin::NamedPropertyInterceptor(jscore->GetContextHolder()->isolate(), this),
                                           object_id_(object_id),
                                           converter_(content::V8ValueConverter::Create()),
                                           template_cache_(jscore->GetContextHolder()->isolate()) {
  converter_->SetDateAllowed(false);
  converter_->SetRegExpAllowed(false);
  converter_->SetFunctionAllowed(true);
  jscore_ = jscore;
}

GinJavaBridgeObject::~GinJavaBridgeObject() {}

gin::ObjectTemplateBuilder GinJavaBridgeObject::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<GinJavaBridgeObject>::GetObjectTemplateBuilder(isolate).AddNamedPropertyInterceptor();
}

v8::Local<v8::Value> GinJavaBridgeObject::GetNamedProperty(
    v8::Isolate* isolate,
    const std::string& property) {
  scoped_refptr<content::GinJavaBoundObject> bound_object = jscore_->GetObject(object_id_);
  bool result = bound_object->HasMethod(property);
  LOG(INFO) << "GetNamedProperty HasMethod(" << property << ") result " << result;
  if (result) {
    return GetFunctionTemplate(isolate, property)
        ->GetFunction(isolate->GetCurrentContext())
        .FromMaybe(v8::Local<v8::Value>());
  } else {
    return v8::Local<v8::Value>();
  }
}

std::vector<std::string> GinJavaBridgeObject::EnumerateNamedProperties(v8::Isolate* isolate) {
  scoped_refptr<content::GinJavaBoundObject> bound_object = jscore_->GetObject(object_id_);
  std::set<std::string> method_names = bound_object->GetMethodNames();
  LOG(INFO) << " EnumerateNamedProperties " << " method_names.size " << method_names.size();

  return std::vector<std::string> (method_names.begin(), method_names.end());
}

JavaObjectWeakGlobalRef GinJavaBridgeObject::GetObjectWeakRef(content::GinJavaBoundObject::ObjectID object_id) {
  LOG(INFO) << " GinJavaBridgeObject GetObjectWeakRef " << object_id;
  return JavaObjectWeakGlobalRef();
}

v8::Local<v8::Value> GinJavaBridgeObject::Invoke(const std::string& method_name, gin::Arguments* args) {
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

  scoped_refptr<content::GinJavaBoundObject> bound_object = jscore_->GetObject(object_id_);
  content::GinJavaBridgeError error;
  scoped_refptr<content::GinJavaMethodInvocationHelper> result =
    new content::GinJavaMethodInvocationHelper(
        std::make_unique<content::GinJavaBoundObjectDelegate>(bound_object),
        method_name, arguments);

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
    return jscore_->InjectObject(result->GetObjectResult(), result->GetSafeAnnotationClass());
  }
  return v8::Undefined(args->isolate());
}

v8::Local<v8::FunctionTemplate> GinJavaBridgeObject::GetFunctionTemplate(
    v8::Isolate* isolate,
    const std::string& name) {
  v8::Local<v8::FunctionTemplate> function_template = template_cache_.Get(name);
  LOG(INFO) << " GetFunctionTemplate " << name << " function_template.IsEmpty " << function_template.IsEmpty();
  if (!function_template.IsEmpty())
    return function_template;
  function_template = gin::CreateFunctionTemplate(
      isolate,
      base::BindRepeating(&GinJavaBridgeObject::Invoke,
                          base::Unretained(this), name));
  template_cache_.Set(name, function_template);
  return function_template;
}

gin::WrapperInfo GinJavaBridgeObject::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace andjs 
