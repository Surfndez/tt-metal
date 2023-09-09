// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "single_device_fixture.hpp"
#include "gtest/gtest.h"
#include "test_buffer_utils.hpp"
#include "tt_metal/host_api.hpp"
#include "tt_metal/test_utils/comparison.hpp"
#include "tt_metal/test_utils/df/df.hpp"
#include "tt_metal/test_utils/print_helpers.hpp"
#include "tt_metal/test_utils/stimulus.hpp"


using tt::tt_metal::Device;
using namespace tt::test_utils;
using namespace tt::test::buffer::detail;

namespace tt::test::buffer::detail {
    bool SimpleDramReadOnly (Device* device, size_t local_address, size_t byte_size) {
        std::vector<uint32_t> inputs =
            generate_uniform_random_vector<uint32_t>(0, UINT32_MAX, byte_size / sizeof(uint32_t));
        std::vector<uint32_t> outputs;
        auto buffer = tt::tt_metal::Buffer(device, byte_size, local_address, byte_size, tt::tt_metal::BufferType::DRAM);
        writeDramBackdoor(device, buffer.dram_channel_from_bank_id(0), buffer.address(), inputs);
        tt::tt_metal::ReadFromBuffer(buffer, outputs);
        bool pass = (inputs == outputs);
        if (not pass) {
            tt::log_info("Mismatch at Channel={}, Packet Size(in Bytes)={}", buffer.dram_channel_from_bank_id(0), byte_size);
        }
        return pass;
    }
    bool SimpleDramWriteOnly (Device* device, size_t local_address, size_t byte_size) {
        std::vector<uint32_t> inputs =
            generate_uniform_random_vector<uint32_t>(0, UINT32_MAX, byte_size / sizeof(uint32_t));
        std::vector<uint32_t> outputs;
        auto buffer = tt::tt_metal::Buffer(device, byte_size, local_address, byte_size, tt::tt_metal::BufferType::DRAM);
        tt::tt_metal::WriteToBuffer(buffer, inputs);
        readDramBackdoor(device, buffer.dram_channel_from_bank_id(0), buffer.address(), byte_size, outputs);
        bool pass = (inputs == outputs);
        if (not pass) {
            tt::log_info("Mismatch at Channel={}, Packet Size(in Bytes)={}", buffer.dram_channel_from_bank_id(0), byte_size);
        }
        return pass;
    }
}


TEST_F(SingleDeviceFixture, TestSimpleDramBufferReadOnlyLo) {
    size_t lo_address = 0;
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, lo_address, 4));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, lo_address, 8));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, lo_address, 16));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, lo_address, 32));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, lo_address, 1024));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, lo_address, 16*1024));
}
TEST_F(SingleDeviceFixture, TestSimpleDramBufferReadOnlyHi) {
    size_t hi_address = this->device_->dram_bank_size() - (16*1024);
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, hi_address, 4));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, hi_address, 8));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, hi_address, 16));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, hi_address, 32));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, hi_address, 1024));
    ASSERT_TRUE(SimpleDramReadOnly(this->device_, hi_address, 16*1024));
}
TEST_F(SingleDeviceFixture, TestSimpleDramBufferWriteOnlyLo) {
    size_t lo_address = 0;
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, lo_address, 4));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, lo_address, 8));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, lo_address, 16));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, lo_address, 32));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, lo_address, 1024));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, lo_address, 16*1024));
}
TEST_F(SingleDeviceFixture, TestSimpleDramBufferWriteOnlyHi) {
    size_t hi_address = this->device_->dram_bank_size() - (16*1024);
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, hi_address, 4));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, hi_address, 8));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, hi_address, 16));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, hi_address, 32));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, hi_address, 1024));
    ASSERT_TRUE(SimpleDramWriteOnly(this->device_, hi_address, 16*1024));
}
