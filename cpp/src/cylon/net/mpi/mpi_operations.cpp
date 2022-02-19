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

#include <cylon/status.hpp>
#include <cylon/net/mpi/mpi_operations.hpp>

MPI_Op cylon::mpi::GetMPIOp(cylon::net::ReduceOp reduce_op) {
  switch (reduce_op) {
    case net::SUM: return MPI_SUM;
    case net::MIN: return MPI_MIN;
    case net::MAX: return MPI_MAX;
    case net::PROD: return MPI_PROD;
    case net::LAND:return MPI_LAND;
    case net::LOR:return MPI_LOR;
    case net::BAND:return MPI_BAND;
    case net::BOR:return MPI_BOR;
    default: return MPI_OP_NULL;
  }
}

MPI_Datatype cylon::mpi::GetMPIDataType(const std::shared_ptr<DataType> &data_type) {
  switch (data_type->getType()) {
    case Type::BOOL:return MPI_C_BOOL;
    case Type::UINT8:return MPI_UINT8_T;
    case Type::INT8:return MPI_INT8_T;
    case Type::UINT16:return MPI_UINT16_T;
    case Type::INT16:return MPI_INT16_T;
    case Type::UINT32:return MPI_UINT32_T;
    case Type::INT32:return MPI_INT32_T;
    case Type::UINT64:return MPI_UINT64_T;
    case Type::INT64:return MPI_INT64_T;
    case Type::FLOAT:return MPI_FLOAT;
    case Type::DOUBLE:return MPI_DOUBLE;
    case Type::FIXED_SIZE_BINARY:
    case Type::STRING:
    case Type::BINARY:
    case Type::LARGE_STRING:
    case Type::LARGE_BINARY:return MPI_BYTE;
      //todo: MPI does not support 16byte floats. We'll have to use a custom datatype for this later.
    case Type::HALF_FLOAT: break;
    case Type::DATE32:return MPI_UINT32_T;
    case Type::DATE64:return MPI_UINT64_T;
    case Type::TIMESTAMP:return MPI_UINT64_T;
    case Type::TIME32:return MPI_UINT32_T;
    case Type::TIME64:return MPI_UINT64_T;
    case Type::DECIMAL:
    case Type::DURATION:
    case Type::INTERVAL:
    case Type::LIST:
    case Type::FIXED_SIZE_LIST:
    case Type::EXTENSION:break;
    case Type::MAX_ID:break;
  }
  return MPI_DATATYPE_NULL;
}

cylon::Status cylon::mpi::AllReduce(const std::shared_ptr<CylonContext> &ctx,
                                    const void *send_buf,
                                    void *rcv_buf,
                                    int count,
                                    const std::shared_ptr<DataType> &data_type,
                                    cylon::net::ReduceOp reduce_op) {
  auto comm = GetMpiComm(ctx);

  MPI_Datatype mpi_data_type = cylon::mpi::GetMPIDataType(data_type);
  MPI_Op mpi_op = cylon::mpi::GetMPIOp(reduce_op);

  if (mpi_data_type == MPI_DATATYPE_NULL || mpi_op == MPI_OP_NULL) {
    return cylon::Status(cylon::Code::NotImplemented, "Unknown data type or operation for MPI");
  }

  if (MPI_Allreduce(send_buf, rcv_buf, count, mpi_data_type, mpi_op, comm) == MPI_SUCCESS) {
    return cylon::Status::OK();
  } else {
    return cylon::Status(cylon::Code::ExecutionError, "MPI operation failed!");
  }
}

