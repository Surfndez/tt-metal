// SPDX-FileCopyrightText: © 2024 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

// This file contains the TT Hardware Abstraction Layer interface
// This layer abstracts which TT chip is running from the higher
// level APIs
//

#include <cstdint>
#include <functional>
#include <variant>
#include <vector>
#include <memory>
#include "tt_metal/common/assert.hpp"
#include "tt_metal/common/utils.hpp"

enum class CoreType;

namespace tt {

enum class ARCH;

namespace tt_metal {

enum class HalProgrammableCoreType { TENSIX = 0, ACTIVE_ETH = 1, IDLE_ETH = 2, COUNT = 3 };

static constexpr uint32_t NumHalProgrammableCoreTypes = static_cast<uint32_t>(HalProgrammableCoreType::COUNT);

enum class HalProcessorClassType : uint8_t {
    DM = 0,
    // Setting this to 2 because we currently treat brisc and ncrisc as two unique processor classes on Tensix
    // TODO: Uplift view of Tensix processor classes to be 1 DM class with 2 processor types
    COMPUTE = 2
};

enum class HalL1MemAddrType : uint8_t {
    BASE,
    BARRIER,
    MAILBOX,
    LAUNCH,
    WATCHER,
    DPRINT,
    PROFILER,
    KERNEL_CONFIG,
    UNRESERVED,
    CORE_INFO,
    GO_MSG,
    LAUNCH_MSG_BUFFER_RD_PTR,
    FW_VERSION_ADDR,  // Really only applicable to active eth core right now
    LOCAL,
    COUNT             // Keep this last so it always indicates number of enum options
};

enum class HalDramMemAddrType : uint8_t { DRAM_BARRIER = 0, COUNT = 1 };

enum class HalMemType : uint8_t { L1 = 0, DRAM = 1, HOST = 2, COUNT = 3 };

using DeviceAddr = std::uint64_t;

struct HalJitBuildConfig {
    DeviceAddr fw_base_addr;
    DeviceAddr local_init_addr;
};

class Hal;

// Core information instanced once per core type
class HalCoreInfoType {
    friend class Hal;

private:
    HalProgrammableCoreType programmable_core_type_;
    CoreType core_type_;
    // indices represents processor class and type positions, value is build configuration params
    std::vector<std::vector<HalJitBuildConfig>> processor_classes_;
    std::vector<DeviceAddr> mem_map_bases_;
    std::vector<uint32_t> mem_map_sizes_;
    bool supports_cbs_;

public:
    HalCoreInfoType(
        HalProgrammableCoreType programmable_core_type,
        CoreType core_type,
        const std::vector<std::vector<HalJitBuildConfig>>& processor_classes,
        const std::vector<DeviceAddr>& mem_map_bases,
        const std::vector<uint32_t>& mem_map_sizes,
        bool supports_cbs);

    template <typename T = DeviceAddr>
    T get_dev_addr(HalL1MemAddrType addr_type) const;
    uint32_t get_dev_size(HalL1MemAddrType addr_type) const;
    uint32_t get_processor_classes_count() const;
    uint32_t get_processor_types_count(uint32_t processor_class_idx) const;
    template <typename T = DeviceAddr>
    T get_base_firmware_addr(uint32_t processor_class_idx, uint32_t processor_type_idx) const;
    template <typename T = DeviceAddr>
    T get_binary_local_init_addr(uint32_t processor_class_idx, uint32_t processor_type_idx) const;
};

template <typename T>
inline T HalCoreInfoType::get_dev_addr(HalL1MemAddrType addr_type) const {
    uint32_t index = utils::underlying_type<HalL1MemAddrType>(addr_type);
    TT_ASSERT(index < this->mem_map_bases_.size());
    return reinterpret_cast<T>(this->mem_map_bases_[index]);
}

inline uint32_t HalCoreInfoType::get_dev_size(HalL1MemAddrType addr_type) const {
    uint32_t index = utils::underlying_type<HalL1MemAddrType>(addr_type);
    TT_ASSERT(index < this->mem_map_sizes_.size());
    return this->mem_map_sizes_[index];
}

inline uint32_t HalCoreInfoType::get_processor_classes_count() const { return this->processor_classes_.size(); }

inline uint32_t HalCoreInfoType::get_processor_types_count(uint32_t processor_class_idx) const {
    TT_ASSERT(processor_class_idx < this->processor_classes_.size());
    return this->processor_classes_[processor_class_idx].size();
}

template <typename T>
inline T HalCoreInfoType::get_base_firmware_addr(uint32_t processor_class_idx, uint32_t processor_type_idx) const {
    TT_ASSERT(processor_class_idx < this->processor_classes_.size());
    TT_ASSERT(processor_type_idx < this->processor_classes_[processor_class_idx].size());
    return this->processor_classes_[processor_class_idx][processor_type_idx].fw_base_addr;
}

template <typename T>
inline T HalCoreInfoType::get_binary_local_init_addr(uint32_t processor_class_idx, uint32_t processor_type_idx) const {
    TT_ASSERT(processor_class_idx < this->processor_classes_.size());
    TT_ASSERT(processor_type_idx < this->processor_classes_[processor_class_idx].size());
    return this->processor_classes_[processor_class_idx][processor_type_idx].local_init_addr;
}

class Hal {
public:
    using RelocateFunc = std::function<uint64_t(uint64_t, uint64_t)>;
    using ValidRegAddrFunc = std::function<bool(uint32_t)>;

private:
    tt::ARCH arch_;
    std::vector<HalCoreInfoType> core_info_;
    std::vector<DeviceAddr> dram_bases_;
    std::vector<uint32_t> dram_sizes_;
    std::vector<uint32_t> mem_alignments_;

