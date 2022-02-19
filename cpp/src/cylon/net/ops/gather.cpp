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

#include <mpi.h>
#include <numeric>
#include <arrow/result.h>

#include "cylon/util/macros.hpp"
#include "cylon/net/mpi/mpi_communicator.hpp"
#include "cylon/net/mpi/mpi_operations.hpp"

std::vector<int32_t> totalBufferSizes(const std::vector<int32_t> &all_buffer_sizes,
                                      int num_buffers,
                                      int world_size) {
  std::vector<int32_t> total_buffer_sizes(num_buffers, 0);
  for (int w = 0; w < world_size; w++) {
    for (int i = 0; i < num_buffers; i++) {
      total_buffer_sizes[i] += all_buffer_sizes[w * num_buffers + i];
    }
  }
  return total_buffer_sizes;
}

std::vector<int32_t> receiveCounts(const std::vector<int32_t> &all_buffer_sizes,
                                   int receiveNo,
                                   int num_buffers,
                                   int world_size) {
  std::vector<int32_t> receive_counts(world_size, 0);
  for (int i = 0; i < world_size; ++i) {
    receive_counts[i] = all_buffer_sizes[i * num_buffers + receiveNo];
  }
  return receive_counts;
}

std::vector<int32_t> displacementsPerBuffer(const std::vector<int32_t> &all_buffer_sizes,
                                            int receiveNo,
                                            int num_buffers,
                                            int world_size) {
  std::vector<int32_t> disp_array(world_size, 0);
  disp_array[0] = 0;
  for (int i = 0; i < world_size - 1; ++i) {
    disp_array[i + 1] = disp_array[i] + all_buffer_sizes[i * num_buffers + receiveNo];
  }
  return disp_array;
}


cylon::Status cylon::mpi::Gather(const std::shared_ptr<cylon::TableSerializer> &serializer,
                                 int gather_root,
                                 bool gather_from_root,
                                 const std::shared_ptr<cylon::Allocator> &allocator,
                                 std::vector<int32_t> &all_buffer_sizes,
                                 std::vector<std::shared_ptr<cylon::Buffer>> &receive_buffers,
                                 std::vector<std::vector<int32_t>> &displacements,
                                 const std::shared_ptr<cylon::CylonContext> &ctx
) {

  // first gather table buffer sizes
  std::vector<int32_t> local_buffer_sizes;
  if (AmIRoot(gather_root, ctx) && !gather_from_root) {
    local_buffer_sizes = serializer->getEmptyTableBufferSizes();
  } else {
    local_buffer_sizes = serializer->getBufferSizes();
  }

  int32_t num_buffers = local_buffer_sizes.size();

  // gather size buffers
  if (AmIRoot(gather_root, ctx)) {
    all_buffer_sizes.resize(ctx->GetWorldSize() * num_buffers);
  }

  auto comm = GetMpiComm(ctx);

  int status = MPI_Gather(local_buffer_sizes.data(),
                          num_buffers,
                          MPI_INT32_T,
                          all_buffer_sizes.data(),
                          num_buffers,
                          MPI_INT32_T,
                          gather_root,
                          comm);
  if (status != MPI_SUCCESS) {
    return cylon::Status(cylon::Code::ExecutionError, "MPI_Gather failed!");
  }

  std::vector<int32_t> total_buffer_sizes;
  if (AmIRoot(gather_root, ctx)) {
    totalBufferSizes(all_buffer_sizes, num_buffers, ctx->GetWorldSize()).swap(total_buffer_sizes);
  }

  std::vector<MPI_Request> requests(num_buffers);
  std::vector<MPI_Status> statuses(num_buffers);
  const std::vector<const uint8_t *> &send_buffers = serializer->getDataBuffers();

  for (int32_t i = 0; i < num_buffers; ++i) {
    if (AmIRoot(gather_root, ctx)) {
      std::shared_ptr<cylon::Buffer> receive_buf;
      RETURN_CYLON_STATUS_IF_FAILED(allocator->Allocate(total_buffer_sizes[i], &receive_buf));
      const auto &receive_counts = receiveCounts(all_buffer_sizes,
                                                 i,
                                                 num_buffers,
                                                 ctx->GetWorldSize());
      const auto &disp_per_buffer = displacementsPerBuffer(all_buffer_sizes,
                                                           i,
                                                           num_buffers,
                                                           ctx->GetWorldSize());
      displacements.push_back(disp_per_buffer);

      status = MPI_Igatherv(send_buffers[i],
                            local_buffer_sizes[i],
                            MPI_UINT8_T,
                            receive_buf->GetByteBuffer(),
                            receive_counts.data(),
                            disp_per_buffer.data(),
                            MPI_UINT8_T,
                            gather_root,
                            comm,
                            &requests[i]);
      if (status != MPI_SUCCESS) {
        return cylon::Status(cylon::Code::ExecutionError, "MPI_Igatherv failed!");
      }
      receive_buffers.push_back(receive_buf);

    } else {
      status = MPI_Igatherv(send_buffers[i],
                            local_buffer_sizes[i],
                            MPI_UINT8_T,
                            nullptr,
                            nullptr,
                            nullptr,
                            MPI_UINT8_T,
                            gather_root,
                            comm,
                            &requests[i]);
      if (status != MPI_SUCCESS) {
        return cylon::Status(cylon::Code::ExecutionError, "MPI_Igatherv failed!");
      }
    }
  }

  status = MPI_Waitall(num_buffers, requests.data(), statuses.data());
  if (status != MPI_SUCCESS) {
    return cylon::Status(cylon::Code::ExecutionError, "MPI_Igatherv failed!");
  }

  return cylon::Status::OK();
}

