// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_root_view.h"

#include <cmath>
#include <string>

#include "base/metrics/user_metrics.h"
#include "base/task/post_task.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"

namespace {

using FileSupportedCallback =
    base::OnceCallback<void(const GURL& url, bool supported)>;

// Get the MIME type of the file pointed to by the url, based on the file's
// extension. Must be called on a thread that allows IO.
std::string FindURLMimeType(const GURL& url) {
  base::AssertBlockingAllowed();
  base::FilePath full_path;
  net::FileURLToFilePath(url, &full_path);

  // Get the MIME type based on the filename.
  std::string mime_type;
  net::GetMimeTypeFromFile(full_path, &mime_type);

  return mime_type;
}

void OnFindURLMimeType(const GURL& url,
                       Profile* profile,
                       FileSupportedCallback callback,
                       const std::string& mime_type) {
  // Check whether the mime type, if given, is known to be supported or whether
  // there is a plugin that supports the mime type (e.g. PDF).
  // TODO(bauerb): This possibly uses stale information, but it's guaranteed not
  // to do disk access.
  content::WebPluginInfo plugin;
  std::move(callback).Run(
      url,
      mime_type.empty() || blink::IsSupportedMimeType(mime_type) ||
          content::PluginService::GetInstance()->GetPluginInfo(
              -1,                // process ID
              MSG_ROUTING_NONE,  // routing ID
              profile->GetResourceContext(), url, url::Origin(), mime_type,
              false, nullptr, &plugin, nullptr));
}

bool GetURLForDrop(const ui::DropTargetEvent& event, GURL* url) {
  DCHECK(url);
  base::string16 title;
  return event.data().GetURLAndTitle(ui::OSExchangeData::CONVERT_FILENAMES, url,
                                     &title) &&
         url->is_valid();
}

int GetDropEffect(const ui::DropTargetEvent& event, const GURL& url) {
  const int source_ops = event.source_operations();
  if (source_ops & ui::DragDropTypes::DRAG_COPY)
    return ui::DragDropTypes::DRAG_COPY;
  if (source_ops & ui::DragDropTypes::DRAG_LINK)
    return ui::DragDropTypes::DRAG_LINK;
  return ui::DragDropTypes::DRAG_MOVE;
}

}  // namespace

BrowserRootView::DropInfo::DropInfo() = default;

BrowserRootView::DropInfo::~DropInfo() {
  if (target)
    target->HandleDragExited();
}

// static
const char BrowserRootView::kViewClassName[] =
    "browser/ui/views/frame/BrowserRootView";

BrowserRootView::BrowserRootView(BrowserView* browser_view,
                                 views::Widget* widget)
    : views::internal::RootView(widget),
      browser_view_(browser_view),
      weak_ptr_factory_(this) {}

BrowserRootView::~BrowserRootView() = default;

bool BrowserRootView::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  if (tabstrip()->visible() || toolbar()->visible()) {
    *formats = ui::OSExchangeData::URL | ui::OSExchangeData::STRING;
    return true;
  }
  return false;
}

bool BrowserRootView::AreDropTypesRequired() {
  return true;
}

bool BrowserRootView::CanDrop(const ui::OSExchangeData& data) {
  // If it's not tabbed browser, we don't have to support drag and drops.
  if (!browser_view_->IsBrowserTypeNormal())
    return false;

  if (!tabstrip()->visible() && !toolbar()->visible())
    return false;

  // If there is a URL, we'll allow the drop.
  if (data.HasURL(ui::OSExchangeData::CONVERT_FILENAMES))
    return true;

  // If there isn't a URL, see if we can 'paste and go'.
  return GetPasteAndGoURL(data, nullptr);
}

void BrowserRootView::OnDragEntered(const ui::DropTargetEvent& event) {
  drop_info_ = std::make_unique<DropInfo>();
  GURL url;
  if (GetURLForDrop(event, &url)) {
    drop_info_->url = url;

    // Check if the file is supported.
    if (url.SchemeIsFile()) {
      base::PostTaskWithTraitsAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&FindURLMimeType, url),
          base::BindOnce(&OnFindURLMimeType, url, browser_view_->GetProfile(),
                         base::BindOnce(&BrowserRootView::OnFileSupported,
                                        weak_ptr_factory_.GetWeakPtr())));
    }
  }
}

