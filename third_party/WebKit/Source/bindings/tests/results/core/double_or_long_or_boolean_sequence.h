// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef DoubleOrLongOrBooleanSequence_h
#define DoubleOrLongOrBooleanSequence_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LongOrBoolean;

class CORE_EXPORT DoubleOrLongOrBooleanSequence final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  DoubleOrLongOrBooleanSequence();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsDouble() const { return type_ == SpecificType::kDouble; }
  double GetAsDouble() const;
  void SetDouble(double);
  static DoubleOrLongOrBooleanSequence FromDouble(double);

  bool IsLongOrBooleanSequence() const { return type_ == SpecificType::kLongOrBooleanSequence; }
  const HeapVector<LongOrBoolean>& GetAsLongOrBooleanSequence() const;
  void SetLongOrBooleanSequence(const HeapVector<LongOrBoolean>&);
  static DoubleOrLongOrBooleanSequence FromLongOrBooleanSequence(const HeapVector<LongOrBoolean>&);

  DoubleOrLongOrBooleanSequence(const DoubleOrLongOrBooleanSequence&);
  ~DoubleOrLongOrBooleanSequence();
  DoubleOrLongOrBooleanSequence& operator=(const DoubleOrLongOrBooleanSequence&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kDouble,
    kLongOrBooleanSequence,
  };
  SpecificType type_;

  double double_;
  HeapVector<LongOrBoolean> long_or_boolean_sequence_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const DoubleOrLongOrBooleanSequence&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8DoubleOrLongOrBooleanSequence final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, DoubleOrLongOrBooleanSequence&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const DoubleOrLongOrBooleanSequence&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, DoubleOrLongOrBooleanSequence& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, DoubleOrLongOrBooleanSequence& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<DoubleOrLongOrBooleanSequence> : public NativeValueTraitsBase<DoubleOrLongOrBooleanSequence> {
  CORE_EXPORT static DoubleOrLongOrBooleanSequence NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static DoubleOrLongOrBooleanSequence NullValue() { return DoubleOrLongOrBooleanSequence(); }
};

template <>
struct V8TypeOf<DoubleOrLongOrBooleanSequence> {
  typedef V8DoubleOrLongOrBooleanSequence Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::DoubleOrLongOrBooleanSequence);

#endif  // DoubleOrLongOrBooleanSequence_h
