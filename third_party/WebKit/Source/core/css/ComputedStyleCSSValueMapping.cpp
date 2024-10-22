/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "core/css/ComputedStyleCSSValueMapping.h"

#include "base/macros.h"
#include "core/StylePropertyShorthand.h"
#include "core/animation/css/CSSAnimationData.h"
#include "core/animation/css/CSSTransitionData.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSCounterValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFontStyleRangeValue.h"
#include "core/css/CSSFontVariationValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/ZoomAdjustedPixelValue.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ContentData.h"
#include "core/style/CursorData.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/StyleNonInheritedVariables.h"
#include "platform/LengthFunctions.h"

namespace blink {

using namespace cssvalue;

inline static bool IsFlexOrGrid(const ComputedStyle* style) {
  return style && style->IsDisplayFlexibleOrGridBox();
}

inline static CSSValue* ZoomAdjustedPixelValueOrAuto(
    const Length& length,
    const ComputedStyle& style) {
  if (length.IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);
  return ZoomAdjustedPixelValue(length.Value(), style);
}

static CSSValue* PixelValueForUnzoomedLength(
    const UnzoomedLength& unzoomed_length,
    const ComputedStyle& style) {
  const Length& length = unzoomed_length.length();
  if (length.IsFixed())
    return CSSPrimitiveValue::Create(length.Value(),
                                     CSSPrimitiveValue::UnitType::kPixels);
  return CSSValue::Create(length, style.EffectiveZoom());
}

static CSSValue* ValueForPositionOffset(const ComputedStyle& style,
                                        const CSSProperty& property,
                                        const LayoutObject* layout_object) {
  std::pair<const Length*, const Length*> positions;
  switch (property.PropertyID()) {
    case CSSPropertyLeft:
      positions = std::make_pair(&style.Left(), &style.Right());
      break;
    case CSSPropertyRight:
      positions = std::make_pair(&style.Right(), &style.Left());
      break;
    case CSSPropertyTop:
      positions = std::make_pair(&style.Top(), &style.Bottom());
      break;
    case CSSPropertyBottom:
      positions = std::make_pair(&style.Bottom(), &style.Top());
      break;
    default:
      NOTREACHED();
      return nullptr;
  }
  DCHECK(positions.first && positions.second);

  const Length& offset = *positions.first;
  const Length& opposite = *positions.second;

  if (offset.IsPercentOrCalc() && layout_object && layout_object->IsBox() &&
      layout_object->IsPositioned()) {
    LayoutUnit containing_block_size =
        (property.IDEquals(CSSPropertyLeft) ||
         property.IDEquals(CSSPropertyRight))
            ? ToLayoutBox(layout_object)
                  ->ContainingBlockLogicalWidthForContent()
            : ToLayoutBox(layout_object)
                  ->ContainingBlockLogicalHeightForGetComputedStyle();
    return ZoomAdjustedPixelValue(ValueForLength(offset, containing_block_size),
                                  style);
  }

  if (offset.IsAuto() && layout_object) {
    // If the property applies to a positioned element and the resolved value of
    // the display property is not none, the resolved value is the used value.
    // Position offsets have special meaning for position sticky so we return
    // auto when offset.isAuto() on a sticky position object:
    // https://crbug.com/703816.
    if (layout_object->IsRelPositioned()) {
      // If e.g. left is auto and right is not auto, then left's computed value
      // is negative right. So we get the opposite length unit and see if it is
      // auto.
      if (opposite.IsAuto())
        return CSSPrimitiveValue::Create(0,
                                         CSSPrimitiveValue::UnitType::kPixels);

      if (opposite.IsPercentOrCalc()) {
        if (layout_object->IsBox()) {
          LayoutUnit containing_block_size =
              (property.IDEquals(CSSPropertyLeft) ||
               property.IDEquals(CSSPropertyRight))
                  ? ToLayoutBox(layout_object)
                        ->ContainingBlockLogicalWidthForContent()
                  : ToLayoutBox(layout_object)
                        ->ContainingBlockLogicalHeightForGetComputedStyle();
          return ZoomAdjustedPixelValue(
              -FloatValueForLength(opposite, containing_block_size), style);
        }
        // FIXME:  fall back to auto for position:relative, display:inline
        return CSSIdentifierValue::Create(CSSValueAuto);
      }

      // Length doesn't provide operator -, so multiply by -1.
      Length negated_opposite = opposite;
      negated_opposite *= -1.f;
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          negated_opposite, style);
    }

    if (layout_object->IsOutOfFlowPositioned() && layout_object->IsBox()) {
      // For fixed and absolute positioned elements, the top, left, bottom, and
      // right are defined relative to the corresponding sides of the containing
      // block.
      LayoutBlock* container = layout_object->ContainingBlock();
      const LayoutBox* layout_box = ToLayoutBox(layout_object);

      // clientOffset is the distance from this object's border edge to the
      // container's padding edge. Thus it includes margins which we subtract
      // below.
      const LayoutSize client_offset =
          layout_box->LocationOffset() -
          LayoutSize(container->ClientLeft(), container->ClientTop());
      LayoutUnit position;

      switch (property.PropertyID()) {
        case CSSPropertyLeft:
          position = client_offset.Width() - layout_box->MarginLeft();
          break;
        case CSSPropertyTop:
          position = client_offset.Height() - layout_box->MarginTop();
          break;
        case CSSPropertyRight:
          position = container->ClientWidth() - layout_box->MarginRight() -
                     (layout_box->OffsetWidth() + client_offset.Width());
          break;
        case CSSPropertyBottom:
          position = container->ClientHeight() - layout_box->MarginBottom() -
                     (layout_box->OffsetHeight() + client_offset.Height());
          break;
        default:
          NOTREACHED();
      }
      return ZoomAdjustedPixelValue(position, style);
    }
  }

  if (offset.IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);

  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(offset, style);
}

static CSSValueList* ValueForItemPositionWithOverflowAlignment(
    const StyleSelfAlignmentData& data) {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();
  if (data.PositionType() == ItemPositionType::kLegacy)
    result->Append(*CSSIdentifierValue::Create(CSSValueLegacy));
  if (data.GetPosition() == ItemPosition::kBaseline) {
    result->Append(
        *CSSValuePair::Create(CSSIdentifierValue::Create(CSSValueBaseline),
                              CSSIdentifierValue::Create(CSSValueBaseline),
                              CSSValuePair::kDropIdenticalValues));
  } else if (data.GetPosition() == ItemPosition::kLastBaseline) {
    result->Append(
        *CSSValuePair::Create(CSSIdentifierValue::Create(CSSValueLast),
                              CSSIdentifierValue::Create(CSSValueBaseline),
                              CSSValuePair::kDropIdenticalValues));
  } else {
    result->Append(*CSSIdentifierValue::Create(data.GetPosition()));
  }
  if (data.GetPosition() >= ItemPosition::kCenter &&
      data.Overflow() != OverflowAlignment::kDefault)
    result->Append(*CSSIdentifierValue::Create(data.Overflow()));
  DCHECK_LE(result->length(), 2u);
  return result;
}

static CSSValueList* ValuesForGridShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSlashSeparated();
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        *shorthand.properties()[i], style, layout_object, styled_node,
        allow_visited_style);
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

static CSSValueList* ValuesForShorthandProperty(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        *shorthand.properties()[i], style, layout_object, styled_node,
        allow_visited_style);
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

static CSSValue* ExpandNoneLigaturesValue() {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueNoCommonLigatures));
  list->Append(*CSSIdentifierValue::Create(CSSValueNoDiscretionaryLigatures));
  list->Append(*CSSIdentifierValue::Create(CSSValueNoHistoricalLigatures));
  list->Append(*CSSIdentifierValue::Create(CSSValueNoContextual));
  return list;
}

static CSSValue* ValuesForFontVariantProperty(const ComputedStyle& style,
                                              const LayoutObject* layout_object,
                                              Node* styled_node,
                                              bool allow_visited_style) {
  enum VariantShorthandCases {
    kAllNormal,
    kNoneLigatures,
    kConcatenateNonNormal
  };
  StylePropertyShorthand shorthand = fontVariantShorthand();
  VariantShorthandCases shorthand_case = kAllNormal;
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        *shorthand.properties()[i], style, layout_object, styled_node,
        allow_visited_style);

    if (shorthand_case == kAllNormal && value->IsIdentifierValue() &&
        ToCSSIdentifierValue(value)->GetValueID() == CSSValueNone &&
        shorthand.properties()[i]->IDEquals(CSSPropertyFontVariantLigatures)) {
      shorthand_case = kNoneLigatures;
    } else if (!(value->IsIdentifierValue() &&
                 ToCSSIdentifierValue(value)->GetValueID() == CSSValueNormal)) {
      shorthand_case = kConcatenateNonNormal;
      break;
    }
  }

  switch (shorthand_case) {
    case kAllNormal:
      return CSSIdentifierValue::Create(CSSValueNormal);
    case kNoneLigatures:
      return CSSIdentifierValue::Create(CSSValueNone);
    case kConcatenateNonNormal: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      for (size_t i = 0; i < shorthand.length(); ++i) {
        const CSSValue* value = ComputedStyleCSSValueMapping::Get(
            *shorthand.properties()[i], style, layout_object, styled_node,
            allow_visited_style);
        DCHECK(value);
        if (value->IsIdentifierValue() &&
            ToCSSIdentifierValue(value)->GetValueID() == CSSValueNone) {
          list->Append(*ExpandNoneLigaturesValue());
        } else if (!(value->IsIdentifierValue() &&
                     ToCSSIdentifierValue(value)->GetValueID() ==
                         CSSValueNormal)) {
          list->Append(*value);
        }
      }
      return list;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValueList*
ValueForContentPositionAndDistributionWithOverflowAlignment(
    const StyleContentAlignmentData& data) {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();
  // Handle content-distribution values
  if (data.Distribution() != ContentDistributionType::kDefault)
    result->Append(*CSSIdentifierValue::Create(data.Distribution()));

  // Handle content-position values (either as fallback or actual value)
  switch (data.GetPosition()) {
    case ContentPosition::kNormal:
      // Handle 'normal' value, not valid as content-distribution fallback.
      if (data.Distribution() == ContentDistributionType::kDefault) {
        result->Append(*CSSIdentifierValue::Create(CSSValueNormal));
      }
      break;
    case ContentPosition::kLastBaseline:
      result->Append(
          *CSSValuePair::Create(CSSIdentifierValue::Create(CSSValueLast),
                                CSSIdentifierValue::Create(CSSValueBaseline),
                                CSSValuePair::kDropIdenticalValues));
      break;
    default:
      result->Append(*CSSIdentifierValue::Create(data.GetPosition()));
  }

  // Handle overflow-alignment (only allowed for content-position values)
  if ((data.GetPosition() >= ContentPosition::kCenter ||
       data.Distribution() != ContentDistributionType::kDefault) &&
      data.Overflow() != OverflowAlignment::kDefault)
    result->Append(*CSSIdentifierValue::Create(data.Overflow()));
  DCHECK_GT(result->length(), 0u);
  DCHECK_LE(result->length(), 3u);
  return result;
}

static CSSValue* ValueForLineHeight(const ComputedStyle& style) {
  const Length& length = style.LineHeight();
  if (length.IsNegative())
    return CSSIdentifierValue::Create(CSSValueNormal);

  return ZoomAdjustedPixelValue(
      FloatValueForLength(length, style.GetFontDescription().ComputedSize()),
      style);
}

static CSSValueID IdentifierForFamily(const AtomicString& family) {
  if (family == FontFamilyNames::webkit_cursive)
    return CSSValueCursive;
  if (family == FontFamilyNames::webkit_fantasy)
    return CSSValueFantasy;
  if (family == FontFamilyNames::webkit_monospace)
    return CSSValueMonospace;
  if (family == FontFamilyNames::webkit_pictograph)
    return CSSValueWebkitPictograph;
  if (family == FontFamilyNames::webkit_sans_serif)
    return CSSValueSansSerif;
  if (family == FontFamilyNames::webkit_serif)
    return CSSValueSerif;
  return CSSValueInvalid;
}

static CSSValue* ValueForFamily(const AtomicString& family) {
  if (CSSValueID family_identifier = IdentifierForFamily(family))
    return CSSIdentifierValue::Create(family_identifier);
  return CSSFontFamilyValue::Create(family.GetString());
}

static CSSValueList* ValueForFontFamily(const ComputedStyle& style) {
  const FontFamily& first_family = style.GetFontDescription().Family();
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FontFamily* family = &first_family; family;
       family = family->Next())
    list->Append(*ValueForFamily(family->Family()));
  return list;
}

static CSSPrimitiveValue* ValueForFontSize(const ComputedStyle& style) {
  return ZoomAdjustedPixelValue(style.GetFontDescription().ComputedSize(),
                                style);
}

static CSSPrimitiveValue* ValueForFontStretch(const ComputedStyle& style) {
  return CSSPrimitiveValue::Create(style.GetFontDescription().Stretch(),
                                   CSSPrimitiveValue::UnitType::kPercentage);
}