int BrowserRootView::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (!drop_info_)
    OnDragEntered(event);

  if (auto* drop_target = GetDropTarget(event)) {
    if (drop_info_->target && drop_info_->target != drop_target)
      drop_info_->target->HandleDragExited();
    drop_info_->target = drop_target;

    if (!drop_info_->file_supported ||
        drop_info_->url.SchemeIs(url::kJavaScriptScheme)) {
      drop_info_->index.reset();
    } else {
      drop_info_->index =
          GetDropIndexForEvent(event, event.data(), drop_target);
    }

    drop_target->HandleDragUpdate(drop_info_->index);
    return drop_info_->index ? GetDropEffect(event, drop_info_->url)
                             : ui::DragDropTypes::DRAG_NONE;
  }

  OnDragExited();
  return ui::DragDropTypes::DRAG_NONE;
}

void BrowserRootView::OnDragExited() {
  drop_info_.reset();
}

int BrowserRootView::OnPerformDrop(const ui::DropTargetEvent& event) {
  using base::UserMetricsAction;

  if (!drop_info_)
    return ui::DragDropTypes::DRAG_NONE;

  // Ensure we call HandleDragExited() on |drop_info_|'s |target| when this
  // function returns.
  std::unique_ptr<DropInfo> drop_info = std::move(drop_info_);

  // Extract the URL and create a new ui::OSExchangeData containing the URL. We
  // do this as the TabStrip doesn't know about the autocomplete edit and needs
  // to know about it to handle 'paste and go'.
  GURL url;
  if (!GetURLForDrop(event, &url)) {
    // The url isn't valid. Use the paste and go url.
    GetPasteAndGoURL(event.data(), &url);
  }

  // Do nothing if the file was unsupported, the URL is invalid, or this is a
  // javascript: URL (prevent self-xss). The URL may have been changed after
  // |drop_info| was created.
  if (!drop_info->file_supported || !url.is_valid() ||
      url.SchemeIs(url::kJavaScriptScheme))
    return ui::DragDropTypes::DRAG_NONE;

  NavigateParams params(browser_view_->browser(), url,
                        ui::PAGE_TRANSITION_LINK);
  params.tabstrip_index = drop_info->index->value;
  if (drop_info->index->drop_before) {
    base::RecordAction(UserMetricsAction("Tab_DropURLBetweenTabs"));
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  } else {
    base::RecordAction(UserMetricsAction("Tab_DropURLOnTab"));
    params.disposition = WindowOpenDisposition::CURRENT_TAB;
    Browser* browser = browser_view_->browser();
    TabStripModel* model = browser->tab_strip_model();
    params.source_contents = model->GetWebContentsAt(drop_info->index->value);
  }

  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);

  return GetDropEffect(event, url);
}

const char* BrowserRootView::GetClassName() const {
  return kViewClassName;
}

