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
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "metrics.hpp"

#include <rocprofiler-sdk/rocprofiler.h>

#include "lib/common/filesystem.hpp"
#include "lib/common/static_object.hpp"
#include "lib/common/utility.hpp"
#include "lib/common/xml.hpp"
#include "lib/rocprofiler-sdk/agent.hpp"

#include "glog/logging.h"

#include <dlfcn.h>  // for dladdr
#include <cstdint>
#include <cstdlib>

namespace rocprofiler
{
namespace counters
{
namespace
{
uint64_t&
current_id()
{
    static uint64_t id = 0;
    return id;
}

/**
 * Constant/speical metrics are treated as psudo-metrics in that they
 * are given their own metric id. MAX_WAVE_SIZE for example is not collected
 * by AQL Profiler but is a constant from the topology. It will still have
 * a counter associated with it. Nearly all metrics contained in
 * rocprofiler_agent_t will have a counter id associated with it and can be
 * used in derived counters (exact support properties that can be used can
 * be viewed in evaluate_ast.cpp:get_agent_property()).
 */
const std::vector<Metric>&
get_constants()
{
    static std::vector<Metric> constants;
    if(!constants.empty()) return constants;
    // Ensure topology is read
    rocprofiler::agent::get_agents();
    for(const auto& prop : rocprofiler::agent::get_agent_available_properties())
    {
        constants.emplace_back("constant",
                               prop,
                               "",
                               "",
                               fmt::format("Constant value {} from agent properties", prop),
                               "",
                               "yes",
                               current_id());
        current_id()++;
    }
    return constants;
}

// Future TODO: inheritance? does it work for derived_counters.xml?
MetricMap
loadXml(const std::string& filename, bool load_constants = false)
{
    MetricMap ret;
    ROCP_INFO << "Loading Counter Config: " << filename;
    // todo: return unique_ptr....
    auto xml = common::Xml::Create(filename);
    ROCP_FATAL_IF(!xml)
        << "Could not open XML Counter Config File (set env ROCPROFILER_METRICS_PATH)";

    const auto& constant_metrics = get_constants();
    for(const auto& [gfx_name, nodes] : xml->GetAllNodes())
    {
        /**
         * "top." is used to designate the root encapsulation of all contained XML subroots (in our
         * case "gfxX"). This is inserted by the parser so it will always be present. .metric
         * denotes XML tags that are contained in the subroots. This will not change unless we
         * respec the XML (which we should...).
         */
        if(gfx_name.find("metric") == std::string::npos ||
           gfx_name.find("top.") == std::string::npos)
            continue;

        auto& metricVec =
            ret.emplace(gfx_name.substr(strlen("top."),
                                        gfx_name.size() - strlen("top.") - strlen(".metric")),
                        std::vector<Metric>())
                .first->second;
        for(const auto& node : nodes)
        {
            metricVec.emplace_back(gfx_name,
                                   node->opts["name"],
                                   node->opts["block"],
                                   node->opts["event"],
                                   node->opts["descr"],
                                   node->opts["expr"],
                                   node->opts["special"],
                                   current_id());
            current_id()++;
        }

        if(load_constants)
        {
            metricVec.insert(metricVec.end(), constant_metrics.begin(), constant_metrics.end());
        }
    }

    ROCP_FATAL_IF(current_id() > 65536)
        << "Counter count exceeds 16 bits, which may break counter id output";
    return ret;
}

std::string
findViaInstallPath(const std::string& filename)
{
    Dl_info dl_info = {};
    ROCP_INFO << filename << " is being looked up via install path";
    if(dladdr(reinterpret_cast<const void*>(rocprofiler_query_available_agents), &dl_info) != 0)
    {
        return common::filesystem::path{dl_info.dli_fname}.parent_path().parent_path() /
               fmt::format("share/rocprofiler-sdk/{}", filename);
    }
    return filename;
}

std::string
findViaEnvironment(const std::string& filename)
{
    if(const char* metrics_path = nullptr; (metrics_path = getenv("ROCPROFILER_METRICS_PATH")))
    {
        ROCP_INFO << filename << " is being looked up via env variable ROCPROFILER_METRICS_PATH";
        return common::filesystem::path{std::string{metrics_path}} / filename;
    }
    // No environment variable, lookup via install path
    return findViaInstallPath(filename);
}

}  // namespace

MetricMap
getDerivedHardwareMetrics()
{
    auto counters_path = findViaEnvironment("derived_counters.xml");
    ROCP_FATAL_IF(!common::filesystem::exists(counters_path))
        << "metric xml file '" << counters_path << "' does not exist";
    return loadXml(counters_path);
}

MetricMap
getBaseHardwareMetrics()
{
    auto counters_path = findViaEnvironment("basic_counters.xml");
    ROCP_FATAL_IF(!common::filesystem::exists(counters_path))
        << "metric xml file '" << counters_path << "' does not exist";
    return loadXml(counters_path, true);
}

const MetricIdMap*
getMetricIdMap()
{
    static MetricIdMap*& id_map = common::static_object<MetricIdMap>::construct([]() {
        MetricIdMap map;
        for(const auto& [_, val] : *CHECK_NOTNULL(getMetricMap()))
        {
            for(const auto& metric : val)
            {
                map.emplace(metric.id(), metric);
            }
        }
        return map;
    }());
    return id_map;
}

const MetricMap*
getMetricMap()
{
    static MetricMap*& map = common::static_object<MetricMap>::construct([]() {
        MetricMap ret = getBaseHardwareMetrics();
        for(auto& [key, val] : getDerivedHardwareMetrics())
        {
            auto [iter, inserted] = ret.emplace(key, val);
            if(!inserted)
            {
                iter->second.insert(iter->second.end(), val.begin(), val.end());
            }
        }
        return ret;
    }());
    return map;
}

std::vector<Metric>
getMetricsForAgent(const std::string& agent)
{
    const auto& map = *CHECK_NOTNULL(getMetricMap());
    if(const auto* metric_ptr = rocprofiler::common::get_val(map, agent))
    {
        return *metric_ptr;
    }

    return std::vector<Metric>{};
}

bool
checkValidMetric(const std::string& agent, const Metric& metric)
{
    static auto*& agent_to_id =
        common::static_object<std::unordered_map<std::string, std::unordered_set<uint64_t>>>::
            construct([]() -> std::unordered_map<std::string, std::unordered_set<uint64_t>> {
                std::unordered_map<std::string, std::unordered_set<uint64_t>> ret;
                const auto& map = *CHECK_NOTNULL(getMetricMap());
                for(const auto& [agent_name, metrics] : map)
                {
                    auto& id_set =
                        ret.emplace(agent_name, std::unordered_set<uint64_t>{}).first->second;
                    for(const auto& m : metrics)
                    {
                        id_set.insert(m.id());
                    }
                }
                return ret;
            }());

    const auto* agent_map = common::get_val(*agent_to_id, agent);
    return agent_map != nullptr && agent_map->count(metric.id()) > 0;
}

bool
operator<(Metric const& lhs, Metric const& rhs)
{
    return lhs.id() < rhs.id();
}

bool
operator==(Metric const& lhs, Metric const& rhs)
{
    auto get_tie = [](auto& x) {
        return std::tie(x.name_,
                        x.block_,
                        x.event_,
                        x.description_,
                        x.expression_,
                        x.special_,
                        x.id_,
                        x.empty_);
    };
    return get_tie(lhs) == get_tie(rhs);
}
}  // namespace counters
}  // namespace rocprofiler