static CSSIdentifierValue* ValueForFontStretchAsKeyword(
    const ComputedStyle& style) {
  FontSelectionValue stretch_value = style.GetFontDescription().Stretch();
  CSSValueID value_id = CSSValueInvalid;
  if (stretch_value == UltraCondensedWidthValue())
    value_id = CSSValueUltraCondensed;
  if (stretch_value == UltraCondensedWidthValue())
    value_id = CSSValueUltraCondensed;
  if (stretch_value == ExtraCondensedWidthValue())
    value_id = CSSValueExtraCondensed;
  if (stretch_value == CondensedWidthValue())
    value_id = CSSValueCondensed;
  if (stretch_value == SemiCondensedWidthValue())
    value_id = CSSValueSemiCondensed;
  if (stretch_value == NormalWidthValue())
    value_id = CSSValueNormal;
  if (stretch_value == SemiExpandedWidthValue())
    value_id = CSSValueSemiExpanded;
  if (stretch_value == ExpandedWidthValue())
    value_id = CSSValueExpanded;
  if (stretch_value == ExtraExpandedWidthValue())
    value_id = CSSValueExtraExpanded;
  if (stretch_value == UltraExpandedWidthValue())
    value_id = CSSValueUltraExpanded;

  if (value_id != CSSValueInvalid)
    return CSSIdentifierValue::Create(value_id);
  return nullptr;
}

static CSSIdentifierValue* ValueForFontStyle(const ComputedStyle& style) {
  FontSelectionValue angle = style.GetFontDescription().Style();
  if (angle == NormalSlopeValue()) {
    return CSSIdentifierValue::Create(CSSValueNormal);
  }

  if (angle == ItalicSlopeValue()) {
    return CSSIdentifierValue::Create(CSSValueItalic);
  }

  NOTREACHED();
  return CSSIdentifierValue::Create(CSSValueNormal);
}

static CSSPrimitiveValue* ValueForFontWeight(const ComputedStyle& style) {
  return CSSPrimitiveValue::Create(style.GetFontDescription().Weight(),
                                   CSSPrimitiveValue::UnitType::kNumber);
}

static CSSIdentifierValue* ValueForFontVariantCaps(const ComputedStyle& style) {
  FontDescription::FontVariantCaps variant_caps =
      style.GetFontDescription().VariantCaps();
  switch (variant_caps) {
    case FontDescription::kCapsNormal:
      return CSSIdentifierValue::Create(CSSValueNormal);
    case FontDescription::kSmallCaps:
      return CSSIdentifierValue::Create(CSSValueSmallCaps);
    case FontDescription::kAllSmallCaps:
      return CSSIdentifierValue::Create(CSSValueAllSmallCaps);
    case FontDescription::kPetiteCaps:
      return CSSIdentifierValue::Create(CSSValuePetiteCaps);
    case FontDescription::kAllPetiteCaps:
      return CSSIdentifierValue::Create(CSSValueAllPetiteCaps);
    case FontDescription::kUnicase:
      return CSSIdentifierValue::Create(CSSValueUnicase);
    case FontDescription::kTitlingCaps:
      return CSSIdentifierValue::Create(CSSValueTitlingCaps);
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValue* ValueForFontVariantLigatures(const ComputedStyle& style) {
  FontDescription::LigaturesState common_ligatures_state =
      style.GetFontDescription().CommonLigaturesState();
  FontDescription::LigaturesState discretionary_ligatures_state =
      style.GetFontDescription().DiscretionaryLigaturesState();
  FontDescription::LigaturesState historical_ligatures_state =
      style.GetFontDescription().HistoricalLigaturesState();
  FontDescription::LigaturesState contextual_ligatures_state =
      style.GetFontDescription().ContextualLigaturesState();
  if (common_ligatures_state == FontDescription::kNormalLigaturesState &&
      discretionary_ligatures_state == FontDescription::kNormalLigaturesState &&
      historical_ligatures_state == FontDescription::kNormalLigaturesState &&
      contextual_ligatures_state == FontDescription::kNormalLigaturesState)
    return CSSIdentifierValue::Create(CSSValueNormal);

  if (common_ligatures_state == FontDescription::kDisabledLigaturesState &&
      discretionary_ligatures_state ==
          FontDescription::kDisabledLigaturesState &&
      historical_ligatures_state == FontDescription::kDisabledLigaturesState &&
      contextual_ligatures_state == FontDescription::kDisabledLigaturesState)
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  if (common_ligatures_state != FontDescription::kNormalLigaturesState)
    value_list->Append(*CSSIdentifierValue::Create(
        common_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueNoCommonLigatures
            : CSSValueCommonLigatures));
  if (discretionary_ligatures_state != FontDescription::kNormalLigaturesState)
    value_list->Append(*CSSIdentifierValue::Create(
        discretionary_ligatures_state ==
                FontDescription::kDisabledLigaturesState
            ? CSSValueNoDiscretionaryLigatures
            : CSSValueDiscretionaryLigatures));
  if (historical_ligatures_state != FontDescription::kNormalLigaturesState)
    value_list->Append(*CSSIdentifierValue::Create(
        historical_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueNoHistoricalLigatures
            : CSSValueHistoricalLigatures));
  if (contextual_ligatures_state != FontDescription::kNormalLigaturesState)
    value_list->Append(*CSSIdentifierValue::Create(
        contextual_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueNoContextual
            : CSSValueContextual));
  return value_list;
}

static CSSValue* ValueForFontVariantNumeric(const ComputedStyle& style) {
  FontVariantNumeric variant_numeric =
      style.GetFontDescription().VariantNumeric();
  if (variant_numeric.IsAllNormal())
    return CSSIdentifierValue::Create(CSSValueNormal);

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  if (variant_numeric.NumericFigureValue() != FontVariantNumeric::kNormalFigure)
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericFigureValue() == FontVariantNumeric::kLiningNums
            ? CSSValueLiningNums
            : CSSValueOldstyleNums));
  if (variant_numeric.NumericSpacingValue() !=
      FontVariantNumeric::kNormalSpacing) {
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericSpacingValue() ==
                FontVariantNumeric::kProportionalNums
            ? CSSValueProportionalNums
            : CSSValueTabularNums));
  }
  if (variant_numeric.NumericFractionValue() !=
      FontVariantNumeric::kNormalFraction)
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericFractionValue() ==
                FontVariantNumeric::kDiagonalFractions
            ? CSSValueDiagonalFractions
            : CSSValueStackedFractions));
  if (variant_numeric.OrdinalValue() == FontVariantNumeric::kOrdinalOn)
    value_list->Append(*CSSIdentifierValue::Create(CSSValueOrdinal));
  if (variant_numeric.SlashedZeroValue() == FontVariantNumeric::kSlashedZeroOn)
    value_list->Append(*CSSIdentifierValue::Create(CSSValueSlashedZero));

  return value_list;
}

static CSSValue* ValueForFontVariantEastAsian(const ComputedStyle& style) {
  FontVariantEastAsian east_asian =
      style.GetFontDescription().VariantEastAsian();
  if (east_asian.IsAllNormal())
    return CSSIdentifierValue::Create(CSSValueNormal);

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  switch (east_asian.Form()) {
    case FontVariantEastAsian::kNormalForm:
      break;
    case FontVariantEastAsian::kJis78:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueJis78));
      break;
    case FontVariantEastAsian::kJis83:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueJis83));
      break;
    case FontVariantEastAsian::kJis90:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueJis90));
      break;
    case FontVariantEastAsian::kJis04:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueJis04));
      break;
    case FontVariantEastAsian::kSimplified:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueSimplified));
      break;
    case FontVariantEastAsian::kTraditional:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueTraditional));
      break;
    default:
      NOTREACHED();
  }
  switch (east_asian.Width()) {
    case FontVariantEastAsian::kNormalWidth:
      break;
    case FontVariantEastAsian::kFullWidth:
      value_list->Append(*CSSIdentifierValue::Create(CSSValueFullWidth));
      break;
    case FontVariantEastAsian::kProportionalWidth:
      value_list->Append(
          *CSSIdentifierValue::Create(CSSValueProportionalWidth));
      break;
    default:
      NOTREACHED();
  }
  if (east_asian.Ruby())
    value_list->Append(*CSSIdentifierValue::Create(CSSValueRuby));
  return value_list;
}

static CSSValue* SpecifiedValueForGridTrackBreadth(
    const GridLength& track_breadth,
    const ComputedStyle& style) {
  if (!track_breadth.IsLength())
    return CSSPrimitiveValue::Create(track_breadth.Flex(),
                                     CSSPrimitiveValue::UnitType::kFraction);

  const Length& track_breadth_length = track_breadth.length();
  if (track_breadth_length.IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      track_breadth_length, style);
}

static CSSValue* SpecifiedValueForGridTrackSize(const GridTrackSize& track_size,
                                                const ComputedStyle& style) {
  switch (track_size.GetType()) {
    case kLengthTrackSizing:
      return SpecifiedValueForGridTrackBreadth(track_size.MinTrackBreadth(),
                                               style);
    case kMinMaxTrackSizing: {
      if (track_size.MinTrackBreadth().IsAuto() &&
          track_size.MaxTrackBreadth().IsFlex()) {
        return CSSPrimitiveValue::Create(
            track_size.MaxTrackBreadth().Flex(),
            CSSPrimitiveValue::UnitType::kFraction);
      }

      auto* min_max_track_breadths = CSSFunctionValue::Create(CSSValueMinmax);
      min_max_track_breadths->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.MinTrackBreadth(), style));
      min_max_track_breadths->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.MaxTrackBreadth(), style));
      return min_max_track_breadths;
    }
    case kFitContentTrackSizing: {
      auto* fit_content_track_breadth =
          CSSFunctionValue::Create(CSSValueFitContent);
      fit_content_track_breadth->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.FitContentTrackBreadth(), style));
      return fit_content_track_breadth;
    }
  }
  NOTREACHED();
  return nullptr;
}

class OrderedNamedLinesCollector {
  STACK_ALLOCATED();

 public:
  OrderedNamedLinesCollector(const ComputedStyle& style,
                             bool is_row_axis,
                             size_t auto_repeat_tracks_count)
      : ordered_named_grid_lines_(is_row_axis
                                      ? style.OrderedNamedGridColumnLines()
                                      : style.OrderedNamedGridRowLines()),
        ordered_named_auto_repeat_grid_lines_(
            is_row_axis ? style.AutoRepeatOrderedNamedGridColumnLines()
                        : style.AutoRepeatOrderedNamedGridRowLines()),
        insertion_point_(is_row_axis
                             ? style.GridAutoRepeatColumnsInsertionPoint()
                             : style.GridAutoRepeatRowsInsertionPoint()),
        auto_repeat_total_tracks_(auto_repeat_tracks_count),
        auto_repeat_track_list_length_(
            is_row_axis ? style.GridAutoRepeatColumns().size()
                        : style.GridAutoRepeatRows().size()) {}

  bool IsEmpty() const {
    return ordered_named_grid_lines_.IsEmpty() &&
           ordered_named_auto_repeat_grid_lines_.IsEmpty();
  }
  void CollectLineNamesForIndex(CSSGridLineNamesValue&, size_t index) const;

 private:
  enum NamedLinesType { kNamedLines, kAutoRepeatNamedLines };
  void AppendLines(CSSGridLineNamesValue&, size_t index, NamedLinesType) const;

  const OrderedNamedGridLines& ordered_named_grid_lines_;
  const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines_;
  size_t insertion_point_;
  size_t auto_repeat_total_tracks_;
  size_t auto_repeat_track_list_length_;
  DISALLOW_COPY_AND_ASSIGN(OrderedNamedLinesCollector);
};

void OrderedNamedLinesCollector::AppendLines(
    CSSGridLineNamesValue& line_names_value,
    size_t index,
    NamedLinesType type) const {
  auto iter = type == kNamedLines
                  ? ordered_named_grid_lines_.find(index)
                  : ordered_named_auto_repeat_grid_lines_.find(index);
  auto end_iter = type == kNamedLines
                      ? ordered_named_grid_lines_.end()
                      : ordered_named_auto_repeat_grid_lines_.end();
  if (iter == end_iter)
    return;

  for (auto line_name : iter->value)
    line_names_value.Append(
        *CSSCustomIdentValue::Create(AtomicString(line_name)));
}

void OrderedNamedLinesCollector::CollectLineNamesForIndex(
    CSSGridLineNamesValue& line_names_value,
    size_t i) const {
  DCHECK(!IsEmpty());
  if (ordered_named_auto_repeat_grid_lines_.IsEmpty() || i < insertion_point_) {
    AppendLines(line_names_value, i, kNamedLines);
    return;
  }

  DCHECK(auto_repeat_total_tracks_);

  if (i > insertion_point_ + auto_repeat_total_tracks_) {
    AppendLines(line_names_value, i - (auto_repeat_total_tracks_ - 1),
                kNamedLines);
    return;
  }

  if (i == insertion_point_) {
    AppendLines(line_names_value, i, kNamedLines);
    AppendLines(line_names_value, 0, kAutoRepeatNamedLines);
    return;
  }

  if (i == insertion_point_ + auto_repeat_total_tracks_) {
    AppendLines(line_names_value, auto_repeat_track_list_length_,
                kAutoRepeatNamedLines);
    AppendLines(line_names_value, insertion_point_ + 1, kNamedLines);
    return;
  }

  size_t auto_repeat_index_in_first_repetition =
      (i - insertion_point_) % auto_repeat_track_list_length_;
  if (!auto_repeat_index_in_first_repetition && i > insertion_point_)
    AppendLines(line_names_value, auto_repeat_track_list_length_,
                kAutoRepeatNamedLines);
  AppendLines(line_names_value, auto_repeat_index_in_first_repetition,
              kAutoRepeatNamedLines);
}

