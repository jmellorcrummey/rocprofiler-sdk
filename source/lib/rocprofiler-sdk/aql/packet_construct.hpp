// MIT License
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <functional>
#include <map>
#include <vector>

#include <hsa/hsa.h>
#include <hsa/hsa_api_trace.h>
#include <hsa/hsa_ven_amd_aqlprofile.h>

#include "lib/rocprofiler-sdk/aql/aql_profile_v2.h"
#include "lib/rocprofiler-sdk/aql/helpers.hpp"
#include "lib/rocprofiler-sdk/counters/metrics.hpp"
#include "lib/rocprofiler-sdk/hsa/agent_cache.hpp"
#include "lib/rocprofiler-sdk/hsa/queue.hpp"
#include "lib/rocprofiler-sdk/thread_trace/att_core.hpp"

namespace rocprofiler
{
namespace aql
{
/**
 * Class to construct AQL Packets for a specific agent and metric set.
 * Thie class checks that the counters supplied are collectable on the
 * agent in question (including making sure that they stay within block
 * limits). construct_packet returns an AQLPacket class containing the
 * consturcted start/stop/read packets along with allocated buffers needed
 * to collect the counter data.
 */
class CounterPacketConstruct
{
public:
    CounterPacketConstruct(rocprofiler_agent_id_t               agent,
                           const std::vector<counters::Metric>& metrics);
    std::unique_ptr<hsa::CounterAQLPacket> construct_packet(const AmdExtTable&);

    const counters::Metric* event_to_metric(const hsa_ven_amd_aqlprofile_event_t& event) const;
    std::vector<hsa_ven_amd_aqlprofile_event_t> get_all_events() const;
    const std::vector<aqlprofile_pmc_event_t>&  get_counter_events(const counters::Metric&) const;

    rocprofiler_agent_id_t agent() const { return _agent; }

private:
    static constexpr size_t MEM_PAGE_ALIGN = 0x1000;
    static constexpr size_t MEM_PAGE_MASK  = MEM_PAGE_ALIGN - 1;
    static size_t getPageAligned(size_t p) { return (p + MEM_PAGE_MASK) & ~MEM_PAGE_MASK; }

protected:
    struct AQLProfileMetric
    {
        counters::Metric                            metric;
        std::vector<hsa_ven_amd_aqlprofile_event_t> instances;
        std::vector<aqlprofile_pmc_event_t>         events;
    };

    void can_collect();

    rocprofiler_agent_id_t                      _agent;
    std::vector<AQLProfileMetric>               _metrics;
    std::vector<hsa_ven_amd_aqlprofile_event_t> _events;
    std::map<std::tuple<hsa_ven_amd_aqlprofile_block_name_t, uint32_t, uint32_t>, counters::Metric>
        _event_to_metric;
};

class ThreadTraceAQLPacketFactory
{
public:
    ThreadTraceAQLPacketFactory(const hsa::AgentCache&             agent,
                                const thread_trace_parameter_pack& params,
                                const CoreApiTable&                coreapi,
                                const AmdExtTable&                 ext);
    std::unique_ptr<hsa::TraceControlAQLPacket>  construct_packet();
    std::unique_ptr<hsa::CodeobjMarkerAQLPacket> construct_load_marker_packet(uint64_t id,
                                                                              uint64_t addr,
                                                                              uint64_t size);
    std::unique_ptr<hsa::CodeobjMarkerAQLPacket> construct_unload_marker_packet(uint64_t id);

private:
    hsa::TraceMemoryPool                            tracepool;
    std::vector<hsa_ven_amd_aqlprofile_parameter_t> aql_params;
};

}  // namespace aql
}  // namespace rocprofiler
