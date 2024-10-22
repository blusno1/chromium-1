// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/favicon_cache.h"

#include "components/favicon/core/test/mock_favicon_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace {

favicon_base::FaviconImageResult GetDummyFaviconResult() {
  favicon_base::FaviconImageResult result;

  result.icon_url = GURL("http://example.com/favicon.ico");

  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorBLUE);
  result.image = gfx::Image::CreateFrom1xBitmap(bitmap);

  return result;
}

void VerifyFetchedFavicon(int* count, const gfx::Image& favicon) {
  DCHECK(count);
  EXPECT_FALSE(favicon.IsEmpty());
  ++(*count);
}

void Fail(const gfx::Image& favicon) {
  FAIL() << "The favicon should have been provided synchronously by the cache, "
            "and this asynchronous callback should never have been called.";
}

void NoOp(const gfx::Image& favicon) {}

}  // namespace

class FaviconCacheTest : public testing::Test {
 protected:
  const GURL kUrlA = GURL("http://www.a.com/");
  const GURL kUrlB = GURL("http://www.b.com/");

  FaviconCacheTest()
      : cache_(&favicon_service_, nullptr /* history_service */) {}

  testing::NiceMock<favicon::MockFaviconService> favicon_service_;

  void ExpectFaviconServiceCalls(int a_site_calls, int b_site_calls) {
    if (a_site_calls > 0) {
      EXPECT_CALL(
          favicon_service_,
          GetFaviconImageForPageURL(kUrlA, _ /* callback */, _ /* tracker */))
          .Times(a_site_calls)
          .WillRepeatedly(
              DoAll(SaveArg<1>(&favicon_service_a_site_response_),
                    Return(base::CancelableTaskTracker::kBadTaskId)));
    }

    if (b_site_calls > 0) {
      EXPECT_CALL(
          favicon_service_,
          GetFaviconImageForPageURL(kUrlB, _ /* callback */, _ /* tracker */))
          .Times(b_site_calls)
          .WillRepeatedly(
              DoAll(SaveArg<1>(&favicon_service_b_site_response_),
                    Return(base::CancelableTaskTracker::kBadTaskId)));
    }
  }

  favicon_base::FaviconImageCallback favicon_service_a_site_response_;
  favicon_base::FaviconImageCallback favicon_service_b_site_response_;

  FaviconCache cache_;
};

TEST_F(FaviconCacheTest, Basic) {
  ExpectFaviconServiceCalls(1, 0);

  int response_count = 0;
  gfx::Image result = cache_.GetFaviconForPageUrl(
      kUrlA, base::BindOnce(&VerifyFetchedFavicon, &response_count));

  // Expect the synchronous result to be empty.
  EXPECT_TRUE(result.IsEmpty());

  favicon_service_a_site_response_.Run(GetDummyFaviconResult());

  // Re-request the same favicon and expect a non-empty result now that the
  // cache is populated. The above EXPECT_CALL will also verify that the
  // backing FaviconService is not hit again.
  result = cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail));

  EXPECT_FALSE(result.IsEmpty());
  EXPECT_EQ(1, response_count);
}

TEST_F(FaviconCacheTest, MultipleRequestsAreCoalesced) {
  ExpectFaviconServiceCalls(1, 0);

  int response_count = 0;
  for (int i = 0; i < 10; ++i) {
    cache_.GetFaviconForPageUrl(
        kUrlA, base::BindOnce(&VerifyFetchedFavicon, &response_count));
  }

  favicon_service_a_site_response_.Run(GetDummyFaviconResult());

  EXPECT_EQ(10, response_count);
}

TEST_F(FaviconCacheTest, SeparateOriginsAreCachedSeparately) {
  ExpectFaviconServiceCalls(1, 1);

  int a_site_response_count = 0;
  int b_site_response_count = 0;

  gfx::Image a_site_return = cache_.GetFaviconForPageUrl(
      kUrlA, base::BindOnce(&VerifyFetchedFavicon, &a_site_response_count));
  gfx::Image b_site_return = cache_.GetFaviconForPageUrl(
      kUrlB, base::BindOnce(&VerifyFetchedFavicon, &b_site_response_count));

  EXPECT_TRUE(a_site_return.IsEmpty());
  EXPECT_TRUE(b_site_return.IsEmpty());
  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(0, b_site_response_count);

  favicon_service_b_site_response_.Run(GetDummyFaviconResult());

  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  a_site_return = cache_.GetFaviconForPageUrl(
      kUrlA, base::BindOnce(&VerifyFetchedFavicon, &a_site_response_count));
  b_site_return = cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail));

  EXPECT_TRUE(a_site_return.IsEmpty());
  EXPECT_FALSE(b_site_return.IsEmpty());
  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  favicon_service_a_site_response_.Run(GetDummyFaviconResult());

  EXPECT_EQ(2, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  a_site_return = cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail));
  b_site_return = cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail));

  EXPECT_FALSE(a_site_return.IsEmpty());
  EXPECT_FALSE(b_site_return.IsEmpty());
}

TEST_F(FaviconCacheTest, ClearIconsWithHistoryDeletions) {
  ExpectFaviconServiceCalls(3, 2);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&NoOp)).IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&NoOp)).IsEmpty());

  favicon_service_a_site_response_.Run(GetDummyFaviconResult());
  favicon_service_b_site_response_.Run(GetDummyFaviconResult());

  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Delete just the entry for kUrlA.
  history::URLRows a_rows = {history::URLRow(kUrlA)};
  cache_.OnURLsDeleted(nullptr /* history_service */, false /* all_history */,
                       false /* expired */, a_rows, {} /* favicon_urls */);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&NoOp)).IsEmpty());
  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Restore the cache entry for kUrlA.
  favicon_service_a_site_response_.Run(GetDummyFaviconResult());

  // Delete all history.
  cache_.OnURLsDeleted(nullptr /* history_service */, true /* all_history */,
                       false /* expired */, {} /* deleted_rows */,
                       {} /* favicon_urls */);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&NoOp)).IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&NoOp)).IsEmpty());
}