    void initialize_gs();
    void initialize_wh();
    void initialize_bh();

    // Functions where implementation varies by architecture
    RelocateFunc relocate_func_;
    ValidRegAddrFunc valid_reg_addr_func_;

public:
    Hal();

    tt::ARCH get_arch() const { return arch_; }

    template <typename IndexType, typename SizeType, typename CoordType>
    auto noc_coordinate(IndexType noc_index, SizeType noc_size, CoordType coord) const
        -> decltype(noc_size - 1 - coord) {
        return noc_index == 0 ? coord : (noc_size - 1 - coord);
    }

    uint32_t get_programmable_core_type_count() const;
    HalProgrammableCoreType get_programmable_core_type(uint32_t core_type_index) const;
    uint32_t get_programmable_core_type_index(HalProgrammableCoreType programmable_core_type_index) const;
    CoreType get_core_type(uint32_t programmable_core_type_index) const;
    uint32_t get_processor_classes_count(std::variant<HalProgrammableCoreType, uint32_t> programmable_core_type) const;
    uint32_t get_processor_class_type_index(HalProcessorClassType processor_class);
    uint32_t get_processor_types_count(
        std::variant<HalProgrammableCoreType, uint32_t> programmable_core_type, uint32_t processor_class_idx) const;

    template <typename T = DeviceAddr>
    T get_dev_addr(HalProgrammableCoreType programmable_core_type, HalL1MemAddrType addr_type) const;
    template <typename T = DeviceAddr>
    T get_dev_addr(uint32_t programmable_core_type_index, HalL1MemAddrType addr_type) const;
    uint32_t get_dev_size(HalProgrammableCoreType programmable_core_type, HalL1MemAddrType addr_type) const;

    // Overloads for Dram Params
    template <typename T = DeviceAddr>
    T get_dev_addr(HalDramMemAddrType addr_type) const;
    uint32_t get_dev_size(HalDramMemAddrType addr_type) const;

    uint32_t get_alignment(HalMemType memory_type) const;

    bool get_supports_cbs(uint32_t programmable_core_type_index) const;

    uint32_t get_num_risc_processors() const;

    template <typename T = DeviceAddr>
    T get_base_firmware_addr(
        uint32_t programmable_core_type_index, uint32_t processor_class_idx, uint32_t processor_type_idx) const;
    template <typename T = DeviceAddr>
    T get_binary_local_init_addr(
        uint32_t programmable_core_type_index, uint32_t processor_class_idx, uint32_t processor_type_idx) const;

    uint64_t relocate_dev_addr(uint64_t addr, uint64_t local_init_addr = 0) {
        return relocate_func_(addr, local_init_addr);
    }

