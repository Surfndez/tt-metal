// SPDX-FileCopyrightText: © 2024 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "ttnn/decorators.hpp"
#include "ttnn/operations/core/compute_kernel/compute_kernel_config.hpp"
namespace ttnn::operations::moreh::moreh_sum {
struct MorehSum {
    static Tensor invoke(
        const Tensor& input,
        std::optional<std::variant<int64_t, std::vector<int64_t>>> dims,
        const bool keep_batch_dim,
        const std::optional<Tensor>& output,
        const std::optional<MemoryConfig>& output_mem_config,
        const std::optional<DeviceComputeKernelConfig>& compute_kernel_config);
};
}  // namespace ttnn::operations::moreh::moreh_sum

namespace ttnn {
constexpr auto moreh_sum = ttnn::register_operation<"ttnn::moreh_sum", ttnn::operations::moreh::moreh_sum::MorehSum>();
}