static void AddValuesForNamedGridLinesAtIndex(
    OrderedNamedLinesCollector& collector,
    size_t i,
    CSSValueList& list) {
  if (collector.IsEmpty())
    return;

  CSSGridLineNamesValue* line_names = CSSGridLineNamesValue::Create();
  collector.CollectLineNamesForIndex(*line_names, i);
  if (line_names->length())
    list.Append(*line_names);
}

static CSSValue* ValueForGridTrackSizeList(GridTrackSizingDirection direction,
                                           const ComputedStyle& style) {
  const Vector<GridTrackSize>& auto_track_sizes =
      direction == kForColumns ? style.GridAutoColumns() : style.GridAutoRows();

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (auto& track_size : auto_track_sizes)
    list->Append(*SpecifiedValueForGridTrackSize(track_size, style));
  return list;
}

static CSSValue* ValueForGridTrackList(GridTrackSizingDirection direction,
                                       const LayoutObject* layout_object,
                                       const ComputedStyle& style) {
  bool is_row_axis = direction == kForColumns;
  const Vector<GridTrackSize>& track_sizes =
      is_row_axis ? style.GridTemplateColumns() : style.GridTemplateRows();
  const Vector<GridTrackSize>& auto_repeat_track_sizes =
      is_row_axis ? style.GridAutoRepeatColumns() : style.GridAutoRepeatRows();
  bool is_layout_grid = layout_object && layout_object->IsLayoutGrid();

  // Handle the 'none' case.
  bool track_list_is_empty =
      track_sizes.IsEmpty() && auto_repeat_track_sizes.IsEmpty();
  if (is_layout_grid && track_list_is_empty) {
    // For grids we should consider every listed track, whether implicitly or
    // explicitly created. Empty grids have a sole grid line per axis.
    auto& positions = is_row_axis
                          ? ToLayoutGrid(layout_object)->ColumnPositions()
                          : ToLayoutGrid(layout_object)->RowPositions();
    track_list_is_empty = positions.size() == 1;
  }

  if (track_list_is_empty)
    return CSSIdentifierValue::Create(CSSValueNone);

  size_t auto_repeat_total_tracks =
      is_layout_grid
          ? ToLayoutGrid(layout_object)->AutoRepeatCountForDirection(direction)
          : 0;
  OrderedNamedLinesCollector collector(style, is_row_axis,
                                       auto_repeat_total_tracks);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  size_t insertion_index;
  if (is_layout_grid) {
    const auto* grid = ToLayoutGrid(layout_object);
    Vector<LayoutUnit> computed_track_sizes =
        grid->TrackSizesForComputedStyle(direction);
    size_t num_tracks = computed_track_sizes.size();

    for (size_t i = 0; i < num_tracks; ++i) {
      AddValuesForNamedGridLinesAtIndex(collector, i, *list);
      list->Append(*ZoomAdjustedPixelValue(computed_track_sizes[i], style));
    }
    AddValuesForNamedGridLinesAtIndex(collector, num_tracks + 1, *list);

    insertion_index = num_tracks;
  } else {
    for (size_t i = 0; i < track_sizes.size(); ++i) {
      AddValuesForNamedGridLinesAtIndex(collector, i, *list);
      list->Append(*SpecifiedValueForGridTrackSize(track_sizes[i], style));
    }
    insertion_index = track_sizes.size();
  }
  // Those are the trailing <string>* allowed in the syntax.
  AddValuesForNamedGridLinesAtIndex(collector, insertion_index, *list);
  return list;
}

static CSSValue* ValueForGridPosition(const GridPosition& position) {
  if (position.IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);

  if (position.IsNamedGridArea())
    return CSSCustomIdentValue::Create(position.NamedGridLine());

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (position.IsSpan()) {
    list->Append(*CSSIdentifierValue::Create(CSSValueSpan));
    list->Append(*CSSPrimitiveValue::Create(
        position.SpanPosition(), CSSPrimitiveValue::UnitType::kNumber));
  } else {
    list->Append(*CSSPrimitiveValue::Create(
        position.IntegerPosition(), CSSPrimitiveValue::UnitType::kNumber));
  }

  if (!position.NamedGridLine().IsNull())
    list->Append(*CSSCustomIdentValue::Create(position.NamedGridLine()));
  return list;
}

static LayoutRect SizingBox(const LayoutObject& layout_object) {
  if (!layout_object.IsBox())
    return LayoutRect();

  const LayoutBox& box = ToLayoutBox(layout_object);
  return box.StyleRef().BoxSizing() == EBoxSizing::kBorderBox
             ? box.BorderBoxRect()
             : box.ComputedCSSContentBoxRect();
}

static CSSValue* RenderTextDecorationFlagsToCSSValue(
    TextDecoration text_decoration) {
  // Blink value is ignored.
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (EnumHasFlags(text_decoration, TextDecoration::kUnderline))
    list->Append(*CSSIdentifierValue::Create(CSSValueUnderline));
  if (EnumHasFlags(text_decoration, TextDecoration::kOverline))
    list->Append(*CSSIdentifierValue::Create(CSSValueOverline));
  if (EnumHasFlags(text_decoration, TextDecoration::kLineThrough))
    list->Append(*CSSIdentifierValue::Create(CSSValueLineThrough));

  if (!list->length())
    return CSSIdentifierValue::Create(CSSValueNone);
  return list;
}

static CSSValue* ValueForTextDecorationStyle(
    ETextDecorationStyle text_decoration_style) {
  switch (text_decoration_style) {
    case ETextDecorationStyle::kSolid:
      return CSSIdentifierValue::Create(CSSValueSolid);
    case ETextDecorationStyle::kDouble:
      return CSSIdentifierValue::Create(CSSValueDouble);
    case ETextDecorationStyle::kDotted:
      return CSSIdentifierValue::Create(CSSValueDotted);
    case ETextDecorationStyle::kDashed:
      return CSSIdentifierValue::Create(CSSValueDashed);
    case ETextDecorationStyle::kWavy:
      return CSSIdentifierValue::Create(CSSValueWavy);
  }

  NOTREACHED();
  return CSSInitialValue::Create();
}

static CSSValue* ValueForTextDecorationSkipInk(
    ETextDecorationSkipInk text_decoration_skip_ink) {
  if (text_decoration_skip_ink == ETextDecorationSkipInk::kNone)
    return CSSIdentifierValue::Create(CSSValueNone);
  return CSSIdentifierValue::Create(CSSValueAuto);
}

static CSSValue* TouchActionFlagsToCSSValue(TouchAction touch_action) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (touch_action == TouchAction::kTouchActionAuto) {
    list->Append(*CSSIdentifierValue::Create(CSSValueAuto));
  } else if (touch_action == TouchAction::kTouchActionNone) {
    list->Append(*CSSIdentifierValue::Create(CSSValueNone));
  } else if (touch_action == TouchAction::kTouchActionManipulation) {
    list->Append(*CSSIdentifierValue::Create(CSSValueManipulation));
  } else {
    if ((touch_action & TouchAction::kTouchActionPanX) ==
        TouchAction::kTouchActionPanX)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanX));
    else if (touch_action & TouchAction::kTouchActionPanLeft)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanLeft));
    else if (touch_action & TouchAction::kTouchActionPanRight)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanRight));
    if ((touch_action & TouchAction::kTouchActionPanY) ==
        TouchAction::kTouchActionPanY)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanY));
    else if (touch_action & TouchAction::kTouchActionPanUp)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanUp));
    else if (touch_action & TouchAction::kTouchActionPanDown)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanDown));

    if ((touch_action & TouchAction::kTouchActionPinchZoom) ==
        TouchAction::kTouchActionPinchZoom)
      list->Append(*CSSIdentifierValue::Create(CSSValuePinchZoom));
  }

  DCHECK(list->length());
  return list;
}

static CSSValue* ValueForWillChange(
    const Vector<CSSPropertyID>& will_change_properties,
    bool will_change_contents,
    bool will_change_scroll_position) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (will_change_contents)
    list->Append(*CSSIdentifierValue::Create(CSSValueContents));
  if (will_change_scroll_position)
    list->Append(*CSSIdentifierValue::Create(CSSValueScrollPosition));
  for (size_t i = 0; i < will_change_properties.size(); ++i)
    list->Append(*CSSCustomIdentValue::Create(will_change_properties[i]));
  if (!list->length())
    list->Append(*CSSIdentifierValue::Create(CSSValueAuto));
  return list;
}

static CSSValue* ValueForAnimationDelay(const CSSTimingData* timing_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (timing_data) {
    for (size_t i = 0; i < timing_data->DelayList().size(); ++i)
      list->Append(*CSSPrimitiveValue::Create(
          timing_data->DelayList()[i], CSSPrimitiveValue::UnitType::kSeconds));
  } else {
    list->Append(*CSSPrimitiveValue::Create(
        CSSTimingData::InitialDelay(), CSSPrimitiveValue::UnitType::kSeconds));
  }
  return list;
}

static CSSValue* ValueForAnimationDirection(
    Timing::PlaybackDirection direction) {
  switch (direction) {
    case Timing::PlaybackDirection::NORMAL:
      return CSSIdentifierValue::Create(CSSValueNormal);
    case Timing::PlaybackDirection::ALTERNATE_NORMAL:
      return CSSIdentifierValue::Create(CSSValueAlternate);
    case Timing::PlaybackDirection::REVERSE:
      return CSSIdentifierValue::Create(CSSValueReverse);
    case Timing::PlaybackDirection::ALTERNATE_REVERSE:
      return CSSIdentifierValue::Create(CSSValueAlternateReverse);
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValue* ValueForAnimationDuration(const CSSTimingData* timing_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (timing_data) {
    for (size_t i = 0; i < timing_data->DurationList().size(); ++i)
      list->Append(
          *CSSPrimitiveValue::Create(timing_data->DurationList()[i],
                                     CSSPrimitiveValue::UnitType::kSeconds));
  } else {
    list->Append(
        *CSSPrimitiveValue::Create(CSSTimingData::InitialDuration(),
                                   CSSPrimitiveValue::UnitType::kSeconds));
  }
  return list;
}

static CSSValue* ValueForAnimationFillMode(Timing::FillMode fill_mode) {
  switch (fill_mode) {
    case Timing::FillMode::NONE:
      return CSSIdentifierValue::Create(CSSValueNone);
    case Timing::FillMode::FORWARDS:
      return CSSIdentifierValue::Create(CSSValueForwards);
    case Timing::FillMode::BACKWARDS:
      return CSSIdentifierValue::Create(CSSValueBackwards);
    case Timing::FillMode::BOTH:
      return CSSIdentifierValue::Create(CSSValueBoth);
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValue* ValueForAnimationIterationCount(double iteration_count) {
  if (iteration_count == std::numeric_limits<double>::infinity())
    return CSSIdentifierValue::Create(CSSValueInfinite);
  return CSSPrimitiveValue::Create(iteration_count,
                                   CSSPrimitiveValue::UnitType::kNumber);
}

static CSSValue* ValueForAnimationPlayState(EAnimPlayState play_state) {
  if (play_state == EAnimPlayState::kPlaying)
    return CSSIdentifierValue::Create(CSSValueRunning);
  DCHECK_EQ(play_state, EAnimPlayState::kPaused);
  return CSSIdentifierValue::Create(CSSValuePaused);
}

static CSSValue* CreateTimingFunctionValue(
    const TimingFunction* timing_function) {
  switch (timing_function->GetType()) {
    case TimingFunction::Type::CUBIC_BEZIER: {
      const CubicBezierTimingFunction* bezier_timing_function =
          ToCubicBezierTimingFunction(timing_function);
      if (bezier_timing_function->GetEaseType() !=
          CubicBezierTimingFunction::EaseType::CUSTOM) {
        CSSValueID value_id = CSSValueInvalid;
        switch (bezier_timing_function->GetEaseType()) {
          case CubicBezierTimingFunction::EaseType::EASE:
            value_id = CSSValueEase;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_IN:
            value_id = CSSValueEaseIn;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_OUT:
            value_id = CSSValueEaseOut;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_IN_OUT:
            value_id = CSSValueEaseInOut;
            break;
          default:
            NOTREACHED();
            return nullptr;
        }
        return CSSIdentifierValue::Create(value_id);
      }
      return CSSCubicBezierTimingFunctionValue::Create(
          bezier_timing_function->X1(), bezier_timing_function->Y1(),
          bezier_timing_function->X2(), bezier_timing_function->Y2());
    }

    case TimingFunction::Type::STEPS: {
      const StepsTimingFunction* steps_timing_function =
          ToStepsTimingFunction(timing_function);
      StepsTimingFunction::StepPosition position =
          steps_timing_function->GetStepPosition();
      int steps = steps_timing_function->NumberOfSteps();
      DCHECK(position == StepsTimingFunction::StepPosition::START ||
             position == StepsTimingFunction::StepPosition::END);

      if (steps > 1)
        return CSSStepsTimingFunctionValue::Create(steps, position);
      CSSValueID value_id = position == StepsTimingFunction::StepPosition::START
                                ? CSSValueStepStart
                                : CSSValueStepEnd;
      return CSSIdentifierValue::Create(value_id);
    }

    case TimingFunction::Type::FRAMES: {
      const FramesTimingFunction* frames_timing_function =
          ToFramesTimingFunction(timing_function);
      int frames = frames_timing_function->NumberOfFrames();
      return CSSFramesTimingFunctionValue::Create(frames);
    }

    default:
      return CSSIdentifierValue::Create(CSSValueLinear);
  }
}

static CSSValue* ValueForAnimationTimingFunction(
    const CSSTimingData* timing_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (timing_data) {
    for (size_t i = 0; i < timing_data->TimingFunctionList().size(); ++i) {
      list->Append(*CreateTimingFunctionValue(
          timing_data->TimingFunctionList()[i].get()));
    }
  } else {
    list->Append(*CreateTimingFunctionValue(
        CSSTimingData::InitialTimingFunction().get()));
  }
  return list;
}

static CSSValueList* ValuesForBorderRadiusCorner(const LengthSize& radius,
                                                 const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (radius.Width().GetType() == kPercent)
    list->Append(*CSSPrimitiveValue::Create(
        radius.Width().Percent(), CSSPrimitiveValue::UnitType::kPercentage));
  else
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        radius.Width(), style));
  if (radius.Height().GetType() == kPercent)
    list->Append(*CSSPrimitiveValue::Create(
        radius.Height().Percent(), CSSPrimitiveValue::UnitType::kPercentage));
  else
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        radius.Height(), style));
  return list;
}

