/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebBlob_h
#define WebBlob_h

#include "public/platform/WebBlobData.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace v8 {
class Isolate;
class Object;
class Value;
template <class T>
class Local;
}

namespace blink {

class Blob;

class WebBlob {
 public:
  ~WebBlob() { Reset(); }

  WebBlob() = default;
  WebBlob(const WebBlob& b) { Assign(b); }
  WebBlob& operator=(const WebBlob& b) {
    Assign(b);
    return *this;
  }

  BLINK_EXPORT static WebBlob CreateFromUUID(const WebString& uuid,
                                             const WebString& type,
                                             long long size);
  BLINK_EXPORT static WebBlob CreateFromFile(const WebString& path,
                                             long long size);
  BLINK_EXPORT static WebBlob FromV8Value(v8::Local<v8::Value>);

  BLINK_EXPORT void Reset();
  BLINK_EXPORT void Assign(const WebBlob&);
  BLINK_EXPORT WebString Uuid();

  bool IsNull() const { return private_.IsNull(); }

  BLINK_EXPORT v8::Local<v8::Value> ToV8Value(
      v8::Local<v8::Object> creation_context,
      v8::Isolate*);

#if INSIDE_BLINK
  WebBlob(Blob*);
  WebBlob& operator=(Blob*);
#endif

 protected:
  WebPrivatePtr<Blob> private_;
};

}  // namespace blink

#endif  // WebBlob_h
