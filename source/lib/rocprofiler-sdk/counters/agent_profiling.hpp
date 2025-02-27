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

#include <rocprofiler-sdk/fwd.h>
#include <rocprofiler-sdk/hsa.h>
#include <rocprofiler-sdk/rocprofiler.h>

#include "lib/rocprofiler-sdk/hsa/aql_packet.hpp"

namespace rocprofiler
{
namespace context
{
struct context;
}

namespace counters
{
struct agent_callback_data
{
    CoreApiTable                           table;
    hsa_queue_t*                           queue{nullptr};
    std::unique_ptr<hsa::CounterAQLPacket> packet;

    // Tri-state signal used to know what the current state of processing
    // a sample is. The states are:
    //   1: allow next sample to start (i.e. no in progress work)
    //   0: sample in progress
    //  -1: sample complete  (i.e. signal for caller that sample is ready)
    hsa_signal_t            completion{.handle = 0};
    rocprofiler_user_data_t user_data{.value = 0};
    ~agent_callback_data();
};

// If we have contexts that are started before HSA init. This
// function will start those contexts. Should only be called
// as part of the HSA init process in rocprofiler.
rocprofiler_status_t
agent_profile_hsa_registration();

// Send the AQL start packet to a queue on the agent to start
// collecting counter data. This function is synchronous and will
// return when the agent has started collecting data (or if there
// is an error).
rocprofiler_status_t
start_agent_ctx(const context::context* ctx);

// Send the AQL end packet to a queue on the agent to stop
// collecting counter data. This function is synchronous and will
// return when the agent has stopped collecting data (or if there
// is an error).
rocprofiler_status_t
stop_agent_ctx(const context::context* ctx);

// Read the counter data from the agent. This function is synchronous
// if flags is not set to ASYNC. If ASYNC is set, the function will
// return before data has been written to the buffer. Overlapping
// read calls are not allowed in ASYNC mode and will result in
// this call waiting for the previous sample to complete.
rocprofiler_status_t
read_agent_ctx(const context::context*    ctx,
               rocprofiler_user_data_t    user_data,
               rocprofiler_counter_flag_t flags);

}  // namespace counters
}  // namespace rocprofiler