cylon::Status cylon::mpi::GatherArrowBuffer(const std::shared_ptr<arrow::Buffer> &buf,
                                            int gather_root,
                                            const std::shared_ptr<cylon::CylonContext> &ctx,
                                            std::vector<std::shared_ptr<arrow::Buffer>> &buffers) {
  auto comm = GetMpiComm(ctx);

  std::vector<int32_t> all_buffer_sizes;
  if (AmIRoot(gather_root, ctx)) {
    all_buffer_sizes.resize(ctx->GetWorldSize(), 0);
  }

  int32_t size = static_cast<int32_t>(buf->size());
  int status = MPI_Gather(&size,
                          1,
                          MPI_INT32_T,
                          all_buffer_sizes.data(),
                          1,
                          MPI_INT32_T,
                          gather_root,
                          comm);
  if (status != MPI_SUCCESS) {
    return cylon::Status(cylon::Code::ExecutionError, "MPI_Gather failed when receiving buffer sizes!");
  }

  CYLON_ASSIGN_OR_RAISE(std::shared_ptr<arrow::Buffer> all_buf, arrow::AllocateBuffer(0));
  std::vector<int32_t> disps;

  if (AmIRoot(gather_root, ctx)) {
    auto total_size = std::accumulate(all_buffer_sizes.begin(), all_buffer_sizes.end(), 0);
    CYLON_ASSIGN_OR_RAISE(all_buf, arrow::AllocateBuffer(total_size));

    disps.resize(ctx->GetWorldSize(), 0);
    std::partial_sum(all_buffer_sizes.begin(), all_buffer_sizes.end() - 1, disps.begin() + 1);
  }

  status = MPI_Gatherv(buf->data(),
                       size,
                       MPI_UINT8_T,
                       (void *) all_buf->data(),
                       all_buffer_sizes.data(),
                       disps.data(),
                       MPI_UINT8_T,
                       gather_root,
                       comm);
  if (status != MPI_SUCCESS) {
    return cylon::Status(cylon::Code::ExecutionError, "MPI_Gatherv failed when receiving buffers!");
  }

  if (gather_root == ctx->GetRank()) {
    buffers.resize(ctx->GetWorldSize());
    for (int i = 0; i < ctx->GetWorldSize(); ++i) {
      buffers[i] = arrow::SliceBuffer(all_buf, disps[i], all_buffer_sizes[i]);
    }
  }

  return cylon::Status::OK();
}

