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
#include <chrono>

#include <cylon/table.hpp>
#include "example_utils.hpp"

#define CYLON_LOG_HELP() \
  do{                    \
    LOG(ERROR) << "input arg error " << std::endl \
               << "./union_perf m num_tuples_per_worker 0.0-1.0 null_probability" << std::endl \
               << "./union_perf f csv_file1" << std::endl; \
    return 1;                                                  \
  } while(0)

int main(int argc, char *argv[]) {
  if (argc != 3 && argc != 5) {
    CYLON_LOG_HELP();
  }

  auto start_start = std::chrono::steady_clock::now();
  auto mpi_config = std::make_shared<cylon::net::MPIConfig>();
  auto ctx = cylon::CylonContext::InitDistributed(mpi_config);

  std::shared_ptr<cylon::Table> table, output;
  auto read_options = cylon::io::config::CSVReadOptions().UseThreads(false).BlockSize(1 << 30);

  std::string mem = std::string(argv[1]);
  if (mem == "m" && argc == 5) {
    LOG(INFO) << "using in-mem tables";
    int64_t count = std::stoll(argv[2]);
    double dup = std::stod(argv[3]);
    double null_prob = std::stod(argv[4]);
    if (cylon::examples::create_in_memory_tables(count, dup, ctx, table, null_prob)) {
      LOG(ERROR) << "table creation failed!";
      return 1;
    }
  } else if (mem == "f" && argc == 3) {
    LOG(INFO) << "using files";
    if (!cylon::FromCSV(ctx, std::string(argv[2]) + std::to_string(ctx->GetRank()) + ".csv", table)
        .is_ok()) {
      LOG(ERROR) << "file reading failed!";
      return 1;
    }
  } else {
    CYLON_LOG_HELP();
  }

  ctx->Barrier();
  auto read_end_time = std::chrono::steady_clock::now();
  LOG(INFO) << "Input tables created in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                read_end_time - start_start).count() << "[ms]";

  std::vector<int> cols(table->Columns());
  std::iota(cols.begin(), cols.end(), 0);
  auto status = cylon::DistributedUnique(table, cols, output);

  if (!status.is_ok()) {
    LOG(INFO) << "Unique failed " << status.get_msg();
    ctx->Finalize();
    return 1;
  }

  auto end_time = std::chrono::steady_clock::now();
  LOG(INFO) << "Table had : " << table->Rows() << ", output has : " << output->Rows();
  LOG(INFO) << "Completed in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - read_end_time)
                .count() << "[ms]";

  ctx->Finalize();
  return 0;
}
