////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2019, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
////////////////////////////////////////////////////////////////////////////////

#define LBANN_L2_NORM2_LAYER_INSTANTIATE
#include "lbann/layers/loss/l2_norm2.hpp"

namespace lbann {

namespace {

template <El::Int block_size>
__global__ void fp_kernel(El::Int local_height,
                          El::Int local_width,
                          const DataType* __restrict__ input,
                          El::Int input_ldim,
                          DataType* __restrict__ contribution) {

  // Indices
  const El::Int tid = threadIdx.x;
  const El::Int gidx = threadIdx.x + blockIdx.x * blockDim.x;
  const El::Int bidy = blockIdx.y;
  const El::Int nthreadsx = blockDim.x * gridDim.x;

  // Compute local contribution for each matrix column
  for (El::Int col = bidy; col < local_width; col += gridDim.y) {

    // Compute contributions for each thread
    DataType private_contribution = 0;
    for (El::Int row = gidx; row < local_height; row += nthreadsx) {
      const auto& x = input[row + col * input_ldim];
      private_contribution += x * x;
    }

    // Shared memory reduction to get contribution for each block
    /// @todo unroll loops
    __shared__ DataType shared_contribution[block_size];
    shared_contribution[tid] = private_contribution;
    for (El::Int stride = block_size / 2; stride > 0; stride /= 2) {
      __syncthreads();
      if (tid < stride) {
        shared_contribution[tid] += shared_contribution[tid + stride];
      }
    }
    if (tid == 0) {
      cuda::atomic_add(&contribution[col], shared_contribution[0]);
    }

  }

}

void local_fp_gpu(const AbsMat& local_input, AbsMat& local_contribution) {
  El::Zero(local_contribution);
  if (!local_input.IsEmpty()) {
    const auto& local_height = local_input.Height();
    const auto& local_width = local_input.Width();
    const El::Int block_size = 256;
    dim3 block_dims, grid_dims;
    block_dims.x = block_size;
    grid_dims.x = (local_height + block_size - 1) / block_size;
    grid_dims.y = local_width;
    CHECK_CUDA(cudaSetDevice(El::GPUManager::Device()));
    fp_kernel<block_size>
      <<<grid_dims, block_dims, 0, El::GPUManager::Stream()>>>(
        local_height, local_width,
        local_input.LockedBuffer(), local_input.LDim(),
        local_contribution.Buffer());
  }
}

template <El::Int block_size>
__global__ void bp_kernel(El::Int local_height, El::Int local_width,
                          const DataType* __restrict__ input,
                          El::Int input_ldim,
                          const DataType* __restrict__ gradient_wrt_output,
                          DataType* __restrict__ gradient_wrt_input,
                          El::Int gradient_wrt_input_ldim) {
  const El::Int gidx = threadIdx.x + blockIdx.x * blockDim.x;
  const El::Int bidy = blockIdx.y;
  const El::Int nthreadsx = blockDim.x * gridDim.x;
  for (El::Int col = bidy; col < local_width; col += gridDim.y) {
    const auto& dy = gradient_wrt_output[col];
    for (El::Int row = gidx; row < local_height; row += nthreadsx) {
      const auto& x = input[row + col * input_ldim];
      auto& dx = gradient_wrt_input[row + col * gradient_wrt_input_ldim];
      dx = 2 * x * dy;
    }
  }
}

void local_bp_gpu(const AbsMat& local_input,
                  const AbsMat& local_gradient_wrt_output,
                  AbsMat& local_gradient_wrt_input) {
  if (!local_input.IsEmpty()) {
    const auto& local_height = local_input.Height();
    const auto& local_width = local_input.Width();
    const El::Int block_size = 256;
    dim3 block_dims, grid_dims;
    block_dims.x = block_size;
    grid_dims.x = (local_height + block_size - 1) / block_size;
    grid_dims.y = local_width;
    CHECK_CUDA(cudaSetDevice(El::GPUManager::Device()));
    bp_kernel<block_size>
      <<<grid_dims, block_dims, 0, El::GPUManager::Stream()>>>(
        local_height, local_width,
        local_input.LockedBuffer(), local_input.LDim(),
        local_gradient_wrt_output.LockedBuffer(),
        local_gradient_wrt_input.Buffer(),
        local_gradient_wrt_input.LDim());
  }
}

} // namespace

template <>
void l2_norm2_layer<data_layout::MODEL_PARALLEL, El::Device::GPU>
     ::local_fp_compute(const AbsMat& local_input,
                        AbsMat& local_contribution) {
  local_fp_gpu(local_input, local_contribution);
}
template <>
void l2_norm2_layer<data_layout::MODEL_PARALLEL, El::Device::GPU>
     ::local_bp_compute(const AbsMat& local_input,
                        const AbsMat& local_gradient_wrt_output,
                        AbsMat& local_gradient_wrt_input) {
  local_bp_gpu(local_input,
               local_gradient_wrt_output,
               local_gradient_wrt_input);
}
template <>
void l2_norm2_layer<data_layout::DATA_PARALLEL, El::Device::GPU>
     ::local_fp_compute(const AbsMat& local_input,
                        AbsMat& local_contribution) {
  local_fp_gpu(local_input, local_contribution);
}
template <>
void l2_norm2_layer<data_layout::DATA_PARALLEL, El::Device::GPU>
     ::local_bp_compute(const AbsMat& local_input,
                        const AbsMat& local_gradient_wrt_output,
                        AbsMat& local_gradient_wrt_input) {
  local_bp_gpu(local_input,
               local_gradient_wrt_output,
               local_gradient_wrt_input);
}

template class l2_norm2_layer<
  data_layout::DATA_PARALLEL, El::Device::GPU>;
template class l2_norm2_layer<
  data_layout::MODEL_PARALLEL, El::Device::GPU>;

} // namespace lbann