cylon::Status cylon::mpi::AllGather(const std::shared_ptr<cylon::TableSerializer> &serializer,
                                    const std::shared_ptr<cylon::Allocator> &allocator,
                                    std::vector<int32_t> &all_buffer_sizes,
                                    std::vector<std::shared_ptr<cylon::Buffer>> &received_buffers,
                                    std::vector<std::vector<int32_t>> &displacements,
                                    const std::shared_ptr<cylon::CylonContext> &ctx) {
  auto comm = GetMpiComm(ctx);

  // first gather table buffer sizes
  const auto &local_buffer_sizes = serializer->getBufferSizes();
  int32_t num_buffers = local_buffer_sizes.size();

  all_buffer_sizes.resize(ctx->GetWorldSize() * num_buffers);

  int status = MPI_Allgather(local_buffer_sizes.data(),
                             num_buffers,
                             MPI_INT32_T,
                             all_buffer_sizes.data(),
                             num_buffers,
                             MPI_INT32_T,
                             comm);
  if (status != MPI_SUCCESS) {
    return cylon::Status(cylon::Code::ExecutionError, "MPI_Allgather failed when receiving table sizes!");
  }

  std::vector<int32_t> total_buffer_sizes = totalBufferSizes(all_buffer_sizes, num_buffers, ctx->GetWorldSize());

  std::vector<MPI_Request> requests(num_buffers);
  std::vector<MPI_Status> statuses(num_buffers);
  const std::vector<const uint8_t *> &send_buffers = serializer->getDataBuffers();

  for (int32_t i = 0; i < num_buffers; ++i) {
    std::shared_ptr<cylon::Buffer> receive_buf;
    RETURN_CYLON_STATUS_IF_FAILED(allocator->Allocate(total_buffer_sizes[i], &receive_buf));
    const auto &receive_counts = receiveCounts(all_buffer_sizes, i, num_buffers,
                                               ctx->GetWorldSize());
    auto disp_per_buffer = displacementsPerBuffer(all_buffer_sizes, i, num_buffers,
                                                  ctx->GetWorldSize());
    status = MPI_Iallgatherv(send_buffers[i],
                             local_buffer_sizes[i],
                             MPI_UINT8_T,
                             receive_buf->GetByteBuffer(),
                             receive_counts.data(),
                             disp_per_buffer.data(),
                             MPI_UINT8_T,
                             comm,
                             &requests[i]);
    if (status != MPI_SUCCESS) {
      return cylon::Status(cylon::Code::ExecutionError, "MPI_Iallgatherv failed when receiving table data!");
    }
    displacements.push_back(std::move(disp_per_buffer));
    received_buffers.push_back(std::move(receive_buf));
  }

  status = MPI_Waitall(num_buffers, requests.data(), statuses.data());
  if (status != MPI_SUCCESS) {
    return cylon::Status(cylon::Code::ExecutionError, "MPI_Iallgatherv failed!");
  }

  return cylon::Status::OK();
}

cylon::Status cylon::mpi::AllGatherArrowBuffer(const std::shared_ptr<arrow::Buffer> &buf,
                                               const std::shared_ptr<cylon::CylonContext> &ctx,
                                               std::vector<std::shared_ptr<arrow::Buffer>> &buffers) {
  auto comm = GetMpiComm(ctx);

  std::vector<int32_t> all_buffer_sizes(ctx->GetWorldSize(), 0);
  int32_t size = static_cast<int32_t>(buf->size());

  int status = MPI_Allgather(&size,
                             1,
                             MPI_INT32_T,
                             all_buffer_sizes.data(),
                             1,
                             MPI_INT32_T,
                             comm);
  if (status != MPI_SUCCESS) {
    return cylon::Status(cylon::Code::ExecutionError, "MPI_Allgather failed when receiving buffer sizes!");
  }

  auto total_size = std::accumulate(all_buffer_sizes.begin(), all_buffer_sizes.end(), 0);
  CYLON_ASSIGN_OR_RAISE(std::shared_ptr<arrow::Buffer> all_buf, arrow::AllocateBuffer(total_size));

  std::vector<int32_t> disps(ctx->GetWorldSize(), 0);
  std::partial_sum(all_buffer_sizes.begin(), all_buffer_sizes.end() - 1, disps.begin() + 1);

  status = MPI_Allgatherv(buf->data(),
                          size,
                          MPI_UINT8_T,
                          (void *) all_buf->data(),
                          all_buffer_sizes.data(),
                          disps.data(),
                          MPI_UINT8_T,
                          comm);
  if (status != MPI_SUCCESS) {
    return cylon::Status(cylon::Code::ExecutionError,
                         "MPI_Allgatherv failed when receiving buffers!");
  }

  buffers.resize(ctx->GetWorldSize());
  for (int i = 0; i < ctx->GetWorldSize(); ++i) {
    buffers[i] = arrow::SliceBuffer(all_buf, disps[i], all_buffer_sizes[i]);
  }

  return cylon::Status::OK();
}

MPI_Comm cylon::mpi::GetMpiComm(const std::shared_ptr<CylonContext> &ctx) {
  return std::static_pointer_cast<cylon::net::MPICommunicator>(ctx->GetCommunicator())->mpi_comm();
}
