// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/MinHeight.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {

class CSSParserLocalContext;

namespace CSSLonghand {

const CSSValue* MinHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeWidthOrHeight(
      range, context, CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

}  // namespace CSSLonghand
}  // namespace blink
