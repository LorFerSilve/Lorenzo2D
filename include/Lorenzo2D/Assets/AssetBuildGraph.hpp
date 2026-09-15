#pragma once

#include <Lorenzo2D/Assets/AssetManifest.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace l2d
{
    // CPU-only whole-project dependency graph for authoring/cook workflows.
    //
    // AssetBuildGraph consumes AssetManifest identities and never owns runtime
    // AssetManager resources. Graph construction is transactional and validates
    // that every dependency resolves inside the manifest and that the project is
    // acyclic. Rebuild planning compares manifest cook identities and then
    // invalidates transitive dependents in deterministic dependency-first order.
    class AssetBuildGraph
    {
      public:
        bool build(const AssetManifest& manifest, std::string* error = nullptr)
        {
            AssetBuildGraph candidate;
            const auto entries = manifest.entries();

            for (const auto& entry : entries)
            {
                candidate.m_dependencies.emplace(entry.source.id, entry.source.dependencies);
                candidate.m_dependents.emplace(entry.source.id, std::vector<AssetId>{});
            }

            for (const auto& entry : entries)
            {
                for (const auto& dependency : entry.source.dependencies)
                {
                    const auto dependencyNode = candidate.m_dependencies.find(dependency);
                    if (dependencyNode == candidate.m_dependencies.end())
                    {
                        return fail(error, "asset '" + entry.source.id +
                                               "' depends on missing asset '" + dependency + "'");
                    }
                    candidate.m_dependents[dependency].push_back(entry.source.id);
                }
            }

            for (auto& dependents : candidate.m_dependents)
            {
                std::sort(dependents.second.begin(), dependents.second.end());
            }

            std::vector<AssetId> topologicalOrder;
            if (!candidate.makeTopologicalOrder(topologicalOrder, error))
            {
                return false;
            }

            candidate.m_topologicalOrder = std::move(topologicalOrder);
            *this = std::move(candidate);
            clearError(error);
            return true;
        }

        void clear() noexcept
        {
            m_dependencies.clear();
            m_dependents.clear();
            m_topologicalOrder.clear();
        }

        bool contains(const AssetId& id) const noexcept
        {
            return m_dependencies.find(id) != m_dependencies.end();
        }

        std::size_t size() const noexcept
        {
            return m_dependencies.size();
        }

        std::vector<AssetId> dependenciesOf(const AssetId& id) const
        {
            const auto found = m_dependencies.find(id);
            return found == m_dependencies.end() ? std::vector<AssetId>{} : found->second;
        }

        std::vector<AssetId> dependentsOf(const AssetId& id, const bool transitive = true) const
        {
            const auto found = m_dependents.find(id);
            if (found == m_dependents.end())
            {
                return {};
            }
            if (!transitive)
            {
                return found->second;
            }

            std::set<AssetId> visited;
            std::vector<AssetId> pending = found->second;
            while (!pending.empty())
            {
                const AssetId current = pending.back();
                pending.pop_back();
                if (!visited.insert(current).second)
                {
                    continue;
                }

                const auto dependents = m_dependents.find(current);
                if (dependents != m_dependents.end())
                {
                    pending.insert(pending.end(), dependents->second.begin(), dependents->second.end());
                }
            }
            return {visited.begin(), visited.end()};
        }

        const std::vector<AssetId>& topologicalOrder() const noexcept
        {
            return m_topologicalOrder;
        }

        static bool validate(const AssetManifest& manifest, std::string* error = nullptr)
        {
            AssetBuildGraph graph;
            return graph.build(manifest, error);
        }

        static bool computeRebuildOrder(const AssetManifest& previous,
                                        const AssetManifest& current,
                                        std::vector<AssetId>& output,
                                        std::string* error = nullptr)
        {
            AssetBuildGraph previousGraph;
            std::string graphError;
            if (!previousGraph.build(previous, &graphError))
            {
                return fail(error, "previous manifest dependency graph is invalid: " + graphError);
            }

            AssetBuildGraph currentGraph;
            if (!currentGraph.build(current, &graphError))
            {
                return fail(error, "current manifest dependency graph is invalid: " + graphError);
            }

            std::set<AssetId> invalidated;
            for (const auto& currentEntry : current.entries())
            {
                const auto* previousEntry = previous.find(currentEntry.source.id);
                if (previousEntry == nullptr || previousEntry->cookKey != currentEntry.cookKey ||
                    previousEntry->cookedPath.lexically_normal() !=
                        currentEntry.cookedPath.lexically_normal())
                {
                    invalidated.insert(currentEntry.source.id);
                }
            }

            std::vector<AssetId> changed(invalidated.begin(), invalidated.end());
            for (const auto& changedAsset : changed)
            {
                const auto dependents = currentGraph.dependentsOf(changedAsset, true);
                invalidated.insert(dependents.begin(), dependents.end());
            }

            std::vector<AssetId> candidate;
            candidate.reserve(invalidated.size());
            for (const auto& id : currentGraph.topologicalOrder())
            {
                if (invalidated.find(id) != invalidated.end())
                {
                    candidate.push_back(id);
                }
            }

            output = std::move(candidate);
            clearError(error);
            return true;
        }

      private:
        static void clearError(std::string* error)
        {
            if (error != nullptr)
            {
                error->clear();
            }
        }

        static bool fail(std::string* error, const std::string& message)
        {
            if (error != nullptr)
            {
                *error = message;
            }
            return false;
        }

        bool makeTopologicalOrder(std::vector<AssetId>& output, std::string* error) const
        {
            std::map<AssetId, std::size_t> indegree;
            std::set<AssetId> ready;
            for (const auto& node : m_dependencies)
            {
                indegree.emplace(node.first, node.second.size());
                if (node.second.empty())
                {
                    ready.insert(node.first);
                }
            }

            std::vector<AssetId> order;
            order.reserve(m_dependencies.size());
            while (!ready.empty())
            {
                const AssetId current = *ready.begin();
                ready.erase(ready.begin());
                order.push_back(current);

                const auto dependents = m_dependents.find(current);
                if (dependents == m_dependents.end())
                {
                    continue;
                }
                for (const auto& dependent : dependents->second)
                {
                    auto degree = indegree.find(dependent);
                    if (degree == indegree.end() || degree->second == 0u)
                    {
                        return fail(error, "asset dependency graph contains inconsistent edges");
                    }
                    --degree->second;
                    if (degree->second == 0u)
                    {
                        ready.insert(dependent);
                    }
                }
            }

            if (order.size() != m_dependencies.size())
            {
                std::vector<AssetId> cyclic;
                for (const auto& degree : indegree)
                {
                    if (degree.second != 0u)
                    {
                        cyclic.push_back(degree.first);
                    }
                }

                std::string message = "asset dependency graph contains a cycle involving";
                for (const auto& id : cyclic)
                {
                    message += " '" + id + "'";
                }
                return fail(error, message);
            }

            output = std::move(order);
            clearError(error);
            return true;
        }

        std::map<AssetId, std::vector<AssetId>> m_dependencies;
        std::map<AssetId, std::vector<AssetId>> m_dependents;
        std::vector<AssetId> m_topologicalOrder;
    };
}
