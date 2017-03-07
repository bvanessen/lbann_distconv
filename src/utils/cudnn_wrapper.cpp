////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC. 
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
//
// cudnn_wrapper .hpp .cpp - cuDNN support - wrapper classes, utility functions
////////////////////////////////////////////////////////////////////////////////

#include "lbann/utils/cudnn_wrapper.hpp"
#include "lbann/utils/lbann_exception.hpp"

#include <iostream>

#include "El.hpp"

#ifdef __LIB_CUDNN

using namespace cudnn;
using namespace lbann;

#define _ALLOC_DEVICE_MEM_ONCE_

cudnn_manager::cudnn_manager(lbann::lbann_comm* _comm, int max_num_gpus)
  : comm(_comm)
{

  // Initialize GPU memory pool
  m_gpu_memory = new cub::CachingDeviceAllocator(8u, 3u);

  // Determine number of available GPUs
  checkCUDA(cudaGetDeviceCount(&m_num_total_gpus));
  if(max_num_gpus >= 0 && max_num_gpus < m_num_total_gpus) {
    m_num_total_gpus = max_num_gpus;
  }
  if(m_num_total_gpus < 1) {
    throw lbann::lbann_exception("cudnn_wrapper: no GPUs allocated or found for cuDNN");
  }

  // Determine number of MPI ranks on current compute node
  const int rank_in_node = comm->get_rank_in_node();
  const int procs_per_node = comm->get_procs_per_node();
  
  // Case where compute node has more GPUs than MPI ranks
  if(m_num_total_gpus >= procs_per_node) {
    int gpu = rank_in_node;
    while(gpu < m_num_total_gpus) {
      checkCUDA(cudaSetDevice(gpu));
      m_gpus.push_back(gpu);
      m_streams.push_back(NULL);
      m_handles.push_back(NULL);
      cudaStream_t& stream = m_streams.back();
      cudnnHandle_t& handle = m_handles.back();
      checkCUDA(cudaStreamCreate(&stream));
      checkCUDNN(cudnnCreate(&handle));
      checkCUDNN(cudnnSetStream(handle, stream));
      gpu += procs_per_node;
    }
  }

  // Case where compute node has fewers GPUs than MPI ranks
  else {
    const int gpu = rank_in_node % m_num_total_gpus;
    checkCUDA(cudaSetDevice(gpu));
    m_gpus.push_back(gpu);
    m_streams.push_back(NULL);
    m_handles.push_back(NULL);
    cudaStream_t& stream = m_streams.back();
    cudnnHandle_t& handle = m_handles.back();
    checkCUDA(cudaStreamCreate(&stream));
    checkCUDNN(cudnnCreate(&handle));
    checkCUDNN(cudnnSetStream(handle, stream));
  }

  // Get number of GPUs for current MPI rank
  m_num_gpus = m_gpus.size();

}

cudnn_manager::~cudnn_manager()
{
  // Destroy GPU memory pool
  if(m_gpu_memory) {
    delete m_gpu_memory;
  }

  // Destroy cuDNN handles
  for(int i=0; i<m_gpus.size(); ++i) {
    checkCUDA(cudaSetDevice(m_gpus[i]));
    if(m_streams[i])
      checkCUDA(cudaStreamDestroy(m_streams[i]));
    if(m_handles[i])
      checkCUDNN(cudnnDestroy(m_handles[i]));
  }
  unpin_ptrs();
}

void cudnn_manager::cudnn_manager::copy_to_gpus(std::vector<DataType*>& gpu_data,
                                                const Mat& cpu_data,
                                                Int width_per_gpu) {

  // Get matrix properties
  const Int height = cpu_data.Height();
  const Int width = cpu_data.Width();
  const Int cpu_ldim = cpu_data.LDim();

  // Perform memory transfer on each GPU
#pragma omp parallel for
  for(Int i=0; i<m_num_gpus; ++i) {
    checkCUDA(cudaSetDevice(m_gpus[i]));

    // Find number of columns to transfer to current GPU
    const Int first_pos = Min(i * width_per_gpu, width);
    const Int last_pos = Min((i+1) * width_per_gpu, width);
    const Int current_width = last_pos - first_pos;

    // Transfer data to current GPU
    if(current_width > 0) {
      if(cpu_ldim > height) {
        checkCUDA(cudaMemcpy2DAsync(gpu_data[i],
                                    height*sizeof(DataType),
                                    cpu_data.LockedBuffer(0,first_pos),
                                    cpu_ldim*sizeof(DataType),
                                    height*sizeof(DataType),
                                    current_width,
                                    cudaMemcpyHostToDevice,
                                    m_streams[i]));
      }
      else {
        checkCUDA(cudaMemcpyAsync(gpu_data[i],
                                  cpu_data.LockedBuffer(0,first_pos),
                                  height*current_width*sizeof(DataType),
                                  cudaMemcpyHostToDevice,
                                  m_streams[i]));
      }
    }

    // Set unused GPU memory to zero
    if(current_width < width_per_gpu) {
      checkCUDA(cudaMemsetAsync(gpu_data[i] + height*current_width,
                                0,
                                height*(width_per_gpu-current_width),
                                m_streams[i]));
    }
    
  }

}

