// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDJS_JAVA_GIN_JAVA_FUNCTION_INVOCATION_HELPER_H_
#define ANDJS_JAVA_GIN_JAVA_FUNCTION_INVOCATION_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/browser/android/java/gin_java_bound_object_delegate.h"
#include "content/browser/android/java/gin_java_bound_object.h"
//#include "andjs/java/gin_java_bridge_object.h"

namespace andjs {

class GinJavaFunctionInvocationHelper : public content::GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:
  //GinJavaFunctionInvocationHelper(const std::string& method_name, scoped_refptr<GinJavaBridgeObject> object);
  GinJavaFunctionInvocationHelper(const std::string& method_name, scoped_refptr<content::GinJavaBoundObject> bound_object);
  ~GinJavaFunctionInvocationHelper() override;

  v8::Local<v8::Value> Invoke(gin::Arguments* args);

  // GinJavaMethodInvocationHelper::DispatcherDelegate
  JavaObjectWeakGlobalRef GetObjectWeakRef(
      content::GinJavaBoundObject::ObjectID object_id) override;

 private:
  std::string method_name_;
  //scoped_refptr<GinJavaBridgeObject> object_;
  scoped_refptr<content::GinJavaBoundObject> bound_object_;
  std::unique_ptr<content::V8ValueConverter> converter_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaFunctionInvocationHelper);
};

}  // namespace content

#endif  // ANDJS_JAVA_GIN_JAVA_FUNCTION_INVOCATION_HELPER_H_
