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

#ifndef CYLON_CPP_SRC_CYLON_NET_COMM_OPERATIONS_HPP_
#define CYLON_CPP_SRC_CYLON_NET_COMM_OPERATIONS_HPP_

#include <mpi.h>
#include <cylon/data_types.hpp>

namespace cylon {
namespace net {

/**
 * Cylon reduction operations
 */
enum ReduceOp {
  SUM,
  MIN,
  MAX,
  PROD,
  LAND,
  LOR,
  BAND,
  BOR,
};

}
}
#endif //CYLON_CPP_SRC_CYLON_NET_COMM_OPERATIONS_HPP_
