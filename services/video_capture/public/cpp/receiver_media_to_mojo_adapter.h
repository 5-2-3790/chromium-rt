// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MEDIA_TO_MOJO_ADAPTER_H_

#include "media/capture/video/video_frame_receiver.h"
#include "services/video_capture/public/mojom/receiver.mojom.h"

namespace video_capture {

// Adapter that allows a media::VideoFrameReceiver to be used in place of
// a mojom::VideoFrameReceiver.
class ReceiverMediaToMojoAdapter : public mojom::Receiver {
 public:
  ReceiverMediaToMojoAdapter(
      std::unique_ptr<media::VideoFrameReceiver> receiver);
  ~ReceiverMediaToMojoAdapter() override;

  // video_capture::mojom::Receiver:
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      int32_t frame_feedback_id,
      mojom::ScopedAccessPermissionPtr access_permission,
      media::mojom::VideoFrameInfoPtr frame_info) override;
  void OnBufferRetired(int32_t buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;

 private:
  std::unique_ptr<media::VideoFrameReceiver> receiver_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MEDIA_TO_MOJO_ADAPTER_H_
