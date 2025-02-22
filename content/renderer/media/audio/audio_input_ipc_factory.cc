// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/audio_input_ipc_factory.h"

#include <utility>

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "content/renderer/media/audio/mojo_audio_input_ipc.h"
#include "content/renderer/render_frame_impl.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

namespace {

void CreateMojoAudioInputStreamOnMainThread(
    int frame_id,
    const media::AudioSourceParameters& source_params,
    mojom::RendererAudioInputStreamFactoryClientPtr client,
    const media::AudioParameters& params,
    bool automatic_gain_control,
    uint32_t total_segments) {
  // TODO(ossu): Make an AudioProcessingConfig from params and send that along.
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(frame_id);
  if (frame) {
    frame->GetAudioInputStreamFactory()->CreateStream(
        std::move(client), source_params.session_id, params,
        automatic_gain_control, total_segments);
  }
}

void CreateMojoAudioInputStream(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    int frame_id,
    const media::AudioSourceParameters& source_params,
    mojom::RendererAudioInputStreamFactoryClientPtr client,
    const media::AudioParameters& params,
    bool automatic_gain_control,
    uint32_t total_segments) {
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateMojoAudioInputStreamOnMainThread, frame_id,
                     source_params, std::move(client), params,
                     automatic_gain_control, total_segments));
}

void AssociateInputAndOutputForAec(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    int frame_id,
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id) {
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](int frame_id, const base::UnguessableToken& input_stream_id,
             const std::string& output_device_id) {
            RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(frame_id);
            if (frame) {
              frame->GetAudioInputStreamFactory()
                  ->AssociateInputAndOutputForAec(input_stream_id,
                                                  output_device_id);
            }
          },
          frame_id, input_stream_id, output_device_id));
}
}  // namespace

AudioInputIPCFactory* AudioInputIPCFactory::instance_ = nullptr;

AudioInputIPCFactory::AudioInputIPCFactory(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : main_task_runner_(std::move(main_task_runner)),
      io_task_runner_(std::move(io_task_runner)) {
  DCHECK(!instance_);
  instance_ = this;
}

AudioInputIPCFactory::~AudioInputIPCFactory() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

std::unique_ptr<media::AudioInputIPC> AudioInputIPCFactory::CreateAudioInputIPC(
    int frame_id,
    const media::AudioSourceParameters& source_params) const {
  DCHECK_NE(0, source_params.session_id);
  return std::make_unique<MojoAudioInputIPC>(
      base::BindRepeating(&CreateMojoAudioInputStream, main_task_runner_,
                          frame_id, source_params),
      base::BindRepeating(&AssociateInputAndOutputForAec, main_task_runner_,
                          frame_id));
}

}  // namespace content