void cudnn_manager::cudnn_manager::copy_from_gpus(Mat& cpu_data,
                                                  const std::vector<DataType*>& gpu_data,
                                                  Int width_per_gpu) {

  // Get matrix properties
  const Int height = cpu_data.Height();
  const Int width = cpu_data.Width();
  const Int cpu_ldim = cpu_data.LDim();

  // Perform memory transfer on each GPU
#pragma omp parallel for
  for(Int i=0; i<m_num_gpus; ++i) {
    checkCUDA(cudaSetDevice(m_gpus[i]));

    // Find number of columns to transfer to current GPU
    const Int first_pos = Min(i * width_per_gpu, width);
    const Int last_pos = Min((i+1) * width_per_gpu, width);
    const Int current_width = last_pos - first_pos;

    // Transfer data from current GPU
    if(current_width > 0) {
      if(cpu_ldim > height) {
        checkCUDA(cudaMemcpy2DAsync(cpu_data.Buffer(0,first_pos),
                                    cpu_ldim*sizeof(DataType),
                                    gpu_data[i],
                                    height*sizeof(DataType),
                                    height*sizeof(DataType),
                                    current_width,
                                    cudaMemcpyDeviceToHost,
                                    m_streams[i]));
      }
      else {
        checkCUDA(cudaMemcpyAsync(cpu_data.Buffer(0,first_pos),
                                  gpu_data[i],
                                  height*current_width*sizeof(DataType),
                                  cudaMemcpyDeviceToHost,
                                  m_streams[i]));
      }
    }
    
  }

}

void cudnn_manager::cudnn_manager::broadcast_to_gpus(std::vector<DataType*>& gpu_data,
                                                     const Mat& cpu_data) {

  // Get matrix properties
  const Int height = cpu_data.Height();
  const Int width = cpu_data.Width();
  const Int cpu_ldim = cpu_data.LDim();

  // Perform memory transfer on each GPU
#pragma omp parallel for
  for(Int i=0; i<m_num_gpus; ++i) {
    checkCUDA(cudaSetDevice(m_gpus[i]));

    // Transfer data to current GPU
    if(cpu_ldim > height) {
      checkCUDA(cudaMemcpy2DAsync(gpu_data[i],
                                  height*sizeof(DataType),
                                  cpu_data.LockedBuffer(),
                                  cpu_ldim*sizeof(DataType),
                                  height*sizeof(DataType),
                                  width,
                                  cudaMemcpyHostToDevice,
                                  m_streams[i]));
    }
    else {
      checkCUDA(cudaMemcpyAsync(gpu_data[i],
                                cpu_data.LockedBuffer(),
                                height*width*sizeof(DataType),
                                cudaMemcpyHostToDevice,
                                m_streams[i]));
    }
    
  }

}