    uint32_t valid_reg_addr(uint32_t addr) { return valid_reg_addr_func_(addr); }
};

inline uint32_t Hal::get_programmable_core_type_count() const { return core_info_.size(); }

inline uint32_t Hal::get_processor_classes_count(
    std::variant<HalProgrammableCoreType, uint32_t> programmable_core_type) const {
    return std::visit(
        [&](auto&& core_type_specifier) -> uint32_t {
            using T = std::decay_t<decltype(core_type_specifier)>;
            uint32_t index = this->core_info_.size();
            if constexpr (std::is_same_v<T, HalProgrammableCoreType>) {
                index = utils::underlying_type<HalProgrammableCoreType>(core_type_specifier);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                index = core_type_specifier;
            }
            TT_ASSERT(index < this->core_info_.size());
            return this->core_info_[index].get_processor_classes_count();
        },
        programmable_core_type);
}

inline uint32_t Hal::get_processor_types_count(
    std::variant<HalProgrammableCoreType, uint32_t> programmable_core_type, uint32_t processor_class_idx) const {
    return std::visit(
        [&](auto&& core_type_specifier) -> uint32_t {
            using T = std::decay_t<decltype(core_type_specifier)>;
            uint32_t index = this->core_info_.size();
            if constexpr (std::is_same_v<T, HalProgrammableCoreType>) {
                index = utils::underlying_type<HalProgrammableCoreType>(core_type_specifier);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                index = core_type_specifier;
            }
            TT_ASSERT(index < this->core_info_.size());
            return this->core_info_[index].get_processor_types_count(processor_class_idx);
        },
        programmable_core_type);
}

inline HalProgrammableCoreType Hal::get_programmable_core_type(uint32_t core_type_index) const {
    return core_info_[core_type_index].programmable_core_type_;
}

inline CoreType Hal::get_core_type(uint32_t core_type_index) const { return core_info_[core_type_index].core_type_; }

template <typename T>
inline T Hal::get_dev_addr(HalProgrammableCoreType programmable_core_type, HalL1MemAddrType addr_type) const {
    uint32_t index = utils::underlying_type<HalProgrammableCoreType>(programmable_core_type);
    TT_ASSERT(index < this->core_info_.size());
    return this->core_info_[index].get_dev_addr<T>(addr_type);
}

template <typename T>
inline T Hal::get_dev_addr(uint32_t programmable_core_type_index, HalL1MemAddrType addr_type) const {
    TT_ASSERT(programmable_core_type_index < this->core_info_.size());
    return this->core_info_[programmable_core_type_index].get_dev_addr<T>(addr_type);
}

inline uint32_t Hal::get_dev_size(HalProgrammableCoreType programmable_core_type, HalL1MemAddrType addr_type) const {
    uint32_t index = utils::underlying_type<HalProgrammableCoreType>(programmable_core_type);
    TT_ASSERT(index < this->core_info_.size());
    return this->core_info_[index].get_dev_size(addr_type);
}

template <typename T>
inline T Hal::get_dev_addr(HalDramMemAddrType addr_type) const {
    uint32_t index = utils::underlying_type<HalDramMemAddrType>(addr_type);
    TT_ASSERT(index < this->dram_bases_.size());
    return reinterpret_cast<T>(this->dram_bases_[index]);
}

inline uint32_t Hal::get_dev_size(HalDramMemAddrType addr_type) const {
    uint32_t index = utils::underlying_type<HalDramMemAddrType>(addr_type);
    TT_ASSERT(index < this->dram_sizes_.size());
    return this->dram_sizes_[index];
}

inline uint32_t Hal::get_alignment(HalMemType memory_type) const {
    uint32_t index = utils::underlying_type<HalMemType>(memory_type);
    TT_ASSERT(index < this->mem_alignments_.size());
    return this->mem_alignments_[index];
}

inline bool Hal::get_supports_cbs(uint32_t programmable_core_type_index) const {
    return this->core_info_[programmable_core_type_index].supports_cbs_;
}

template <typename T>
inline T Hal::get_base_firmware_addr(
    uint32_t programmable_core_type_index, uint32_t processor_class_idx, uint32_t processor_type_idx) const {
    TT_ASSERT(programmable_core_type_index < this->core_info_.size());
    return this->core_info_[programmable_core_type_index].get_base_firmware_addr(
        processor_class_idx, processor_type_idx);
}

template <typename T>
inline T Hal::get_binary_local_init_addr(
    uint32_t programmable_core_type_index, uint32_t processor_class_idx, uint32_t processor_type_idx) const {
    TT_ASSERT(programmable_core_type_index < this->core_info_.size());
    return this->core_info_[programmable_core_type_index].get_binary_local_init_addr(
        processor_class_idx, processor_type_idx);
}

class HalSingleton : public Hal {
private:
    HalSingleton() = default;
    HalSingleton(const HalSingleton&) = delete;
    HalSingleton(HalSingleton&&) = delete;
    ~HalSingleton() = default;

    HalSingleton& operator=(const HalSingleton&) = delete;
    HalSingleton& operator=(HalSingleton&&) = delete;

public:
    static inline HalSingleton& getInstance() {
        static HalSingleton instance;
        return instance;
    }
};

inline auto& hal = HalSingleton::getInstance();  // inline variable requires C++17

}  // namespace tt_metal
}  // namespace tt

#define HAL_MEM_L1_BASE \
    tt::tt_metal::hal.get_dev_addr(tt::tt_metal::HalProgrammableCoreType::TENSIX, tt::tt_metal::HalL1MemAddrType::BASE)
#define HAL_MEM_L1_SIZE \
    tt::tt_metal::hal.get_dev_size(tt::tt_metal::HalProgrammableCoreType::TENSIX, tt::tt_metal::HalL1MemAddrType::BASE)

#define HAL_MEM_ETH_BASE                                   \
    ((tt::tt_metal::hal.get_arch() == tt::ARCH::GRAYSKULL) \
         ? 0                                               \
         : tt::tt_metal::hal.get_dev_addr(                 \
               tt::tt_metal::HalProgrammableCoreType::IDLE_ETH, tt::tt_metal::HalL1MemAddrType::BASE))
#define HAL_MEM_ETH_SIZE                                   \
    ((tt::tt_metal::hal.get_arch() == tt::ARCH::GRAYSKULL) \
         ? 0                                               \
         : tt::tt_metal::hal.get_dev_size(                 \
               tt::tt_metal::HalProgrammableCoreType::IDLE_ETH, tt::tt_metal::HalL1MemAddrType::BASE))