static const CSSValue& ValueForBorderRadiusCorner(const LengthSize& radius,
                                                  const ComputedStyle& style) {
  CSSValueList& list = *ValuesForBorderRadiusCorner(radius, style);
  if (list.Item(0) == list.Item(1))
    return list.Item(0);
  return list;
}

static CSSFunctionValue* ValueForMatrixTransform(
    const TransformationMatrix& transform_param,
    const ComputedStyle& style) {
  // Take TransformationMatrix by reference and then copy it because VC++
  // doesn't guarantee alignment of function parameters.
  TransformationMatrix transform = transform_param;
  CSSFunctionValue* transform_value = nullptr;
  transform.Zoom(1 / style.EffectiveZoom());
  if (transform.IsAffine()) {
    transform_value = CSSFunctionValue::Create(CSSValueMatrix);

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.A(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.B(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.C(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.D(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.E(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.F(), CSSPrimitiveValue::UnitType::kNumber));
  } else {
    transform_value = CSSFunctionValue::Create(CSSValueMatrix3d);

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M11(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M12(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M13(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M14(), CSSPrimitiveValue::UnitType::kNumber));

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M21(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M22(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M23(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M24(), CSSPrimitiveValue::UnitType::kNumber));

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M31(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M32(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M33(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M34(), CSSPrimitiveValue::UnitType::kNumber));

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M41(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M42(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M43(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M44(), CSSPrimitiveValue::UnitType::kNumber));
  }

  return transform_value;
}

static CSSValue* ComputedTransform(const LayoutObject* layout_object,
                                   const ComputedStyle& style) {
  if (!layout_object || !style.HasTransform())
    return CSSIdentifierValue::Create(CSSValueNone);

  IntRect box;
  if (layout_object->IsBox())
    box = PixelSnappedIntRect(ToLayoutBox(layout_object)->BorderBoxRect());

  TransformationMatrix transform;
  style.ApplyTransform(transform, LayoutSize(box.Size()),
                       ComputedStyle::kExcludeTransformOrigin,
                       ComputedStyle::kExcludeMotionPath,
                       ComputedStyle::kExcludeIndependentTransformProperties);

  // FIXME: Need to print out individual functions
  // (https://bugs.webkit.org/show_bug.cgi?id=23924)
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForMatrixTransform(transform, style));

  return list;
}

static CSSValue* CreateTransitionPropertyValue(
    const CSSTransitionData::TransitionProperty& property) {
  if (property.property_type == CSSTransitionData::kTransitionNone)
    return CSSIdentifierValue::Create(CSSValueNone);
  if (property.property_type == CSSTransitionData::kTransitionUnknownProperty)
    return CSSCustomIdentValue::Create(property.property_string);
  DCHECK_EQ(property.property_type,
            CSSTransitionData::kTransitionKnownProperty);
  return CSSCustomIdentValue::Create(
      CSSUnresolvedProperty::Get(property.unresolved_property)
          .GetPropertyNameAtomicString());
}

static CSSValue* ValueForTransitionProperty(
    const CSSTransitionData* transition_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (transition_data) {
    for (size_t i = 0; i < transition_data->PropertyList().size(); ++i)
      list->Append(
          *CreateTransitionPropertyValue(transition_data->PropertyList()[i]));
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueAll));
  }
  return list;
}

CSSValueID ValueForQuoteType(const QuoteType quote_type) {
  switch (quote_type) {
    case QuoteType::kNoOpen:
      return CSSValueNoOpenQuote;
    case QuoteType::kNoClose:
      return CSSValueNoCloseQuote;
    case QuoteType::kClose:
      return CSSValueCloseQuote;
    case QuoteType::kOpen:
      return CSSValueOpenQuote;
  }
  NOTREACHED();
  return CSSValueInvalid;
}

static CSSValue* ValueForContentData(const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const ContentData* content_data = style.GetContentData(); content_data;
       content_data = content_data->Next()) {
    if (content_data->IsCounter()) {
      const CounterContent* counter =
          ToCounterContentData(content_data)->Counter();
      DCHECK(counter);
      CSSCustomIdentValue* identifier =
          CSSCustomIdentValue::Create(counter->Identifier());
      CSSStringValue* separator = CSSStringValue::Create(counter->Separator());
      CSSValueID list_style_ident = CSSValueNone;
      if (counter->ListStyle() != EListStyleType::kNone) {
        // TODO(sashab): Change this to use a converter instead of
        // CSSPrimitiveValueMappings.
        list_style_ident =
            CSSIdentifierValue::Create(counter->ListStyle())->GetValueID();
      }
      CSSIdentifierValue* list_style =
          CSSIdentifierValue::Create(list_style_ident);
      list->Append(*CSSCounterValue::Create(identifier, list_style, separator));
    } else if (content_data->IsImage()) {
      const StyleImage* image = ToImageContentData(content_data)->GetImage();
      DCHECK(image);
      list->Append(*image->ComputedCSSValue());
    } else if (content_data->IsText()) {
      list->Append(
          *CSSStringValue::Create(ToTextContentData(content_data)->GetText()));
    } else if (content_data->IsQuote()) {
      const QuoteType quote_type = ToQuoteContentData(content_data)->Quote();
      list->Append(*CSSIdentifierValue::Create(ValueForQuoteType(quote_type)));
    } else {
      NOTREACHED();
    }
  }
  return list;
}

static CSSValue* ValueForCounterDirectives(const ComputedStyle& style,
                                           const CSSProperty& property) {
  const CounterDirectiveMap* map = style.GetCounterDirectives();
  if (!map)
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const auto& item : *map) {
    bool is_valid_counter_value = property.IDEquals(CSSPropertyCounterIncrement)
                                      ? item.value.IsIncrement()
                                      : item.value.IsReset();
    if (!is_valid_counter_value)
      continue;

    list->Append(*CSSCustomIdentValue::Create(item.key));
    short number = property.IDEquals(CSSPropertyCounterIncrement)
                       ? item.value.IncrementValue()
                       : item.value.ResetValue();
    list->Append(*CSSPrimitiveValue::Create(
        (double)number, CSSPrimitiveValue::UnitType::kInteger));
  }

  if (!list->length())
    return CSSIdentifierValue::Create(CSSValueNone);

  return list;
}

static CSSValue* ValueForShape(const ComputedStyle& style,
                               ShapeValue* shape_value) {
  if (!shape_value)
    return CSSIdentifierValue::Create(CSSValueNone);
  if (shape_value->GetType() == ShapeValue::kBox)
    return CSSIdentifierValue::Create(shape_value->CssBox());
  if (shape_value->GetType() == ShapeValue::kImage) {
    if (shape_value->GetImage())
      return shape_value->GetImage()->ComputedCSSValue();
    return CSSIdentifierValue::Create(CSSValueNone);
  }

  DCHECK_EQ(shape_value->GetType(), ShapeValue::kShape);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForBasicShape(style, shape_value->Shape()));
  if (shape_value->CssBox() != kBoxMissing)
    list->Append(*CSSIdentifierValue::Create(shape_value->CssBox()));
  return list;
}

static CSSValueList* ValuesForSidesShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  // Assume the properties are in the usual order top, right, bottom, left.
  const CSSValue* top_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[0], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* right_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[1], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* bottom_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[2], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* left_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[3], style, layout_object, styled_node,
      allow_visited_style);

  // All 4 properties must be specified.
  if (!top_value || !right_value || !bottom_value || !left_value)
    return nullptr;

  bool show_left = !DataEquivalent(right_value, left_value);
  bool show_bottom = !DataEquivalent(top_value, bottom_value) || show_left;
  bool show_right = !DataEquivalent(top_value, right_value) || show_bottom;

  list->Append(*top_value);
  if (show_right)
    list->Append(*right_value);
  if (show_bottom)
    list->Append(*bottom_value);
  if (show_left)
    list->Append(*left_value);

  return list;
}

static CSSValuePair* ValuesForInlineBlockShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  const CSSValue* start_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[0], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* end_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[1], style, layout_object, styled_node,
      allow_visited_style);
  // Both properties must be specified.
  if (!start_value || !end_value)
    return nullptr;

  CSSValuePair* pair = CSSValuePair::Create(start_value, end_value,
                                            CSSValuePair::kDropIdenticalValues);
  return pair;
}

static CSSValueList* ValueForBorderRadiusShorthand(const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  bool show_horizontal_bottom_left = style.BorderTopRightRadius().Width() !=
                                     style.BorderBottomLeftRadius().Width();
  bool show_horizontal_bottom_right =
      show_horizontal_bottom_left || (style.BorderBottomRightRadius().Width() !=
                                      style.BorderTopLeftRadius().Width());
  bool show_horizontal_top_right =
      show_horizontal_bottom_right || (style.BorderTopRightRadius().Width() !=
                                       style.BorderTopLeftRadius().Width());

  bool show_vertical_bottom_left = style.BorderTopRightRadius().Height() !=
                                   style.BorderBottomLeftRadius().Height();
  bool show_vertical_bottom_right =
      show_vertical_bottom_left || (style.BorderBottomRightRadius().Height() !=
                                    style.BorderTopLeftRadius().Height());
  bool show_vertical_top_right =
      show_vertical_bottom_right || (style.BorderTopRightRadius().Height() !=
                                     style.BorderTopLeftRadius().Height());

  CSSValueList* top_left_radius =
      ValuesForBorderRadiusCorner(style.BorderTopLeftRadius(), style);
  CSSValueList* top_right_radius =
      ValuesForBorderRadiusCorner(style.BorderTopRightRadius(), style);
  CSSValueList* bottom_right_radius =
      ValuesForBorderRadiusCorner(style.BorderBottomRightRadius(), style);
  CSSValueList* bottom_left_radius =
      ValuesForBorderRadiusCorner(style.BorderBottomLeftRadius(), style);

  CSSValueList* horizontal_radii = CSSValueList::CreateSpaceSeparated();
  horizontal_radii->Append(top_left_radius->Item(0));
  if (show_horizontal_top_right)
    horizontal_radii->Append(top_right_radius->Item(0));
  if (show_horizontal_bottom_right)
    horizontal_radii->Append(bottom_right_radius->Item(0));
  if (show_horizontal_bottom_left)
    horizontal_radii->Append(bottom_left_radius->Item(0));

  list->Append(*horizontal_radii);

  CSSValueList* vertical_radii = CSSValueList::CreateSpaceSeparated();
  vertical_radii->Append(top_left_radius->Item(1));
  if (show_vertical_top_right)
    vertical_radii->Append(top_right_radius->Item(1));
  if (show_vertical_bottom_right)
    vertical_radii->Append(bottom_right_radius->Item(1));
  if (show_vertical_bottom_left)
    vertical_radii->Append(bottom_left_radius->Item(1));

  if (!vertical_radii->Equals(ToCSSValueList(list->Item(0))))
    list->Append(*vertical_radii);

  return list;
}

static CSSValue* StrokeDashArrayToCSSValueList(const SVGDashArray& dashes,
                                               const ComputedStyle& style) {
  if (dashes.IsEmpty())
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Length& dash_length : dashes.GetVector()) {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        dash_length, style));
  }

  return list;
}

static CSSValue* PaintOrderToCSSValueList(const SVGComputedStyle& svg_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (int i = 0; i < 3; i++) {
    EPaintOrderType paint_order_type = svg_style.PaintOrderType(i);
    switch (paint_order_type) {
      case PT_FILL:
      case PT_STROKE:
      case PT_MARKERS:
        list->Append(*CSSIdentifierValue::Create(paint_order_type));
        break;
      case PT_NONE:
      default:
        NOTREACHED();
        break;
    }
  }

  return list;
}

static CSSValue* AdjustSVGPaintForCurrentColor(SVGPaintType paint_type,
                                               const String& url,
                                               const Color& color,
                                               const Color& current_color) {
  if (paint_type >= SVG_PAINTTYPE_URI_NONE) {
    CSSValueList* values = CSSValueList::CreateSpaceSeparated();
    values->Append(*CSSURIValue::Create(AtomicString(url)));
    if (paint_type == SVG_PAINTTYPE_URI_NONE)
      values->Append(*CSSIdentifierValue::Create(CSSValueNone));
    else if (paint_type == SVG_PAINTTYPE_URI_CURRENTCOLOR)
      values->Append(*CSSColorValue::Create(current_color.Rgb()));
    else if (paint_type == SVG_PAINTTYPE_URI_RGBCOLOR)
      values->Append(*CSSColorValue::Create(color.Rgb()));
    return values;
  }
  if (paint_type == SVG_PAINTTYPE_NONE)
    return CSSIdentifierValue::Create(CSSValueNone);
  if (paint_type == SVG_PAINTTYPE_CURRENTCOLOR)
    return CSSColorValue::Create(current_color.Rgb());

  return CSSColorValue::Create(color.Rgb());
}