void cudnn_manager::cudnn_manager::reduce_from_gpus(Mat& cpu_data,
                                                    const std::vector<DataType*>& gpu_data) {

  // Get matrix properties
  const Int height = cpu_data.Height();
  const Int width = cpu_data.Width();
  const Int cpu_ldim = cpu_data.LDim();

  // Initialize temporary matrix
  Mat temp;
  if(m_num_gpus > 1) {
    Zeros(temp, height, (m_num_gpus-1)*width);
  }

  // Perform memory transfer on each GPU
#pragma omp parallel for
  for(Int i=0; i<m_num_gpus; ++i) {
    checkCUDA(cudaSetDevice(m_gpus[i]));

    // Transfer data from current GPU
    if(i == 0) {
      if(cpu_ldim > height) {
        checkCUDA(cudaMemcpy2DAsync(cpu_data.Buffer(),
                                    cpu_ldim*sizeof(DataType),
                                    gpu_data[i],
                                    height*sizeof(DataType),
                                    height*sizeof(DataType),
                                    width,
                                    cudaMemcpyDeviceToHost,
                                    m_streams[i]));
      }
      else {
        checkCUDA(cudaMemcpyAsync(cpu_data.Buffer(),
                                  gpu_data[i],
                                  height*width*sizeof(DataType),
                                  cudaMemcpyDeviceToHost,
                                  m_streams[i]));
      }
    }
    else {
      checkCUDA(cudaMemcpyAsync(temp.Buffer(0,(i+1)*width),
                                gpu_data[i],
                                height*width*sizeof(DataType),
                                cudaMemcpyDeviceToHost,
                                m_streams[i]));
    }
  }
 
  // Reduce data from different GPUs
  synchronize();
  for(Int i=0; i<m_num_gpus-1; ++i) {
    cpu_data += temp(ALL, IR(i*width, (i+1)*width));
  }

}

void cudnn_manager::cudnn_manager::synchronize() {
#pragma omp parallel for
  for(Int i=0; i<m_num_gpus; ++i) {
    checkCUDA(cudaSetDevice(m_gpus[i]));
    checkCUDA(cudaStreamSynchronize(m_streams[i]));
  }
}

void cudnn_manager::cudnn_manager::pin_ptr(void* ptr, size_t sz)
{
  if (!ptr) return;
  std::map<void*, size_t>::iterator it = pinned_ptr.find(ptr);
  if (it == pinned_ptr.end()) {
    //std::cout << "adding a new ptr " << reinterpret_cast<unsigned long long>(ptr) << std::endl;
    pinned_ptr[ptr] = sz;
    checkCUDA(cudaHostRegister(ptr, sz, cudaHostRegisterPortable));
  } else {
    // TODO: We can check here if the block defined by (ptr,sz) overlaps with an existing one.
  }
}

void cudnn_manager::pin_memory_block(ElMat *mat)
{
    if (!mat) return;
    const int w = (mat->Matrix()).Width();
    const int h = (mat->Matrix()).Height();
    const int sz = w*h*sizeof(DataType);
    void* ptr = (void*) (mat->Matrix()).Buffer();
    pin_ptr(ptr, w*h*sizeof(DataType));
}

void cudnn_manager::cudnn_manager::unpin_ptr(void* const ptr)
{
  std::map<void*, size_t>::iterator it = pinned_ptr.find(ptr);
  if (it != pinned_ptr.end()) {
    checkCUDA(cudaHostUnregister(it->first));
    pinned_ptr.erase(it);
  }
}

void cudnn_manager::cudnn_manager::unpin_ptrs(void)
{
  std::map<void*, size_t>::iterator it = pinned_ptr.begin();
  std::map<void*, size_t>::iterator itend = pinned_ptr.end();

  for(; it != itend; ++it) {
    checkCUDA(cudaHostUnregister(it->first));
  }
  pinned_ptr.clear();
}

void cudnn_manager::print_version() const {
  std::cout << "cudnnGetVersion() : " << (int)cudnnGetVersion() << " , "
            << "CUDNN_VERSION from cudnn.h : " << CUDNN_VERSION
            << std::endl;
}

cudnnDataType_t cudnn_manager::get_cudnn_data_type() const {
  switch(sizeof(DataType)) {
  case 2: return CUDNN_DATA_HALF;
  case 4: return CUDNN_DATA_FLOAT;
  case 8: return CUDNN_DATA_DOUBLE;
  default: throw lbann::lbann_exception("cudnn_wrapper: invalid data type for cuDNN");
  }
}

int cudnn_manager::get_num_gpus() const {
  return m_num_gpus;
}

int cudnn_manager::get_num_total_gpus() const {
  return m_num_total_gpus;
}

cub::CachingDeviceAllocator* cudnn_manager::get_gpu_memory() {
  return m_gpu_memory;
}

std::vector<cudaStream_t>* cudnn_manager::get_streams() {
  return &m_streams;
}

#endif // #ifdef __LIB_CUDNN
