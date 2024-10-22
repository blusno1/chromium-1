// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

/**
 * Delegate for survey info bar actions.
 */
public interface SurveyInfoBarDelegate {
    /**
     * Notified when the tab containing the infobar is closed.
     */
    void onSurveyInfoBarTabHidden();

    /**
     * Notified when the interactability of the tab containing the infobar is changed.
     */
    void onSurveyInfoBarTabInteractabilityChanged(boolean isInteractable);

    /**
     * Notified when the survey infobar's close button is clicked.
     */
    void onSurveyInfoBarCloseButtonClicked();

    /**
     * Notified when the survey is triggered via the infobar.
     */
    void onSurveyTriggered();

    /**
     * Called to supply the survey info bar with the prompt string.
     * @return The string that will be displayed on the info bar.
     */
    String getSurveyPromptString();
}
