// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_payments_section', function() {
  suite('PaymentsSection', function() {
    /** @type {settings.SyncBrowserProxy} */
    let syncBrowserProxy = null;

    setup(function() {
      syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
      PolymerTest.clearBody();
    });

    /**
     * Creates the payments autofill section for the given list.
     * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>} creditCards
     * @param {!Object} prefValues
     * @return {!Object}
     */
    function createPaymentsSection(creditCards, prefValues) {
      // Override the PaymentsManagerImpl for testing.
      const paymentsManager = new TestPaymentsManager();
      paymentsManager.data.creditCards = creditCards;
      PaymentsManagerImpl.instance_ = paymentsManager;

      const section = document.createElement('settings-payments-section');
      section.prefs = {autofill: prefValues};
      document.body.appendChild(section);
      Polymer.dom.flush();

      return section;
    }

    /**
     * Creates the Edit Credit Card dialog.
     * @param {!chrome.autofillPrivate.CreditCardEntry} creditCardItem
     * @return {!Object}
     */
    function createCreditCardDialog(creditCardItem) {
      const section =
          document.createElement('settings-credit-card-edit-dialog');
      section.creditCard = creditCardItem;
      document.body.appendChild(section);
      Polymer.dom.flush();
      return section;
    }

    test('verifyCreditCardCount', function() {
      const section = createPaymentsSection(
          [], {enabled: {value: true}, credit_card_enabled: {value: true}});

      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(0, creditCardList.querySelectorAll('.list-item').length);

      assertFalse(section.$$('#noCreditCardsLabel').hidden);
      assertTrue(section.$$('#creditCardsHeading').hidden);
      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertFalse(section.$$('#addCreditCard').disabled);
    });

    test('verifyDisabled', function() {
      const section = createPaymentsSection(
          [],
          {enabled: {value: false}, credit_card_enabled: {value: true}});

      assertTrue(section.$$('#autofillCreditCardToggle').disabled);
      assertTrue(section.$$('#addCreditCard').disabled);
    });

    test('verifyCreditCardsDisabled', function() {
      const section = createPaymentsSection(
          [],
          {enabled: {value: true}, credit_card_enabled: {value: false}});

      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertTrue(section.$$('#addCreditCard').disabled);
    });

    test('verifyCreditCardCount', function() {
      const creditCards = [
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
      ];

      const section = createPaymentsSection(
          creditCards,
          {enabled: {value: true}, credit_card_enabled: {value: true}});
      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(
          creditCards.length,
          creditCardList.querySelectorAll('.list-item').length);

      assertTrue(section.$$('#noCreditCardsLabel').hidden);
      assertFalse(section.$$('#creditCardsHeading').hidden);
      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertFalse(section.$$('#addCreditCard').disabled);
    });

    test('verifyCreditCardFields', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      const section = createPaymentsSection([creditCard], {});
      const creditCardList = section.$$('#creditCardList');
      const row = creditCardList.children[0];
      assertTrue(!!row);

      assertEquals(
          creditCard.metadata.summaryLabel,
          row.querySelector('#creditCardLabel').textContent);
      assertEquals(
          creditCard.expirationMonth + '/' + creditCard.expirationYear,
          row.querySelector('#creditCardExpiration').textContent);
    });

    test('verifyCreditCardRowButtonIsDropdownWhenLocal', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isLocal = true;
      const section = createPaymentsSection([creditCard], {});
      const creditCardList = section.$$('#creditCardList');
      const row = creditCardList.children[0];
      assertTrue(!!row);
      const menuButton = row.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      const outlinkButton =
          row.querySelector('paper-icon-button-light.icon-external');
      assertFalse(!!outlinkButton);
    });

    test('verifyCreditCardRowButtonIsOutlinkWhenRemote', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isLocal = false;
      const section = createPaymentsSection([creditCard], {});
      const creditCardList = section.$$('#creditCardList');
      const row = creditCardList.children[0];
      assertTrue(!!row);
      const menuButton = row.querySelector('#creditCardMenu');
      assertFalse(!!menuButton);
      const outlinkButton =
          row.querySelector('paper-icon-button-light.icon-external');
      assertTrue(!!outlinkButton);
    });

    test('verifyAddVsEditCreditCardTitle', function() {
      const newCreditCard = FakeDataMaker.emptyCreditCardEntry();
      const newCreditCardDialog = createCreditCardDialog(newCreditCard);
      const oldCreditCard = FakeDataMaker.creditCardEntry();
      const oldCreditCardDialog = createCreditCardDialog(oldCreditCard);

      assertNotEquals(oldCreditCardDialog.title_, newCreditCardDialog.title_);
      assertNotEquals('', newCreditCardDialog.title_);
      assertNotEquals('', oldCreditCardDialog.title_);

      // Wait for dialogs to open before finishing test.
      return Promise.all([
        test_util.whenAttributeIs(newCreditCardDialog.$.dialog, 'open', ''),
        test_util.whenAttributeIs(oldCreditCardDialog.$.dialog, 'open', ''),
      ]);
    });

    test('verifyExpiredCreditCardYear', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // 2015 is over unless time goes wobbly.
      const twentyFifteen = 2015;
      creditCard.expirationYear = twentyFifteen.toString();

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            const now = new Date();
            const maxYear = now.getFullYear() + 9;
            const yearOptions = creditCardDialog.$.year.options;

            assertEquals('2015', yearOptions[0].textContent.trim());
            assertEquals(
                maxYear.toString(),
                yearOptions[yearOptions.length - 1].textContent.trim());
            assertEquals(
                creditCard.expirationYear, creditCardDialog.$.year.value);
          });
    });

    test('verifyVeryFutureCreditCardYear', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 20 years from now is unusual.
      const now = new Date();
      const farFutureYear = now.getFullYear() + 20;
      creditCard.expirationYear = farFutureYear.toString();

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            const yearOptions = creditCardDialog.$.year.options;

            assertEquals(
                now.getFullYear().toString(),
                yearOptions[0].textContent.trim());
            assertEquals(
                farFutureYear.toString(),
                yearOptions[yearOptions.length - 1].textContent.trim());
            assertEquals(
                creditCard.expirationYear, creditCardDialog.$.year.value);
          });
    });

    test('verifyVeryNormalCreditCardYear', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 2 years from now is not unusual.
      const now = new Date();
      const nearFutureYear = now.getFullYear() + 2;
      creditCard.expirationYear = nearFutureYear.toString();
      const maxYear = now.getFullYear() + 9;

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            const yearOptions = creditCardDialog.$.year.options;

            assertEquals(
                now.getFullYear().toString(),
                yearOptions[0].textContent.trim());
            assertEquals(
                maxYear.toString(),
                yearOptions[yearOptions.length - 1].textContent.trim());
            assertEquals(
                creditCard.expirationYear, creditCardDialog.$.year.value);
          });
    });

    test('verify save disabled for expired credit card', function() {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();

      const now = new Date();
      creditCard.expirationYear = now.getFullYear() - 2;
      // works fine for January.
      creditCard.expirationMonth = now.getMonth() - 1;

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            assertTrue(creditCardDialog.$.saveButton.disabled);
          });
    });

    test('verify save new credit card', function() {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();
      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').
            then(function() {
              // Not expired, but still can't be saved, because there's no
              // name.
              assertTrue(creditCardDialog.$.expired.hidden);
              assertTrue(creditCardDialog.$.saveButton.disabled);

              // Add a name and trigger the on-input handler.
              creditCardDialog.set('creditCard.name', 'Jane Doe');
              creditCardDialog.onCreditCardNameOrNumberChanged_();
              Polymer.dom.flush();

              assertTrue(creditCardDialog.$.expired.hidden);
              assertFalse(creditCardDialog.$.saveButton.disabled);

              const savedPromise = test_util.eventToPromise('save-credit-card',
                  creditCardDialog);
              creditCardDialog.$.saveButton.click();
              return savedPromise;
            }).then(function(event) {
              assertEquals(creditCard.guid, event.detail.guid);
            });
    });

    test('verifyCancelCreditCardEdit', function(done) {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();
      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            test_util.eventToPromise('save-credit-card', creditCardDialog)
              .then(function() {
                // Fail the test because the save event should not be called
                // when cancel is clicked.
                assertTrue(false);
                done();
              });

            test_util.eventToPromise('close', creditCardDialog)
              .then(function() {
                // Test is |done| in a timeout in order to ensure that
                // 'save-credit-card' is NOT fired after this test.
                window.setTimeout(done, 100);
              });

            creditCardDialog.$.cancelButton.click();
          });
    });

    test('verifyLocalCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // When credit card is local, |isCached| will be undefined.
      creditCard.metadata.isLocal = true;
      creditCard.metadata.isCached = undefined;

      const section = createPaymentsSection([creditCard], {});
      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(1, creditCardList.querySelectorAll('.list-item').length);
      const row = creditCardList.children[0];

      // Local credit cards will show the overflow menu.
      assertFalse(!!row.querySelector('#remoteCreditCardLink'));
      const menuButton = row.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);

      menuButton.click();
      Polymer.dom.flush();

      const menu = section.$.creditCardSharedMenu;

      // Menu should have 2 options.
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertFalse(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertTrue(menu.querySelector('#menuClearCreditCard').hidden);

      menu.close();
      Polymer.dom.flush();
    });

    test('verifyCachedCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      creditCard.metadata.isLocal = false;
      creditCard.metadata.isCached = true;

      const section = createPaymentsSection([creditCard], {});
      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(1, creditCardList.querySelectorAll('.list-item').length);
      const row = creditCardList.children[0];

      // Cached remote CCs will show overflow menu.
      assertFalse(!!row.querySelector('#remoteCreditCardLink'));
      const menuButton = row.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);

      menuButton.click();
      Polymer.dom.flush();

      const menu = section.$.creditCardSharedMenu;

      // Menu should have 2 options.
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertTrue(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertFalse(menu.querySelector('#menuClearCreditCard').hidden);

      menu.close();
      Polymer.dom.flush();
    });

    test('verifyNotCachedCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      creditCard.metadata.isLocal = false;
      creditCard.metadata.isCached = false;

      const section = createPaymentsSection([creditCard], {});
      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(1, creditCardList.querySelectorAll('.list-item').length);
      const row = creditCardList.children[0];

      // No overflow menu when not cached.
      assertTrue(!!row.querySelector('#remoteCreditCardLink'));
      assertFalse(!!row.querySelector('#creditCardMenu'));
    });

    test('verifyMigrationButtonNotShownIfMigrationNotEnabled', function() {
      // Disable the migration experimental flag. Won't show migration button.
      loadTimeData.overrideValues({migrationEnabled: false});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard],
          {enabled: {value: true}, credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfNotSignedIn', function() {
      // Enable the migration experimental flag.
      loadTimeData.overrideValues({migrationEnabled: true});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard],
          {enabled: {value: true}, credit_card_enabled: {value: true}});

      // Simulate not Signed-in status. Won't show migration button.
      sync_test_util.simulateSyncStatus({
        signedIn: false,
        syncSystemEnabled: true,
      });

      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfNotSynced', function() {
      // Enable the migration experimental flag.
      loadTimeData.overrideValues({migrationEnabled: true});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard],
          {enabled: {value: true}, credit_card_enabled: {value: true}});

      // Simulate not Synced status. Won't show migration button.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: false,
      });

      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfNoMigratableCard', function() {
      // Enable the migration experimental flag.
      loadTimeData.overrideValues({migrationEnabled: true});

      // Add one credit card but not migratable. Won't show migration button.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = false;
      const section = createPaymentsSection(
          [creditCard],
          {enabled: {value: true}, credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownWhenCreditCardDisabled', function() {
      // Enable the migration experimental flag.
      loadTimeData.overrideValues({migrationEnabled: true});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard],
          {enabled: {value: true}, credit_card_enabled: {value: false}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements meet but credit card is disable, verify
      // migration button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownWhenAutofillDisabled', function() {
      // Enable the migration experimental flag.
      loadTimeData.overrideValues({migrationEnabled: true});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard],
          {enabled: {value: false}, credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements meet but credit card is disable, verify
      // migration button is hidden.
      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonShown', function() {
      // Enable the migration experimental flag.
      loadTimeData.overrideValues({migrationEnabled: true});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard],
          {enabled: {value: true}, credit_card_enabled: {value: true}});

      // Simulate Signed-in and Synced status.
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });

      // All migration requirements meet, verify migration button is shown.
      assertFalse(section.$$('#migrateCreditCards').hidden);
    });
  });
});
