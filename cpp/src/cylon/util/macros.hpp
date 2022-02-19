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

#ifndef CYLON_CPP_SRC_CYLON_UTIL_MACROS_HPP_
#define CYLON_CPP_SRC_CYLON_UTIL_MACROS_HPP_

#define LOG_AND_RETURN_ERROR(code, msg) \
  LOG(ERROR) << msg ; \
  return cylon::Status(code, msg)

#define RETURN_CYLON_STATUS_IF_FAILED(expr) \
  do{                               \
    const auto& _st = (expr);       \
    if (!_st.is_ok()) {             \
      return _st;                   \
    };                              \
  } while (0)

#define RETURN_CYLON_STATUS_IF_ARROW_FAILED(expr) \
  do{                               \
    const auto& _st = (expr);       \
    if (!_st.ok()) {                \
      return cylon::Status(static_cast<int>(_st.code()), _st.message()); \
    };                              \
  } while (0)

#define RETURN_CYLON_STATUS_IF_MPI_FAILED(expr) \
  do{                               \
    const auto& _st = (expr);       \
    if (_st != MPI_SUCCESS) {               \
      char _err_str[MPI_MAX_ERROR_STRING];  \
      int _err_str_len = 0;                 \
      MPI_Error_string(_st, _err_str, &_err_str_len); \
      return cylon::Status(Code::ExecutionError,      \
                           std::string(_err_str, _err_str_len)); \
    };                              \
  } while (0)

#define CYLON_ASSIGN_OR_RAISE_IMPL(result_name, lhs, rexpr)     \
  auto&& result_name = (rexpr);                                 \
  RETURN_CYLON_STATUS_IF_ARROW_FAILED((result_name).status()) ; \
  lhs = std::move(result_name).ValueUnsafe();

#define CYLON_ASSIGN_OR_RAISE(lhs, rexpr)                                              \
  CYLON_ASSIGN_OR_RAISE_IMPL(ARROW_ASSIGN_OR_RAISE_NAME(_error_or_value, __COUNTER__), \
                                  lhs, rexpr);

#define RETURN_ARROW_STATUS_IF_FAILED(expr) \
  do{                               \
    const auto& _st = (expr);       \
    if (!_st.ok()) {                \
      return _st;                   \
    };                              \
  } while (0)

#define COMBINE_CHUNKS_RETURN_CYLON_STATUS(arrow_table, pool)   \
  do{                                                           \
    if ((arrow_table)->column(0)->num_chunks() > 1){            \
      const auto &res = (arrow_table)->CombineChunks((pool));   \
      RETURN_CYLON_STATUS_IF_ARROW_FAILED(res.status());        \
      (arrow_table) = std::move(res).ValueOrDie();              \
    }                                                           \
  } while (0)

#define COMBINE_CHUNKS_RETURN_ARROW_STATUS(arrow_table, pool) \
  do{                                                         \
    if ((arrow_table)->column(0)->num_chunks() > 1){          \
      const auto &res = (arrow_table)->CombineChunks((pool)); \
      if (!res.ok()) {                                        \
        return res.status();                                  \
      }                                                       \
      (arrow_table) = res.ValueOrDie();                       \
    }                                                         \
  } while (0)

#define CYLON_UNUSED(expr) do { (void)(expr); } while (0)

#endif //CYLON_CPP_SRC_CYLON_UTIL_MACROS_HPP_
