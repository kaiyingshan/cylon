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

#include <cylon/net/mpi/mpi_communicator.hpp>
#include <cylon/ctx/cylon_context.hpp>
#include <cylon/table.hpp>
#include <cylon/ops.hpp>

#include "example_utils.hpp"

/**
 * This example reads two csv files and does a union on them.
 */
int main(int argc, char *argv[]) {
  if (argc < 5) {
    LOG(ERROR) << "./union_example m [n | o] num_tuples_per_worker 0.0-1.0 null_probability"
               << std::endl
               << "./union_example f [n | o] csv_file1 csv_file2" << std::endl;
    return 1;
  }

  auto start_time = std::chrono::steady_clock::now();
  auto mpi_config = std::make_shared<cylon::net::MPIConfig>();
  auto ctx = cylon::CylonContext::InitDistributed(mpi_config);

  std::shared_ptr<cylon::Table> first_table, second_table, unioned_table;
  auto read_options = cylon::io::config::CSVReadOptions().UseThreads(false).BlockSize(1 << 30);

  std::string mem = std::string(argv[1]);
  std::string ops_param = std::string(argv[2]);
  if (mem == "m") {
    int64_t count = std::stoll(argv[3]);
    double dup = std::stod(argv[4]);
    double null_prob = std::stod(argv[5]);
    cylon::examples::create_two_in_memory_tables(count, dup, ctx, first_table, second_table,
                                                 null_prob);
  } else if (mem == "f") {
    cylon::FromCSV(ctx, std::string(argv[3]) + std::to_string(ctx->GetRank()) + ".csv", first_table);
    cylon::FromCSV(ctx, std::string(argv[4]) + std::to_string(ctx->GetRank()) + ".csv", second_table);
  }
  ctx->Barrier();
  auto read_end_time = std::chrono::steady_clock::now();
  LOG(INFO) << "Read all in " << std::chrono::duration_cast<std::chrono::milliseconds>(
      read_end_time - start_time).count() << "[ms]";

  auto union_start_time = std::chrono::steady_clock::now();
  cylon::Status status;
  // apply union operation
  if (ops_param == "o") {
    status = cylon::UnionOperation(ctx, first_table, second_table, unioned_table);
  } else if (ops_param == "n") {
    status = cylon::DistributedUnion(first_table, second_table, unioned_table);
  } else {
    LOG(INFO) << "Incorrect arguments";
    return 1;
  }
  if (!status.is_ok()) {
    LOG(INFO) << "Union failed " << status.get_msg();
    ctx->Finalize();
    return 1;
  }
  read_end_time = std::chrono::steady_clock::now();

  LOG(INFO) << "First table had : " << first_table->Rows() << " and Second table had : "
            << second_table->Rows() << ", Union has : "
            << unioned_table->Rows() << " rows";
  LOG(INFO) << "Union done in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                read_end_time - union_start_time).count()
            << "[ms]";

  ctx->Finalize();
  auto end_time = std::chrono::steady_clock::now();
  LOG(INFO) << "Operation took : "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count() << "[ms]";
  return 0;
}
