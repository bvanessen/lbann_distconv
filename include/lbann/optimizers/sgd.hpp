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

#ifndef LBANN_OPTIMIZERS_SGD_HPP_INCLUDED
#define LBANN_OPTIMIZERS_SGD_HPP_INCLUDED

#include "lbann/optimizers/optimizer.hpp"

namespace lbann {

/** @brief Stochastic gradient descent optimizer.
 *  @details Supports momentum and Nesterov acceleration.
 *  @todo Dedicated optimizers for momentum or Nesterov SGD.
 */
class sgd : public optimizer {

public:

  /** @name Life cycle functions */
  ///@{

  sgd(DataType learning_rate,
      DataType momentum = 0,
      bool nesterov = false);
  sgd(const sgd& other);
  sgd& operator=(const sgd& other);
  ~sgd() override = default;
  sgd* copy() const override { return new sgd(*this); }

  ///@}

  /** @name Descriptions */
  ///@{

  /** Human-readable type name. */
  std::string get_type() const override { return "SGD"; }
  /** Human-readable description. */
  description get_description() const override;

  ///@}

  /** @name Access functions */
  ///@{

  /** @brief Decay rate for gradient accumulation.
   *  @details A momentum of zero corresponds to vanilla SGD.
   */
  DataType get_momentum() const noexcept { return m_momentum; }
  /** @brief Decay rate for gradient accumulation.
   *  @details A momentum of zero corresponds to vanilla SGD.
   */
  void set_momentum(DataType momentum) { m_momentum = momentum; }

  /** Whether Nesterov acceleration is applied. */
  bool using_nesterov() const noexcept { return m_nesterov; }
  /** Whether Nesterov acceleration is applied. */
  void set_nesterov(bool nesterov) { m_nesterov = nesterov; }

  /** Accumulated gradients for momentum optimizer. */
  const AbsDistMat& get_velocity() const;
  /** Accumulated gradients for momentum optimizer. */
  AbsDistMat& get_velocity();

  ///@}

  /** @name Setup */
  ///@{

  void setup(weights* w = nullptr) override;

  ///@}

protected:

  /** Computation for an optimization step. */
  void step_compute(AbsDistMat& values, const AbsDistMat& gradient) override;

private:

  /** @brief Decay rate for gradient accumulation.
   *  @details A momentum of zero corresponds to vanilla SGD.
   */
  DataType m_momentum;
  /** Whether Nesterov acceleration is used. */
  bool m_nesterov;
  /** @brief Accumulated gradients.
   *  @details Not used for vanilla SGD.
   */
  std::unique_ptr<AbsDistMat> m_velocity;

  /** CPU implementation of momentum or Nesterov step. */
  void momentum_step_cpu(AbsDistMat& values, const AbsDistMat& gradient);
#ifdef LBANN_HAS_CUDA
  /** GPU implementation of momentum or Nesterov step. */
  void momentum_step_gpu(AbsDistMat& values, const AbsDistMat& gradient);
#endif // LBANN_HAS_CUDA

  /** @name Checkpointing */
  ///@{

  struct packing_header {
    DataType momentum;
  };

  bool pack_scalars(persist& p) {
    p.write_datatype(persist_type::train, "momentum", m_momentum);
    return true;
  }

  bool unpack_scalars(persist& p, struct packing_header *header){
    p.read_datatype(persist_type::train, "momentum",  &m_momentum);

    if(header != nullptr){
      header->momentum = m_momentum;
    }

  return true;
  }

  void unpack_header(struct packing_header& header){
    m_momentum = header.momentum;
  }

  bool save_to_checkpoint_shared(persist& p, std::string m_name) override;
  bool load_from_checkpoint_shared(persist& p, std::string m_name) override;
  bool save_to_checkpoint_distributed(persist& p, std::string m_name) override;
  bool load_from_checkpoint_distributed(persist& p, std::string m_name) override;

  ///@}

};

std::unique_ptr<optimizer>
build_sgd_optimizer_from_pbuf(
  google::protobuf::Message const&);

} // namespace lbann

#endif // LBANN_OPTIMIZERS_SGD_HPP_INCLUDED
