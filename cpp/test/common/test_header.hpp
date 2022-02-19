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

#ifndef __CYLON_TEST_HEADER_
#define __CYLON_TEST_HEADER_

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <mpi.h>
#include <iostream>
#include <glog/logging.h>
#include <cylon/ctx/cylon_context.hpp>
#include <cylon/table.hpp>
#include <chrono>
#include <cylon/net/mpi/mpi_communicator.hpp>

#include "test_utils.hpp"
#include "test_macros.hpp"
#include "test_arrow_utils.hpp"

std::shared_ptr<cylon::CylonContext> ctx = nullptr;
int RANK = 0;
int WORLD_SZ = 0;

int main(int argc, char *argv[]) {
  // global setup...
  auto mpi_config = cylon::net::MPIConfig::Make();
  auto st = cylon::CylonContext::InitDistributed(mpi_config, &ctx);
  if (!st.is_ok()) {
    LOG(ERROR) << "ctx init failed: " << st.get_msg();
    return st.get_code();
  }

  RANK = ctx->GetRank();
  WORLD_SZ = ctx->GetWorldSize();

  LOG(INFO) << "wz: " << WORLD_SZ << " rank: " << RANK << std::endl;
  int result = Catch::Session().run(argc, argv);

  // global clean-up...
  ctx->Finalize();
  return result;
}

// Other common stuff goes here ...

#endif
