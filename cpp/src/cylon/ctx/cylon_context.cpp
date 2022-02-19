/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <glog/logging.h>
#include <utility>
#include <vector>
#include <arrow/memory_pool.h>

#include "cylon/ctx/cylon_context.hpp"
#include "cylon/util/macros.hpp"

#include "cylon/net/mpi/mpi_communicator.hpp"
#ifdef BUILD_CYLON_UCX
#include "cylon/net/ucx/ucx_communicator.hpp"
#endif

namespace cylon {

std::shared_ptr<CylonContext> CylonContext::Init() {
  return std::make_shared<CylonContext>(false);
}
CylonContext::CylonContext(bool distributed) {
  this->is_distributed = distributed;
}

std::shared_ptr<CylonContext> CylonContext::InitDistributed(const std::shared_ptr<cylon::net::CommConfig> &config) {
  if (config->Type() == net::CommType::MPI) {
    auto ctx = std::make_shared<CylonContext>(true);
    ctx->communicator = std::make_shared<net::MPICommunicator>();
    ctx->communicator->Init(config);
    ctx->is_distributed = true;
    return ctx;
  }

#ifdef BUILD_CYLON_UCX
  else if (config->Type() == net::CommType::UCX) {
    auto ctx = std::make_shared<CylonContext>(true);
    ctx->communicator = std::make_shared<net::UCXCommunicator>();
    ctx->communicator->Init(config);
    ctx->is_distributed = true;
    return ctx;
  }
#endif
  else {
    throw "Unsupported communication type";
  }
  return nullptr;
}

Status CylonContext::InitDistributed(const std::shared_ptr<cylon::net::CommConfig> &config,
                                     std::shared_ptr<CylonContext> *ctx) {
  switch (config->Type()) {
    case net::LOCAL: return {Code::Invalid, "InitDistributed called on Local communication"};

    case net::MPI: {
      *ctx = std::make_shared<CylonContext>(true);
      (*ctx)->communicator = std::make_shared<net::MPICommunicator>();
      const auto &status = (*ctx)->communicator->Init(config);
      if (!status.is_ok()) {
        ctx->reset();
        return status;
      }
      return Status::OK();
    }

    case net::UCX: {
#ifdef BUILD_CYLON_UCX
      *ctx = std::make_shared<CylonContext>(true);
      (*ctx)->communicator = std::make_shared<net::UCXCommunicator>();
      const auto &status = (*ctx)->communicator->Init(config);
      if (!status.is_ok()) {
        ctx->reset();
        return status;
      }
      return Status::OK();
#else
      return {Code::NotImplemented, "UCX communication not implemented"};
#endif
    }

    case net::TCP:return {Code::NotImplemented, "TCP communication not implemented"};
  }
  return Status::OK();
}

const std::shared_ptr<net::Communicator> &CylonContext::GetCommunicator() const {
  if (!is_distributed) {
    LOG(FATAL) << "No communicator available for local mode!";
  }
  return this->communicator;
}

void CylonContext::setCommunicator(const std::shared_ptr<cylon::net::Communicator> &communicator1) {
  this->communicator = communicator1;
}

void CylonContext::setDistributed(bool distributed) {
  this->is_distributed = distributed;
}

void CylonContext::AddConfig(const std::string &key, const std::string &value) {
  this->config.insert(std::pair<std::string, std::string>(key, value));
}
std::string CylonContext::GetConfig(const std::string &key, const std::string &def) {
  auto find = this->config.find(key);
  if (find == this->config.end()) {
    return def;
  }
  return find->second;
}
int CylonContext::GetRank() const {
  if (this->is_distributed) {
    return this->communicator->GetRank();
  }
  return 0;
}
int CylonContext::GetWorldSize() const {
  if (this->is_distributed) {
    return this->communicator->GetWorldSize();
  }
  return 1;
}
void CylonContext::Finalize() {
  if (this->is_distributed) {
    this->communicator->Finalize();
  }
}
std::vector<int> CylonContext::GetNeighbours(bool include_self) {
  std::vector<int> neighbours{};
  neighbours.reserve(this->GetWorldSize());
  for (int i = 0; i < this->GetWorldSize(); i++) {
    if (i == this->GetRank() && !include_self) {
      continue;
    }
    neighbours.push_back(i);
  }
  return neighbours;
}

cylon::MemoryPool *CylonContext::GetMemoryPool() {
  return this->memory_pool;
}

void CylonContext::SetMemoryPool(cylon::MemoryPool *mem_pool) {
  this->memory_pool = mem_pool;
}
int32_t CylonContext::GetNextSequence() {
  return this->sequence_no++;
}

bool CylonContext::IsDistributed() const {
  return is_distributed;
}
cylon::net::CommType CylonContext::GetCommType() {
  return is_distributed ? this->communicator->GetCommType() : net::CommType::LOCAL;
}
}  // namespace cylon
