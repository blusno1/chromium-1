// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.languages;

import android.content.Context;
import android.preference.Preference;
import android.support.v7.widget.DividerItemDecoration;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.browser.widget.ListMenuButton.Item;
import org.chromium.chrome.browser.widget.TintedDrawable;

import java.util.ArrayList;

/**
 * A preference that displays the current accept language list.
 */
public class LanguageListPreference extends Preference {
    private static class LanguageListAdapter
            extends LanguageListBaseAdapter implements LanguagesManager.AcceptLanguageObserver {

        LanguageListAdapter(Context context) {
            super(context);
        }

        @Override
        public void onBindViewHolder(
                LanguageListBaseAdapter.LanguageRowViewHolder holder, int position) {
            super.onBindViewHolder(holder, position);

            holder.setStartIcon(R.drawable.ic_drag_handle_grey600_24dp);

            final LanguageItem info = getItemByPosition(position);
            holder.setMenuButtonDelegate(new ListMenuButton.Delegate() {
                @Override
                public Item[] getItems() {
                    ArrayList<Item> menuItems = new ArrayList<>();

                    // Show "Offer to translate" option if "Chrome Translate" is enabled.
                    if (PrefServiceBridge.getInstance().isTranslateEnabled()) {
                        // Set this row checked if the language is unblocked.
                        int endIconResId =
                                PrefServiceBridge.getInstance().isBlockedLanguage(info.getCode())
                                ? 0
                                : R.drawable.ic_check_googblue_24dp;
                        // Add checked icon at the end.
                        menuItems.add(new Item(mContext,
                                R.string.languages_item_option_offer_to_translate, endIconResId,
                                info.isSupported()));
                    }

                    // Enable "Remove" option if there are multiple accept languages.
                    menuItems.add(new Item(mContext, R.string.remove, getItemCount() > 1));

                    return menuItems.toArray(new Item[menuItems.size()]);
                }

                @Override
                public void onItemSelected(Item item) {
                    if (item.getTextId() == R.string.languages_item_option_offer_to_translate) {
                        // Toggle current blocked state of this language.
                        boolean state = (item.getEndIconId() == 0);
                        PrefServiceBridge.getInstance().setLanguageBlockedState(
                                info.getCode(), !state);
                        LanguagesManager.recordAction(
                                state ? LanguagesManager.ACTION_ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE
                                      : LanguagesManager
                                                .ACTION_DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);
                    } else if (item.getTextId() == R.string.remove) {
                        LanguagesManager.getInstance().removeFromAcceptLanguages(info.getCode());
                        LanguagesManager.recordAction(LanguagesManager.ACTION_LANGUAGE_REMOVED);
                    }
                }
            });
        }

        @Override
        public void onDataUpdated() {
            reload(LanguagesManager.getInstance().getUserAcceptLanguageItems());
        }
    }

    private View mView;
    private Button mAddLanguageButton;
    private RecyclerView mRecyclerView;
    private LanguageListAdapter mAdapter;

    private AddLanguageFragment.Launcher mLauncher;

    public LanguageListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAdapter = new LanguageListAdapter(context);
    }

    @Override
    public View onCreateView(ViewGroup parent) {
        assert mLauncher != null;

        if (mView != null) return mView;

        mView = super.onCreateView(parent);

        mAddLanguageButton = (Button) mView.findViewById(R.id.add_language);
        ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(mAddLanguageButton,
                TintedDrawable.constructTintedDrawable(
                        getContext().getResources(), R.drawable.plus, R.color.pref_accent_color),
                null, null, null);
        mAddLanguageButton.setOnClickListener(view -> {
            mLauncher.launchAddLanguage();
            LanguagesManager.recordAction(LanguagesManager.ACTION_CLICK_ON_ADD_LANGUAGE);
        });

        mRecyclerView = (RecyclerView) mView.findViewById(R.id.language_list);
        LinearLayoutManager layoutMangager = new LinearLayoutManager(getContext());
        mRecyclerView.setLayoutManager(layoutMangager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(getContext(), layoutMangager.getOrientation()));

        // Due to a known native bug (crbug/640763), the list order written into Preference Service
        // might be different from the order shown after it's adjusted by dragging.
        mAdapter.enableDrag(mRecyclerView);

        return mView;
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        // We do not want the RecyclerView to be announced by screen readers every time
        // the view is bound.
        if (mRecyclerView.getAdapter() != mAdapter) {
            mRecyclerView.setAdapter(mAdapter);
            LanguagesManager.getInstance().setAcceptLanguageObserver(mAdapter);
            // Initialize accept language list.
            mAdapter.reload(LanguagesManager.getInstance().getUserAcceptLanguageItems());
        }
    }

    /**
     * Register a launcher for AddLanguageFragment. Preference's host fragment should call
     * this in its onCreate().
     */
    void registerActivityLauncher(AddLanguageFragment.Launcher launcher) {
        mLauncher = launcher;
    }
}
