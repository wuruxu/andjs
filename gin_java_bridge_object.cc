// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "andjs/gin_java_bridge_object.h"

#include "base/values.h"
#include "andjs/gin_java_function_invocation_helper.h"
#include "content/browser/android/java/gin_java_bound_object_delegate.h"
#include "content/browser/android/java/gin_java_method_invocation_helper.h"
#include "gin/function_template.h"

namespace andjs {

GinJavaBridgeObject::GinJavaBridgeObject(
                     v8::Isolate* isolate,
                     scoped_refptr<content::GinJavaBoundObject> bound_object) 
                     : gin::NamedPropertyInterceptor(isolate, this),
                       bound_object_ (bound_object),
                       template_cache_(isolate) {}

GinJavaBridgeObject::~GinJavaBridgeObject() {}

gin::ObjectTemplateBuilder GinJavaBridgeObject::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<GinJavaBridgeObject>::GetObjectTemplateBuilder(isolate).AddNamedPropertyInterceptor();
}

v8::Local<v8::Value> GinJavaBridgeObject::GetNamedProperty(
    v8::Isolate* isolate,
    const std::string& property) {
  bool result = bound_object_->HasMethod(property);
  LOG(INFO) << "GetNamedProperty HasMethod(" << property << ") result " << result;
  if (result) {
    return GetFunctionTemplate(isolate, property)
        ->GetFunction(isolate->GetCurrentContext())
        .FromMaybe(v8::Local<v8::Value>());
  } else {
    return v8::Local<v8::Value>();
  }
}

std::vector<std::string> GinJavaBridgeObject::EnumerateNamedProperties(
    v8::Isolate* isolate) {
  std::set<std::string> method_names;
  method_names = bound_object_->GetMethodNames();
  LOG(INFO) << " EnumerateNamedProperties " << " method_names.size " << method_names.size();

  return std::vector<std::string> (method_names.begin(), method_names.end());
}

v8::Local<v8::FunctionTemplate> GinJavaBridgeObject::GetFunctionTemplate(
    v8::Isolate* isolate,
    const std::string& name) {
  v8::Local<v8::FunctionTemplate> function_template = template_cache_.Get(name);
  LOG(INFO) << " GetFunctionTemplate " << name << " function_template.IsEmpty " << function_template.IsEmpty();
  if (!function_template.IsEmpty())
    return function_template;
  function_template = gin::CreateFunctionTemplate(
      isolate, base::Bind(&GinJavaFunctionInvocationHelper::Invoke,
                          base::Owned(new GinJavaFunctionInvocationHelper(
                              name, bound_object_))));
  template_cache_.Set(name, function_template);
  return function_template;
}

gin::WrapperInfo GinJavaBridgeObject::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace andjs 
