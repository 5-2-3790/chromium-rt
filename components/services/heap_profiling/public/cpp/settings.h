// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SETTINGS_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SETTINGS_H_

#include "base/feature_list.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"

// These helper functions parse the command line and FeatureList settings to
// return coherent settings to use for the heap profiler at startup.
namespace heap_profiling {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Mode {
  // No profiling enabled.
  kNone = 0,

  // Only profile the browser and GPU processes.
  kMinimal = 1,

  // Profile all processes.
  kAll = 2,

  // Profile only the browser process.
  kBrowser = 3,

  // Profile only the gpu process.
  kGpu = 4,

  // Profile a sampled number of renderer processes.
  kRendererSampling = 5,

  // Profile all renderer processes.
  kAllRenderers = 6,

  // By default, profile no processes. User may choose to start profiling for
  // processes via chrome://memory-internals.
  kManual = 7,

  kCount
};

Mode GetModeForStartup();
Mode ConvertStringToMode(const std::string& input);
mojom::StackMode GetStackModeForStartup();
mojom::StackMode ConvertStringToStackMode(const std::string& input);

// A |sampling_rate| of 1 indicates that all allocations should be recorded.
// A |sampling_rate| greater than 1 describes the Poisson Process sampling
// interval. If |sampling_rate| is N, then on average, an allocation will be
// recorded every N bytes of allocated objects.
uint32_t GetSamplingRateForStartup();

bool IsBackgroundHeapProfilingEnabled();
bool ShouldKeepSmallAllocations();

extern const char kOOPHeapProfilingFeatureSamplingV2[];

// Exposed for testing.
extern const base::Feature kOOPHeapProfilingFeature;
extern const char kOOPHeapProfilingFeatureMode[];

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SETTINGS_H_