bool BrowserRootView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (browser_defaults::kScrollEventChangesTab) {
    // Switch to the left/right tab if the wheel-scroll happens over the
    // tabstrip, or the empty space beside the tabstrip.
    views::View* hit_view = GetEventHandlerForPoint(event.location());
    int hittest =
        GetWidget()->non_client_view()->NonClientHitTest(event.location());
    if (tabstrip()->Contains(hit_view) ||
        hittest == HTCAPTION ||
        hittest == HTTOP) {
      scroll_remainder_x_ += event.x_offset();
      scroll_remainder_y_ += event.y_offset();

      // Number of integer scroll events that have passed in each direction.
      int whole_scroll_amount_x =
          std::lround(static_cast<double>(scroll_remainder_x_) /
                      ui::MouseWheelEvent::kWheelDelta);
      int whole_scroll_amount_y =
          std::lround(static_cast<double>(scroll_remainder_y_) /
                      ui::MouseWheelEvent::kWheelDelta);

      // Adjust the remainder such that any whole scrolls we have taken action
      // for don't count towards the scroll remainder.
      scroll_remainder_x_ -=
          whole_scroll_amount_x * ui::MouseWheelEvent::kWheelDelta;
      scroll_remainder_y_ -=
          whole_scroll_amount_y * ui::MouseWheelEvent::kWheelDelta;

      // Count a scroll in either axis - summing the axes works for this.
      int whole_scroll_offset = whole_scroll_amount_x + whole_scroll_amount_y;

      Browser* browser = browser_view_->browser();
      TabStripModel* model = browser->tab_strip_model();
      // Switch to the next tab only if not at the end of the tab-strip.
      if (whole_scroll_offset < 0 &&
          model->active_index() + 1 < model->count()) {
        chrome::SelectNextTab(browser);
        return true;
      }

      // Switch to the previous tab only if not at the beginning of the
      // tab-strip.
      if (whole_scroll_offset > 0 && model->active_index() > 0) {
        chrome::SelectPreviousTab(browser);
        return true;
      }
    }
  }
  return RootView::OnMouseWheel(event);
}

void BrowserRootView::OnMouseExited(const ui::MouseEvent& event) {
  // Reset the remainders so tab switches occur halfway through a smooth scroll.
  scroll_remainder_x_ = 0;
  scroll_remainder_y_ = 0;
  RootView::OnMouseExited(event);
}

void BrowserRootView::OnEventProcessingStarted(ui::Event* event) {
  if (event->IsGestureEvent()) {
    ui::GestureEvent* gesture_event = event->AsGestureEvent();
    if (gesture_event->type() == ui::ET_GESTURE_TAP &&
        gesture_event->location().y() <= 0 &&
        gesture_event->location().x() <= browser_view_->GetBounds().width()) {
      TouchUMA::RecordGestureAction(TouchUMA::kGestureRootViewTopTap);
    }
  }

  RootView::OnEventProcessingStarted(event);
}

BrowserRootView::DropTarget* BrowserRootView::GetDropTarget(
    const ui::DropTargetEvent& event) {
  // See if we should drop links onto tabstrip first.
  if (tabstrip()->visible()) {
    // Allow the drop as long as the mouse is over tabstrip or vertically
    // before it.
    gfx::Point tabstrip_loc_in_host;
    ConvertPointToTarget(tabstrip(), this, &tabstrip_loc_in_host);
    if (event.y() < tabstrip_loc_in_host.y() + tabstrip()->height())
      return tabstrip();
  }

  // See if we can drop links onto toolbar.
  gfx::Point loc_in_toolbar(event.location());
  ConvertPointToTarget(this, toolbar(), &loc_in_toolbar);
  return toolbar()->HitTestPoint(loc_in_toolbar) ? toolbar() : nullptr;
}

BrowserRootView::DropIndex BrowserRootView::GetDropIndexForEvent(
    const ui::DropTargetEvent& event,
    const ui::OSExchangeData& data,
    DropTarget* target) {
  gfx::Point loc_in_view(event.location());
  ConvertPointToTarget(this, target->GetViewForDrop(), &loc_in_view);
  ui::DropTargetEvent event_in_view(data, gfx::PointF(loc_in_view),
                                    gfx::PointF(loc_in_view),
                                    event.source_operations());
  return target->GetDropIndex(event_in_view);
}

void BrowserRootView::OnFileSupported(const GURL& url, bool supported) {
  if (drop_info_ && drop_info_->url == url)
    drop_info_->file_supported = supported;
}

bool BrowserRootView::GetPasteAndGoURL(const ui::OSExchangeData& data,
                                       GURL* url) {
  if (!data.HasString())
    return false;

  base::string16 text;
  if (!data.GetString(&text) || text.empty())
    return false;
  text = AutocompleteMatch::SanitizeString(text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(
      browser_view_->browser()->profile())->Classify(
          text, false, false, metrics::OmniboxEventProto::INVALID_SPEC, &match,
          nullptr);
  if (!match.destination_url.is_valid())
    return false;

  if (url)
    *url = match.destination_url;
  return true;
}
