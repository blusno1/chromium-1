// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_FAVICON_CACHE_H_
#define CHROME_BROWSER_UI_OMNIBOX_FAVICON_CACHE_H_

#include <list>
#include <map>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"

namespace favicon {
class FaviconService;
}

namespace gfx {
class Image;
}

class GURL;

typedef base::OnceCallback<void(const gfx::Image& favicon)>
    FaviconFetchedCallback;

// We cache a very small number of favicons so we can synchronously deliver
// them to prevent flicker as the user types.
class FaviconCache : public history::HistoryServiceObserver {
 public:
  FaviconCache(favicon::FaviconService* favicon_service,
               history::HistoryService* history_service);
  ~FaviconCache() override;

  gfx::Image GetFaviconForPageUrl(const GURL& page_url,
                                  FaviconFetchedCallback on_favicon_fetched);

 private:
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ClearIconsWithHistoryDeletions);

  void OnFaviconFetched(const GURL& page_url,
                        const favicon_base::FaviconImageResult& result);

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Non-owning pointer to a KeyedService.
  favicon::FaviconService* favicon_service_;

  ScopedObserver<history::HistoryService, FaviconCache> history_observer_;

  base::CancelableTaskTracker task_tracker_;
  std::map<GURL, std::list<FaviconFetchedCallback>> pending_requests_;

  base::MRUCache<GURL, gfx::Image> mru_cache_;
  base::WeakPtrFactory<FaviconCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FaviconCache);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_FAVICON_CACHE_H_