CSSValue* ComputedStyleCSSValueMapping::ValueForShadowData(
    const ShadowData& shadow,
    const ComputedStyle& style,
    bool use_spread) {
  CSSPrimitiveValue* x = ZoomAdjustedPixelValue(shadow.X(), style);
  CSSPrimitiveValue* y = ZoomAdjustedPixelValue(shadow.Y(), style);
  CSSPrimitiveValue* blur = ZoomAdjustedPixelValue(shadow.Blur(), style);
  CSSPrimitiveValue* spread =
      use_spread ? ZoomAdjustedPixelValue(shadow.Spread(), style) : nullptr;
  CSSIdentifierValue* shadow_style =
      shadow.Style() == kNormal ? nullptr
                                : CSSIdentifierValue::Create(CSSValueInset);
  CSSValue* color =
      ComputedStyleUtils::CurrentColorOrValidColor(style, shadow.GetColor());
  return CSSShadowValue::Create(x, y, blur, spread, shadow_style, color);
}

CSSValue* ComputedStyleCSSValueMapping::ValueForShadowList(
    const ShadowList* shadow_list,
    const ComputedStyle& style,
    bool use_spread) {
  if (!shadow_list)
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  size_t shadow_count = shadow_list->Shadows().size();
  for (size_t i = 0; i < shadow_count; ++i)
    list->Append(
        *ValueForShadowData(shadow_list->Shadows()[i], style, use_spread));
  return list;
}

CSSValue* ComputedStyleCSSValueMapping::ValueForFilter(
    const ComputedStyle& style,
    const FilterOperations& filter_operations) {
  if (filter_operations.Operations().IsEmpty())
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  CSSFunctionValue* filter_value = nullptr;

  for (const auto& operation : filter_operations.Operations()) {
    FilterOperation* filter_operation = operation.Get();
    switch (filter_operation->GetType()) {
      case FilterOperation::REFERENCE:
        filter_value = CSSFunctionValue::Create(CSSValueUrl);
        filter_value->Append(*CSSStringValue::Create(
            ToReferenceFilterOperation(filter_operation)->Url()));
        break;
      case FilterOperation::GRAYSCALE:
        filter_value = CSSFunctionValue::Create(CSSValueGrayscale);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicColorMatrixFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::SEPIA:
        filter_value = CSSFunctionValue::Create(CSSValueSepia);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicColorMatrixFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::SATURATE:
        filter_value = CSSFunctionValue::Create(CSSValueSaturate);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicColorMatrixFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::HUE_ROTATE:
        filter_value = CSSFunctionValue::Create(CSSValueHueRotate);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicColorMatrixFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kDegrees));
        break;
      case FilterOperation::INVERT:
        filter_value = CSSFunctionValue::Create(CSSValueInvert);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicComponentTransferFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OPACITY:
        filter_value = CSSFunctionValue::Create(CSSValueOpacity);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicComponentTransferFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::BRIGHTNESS:
        filter_value = CSSFunctionValue::Create(CSSValueBrightness);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicComponentTransferFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::CONTRAST:
        filter_value = CSSFunctionValue::Create(CSSValueContrast);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicComponentTransferFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::BLUR:
        filter_value = CSSFunctionValue::Create(CSSValueBlur);
        filter_value->Append(*ZoomAdjustedPixelValue(
            ToBlurFilterOperation(filter_operation)->StdDeviation().Value(),
            style));
        break;
      case FilterOperation::DROP_SHADOW: {
        const auto& drop_shadow_operation =
            ToDropShadowFilterOperation(*filter_operation);
        filter_value = CSSFunctionValue::Create(CSSValueDropShadow);
        // We want our computed style to look like that of a text shadow (has
        // neither spread nor inset style).
        filter_value->Append(
            *ValueForShadowData(drop_shadow_operation.Shadow(), style, false));
        break;
      }
      default:
        NOTREACHED();
        break;
    }
    list->Append(*filter_value);
  }

  return list;
}

CSSValue* ComputedStyleCSSValueMapping::ValueForFont(
    const ComputedStyle& style) {
  // Add a slash between size and line-height.
  CSSValueList* size_and_line_height = CSSValueList::CreateSlashSeparated();
  size_and_line_height->Append(*ValueForFontSize(style));
  size_and_line_height->Append(*ValueForLineHeight(style));

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForFontStyle(style));

  // Check that non-initial font-variant subproperties are not conflicting with
  // this serialization.
  CSSValue* ligatures_value = ValueForFontVariantLigatures(style);
  CSSValue* numeric_value = ValueForFontVariantNumeric(style);
  CSSValue* east_asian_value = ValueForFontVariantEastAsian(style);
  // FIXME: Use DataEquivalent<CSSValue>(...) once http://crbug.com/729447 is
  // resolved.
  if (!DataEquivalent(
          ligatures_value,
          static_cast<CSSValue*>(CSSIdentifierValue::Create(CSSValueNormal))) ||
      !DataEquivalent(
          numeric_value,
          static_cast<CSSValue*>(CSSIdentifierValue::Create(CSSValueNormal))) ||
      !DataEquivalent(
          east_asian_value,
          static_cast<CSSValue*>(CSSIdentifierValue::Create(CSSValueNormal))))
    return nullptr;

  if (!ValueForFontStretchAsKeyword(style))
    return nullptr;

  CSSIdentifierValue* caps_value = ValueForFontVariantCaps(style);
  if (caps_value->GetValueID() != CSSValueNormal &&
      caps_value->GetValueID() != CSSValueSmallCaps)
    return nullptr;
  list->Append(*caps_value);

  list->Append(*ValueForFontWeight(style));
  list->Append(*ValueForFontStretchAsKeyword(style));
  list->Append(*size_and_line_height);
  list->Append(*ValueForFontFamily(style));

  return list;
}

static CSSValue* ValueForScrollSnapType(const ScrollSnapType& type,
                                        const ComputedStyle& style) {
  if (!type.is_none) {
    return CSSValuePair::Create(CSSIdentifierValue::Create(type.axis),
                                CSSIdentifierValue::Create(type.strictness),
                                CSSValuePair::kDropIdenticalValues);
  }
  return CSSIdentifierValue::Create(CSSValueNone);
}

static CSSValue* ValueForScrollSnapAlign(const ScrollSnapAlign& align,
                                         const ComputedStyle& style) {
  return CSSValuePair::Create(CSSIdentifierValue::Create(align.alignmentX),
                              CSSIdentifierValue::Create(align.alignmentY),
                              CSSValuePair::kDropIdenticalValues);
}

