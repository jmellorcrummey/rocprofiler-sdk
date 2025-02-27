#pragma once
#include <rocprofiler-sdk/fwd.h>
#include <rocprofiler-sdk/rocprofiler.h>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#define ROCPROFILER_CALL(result, msg)                                                              \
    {                                                                                              \
        rocprofiler_status_t CHECKSTATUS = result;                                                 \
        if(CHECKSTATUS != ROCPROFILER_STATUS_SUCCESS)                                              \
        {                                                                                          \
            std::string status_msg = rocprofiler_get_status_string(CHECKSTATUS);                   \
            std::cerr << "[" #result "][" << __FILE__ << ":" << __LINE__ << "] " << msg            \
                      << " failed with error code " << CHECKSTATUS << ": " << status_msg           \
                      << std::endl;                                                                \
            std::stringstream errmsg{};                                                            \
            errmsg << "[" #result "][" << __FILE__ << ":" << __LINE__ << "] " << msg " failure ("  \
                   << status_msg << ")";                                                           \
            throw std::runtime_error(errmsg.str());                                                \
        }                                                                                          \
    }

#define C_API_BEGIN                                                                                \
    try                                                                                            \
    {
#define C_API_END                                                                                  \
    }                                                                                              \
    catch(std::exception & e)                                                                      \
    {                                                                                              \
        std::cerr << "Error in " << __FILE__ << ':' << __LINE__ << ' ' << e.what() << std::endl;   \
    }                                                                                              \
    catch(...) { std::cerr << "Error in " << __FILE__ << ':' << __LINE__ << std::endl; }

namespace ATTTest
{
struct TrackedIsa
{
    std::atomic<size_t> hitcount{0};
    std::atomic<size_t> latency{0};
    std::string         inst{};
};

struct pcInfo
{
    size_t addr;
    size_t marker_id;

    bool operator==(const pcInfo& other) const
    {
        return addr == other.addr && marker_id == other.marker_id;
    }
    bool operator<(const pcInfo& other) const
    {
        if(marker_id == other.marker_id) return addr < other.addr;
        return marker_id < other.marker_id;
    }
};

struct ToolData
{
    std::unordered_map<uint64_t, std::string>     kernel_id_to_kernel_name = {};
    std::map<pcInfo, std::unique_ptr<TrackedIsa>> isa_map;

    std::atomic<int> waves_started = 0;
    std::atomic<int> waves_ended   = 0;
    std::mutex       isa_map_mut;
    std::set<pcInfo> wave_start_locations{};
};

namespace Callbacks
{
void
tool_codeobj_tracing_callback(rocprofiler_callback_tracing_record_t record,
                              rocprofiler_user_data_t*,
                              void* callback_data);

void
shader_data_callback(int64_t se_id, void* se_data, size_t data_size, void* userdata);

void
callbacks_init();

void
callbacks_fini();

};  // namespace Callbacks

};  // namespace ATTTest