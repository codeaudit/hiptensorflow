/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// The CUDA implementation of the StreamExecutorInterface functionality.
// CUDA inclusions are ideally confined to this implementation file.
//
// The notions from the StreamExecutor basically correspond to the CUDA streams
// programming model provided by the libcuda.so driver APIs, so we don't have
// to do much more than wrap the calls to the libraries appropriately.
#ifndef TENSORFLOW_STREAM_EXECUTOR_CUDA_CUDA_KERNEL_H_
#define TENSORFLOW_STREAM_EXECUTOR_CUDA_CUDA_KERNEL_H_

#include "tensorflow/stream_executor/kernel_cache_config.h"
#include "tensorflow/stream_executor/stream_executor_internal.h"
#include "tensorflow/stream_executor/cuda/cuda_driver.h"
#include "tensorflow/stream_executor/lib/casts.h"
#include "tensorflow/stream_executor/platform/port.h"
#include "tensorflow/stream_executor/platform/logging.h"
#include "cuda/include/hip/hip_runtime.h"

//TODO: enable this once all driver APIs got supported 
//#ifdef PLATFORMS_GPUS_CUDA_DYNAMIC_LIBCUDA_DYNAMIC_LIBCUDA_H_
//#error \
//    "No driver calls in this file, wrap driver functionality in cuda_driver.cc."
//#endif

//#ifdef __CUDA_RUNTIME_H__
//#error \
    "CUDA runtime being included into CUDA GPU executor; should be driver only."
//#endif

namespace perftools {
namespace gputools {
namespace cuda {

// Wraps a hipFunction_t to implement the platform-independent KernelInterface.
class CUDAKernel : public internal::KernelInterface {
 public:
  CUDAKernel() : cuda_function_(nullptr), arity_(0),
                 preferred_cache_config_(KernelCacheConfig::kNoPreference) {}

  // Note that the function is unloaded when the module is unloaded, and the
  // module that the function is contained in is owned by the CUDAExecutor.
  ~CUDAKernel() override {}

  // As arity cannot be reflected upon using the CUDA API, the arity is
  // explicitly set during the CUDAExecutor::GetKernel initialization process.
  void set_arity(unsigned arity) { arity_ = arity; }
  unsigned Arity() const override { return arity_; }

  // Returns the hipFunction_t value for passing to the CUDA API.
  hipFunction_t AsCUDAFunctionValue() const {
    DCHECK(cuda_function_ != nullptr);
    return const_cast<hipFunction_t>(cuda_function_);
  }

  // Returns the slot that the hipFunction_t is stored within for this object,
  // for the CUDA API which wants to load into a hipFunction_t*.
  hipFunction_t *cuda_function_ptr() { return &cuda_function_; }

  // CUDA supports setting the preferred cache configuration of a hipFunction_t
  // (more-or-less equivalent to a CUDAKernel). We support this via the below
  // functions; users can set a preference, and that is applied when the kernel
  // is [lazy-]loaded (in CUDAExecutor::Launch). The alternative would be to
  // load the kernel & set the preference when the user calls the setter below;
  // either approach is valid.
  // Sets the current kernel cache configuration preference.
  void SetPreferredCacheConfig(KernelCacheConfig config) override {
    preferred_cache_config_ = config;
  }

  // Returns the current kernel cache configuration preference.
  KernelCacheConfig GetPreferredCacheConfig() const override {
    return preferred_cache_config_;
  }

  // Returns the current kernel cache configuration preference as a
  // CUfunc_cache.
  hipFuncCache_t GetCUDACacheConfig() const {
    switch (preferred_cache_config_) {
      case KernelCacheConfig::kNoPreference:
#ifdef __HIP_PLATFORM_NVCC__
        return cudaFuncCachePreferNone;
#elif defined(__HIP_PLATFORM_HCC__)
        return hipFuncCachePreferNone;
#endif
      case KernelCacheConfig::kPreferShared:
#ifdef __HIP_PLATFORM_NVCC__
        return cudaFuncCachePreferShared;
#elif defined(__HIP_PLATFORM_HCC__)
        return hipFuncCachePreferShared;
#endif
      case KernelCacheConfig::kPreferL1:
#ifdef __HIP_PLATFORM_NVCC__
        return cudaFuncCachePreferL1;
#elif defined(__HIP_PLATFORM_HCC__)
        return hipFuncCachePreferL1;
#endif
      case KernelCacheConfig::kPreferEqual:
#ifdef __HIP_PLATFORM_NVCC__
        return cudaFuncCachePreferEqual;
#elif defined(__HIP_PLATFORM_HCC__)
        return hipFuncCachePreferEqual;
#endif
      default:
        LOG(FATAL) << "Unknown KernelCacheConfig"
                   << static_cast<int32>(preferred_cache_config_);
    }
  }

 private:
  hipFunction_t cuda_function_;  // Wrapped CUDA kernel handle.
  unsigned arity_;            // Number of formal parameters the kernel takes.

  // Preferred (but not required) cache configuration for this kernel.
  KernelCacheConfig preferred_cache_config_;
};

// Given a platform-independent kernel datatype, returns the (const) internal
// CUDA platform implementation pointer.
inline const CUDAKernel *AsCUDAKernel(const KernelBase *kernel) {
  return static_cast<const CUDAKernel *>(kernel->implementation());
}

// Given a platform-independent kernel datatype, returns the (non-const)
// internal CUDA platform implementation pointer.
inline CUDAKernel *AsCUDAKernel(KernelBase *kernel) {
  return static_cast<CUDAKernel *>(kernel->implementation());
}

}  // namespace cuda
}  // namespace gputools
}  // namespace perftools

#endif  // TENSORFLOW_STREAM_EXECUTOR_CUDA_CUDA_KERNEL_H_