// Returns a suitable value for the page-break-(before|after) property, given
// the computed value of the more general break-(before|after) property.
static CSSValue* ValueForPageBreakBetween(EBreakBetween break_value) {
  switch (break_value) {
    case EBreakBetween::kAvoidColumn:
    case EBreakBetween::kColumn:
    case EBreakBetween::kRecto:
    case EBreakBetween::kVerso:
      return CSSIdentifierValue::Create(CSSValueAuto);
    case EBreakBetween::kPage:
      return CSSIdentifierValue::Create(CSSValueAlways);
    case EBreakBetween::kAvoidPage:
      return CSSIdentifierValue::Create(CSSValueAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the -webkit-column-break-(before|after)
// property, given the computed value of the more general break-(before|after)
// property.
static CSSValue* ValueForWebkitColumnBreakBetween(EBreakBetween break_value) {
  switch (break_value) {
    case EBreakBetween::kAvoidPage:
    case EBreakBetween::kLeft:
    case EBreakBetween::kPage:
    case EBreakBetween::kRecto:
    case EBreakBetween::kRight:
    case EBreakBetween::kVerso:
      return CSSIdentifierValue::Create(CSSValueAuto);
    case EBreakBetween::kColumn:
      return CSSIdentifierValue::Create(CSSValueAlways);
    case EBreakBetween::kAvoidColumn:
      return CSSIdentifierValue::Create(CSSValueAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the page-break-inside property, given the
// computed value of the more general break-inside property.
static CSSValue* ValueForPageBreakInside(EBreakInside break_value) {
  switch (break_value) {
    case EBreakInside::kAvoidColumn:
      return CSSIdentifierValue::Create(CSSValueAuto);
    case EBreakInside::kAvoidPage:
      return CSSIdentifierValue::Create(CSSValueAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the -webkit-column-break-inside property, given
// the computed value of the more general break-inside property.
static CSSValue* ValueForWebkitColumnBreakInside(EBreakInside break_value) {
  switch (break_value) {
    case EBreakInside::kAvoidPage:
      return CSSIdentifierValue::Create(CSSValueAuto);
    case EBreakInside::kAvoidColumn:
      return CSSIdentifierValue::Create(CSSValueAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

const CSSValue* ComputedStyleCSSValueMapping::Get(
    const AtomicString custom_property_name,
    const ComputedStyle& style,
    const PropertyRegistry* registry) {
  if (registry) {
    const PropertyRegistration* registration =
        registry->Registration(custom_property_name);
    if (registration) {
      const CSSValue* result = style.GetRegisteredVariable(
          custom_property_name, registration->Inherits());
      if (result)
        return result;
      return registration->Initial();
    }
  }

  bool is_inherited_property = true;
  CSSVariableData* data =
      style.GetVariable(custom_property_name, is_inherited_property);
  if (!data)
    return nullptr;

  return CSSCustomPropertyDeclaration::Create(custom_property_name, data);
}

std::unique_ptr<HashMap<AtomicString, scoped_refptr<CSSVariableData>>>
ComputedStyleCSSValueMapping::GetVariables(const ComputedStyle& style) {
  // TODO(timloh): Also return non-inherited variables
  StyleInheritedVariables* variables = style.InheritedVariables();
  if (variables)
    return variables->GetVariables();
  return nullptr;
}

// https://drafts.csswg.org/cssom/#resolved-value
//
// For 'width' and 'height':
//
// If the property applies to the element or pseudo-element and the resolved
// value of the display property is not none or contents, then the resolved
// value is the used value. Otherwise the resolved value is the computed value
// (https://drafts.csswg.org/css-cascade-4/#computed-value).
//
// (Note that the computed value exists even when the property does not apply.)
static bool WidthOrHeightShouldReturnUsedValue(const LayoutObject* object) {
  // The display property is 'none'.
  if (!object)
    return false;
  // According to
  // http://www.w3.org/TR/CSS2/visudet.html#the-width-property and
  // http://www.w3.org/TR/CSS2/visudet.html#the-height-property, the "width" or
  // "height" property does not apply to non-atomic inline elements.
  if (!object->IsAtomicInlineLevel() && object->IsInline())
    return false;
  // Non-root SVG objects return the resolved value.
  // TODO(fs): Return the used value for <image>, <rect> and <foreignObject> (to
  // which 'width' or 'height' can be said to apply) too? We don't return the
  // used value for other geometric properties ('x', 'y' et.c.)
  return !object->IsSVGChild();
}

const CSSValue* ComputedStyleCSSValueMapping::Get(
    const CSSProperty& property,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  const SVGComputedStyle& svg_style = style.SvgStyle();
  const CSSProperty& resolved_property = property.ResolveDirectionAwareProperty(
      style.Direction(), style.GetWritingMode());

  switch (resolved_property.PropertyID()) {
    case CSSPropertyBorderCollapse:
      if (style.BorderCollapse() == EBorderCollapse::kCollapse)
        return CSSIdentifierValue::Create(CSSValueCollapse);
      return CSSIdentifierValue::Create(CSSValueSeparate);
    case CSSPropertyBorderSpacing: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(
          *ZoomAdjustedPixelValue(style.HorizontalBorderSpacing(), style));
      list->Append(
          *ZoomAdjustedPixelValue(style.VerticalBorderSpacing(), style));
      return list;
    }
    case CSSPropertyBottom:
      return ValueForPositionOffset(style, resolved_property, layout_object);
    case CSSPropertyWebkitBoxDecorationBreak:
      if (style.BoxDecorationBreak() == EBoxDecorationBreak::kSlice)
        return CSSIdentifierValue::Create(CSSValueSlice);
      return CSSIdentifierValue::Create(CSSValueClone);
    case CSSPropertyBoxShadow:
      return ValueForShadowList(style.BoxShadow(), style, true);
    case CSSPropertyColumnCount:
      if (style.HasAutoColumnCount())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSPrimitiveValue::Create(style.ColumnCount(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyColumnGap:
      if (style.HasNormalColumnGap())
        return CSSIdentifierValue::Create(CSSValueNormal);
      return ZoomAdjustedPixelValue(style.ColumnGap(), style);
    case CSSPropertyWebkitColumnBreakAfter:
      return ValueForWebkitColumnBreakBetween(style.BreakAfter());
    case CSSPropertyWebkitColumnBreakBefore:
      return ValueForWebkitColumnBreakBetween(style.BreakBefore());
    case CSSPropertyWebkitColumnBreakInside:
      return ValueForWebkitColumnBreakInside(style.BreakInside());
    case CSSPropertyColumnWidth:
      if (style.HasAutoColumnWidth())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return ZoomAdjustedPixelValue(style.ColumnWidth(), style);
    case CSSPropertyTextSizeAdjust:
      if (style.GetTextSizeAdjust().IsAuto())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSPrimitiveValue::Create(
          style.GetTextSizeAdjust().Multiplier() * 100,
          CSSPrimitiveValue::UnitType::kPercentage);
    case CSSPropertyCursor: {
      CSSValueList* list = nullptr;
      CursorList* cursors = style.Cursors();
      if (cursors && cursors->size() > 0) {
        list = CSSValueList::CreateCommaSeparated();
        for (const CursorData& cursor : *cursors) {
          if (StyleImage* image = cursor.GetImage()) {
            list->Append(*CSSCursorImageValue::Create(
                *image->ComputedCSSValue(), cursor.HotSpotSpecified(),
                cursor.HotSpot()));
          }
        }
      }
      CSSValue* value = CSSIdentifierValue::Create(style.Cursor());
      if (list) {
        list->Append(*value);
        return list;
      }
      return value;
    }
    case CSSPropertyPlaceContent: {
      // TODO (jfernandez): The spec states that we should return the specified
      // value.
      return ValuesForShorthandProperty(placeContentShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    }
    case CSSPropertyPlaceItems: {
      // TODO (jfernandez): The spec states that we should return the specified
      // value.
      return ValuesForShorthandProperty(placeItemsShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    }
    case CSSPropertyPlaceSelf: {
      // TODO (jfernandez): The spec states that we should return the specified
      // value.
      return ValuesForShorthandProperty(placeSelfShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    }
    case CSSPropertyAlignContent:
      return ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.AlignContent());
    case CSSPropertyAlignItems:
      return ValueForItemPositionWithOverflowAlignment(style.AlignItems());
    case CSSPropertyAlignSelf:
      return ValueForItemPositionWithOverflowAlignment(style.AlignSelf());
    case CSSPropertyFlex:
      return ValuesForShorthandProperty(flexShorthand(), style, layout_object,
                                        styled_node, allow_visited_style);
    case CSSPropertyFlexFlow:
      return ValuesForShorthandProperty(flexFlowShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyJustifyContent:
      return ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.JustifyContent());
    case CSSPropertyFloat:
      if (style.Display() != EDisplay::kNone && style.HasOutOfFlowPosition())
        return CSSIdentifierValue::Create(CSSValueNone);
      return CSSIdentifierValue::Create(style.Floating());
    case CSSPropertyFont:
      return ValueForFont(style);
    case CSSPropertyFontFamily:
      return ValueForFontFamily(style);
    case CSSPropertyFontSize:
      return ValueForFontSize(style);
    case CSSPropertyFontSizeAdjust:
      if (style.HasFontSizeAdjust())
        return CSSPrimitiveValue::Create(style.FontSizeAdjust(),
                                         CSSPrimitiveValue::UnitType::kNumber);
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyFontStretch:
      return ValueForFontStretch(style);
    case CSSPropertyFontStyle:
      return ValueForFontStyle(style);
    case CSSPropertyFontVariant:
      return ValuesForFontVariantProperty(style, layout_object, styled_node,
                                          allow_visited_style);
    case CSSPropertyFontWeight:
      return ValueForFontWeight(style);
    case CSSPropertyFontFeatureSettings: {
      const FontFeatureSettings* feature_settings =
          style.GetFontDescription().FeatureSettings();
      if (!feature_settings || !feature_settings->size())
        return CSSIdentifierValue::Create(CSSValueNormal);
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      for (unsigned i = 0; i < feature_settings->size(); ++i) {
        const FontFeature& feature = feature_settings->at(i);
        cssvalue::CSSFontFeatureValue* feature_value =
            cssvalue::CSSFontFeatureValue::Create(feature.Tag(),
                                                  feature.Value());
        list->Append(*feature_value);
      }
      return list;
    }
    case CSSPropertyFontVariationSettings: {
      DCHECK(RuntimeEnabledFeatures::CSSVariableFontsEnabled());
      const FontVariationSettings* variation_settings =
          style.GetFontDescription().VariationSettings();
      if (!variation_settings || !variation_settings->size())
        return CSSIdentifierValue::Create(CSSValueNormal);
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      for (unsigned i = 0; i < variation_settings->size(); ++i) {
        const FontVariationAxis& variation_axis = variation_settings->at(i);
        CSSFontVariationValue* variation_value = CSSFontVariationValue::Create(
            variation_axis.Tag(), variation_axis.Value());
        list->Append(*variation_value);
      }
      return list;
    }
    case CSSPropertyGridAutoFlow: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      switch (style.GetGridAutoFlow()) {
        case kAutoFlowRow:
        case kAutoFlowRowDense:
          list->Append(*CSSIdentifierValue::Create(CSSValueRow));
          break;
        case kAutoFlowColumn:
        case kAutoFlowColumnDense:
          list->Append(*CSSIdentifierValue::Create(CSSValueColumn));
          break;
        default:
          NOTREACHED();
      }

      switch (style.GetGridAutoFlow()) {
        case kAutoFlowRowDense:
        case kAutoFlowColumnDense:
          list->Append(*CSSIdentifierValue::Create(CSSValueDense));
          break;
        default:
          // Do nothing.
          break;
      }

      return list;
    }
    // Specs mention that getComputedStyle() should return the used value of the
    // property instead of the computed one for grid-template-{rows|columns} but
    // not for the grid-auto-{rows|columns} as things like grid-auto-columns:
    // 2fr; cannot be resolved to a value in pixels as the '2fr' means very
    // different things depending on the size of the explicit grid or the number
    // of implicit tracks added to the grid. See
    // http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
    case CSSPropertyGridAutoColumns:
      return ValueForGridTrackSizeList(kForColumns, style);
    case CSSPropertyGridAutoRows:
      return ValueForGridTrackSizeList(kForRows, style);

    case CSSPropertyGridTemplateColumns:
      return ValueForGridTrackList(kForColumns, layout_object, style);
    case CSSPropertyGridTemplateRows:
      return ValueForGridTrackList(kForRows, layout_object, style);

    case CSSPropertyGridColumnStart:
      return ValueForGridPosition(style.GridColumnStart());
    case CSSPropertyGridColumnEnd:
      return ValueForGridPosition(style.GridColumnEnd());
    case CSSPropertyGridRowStart:
      return ValueForGridPosition(style.GridRowStart());
    case CSSPropertyGridRowEnd:
      return ValueForGridPosition(style.GridRowEnd());
    case CSSPropertyGridColumn:
      return ValuesForGridShorthand(gridColumnShorthand(), style, layout_object,
                                    styled_node, allow_visited_style);
    case CSSPropertyGridRow:
      return ValuesForGridShorthand(gridRowShorthand(), style, layout_object,
                                    styled_node, allow_visited_style);
    case CSSPropertyGridArea:
      return ValuesForGridShorthand(gridAreaShorthand(), style, layout_object,
                                    styled_node, allow_visited_style);
    case CSSPropertyGridTemplate:
      return ValuesForGridShorthand(gridTemplateShorthand(), style,
                                    layout_object, styled_node,
                                    allow_visited_style);
    case CSSPropertyGrid:
      return ValuesForGridShorthand(gridShorthand(), style, layout_object,
                                    styled_node, allow_visited_style);
    case CSSPropertyGridTemplateAreas:
      if (!style.NamedGridAreaRowCount()) {
        DCHECK(!style.NamedGridAreaColumnCount());
        return CSSIdentifierValue::Create(CSSValueNone);
      }

      return CSSGridTemplateAreasValue::Create(
          style.NamedGridArea(), style.NamedGridAreaRowCount(),
          style.NamedGridAreaColumnCount());
    case CSSPropertyGridGap:
      return ValuesForShorthandProperty(gridGapShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);

    case CSSPropertyHeight:
      if (WidthOrHeightShouldReturnUsedValue(layout_object)) {
        return ZoomAdjustedPixelValue(SizingBox(*layout_object).Height(),
                                      style);
      }
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Height(),
                                                                 style);
    case CSSPropertyWebkitHighlight:
      if (style.Highlight() == g_null_atom)
        return CSSIdentifierValue::Create(CSSValueNone);
      return CSSStringValue::Create(style.Highlight());
    case CSSPropertyWebkitHyphenateCharacter:
      if (style.HyphenationString().IsNull())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSStringValue::Create(style.HyphenationString());
    case CSSPropertyImageOrientation:
      if (style.RespectImageOrientation() == kRespectImageOrientation)
        return CSSIdentifierValue::Create(CSSValueFromImage);
      return CSSPrimitiveValue::Create(0,
                                       CSSPrimitiveValue::UnitType::kDegrees);
    case CSSPropertyJustifyItems:
      return ValueForItemPositionWithOverflowAlignment(
          style.JustifyItems().GetPosition() == ItemPosition::kAuto
              ? ComputedStyleInitialValues::InitialDefaultAlignment()
              : style.JustifyItems());
    case CSSPropertyJustifySelf:
      return ValueForItemPositionWithOverflowAlignment(style.JustifySelf());
    case CSSPropertyLeft:
      return ValueForPositionOffset(style, resolved_property, layout_object);
    case CSSPropertyLetterSpacing:
      if (!style.LetterSpacing())
        return CSSIdentifierValue::Create(CSSValueNormal);
      return ZoomAdjustedPixelValue(style.LetterSpacing(), style);
    case CSSPropertyWebkitLineClamp:
      if (style.LineClamp().IsNone())
        return CSSIdentifierValue::Create(CSSValueNone);
      return CSSPrimitiveValue::Create(
          style.LineClamp().Value(),
          style.LineClamp().IsPercentage()
              ? CSSPrimitiveValue::UnitType::kPercentage
              : CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyLineHeight:
      return ValueForLineHeight(style);
    case CSSPropertyListStyleImage:
      if (style.ListStyleImage())
        return style.ListStyleImage()->ComputedCSSValue();
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyWebkitLocale:
      if (style.Locale().IsNull())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSStringValue::Create(style.Locale());
    case CSSPropertyMarginTop: {
      const Length& margin_top = style.MarginTop();
      if (margin_top.IsFixed() || !layout_object || !layout_object->IsBox()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_top,
                                                                   style);
      }
      return ZoomAdjustedPixelValue(ToLayoutBox(layout_object)->MarginTop(),
                                    style);
    }
    case CSSPropertyMarginRight: {
      const Length& margin_right = style.MarginRight();
      if (margin_right.IsFixed() || !layout_object || !layout_object->IsBox()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_right,
                                                                   style);
      }
      float value;
      if (margin_right.IsPercentOrCalc()) {
        // LayoutBox gives a marginRight() that is the distance between the
        // right-edge of the child box and the right-edge of the containing box,
        // when display == EDisplay::kBlock. Let's calculate the absolute value
        // of the specified margin-right % instead of relying on LayoutBox's
        // marginRight() value.
        value = MinimumValueForLength(
                    margin_right, ToLayoutBox(layout_object)
                                      ->ContainingBlockLogicalWidthForContent())
                    .ToFloat();
      } else {
        value = ToLayoutBox(layout_object)->MarginRight().ToFloat();
      }
      return ZoomAdjustedPixelValue(value, style);
    }
    case CSSPropertyMarginBottom: {
      const Length& margin_bottom = style.MarginBottom();
      if (margin_bottom.IsFixed() || !layout_object ||
          !layout_object->IsBox()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            margin_bottom, style);
      }
      return ZoomAdjustedPixelValue(ToLayoutBox(layout_object)->MarginBottom(),
                                    style);
    }
    case CSSPropertyMarginLeft: {
      const Length& margin_left = style.MarginLeft();
      if (margin_left.IsFixed() || !layout_object || !layout_object->IsBox()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_left,
                                                                   style);
      }
      return ZoomAdjustedPixelValue(ToLayoutBox(layout_object)->MarginLeft(),
                                    style);
    }
    case CSSPropertyMaxHeight: {
      const Length& max_height = style.MaxHeight();
      if (max_height.IsMaxSizeNone())
        return CSSIdentifierValue::Create(CSSValueNone);
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(max_height,
                                                                 style);
    }
    case CSSPropertyMaxWidth: {
      const Length& max_width = style.MaxWidth();
      if (max_width.IsMaxSizeNone())
        return CSSIdentifierValue::Create(CSSValueNone);
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(max_width,
                                                                 style);
    }
    case CSSPropertyMinHeight:
      if (style.MinHeight().IsAuto()) {
        Node* parent = styled_node->parentNode();
        if (IsFlexOrGrid(parent ? parent->EnsureComputedStyle() : nullptr))
          return CSSIdentifierValue::Create(CSSValueAuto);
        return ZoomAdjustedPixelValue(0, style);
      }
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.MinHeight(), style);
    case CSSPropertyMinWidth:
      if (style.MinWidth().IsAuto()) {
        Node* parent = styled_node->parentNode();
        if (IsFlexOrGrid(parent ? parent->EnsureComputedStyle() : nullptr))
          return CSSIdentifierValue::Create(CSSValueAuto);
        return ZoomAdjustedPixelValue(0, style);
      }
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.MinWidth(), style);
    case CSSPropertyObjectPosition:
      return CSSValuePair::Create(
          ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
              style.ObjectPosition().X(), style),
          ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
              style.ObjectPosition().Y(), style),
          CSSValuePair::kKeepIdenticalValues);
    case CSSPropertyOutlineStyle:
      if (style.OutlineStyleIsAuto())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSIdentifierValue::Create(style.OutlineStyle());
    case CSSPropertyOverflow:
      if (style.OverflowX() == style.OverflowY())
        return CSSIdentifierValue::Create(style.OverflowX());
      return nullptr;
    case CSSPropertyPaddingTop: {
      const Length& padding_top = style.PaddingTop();
      if (padding_top.IsFixed() || !layout_object || !layout_object->IsBox()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_top,
                                                                   style);
      }
      return ZoomAdjustedPixelValue(
          ToLayoutBox(layout_object)->ComputedCSSPaddingTop(), style);
    }
    case CSSPropertyPaddingRight: {
      const Length& padding_right = style.PaddingRight();
      if (padding_right.IsFixed() || !layout_object ||
          !layout_object->IsBox()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            padding_right, style);
      }
      return ZoomAdjustedPixelValue(
          ToLayoutBox(layout_object)->ComputedCSSPaddingRight(), style);
    }
    case CSSPropertyPaddingBottom: {
      const Length& padding_bottom = style.PaddingBottom();
      if (padding_bottom.IsFixed() || !layout_object ||
          !layout_object->IsBox()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            padding_bottom, style);
      }
      return ZoomAdjustedPixelValue(
          ToLayoutBox(layout_object)->ComputedCSSPaddingBottom(), style);
    }
    case CSSPropertyPaddingLeft: {
      const Length& padding_left = style.PaddingLeft();
      if (padding_left.IsFixed() || !layout_object || !layout_object->IsBox()) {
        return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_left,
                                                                   style);
      }
      return ZoomAdjustedPixelValue(
          ToLayoutBox(layout_object)->ComputedCSSPaddingLeft(), style);
    }
    case CSSPropertyPageBreakAfter:
      return ValueForPageBreakBetween(style.BreakAfter());
    case CSSPropertyPageBreakBefore:
      return ValueForPageBreakBetween(style.BreakBefore());
    case CSSPropertyPageBreakInside:
      return ValueForPageBreakInside(style.BreakInside());
    case CSSPropertyQuotes:
      if (!style.Quotes()) {
        // TODO(ramya.v): We should return the quote values that we're actually
        // using.
        return nullptr;
      }
      if (style.Quotes()->size()) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        for (int i = 0; i < style.Quotes()->size(); i++) {
          list->Append(
              *CSSStringValue::Create(style.Quotes()->GetOpenQuote(i)));
          list->Append(
              *CSSStringValue::Create(style.Quotes()->GetCloseQuote(i)));
        }
        return list;
      }
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyRight:
      return ValueForPositionOffset(style, resolved_property, layout_object);
    case CSSPropertyTextDecoration:
      return ValuesForShorthandProperty(textDecorationShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyTextDecorationLine:
      return RenderTextDecorationFlagsToCSSValue(style.GetTextDecoration());
    case CSSPropertyTextDecorationSkipInk:
      return ValueForTextDecorationSkipInk(style.TextDecorationSkipInk());
    case CSSPropertyTextDecorationStyle:
      return ValueForTextDecorationStyle(style.TextDecorationStyle());
    case CSSPropertyWebkitTextDecorationsInEffect:
      return RenderTextDecorationFlagsToCSSValue(
          style.TextDecorationsInEffect());
    case CSSPropertyWebkitTextEmphasisPosition: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      switch (style.GetTextEmphasisPosition()) {
        case TextEmphasisPosition::kOverRight:
          list->Append(*CSSIdentifierValue::Create(CSSValueOver));
          list->Append(*CSSIdentifierValue::Create(CSSValueRight));
          break;
        case TextEmphasisPosition::kOverLeft:
          list->Append(*CSSIdentifierValue::Create(CSSValueOver));
          list->Append(*CSSIdentifierValue::Create(CSSValueLeft));
          break;
        case TextEmphasisPosition::kUnderRight:
          list->Append(*CSSIdentifierValue::Create(CSSValueUnder));
          list->Append(*CSSIdentifierValue::Create(CSSValueRight));
          break;
        case TextEmphasisPosition::kUnderLeft:
          list->Append(*CSSIdentifierValue::Create(CSSValueUnder));
          list->Append(*CSSIdentifierValue::Create(CSSValueLeft));
          break;
      }
      return list;
    }
    case CSSPropertyWebkitTextEmphasisStyle:
      switch (style.GetTextEmphasisMark()) {
        case TextEmphasisMark::kNone:
          return CSSIdentifierValue::Create(CSSValueNone);
        case TextEmphasisMark::kCustom:
          return CSSStringValue::Create(style.TextEmphasisCustomMark());
        case TextEmphasisMark::kAuto:
          NOTREACHED();
        // Fall through
        case TextEmphasisMark::kDot:
        case TextEmphasisMark::kCircle:
        case TextEmphasisMark::kDoubleCircle:
        case TextEmphasisMark::kTriangle:
        case TextEmphasisMark::kSesame: {
          CSSValueList* list = CSSValueList::CreateSpaceSeparated();
          list->Append(
              *CSSIdentifierValue::Create(style.GetTextEmphasisFill()));
          list->Append(
              *CSSIdentifierValue::Create(style.GetTextEmphasisMark()));
          return list;
        }
      }
    case CSSPropertyTextIndent: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.TextIndent(), style));
      if (RuntimeEnabledFeatures::CSS3TextEnabled() &&
          (style.GetTextIndentLine() == TextIndentLine::kEachLine ||
           style.GetTextIndentType() == TextIndentType::kHanging)) {
        if (style.GetTextIndentLine() == TextIndentLine::kEachLine)
          list->Append(*CSSIdentifierValue::Create(CSSValueEachLine));
        if (style.GetTextIndentType() == TextIndentType::kHanging)
          list->Append(*CSSIdentifierValue::Create(CSSValueHanging));
      }
      return list;
    }
    case CSSPropertyTextShadow:
      return ValueForShadowList(style.TextShadow(), style, false);
    case CSSPropertyTextOverflow:
      if (style.TextOverflow() != ETextOverflow::kClip)
        return CSSIdentifierValue::Create(CSSValueEllipsis);
      return CSSIdentifierValue::Create(CSSValueClip);
    case CSSPropertyTop:
      return ValueForPositionOffset(style, resolved_property, layout_object);
    case CSSPropertyTouchAction:
      return TouchActionFlagsToCSSValue(style.GetTouchAction());
    case CSSPropertyVerticalAlign:
      switch (style.VerticalAlign()) {
        case EVerticalAlign::kBaseline:
          return CSSIdentifierValue::Create(CSSValueBaseline);
        case EVerticalAlign::kMiddle:
          return CSSIdentifierValue::Create(CSSValueMiddle);
        case EVerticalAlign::kSub:
          return CSSIdentifierValue::Create(CSSValueSub);
        case EVerticalAlign::kSuper:
          return CSSIdentifierValue::Create(CSSValueSuper);
        case EVerticalAlign::kTextTop:
          return CSSIdentifierValue::Create(CSSValueTextTop);
        case EVerticalAlign::kTextBottom:
          return CSSIdentifierValue::Create(CSSValueTextBottom);
        case EVerticalAlign::kTop:
          return CSSIdentifierValue::Create(CSSValueTop);
        case EVerticalAlign::kBottom:
          return CSSIdentifierValue::Create(CSSValueBottom);
        case EVerticalAlign::kBaselineMiddle:
          return CSSIdentifierValue::Create(CSSValueWebkitBaselineMiddle);
        case EVerticalAlign::kLength:
          return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
              style.GetVerticalAlignLength(), style);
      }
      NOTREACHED();
      return nullptr;
    case CSSPropertyWidth:
      if (WidthOrHeightShouldReturnUsedValue(layout_object))
        return ZoomAdjustedPixelValue(SizingBox(*layout_object).Width(), style);
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Width(),
                                                                 style);
    case CSSPropertyWillChange:
      return ValueForWillChange(style.WillChangeProperties(),
                                style.WillChangeContents(),
                                style.WillChangeScrollPosition());
    case CSSPropertyFontVariantLigatures:
      return ValueForFontVariantLigatures(style);
    case CSSPropertyFontVariantCaps:
      return ValueForFontVariantCaps(style);
    case CSSPropertyFontVariantNumeric:
      return ValueForFontVariantNumeric(style);
    case CSSPropertyFontVariantEastAsian:
      return ValueForFontVariantEastAsian(style);
    case CSSPropertyZIndex:
      if (style.HasAutoZIndex() || !style.IsStackingContext())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSPrimitiveValue::Create(style.ZIndex(),
                                       CSSPrimitiveValue::UnitType::kInteger);
    case CSSPropertyBoxSizing:
      if (style.BoxSizing() == EBoxSizing::kContentBox)
        return CSSIdentifierValue::Create(CSSValueContentBox);
      return CSSIdentifierValue::Create(CSSValueBorderBox);
    case CSSPropertyAnimationDelay:
      return ValueForAnimationDelay(style.Animations());
    case CSSPropertyAnimationDirection: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->DirectionList().size(); ++i)
          list->Append(
              *ValueForAnimationDirection(animation_data->DirectionList()[i]));
      } else {
        list->Append(*CSSIdentifierValue::Create(CSSValueNormal));
      }
      return list;
    }
    case CSSPropertyAnimationDuration:
      return ValueForAnimationDuration(style.Animations());
    case CSSPropertyAnimationFillMode: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->FillModeList().size(); ++i)
          list->Append(
              *ValueForAnimationFillMode(animation_data->FillModeList()[i]));
      } else {
        list->Append(*CSSIdentifierValue::Create(CSSValueNone));
      }
      return list;
    }
    case CSSPropertyAnimationIterationCount: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->IterationCountList().size(); ++i)
          list->Append(*ValueForAnimationIterationCount(
              animation_data->IterationCountList()[i]));
      } else {
        list->Append(*CSSPrimitiveValue::Create(
            CSSAnimationData::InitialIterationCount(),
            CSSPrimitiveValue::UnitType::kNumber));
      }
      return list;
    }
    case CSSPropertyAnimationName: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->NameList().size(); ++i)
          list->Append(
              *CSSCustomIdentValue::Create(animation_data->NameList()[i]));
      } else {
        list->Append(*CSSIdentifierValue::Create(CSSValueNone));
      }
      return list;
    }
    case CSSPropertyAnimationPlayState: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->PlayStateList().size(); ++i)
          list->Append(
              *ValueForAnimationPlayState(animation_data->PlayStateList()[i]));
      } else {
        list->Append(*CSSIdentifierValue::Create(CSSValueRunning));
      }
      return list;
    }
    case CSSPropertyAnimationTimingFunction:
      return ValueForAnimationTimingFunction(style.Animations());
    case CSSPropertyAnimation: {
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        CSSValueList* animations_list = CSSValueList::CreateCommaSeparated();
        for (size_t i = 0; i < animation_data->NameList().size(); ++i) {
          CSSValueList* list = CSSValueList::CreateSpaceSeparated();
          list->Append(
              *CSSCustomIdentValue::Create(animation_data->NameList()[i]));
          list->Append(*CSSPrimitiveValue::Create(
              CSSTimingData::GetRepeated(animation_data->DurationList(), i),
              CSSPrimitiveValue::UnitType::kSeconds));
          list->Append(*CreateTimingFunctionValue(
              CSSTimingData::GetRepeated(animation_data->TimingFunctionList(),
                                         i)
                  .get()));
          list->Append(*CSSPrimitiveValue::Create(
              CSSTimingData::GetRepeated(animation_data->DelayList(), i),
              CSSPrimitiveValue::UnitType::kSeconds));
          list->Append(
              *ValueForAnimationIterationCount(CSSTimingData::GetRepeated(
                  animation_data->IterationCountList(), i)));
          list->Append(*ValueForAnimationDirection(
              CSSTimingData::GetRepeated(animation_data->DirectionList(), i)));
          list->Append(*ValueForAnimationFillMode(
              CSSTimingData::GetRepeated(animation_data->FillModeList(), i)));
          list->Append(*ValueForAnimationPlayState(
              CSSTimingData::GetRepeated(animation_data->PlayStateList(), i)));
          animations_list->Append(*list);
        }
        return animations_list;
      }

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      // animation-name default value.
      list->Append(*CSSIdentifierValue::Create(CSSValueNone));
      list->Append(
          *CSSPrimitiveValue::Create(CSSAnimationData::InitialDuration(),
                                     CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(*CreateTimingFunctionValue(
          CSSAnimationData::InitialTimingFunction().get()));
      list->Append(
          *CSSPrimitiveValue::Create(CSSAnimationData::InitialDelay(),
                                     CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(
          *CSSPrimitiveValue::Create(CSSAnimationData::InitialIterationCount(),
                                     CSSPrimitiveValue::UnitType::kNumber));
      list->Append(
          *ValueForAnimationDirection(CSSAnimationData::InitialDirection()));
      list->Append(
          *ValueForAnimationFillMode(CSSAnimationData::InitialFillMode()));
      // Initial animation-play-state.
      list->Append(*CSSIdentifierValue::Create(CSSValueRunning));
      return list;
    }
    case CSSPropertyPerspective:
      if (!style.HasPerspective())
        return CSSIdentifierValue::Create(CSSValueNone);
      return ZoomAdjustedPixelValue(style.Perspective(), style);
    case CSSPropertyPerspectiveOrigin: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (layout_object) {
        LayoutRect box;
        if (layout_object->IsBox())
          box = ToLayoutBox(layout_object)->BorderBoxRect();

        list->Append(*ZoomAdjustedPixelValue(
            MinimumValueForLength(style.PerspectiveOriginX(), box.Width()),
            style));
        list->Append(*ZoomAdjustedPixelValue(
            MinimumValueForLength(style.PerspectiveOriginY(), box.Height()),
            style));
      } else {
        list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.PerspectiveOriginX(), style));
        list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.PerspectiveOriginY(), style));
      }
      return list;
    }
    case CSSPropertyBorderBottomLeftRadius:
      return &ValueForBorderRadiusCorner(style.BorderBottomLeftRadius(), style);
    case CSSPropertyBorderBottomRightRadius:
      return &ValueForBorderRadiusCorner(style.BorderBottomRightRadius(),
                                         style);
    case CSSPropertyBorderTopLeftRadius:
      return &ValueForBorderRadiusCorner(style.BorderTopLeftRadius(), style);
    case CSSPropertyBorderTopRightRadius:
      return &ValueForBorderRadiusCorner(style.BorderTopRightRadius(), style);
    case CSSPropertyClip: {
      if (style.HasAutoClip())
        return CSSIdentifierValue::Create(CSSValueAuto);
      CSSValue* top = ZoomAdjustedPixelValueOrAuto(style.Clip().Top(), style);
      CSSValue* right =
          ZoomAdjustedPixelValueOrAuto(style.Clip().Right(), style);
      CSSValue* bottom =
          ZoomAdjustedPixelValueOrAuto(style.Clip().Bottom(), style);
      CSSValue* left = ZoomAdjustedPixelValueOrAuto(style.Clip().Left(), style);
      return CSSQuadValue::Create(top, right, bottom, left,
                                  CSSQuadValue::kSerializeAsRect);
    }
    case CSSPropertyTransform:
      return ComputedTransform(layout_object, style);
    case CSSPropertyTransformOrigin: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (layout_object) {
        LayoutRect box;
        if (layout_object->IsBox())
          box = ToLayoutBox(layout_object)->BorderBoxRect();

        list->Append(*ZoomAdjustedPixelValue(
            MinimumValueForLength(style.TransformOriginX(), box.Width()),
            style));
        list->Append(*ZoomAdjustedPixelValue(
            MinimumValueForLength(style.TransformOriginY(), box.Height()),
            style));
        if (style.TransformOriginZ() != 0)
          list->Append(
              *ZoomAdjustedPixelValue(style.TransformOriginZ(), style));
      } else {
        list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.TransformOriginX(), style));
        list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.TransformOriginY(), style));
        if (style.TransformOriginZ() != 0)
          list->Append(
              *ZoomAdjustedPixelValue(style.TransformOriginZ(), style));
      }
      return list;
    }
    case CSSPropertyTransitionDelay:
      return ValueForAnimationDelay(style.Transitions());
    case CSSPropertyTransitionDuration:
      return ValueForAnimationDuration(style.Transitions());
    case CSSPropertyTransitionProperty:
      return ValueForTransitionProperty(style.Transitions());
    case CSSPropertyTransitionTimingFunction:
      return ValueForAnimationTimingFunction(style.Transitions());
    case CSSPropertyTransition: {
      const CSSTransitionData* transition_data = style.Transitions();
      if (transition_data) {
        CSSValueList* transitions_list = CSSValueList::CreateCommaSeparated();
        for (size_t i = 0; i < transition_data->PropertyList().size(); ++i) {
          CSSValueList* list = CSSValueList::CreateSpaceSeparated();
          list->Append(*CreateTransitionPropertyValue(
              transition_data->PropertyList()[i]));
          list->Append(*CSSPrimitiveValue::Create(
              CSSTimingData::GetRepeated(transition_data->DurationList(), i),
              CSSPrimitiveValue::UnitType::kSeconds));
          list->Append(*CreateTimingFunctionValue(
              CSSTimingData::GetRepeated(transition_data->TimingFunctionList(),
                                         i)
                  .get()));
          list->Append(*CSSPrimitiveValue::Create(
              CSSTimingData::GetRepeated(transition_data->DelayList(), i),
              CSSPrimitiveValue::UnitType::kSeconds));
          transitions_list->Append(*list);
        }
        return transitions_list;
      }

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      // transition-property default value.
      list->Append(*CSSIdentifierValue::Create(CSSValueAll));
      list->Append(
          *CSSPrimitiveValue::Create(CSSTransitionData::InitialDuration(),
                                     CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(*CreateTimingFunctionValue(
          CSSTransitionData::InitialTimingFunction().get()));
      list->Append(
          *CSSPrimitiveValue::Create(CSSTransitionData::InitialDelay(),
                                     CSSPrimitiveValue::UnitType::kSeconds));
      return list;
    }
    case CSSPropertyWebkitTextCombine:
      if (style.TextCombine() == ETextCombine::kAll)
        return CSSIdentifierValue::Create(CSSValueHorizontal);
      return CSSIdentifierValue::Create(style.TextCombine());
    case CSSPropertyWebkitTextOrientation:
      if (style.GetTextOrientation() == ETextOrientation::kMixed)
        return CSSIdentifierValue::Create(CSSValueVerticalRight);
      return CSSIdentifierValue::Create(style.GetTextOrientation());
    case CSSPropertyContent:
      return ValueForContentData(style);
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
      return ValueForCounterDirectives(style, resolved_property);
    case CSSPropertyClipPath:
      if (ClipPathOperation* operation = style.ClipPath()) {
        if (operation->GetType() == ClipPathOperation::SHAPE)
          return ValueForBasicShape(
              style, ToShapeClipPathOperation(operation)->GetBasicShape());
        if (operation->GetType() == ClipPathOperation::REFERENCE) {
          return CSSURIValue::Create(
              AtomicString(ToReferenceClipPathOperation(operation)->Url()));
        }
      }
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyShapeMargin:
      return CSSValue::Create(style.ShapeMargin(), style.EffectiveZoom());
    case CSSPropertyShapeOutside:
      return ValueForShape(style, style.ShapeOutside());
    case CSSPropertyFilter:
      return ValueForFilter(style, style.Filter());
    case CSSPropertyBackdropFilter:
      return ValueForFilter(style, style.BackdropFilter());
    case CSSPropertyBorder: {
      const CSSValue* value =
          Get(GetCSSPropertyBorderTop(), style, layout_object, styled_node,
              allow_visited_style);
      static const CSSProperty* kProperties[3] = {&GetCSSPropertyBorderRight(),
                                                  &GetCSSPropertyBorderBottom(),
                                                  &GetCSSPropertyBorderLeft()};
      for (size_t i = 0; i < WTF_ARRAY_LENGTH(kProperties); ++i) {
        if (!DataEquivalent(value, Get(*kProperties[i], style, layout_object,
                                       styled_node, allow_visited_style)))
          return nullptr;
      }
      return value;
    }
    case CSSPropertyBorderBottom:
      return ValuesForShorthandProperty(borderBottomShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorderColor:
      return ValuesForSidesShorthand(borderColorShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyBorderLeft:
      return ValuesForShorthandProperty(borderLeftShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorderRadius:
      return ValueForBorderRadiusShorthand(style);
    case CSSPropertyBorderRight:
      return ValuesForShorthandProperty(borderRightShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorderStyle:
      return ValuesForSidesShorthand(borderStyleShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyBorderTop:
      return ValuesForShorthandProperty(borderTopShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorderWidth:
      return ValuesForSidesShorthand(borderWidthShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyColumnRule:
      return ValuesForShorthandProperty(columnRuleShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyColumns:
      return ValuesForShorthandProperty(columnsShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyListStyle:
      return ValuesForShorthandProperty(listStyleShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyMargin:
      return ValuesForSidesShorthand(marginShorthand(), style, layout_object,
                                     styled_node, allow_visited_style);
    case CSSPropertyOutline:
      return ValuesForShorthandProperty(outlineShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyPadding:
      return ValuesForSidesShorthand(paddingShorthand(), style, layout_object,
                                     styled_node, allow_visited_style);
    case CSSPropertyScrollPadding:
      return ValuesForSidesShorthand(scrollPaddingShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyScrollPaddingBlock:
      return ValuesForInlineBlockShorthand(scrollPaddingBlockShorthand(), style,
                                           layout_object, styled_node,
                                           allow_visited_style);
    case CSSPropertyScrollPaddingInline:
      return ValuesForInlineBlockShorthand(scrollPaddingInlineShorthand(),
                                           style, layout_object, styled_node,
                                           allow_visited_style);
    case CSSPropertyScrollMargin:
      return ValuesForSidesShorthand(scrollMarginShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyScrollMarginBlock:
      return ValuesForInlineBlockShorthand(scrollMarginBlockShorthand(), style,
                                           layout_object, styled_node,
                                           allow_visited_style);
    case CSSPropertyScrollMarginInline:
      return ValuesForInlineBlockShorthand(scrollMarginInlineShorthand(), style,
                                           layout_object, styled_node,
                                           allow_visited_style);
    // SVG properties.
    case CSSPropertyFill:
      return AdjustSVGPaintForCurrentColor(
          svg_style.FillPaintType(), svg_style.FillPaintUri(),
          svg_style.FillPaintColor(), style.GetColor());
    case CSSPropertyStroke:
      return AdjustSVGPaintForCurrentColor(
          svg_style.StrokePaintType(), svg_style.StrokePaintUri(),
          svg_style.StrokePaintColor(), style.GetColor());
    case CSSPropertyStrokeDasharray:
      return StrokeDashArrayToCSSValueList(*svg_style.StrokeDashArray(), style);
    case CSSPropertyStrokeWidth:
      return PixelValueForUnzoomedLength(svg_style.StrokeWidth(), style);
    case CSSPropertyBaselineShift: {
      switch (svg_style.BaselineShift()) {
        case BS_SUPER:
          return CSSIdentifierValue::Create(CSSValueSuper);
        case BS_SUB:
          return CSSIdentifierValue::Create(CSSValueSub);
        case BS_LENGTH:
          return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
              svg_style.BaselineShiftValue(), style);
      }
      NOTREACHED();
      return nullptr;
    }
    case CSSPropertyPaintOrder:
      return PaintOrderToCSSValueList(svg_style);
    case CSSPropertyD:
      if (const StylePath* style_path = svg_style.D())
        return style_path->ComputedCSSValue();
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyScrollSnapType:
      return ValueForScrollSnapType(style.GetScrollSnapType(), style);
    case CSSPropertyScrollSnapAlign:
      return ValueForScrollSnapAlign(style.GetScrollSnapAlign(), style);
    case CSSPropertyOverscrollBehavior: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*CSSIdentifierValue::Create(style.OverscrollBehaviorX()));
      list->Append(*CSSIdentifierValue::Create(style.OverscrollBehaviorY()));
      return list;
    }
    case CSSPropertyTranslate: {
      if (!style.Translate())
        return CSSIdentifierValue::Create(CSSValueNone);

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (layout_object && layout_object->IsBox()) {
        LayoutRect box = ToLayoutBox(layout_object)->BorderBoxRect();
        list->Append(*ZoomAdjustedPixelValue(
            FloatValueForLength(style.Translate()->X(), box.Width().ToFloat()),
            style));
        if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0) {
          list->Append(*ZoomAdjustedPixelValue(
              FloatValueForLength(style.Translate()->Y(),
                                  box.Height().ToFloat()),
              style));
        }
      } else {
        // No box to resolve the percentage values
        list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.Translate()->X(), style));

        if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0) {
          list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
              style.Translate()->Y(), style));
        }
      }

      if (style.Translate()->Z() != 0)
        list->Append(*ZoomAdjustedPixelValue(style.Translate()->Z(), style));

      return list;
    }
    case CSSPropertyRotate: {
      if (!style.Rotate())
        return CSSIdentifierValue::Create(CSSValueNone);

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (style.Rotate()->X() != 0 || style.Rotate()->Y() != 0 ||
          style.Rotate()->Z() != 1) {
        list->Append(*CSSPrimitiveValue::Create(
            style.Rotate()->X(), CSSPrimitiveValue::UnitType::kNumber));
        list->Append(*CSSPrimitiveValue::Create(
            style.Rotate()->Y(), CSSPrimitiveValue::UnitType::kNumber));
        list->Append(*CSSPrimitiveValue::Create(
            style.Rotate()->Z(), CSSPrimitiveValue::UnitType::kNumber));
      }
      list->Append(*CSSPrimitiveValue::Create(
          style.Rotate()->Angle(), CSSPrimitiveValue::UnitType::kDegrees));
      return list;
    }
    case CSSPropertyScale: {
      if (!style.Scale())
        return CSSIdentifierValue::Create(CSSValueNone);
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*CSSPrimitiveValue::Create(
          style.Scale()->X(), CSSPrimitiveValue::UnitType::kNumber));
      if (style.Scale()->Y() == 1 && style.Scale()->Z() == 1)
        return list;
      list->Append(*CSSPrimitiveValue::Create(
          style.Scale()->Y(), CSSPrimitiveValue::UnitType::kNumber));
      if (style.Scale()->Z() != 1)
        list->Append(*CSSPrimitiveValue::Create(
            style.Scale()->Z(), CSSPrimitiveValue::UnitType::kNumber));
      return list;
    }
    case CSSPropertyContain: {
      if (!style.Contain())
        return CSSIdentifierValue::Create(CSSValueNone);
      if (style.Contain() == kContainsStrict)
        return CSSIdentifierValue::Create(CSSValueStrict);
      if (style.Contain() == kContainsContent)
        return CSSIdentifierValue::Create(CSSValueContent);

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (style.ContainsStyle())
        list->Append(*CSSIdentifierValue::Create(CSSValueStyle));
      if (style.Contain() & kContainsLayout)
        list->Append(*CSSIdentifierValue::Create(CSSValueLayout));
      if (style.ContainsPaint())
        list->Append(*CSSIdentifierValue::Create(CSSValuePaint));
      if (style.ContainsSize())
        list->Append(*CSSIdentifierValue::Create(CSSValueSize));
      DCHECK(list->length());
      return list;
    }
    default:
      return resolved_property.CSSValueFromComputedStyle(
          style, layout_object, styled_node, allow_visited_style);
  }
}

}  // namespace blink
