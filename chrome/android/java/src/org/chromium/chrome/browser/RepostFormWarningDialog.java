// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.ui.widget.UiWidgetFactory;

/**
 * Form resubmission warning dialog. Presents the cancel/continue choice and fires one of two
 * callbacks accordingly.
 */
public class RepostFormWarningDialog extends DialogFragment {
    // Warning dialog currently being shown, stored for testing.
    private static Dialog sCurrentDialog;

    private final Tab mTab;
    private final TabObserver mTabObserver;

    /** Empty constructor required for DialogFragments. */
    public RepostFormWarningDialog() {
        mTab = null;
        mTabObserver = null;
    }

    /**
     * Handles the repost form warning for the given Tab.
     * @param tab The tab waiting for confirmation on a repost form warning.
     */
    @SuppressLint("ValidFragment")
    public RepostFormWarningDialog(Tab tab) {
        mTab = tab;
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onDestroyed(Tab tab) {
                dismissAllowingStateLoss();
            }
        };
        mTab.addObserver(mTabObserver);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // If there is savedInstanceState, then the dialog is being recreated by android
        // and will lack the necessary callbacks.  Dismiss immediately as the tab will
        // need to be recreated anyway.
        if (savedInstanceState != null) {
            dismiss();
        }
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        DialogInterface.OnClickListener negativeButtonListener = (dialog, id) -> {
            if (!mTab.isInitialized()) return;
            mTab.getWebContents().getNavigationController().cancelPendingReload();
        };
        DialogInterface.OnClickListener positiveButtonListener = (dialog, id) -> {
            if (!mTab.isInitialized()) return;
            mTab.getWebContents().getNavigationController().continuePendingReload();
        };
        Dialog dialog;
        if (VrModuleProvider.getDelegate().isInVr()) {
            android.app.AlertDialog alertDialog =
                    UiWidgetFactory.getInstance().createAlertDialog(getActivity());
            alertDialog.setMessage(alertDialog.getContext().getString(R.string.http_post_warning));
            alertDialog.setCancelable(true);
            if (savedInstanceState == null) {
                assert mTab != null;
                alertDialog.setButton(DialogInterface.BUTTON_NEGATIVE,
                        alertDialog.getContext().getString(R.string.cancel),
                        negativeButtonListener);
                alertDialog.setButton(DialogInterface.BUTTON_POSITIVE,
                        alertDialog.getContext().getString(R.string.http_post_warning_resend),
                        positiveButtonListener);
            }
            dialog = alertDialog;
        } else {
            AlertDialog.Builder builder =
                    new AlertDialog.Builder(getActivity(), R.style.AlertDialogTheme)
                            .setMessage(R.string.http_post_warning);

            if (savedInstanceState == null) {
                assert mTab != null;
                builder.setNegativeButton(R.string.cancel, negativeButtonListener);
                builder.setPositiveButton(
                        R.string.http_post_warning_resend, positiveButtonListener);
            }
            dialog = builder.create();
        }
        setCurrentDialogForTesting(dialog);

        return dialog;
    }

    @Override
    public void dismiss() {
        if (getFragmentManager() == null) return;
        super.dismiss();
    }

    @Override
    public void dismissAllowingStateLoss() {
        if (getFragmentManager() == null) return;
        super.dismissAllowingStateLoss();
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        setCurrentDialogForTesting(null);

        if (mTab != null && mTabObserver != null) {
            mTab.removeObserver(mTabObserver);
        }
    }

    /**
     * Sets the currently displayed dialog in sCurrentDialog. This is required by findbugs, which
     * allows static fields only to be set from static methods.
     */
    private static void setCurrentDialogForTesting(Dialog dialog) {
        sCurrentDialog = dialog;
    }

    /**
     * @return dialog currently being displayed.
     */
    @VisibleForTesting
    public static Dialog getCurrentDialogForTesting() {
        return sCurrentDialog;
    }
}
