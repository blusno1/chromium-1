// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

Polymer({
  is: 'sync-consent',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Value of "Sync all" trigger button.
     */
    syncAllEnabled_: {
      type: Boolean,
      value: true,
    },

    /**
     * Name of currently active section.
     */
    activeSection_: {
      type: String,
      value: 'overview',
    }
  },

  focus: function() {
    if (this.activeSection_ == 'overview') {
      this.$.syncConsentOverviewDialog.show();
    } else {
      this.$.syncConsentSettingsDialog.show();
    }
  },

  isSectionHidden_: function(activeSectionName, thisSectionName) {
    return activeSectionName != thisSectionName;
  },

  /**
   * This is 'on-tap' event handler for 'Back' button of Settings section.
   * @private
   */
  onSettingsBack_: function() {
    this.activeSection_ = 'overview';
    this.focus();
  },

  /**
   * This updates link to 'Settings' section on locale change.
   * @param {string} locale The UI language used.
   * @private
   */
  updateSettingsLink_: function(locale) {
    this.$.settingsLink.innerHTML = loadTimeData.getStringF(
        'syncConsentScreenSettingsLink', '<a id="settingsLinkAnchor" href="#">',
        '</a>');
    this.$$('#settingsLinkAnchor')
        .addEventListener('click', this.switchToSettingsDialog_.bind(this));
  },

  /**
   * This is called when "Settings" link is clicked.
   * @private
   */
  switchToSettingsDialog_: function() {
    this.activeSection_ = 'settings';
    this.focus();
  },

  /**
   * This is called when "Sync all" trigger value is changed.
   * @param {!Event} event
   * @private
   */
  onSyncAllEnabledChanged_: function(event) {
    this.syncAllEnabled_ = event.currentTarget.checked;
  },

  /**
   * This is 'on-tap' event handler for 'AcceptAndContinue/Next' buttons.
   * @private
   */
  onSettingsSaveAndContinue_: function() {
    chrome.send('login.SyncConsentScreen.userActed', ['save-and-continue']);
  },
});
