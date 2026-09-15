#include <Lorenzo2D/Assets/AssetBuildGraph.hpp>
#include <Lorenzo2D/Assets/AssetCookCache.hpp>
#include <Lorenzo2D/Assets/AssetManifest.hpp>
#include <Lorenzo2D/Assets/AssetMetadata.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace
{
    bool makeEntry(l2d::AssetSourceDescriptor descriptor, const std::uint64_t sourceHash,
                   l2d::AssetManifestEntry& output)
    {
        l2d::AssetCookRequest request;
        if (!l2d::AssetManifest::makeCookRequest(descriptor, sourceHash, "consumer-v1", request))
        {
            return false;
        }

        output.source = request.source;
        output.sourceContentHash = request.sourceContentHash;
        output.importerVersion = request.importerVersion;
        output.cookKey = request.cookKey;
        output.cookedPath = std::filesystem::path("cooked") / (request.source.id + ".asset");
        return true;
    }

    l2d::AssetSourceDescriptor descriptor(std::string id, std::vector<l2d::AssetId> dependencies)
    {
        l2d::AssetSourceDescriptor result;
        result.id = std::move(id);
        result.kind = l2d::AssetSourceKind::Texture;
        result.sourcePath = std::filesystem::path("sources") / (result.id + ".png");
        result.importer = "consumer-test";
        result.importSettings = {{"smooth", "false"}};
        result.dependencies = std::move(dependencies);
        return result;
    }
}

int main()
{
    const auto leafDescriptor = descriptor("textures/consumer/leaf", {});

    l2d::AssetMetadataRegistry registry;
    if (!registry.upsert(leafDescriptor))
    {
        return 1;
    }

    const auto* stored = registry.find(leafDescriptor.id);
    if (stored == nullptr ||
        stored->sourcePath != std::filesystem::path("sources/textures/consumer/leaf.png"))
    {
        return 2;
    }

    const auto middleDescriptor =
        descriptor("materials/consumer/middle", {"textures/consumer/leaf"});
    const auto rootDescriptor = descriptor("prefabs/consumer/root", {"materials/consumer/middle"});

    l2d::AssetManifestEntry leaf;
    l2d::AssetManifestEntry middle;
    l2d::AssetManifestEntry root;
    if (!makeEntry(leafDescriptor, 42u, leaf) || !makeEntry(middleDescriptor, 43u, middle) ||
        !makeEntry(rootDescriptor, 44u, root))
    {
        return 3;
    }

    l2d::AssetManifest manifest;
    if (!manifest.upsert(root) || !manifest.upsert(leaf) || !manifest.upsert(middle))
    {
        return 4;
    }

    const std::string serialized = manifest.serialize();
    l2d::AssetManifest restored;
    if (!restored.deserialize(serialized) || restored.serialize() != serialized)
    {
        return 5;
    }

    l2d::AssetBuildGraph graph;
    std::string error;
    if (!graph.build(restored, &error) || !error.empty() || graph.size() != 3u)
    {
        return 6;
    }

    const std::vector<l2d::AssetId> expectedOrder = {
        "textures/consumer/leaf", "materials/consumer/middle", "prefabs/consumer/root"};
    if (graph.topologicalOrder() != expectedOrder ||
        graph.dependentsOf("textures/consumer/leaf", true) !=
            std::vector<l2d::AssetId>({"materials/consumer/middle", "prefabs/consumer/root"}))
    {
        return 7;
    }

    l2d::AssetManifest missingDependencyManifest;
    auto missingDescriptor = descriptor("textures/consumer/missing-user", {"textures/not-present"});
    l2d::AssetManifestEntry missingEntry;
    if (!makeEntry(missingDescriptor, 50u, missingEntry) ||
        !missingDependencyManifest.upsert(missingEntry) ||
        graph.build(missingDependencyManifest, &error) || error.empty() || graph.size() != 3u)
    {
        return 8;
    }

    l2d::AssetManifest cycleManifest;
    auto cycleA = descriptor("cycle/a", {"cycle/b"});
    auto cycleB = descriptor("cycle/b", {"cycle/a"});
    l2d::AssetManifestEntry cycleAEntry;
    l2d::AssetManifestEntry cycleBEntry;
    if (!makeEntry(cycleA, 60u, cycleAEntry) || !makeEntry(cycleB, 61u, cycleBEntry) ||
        !cycleManifest.upsert(cycleAEntry) || !cycleManifest.upsert(cycleBEntry) ||
        l2d::AssetBuildGraph::validate(cycleManifest, &error) || error.empty())
    {
        return 9;
    }

    l2d::AssetManifest current = restored;
    l2d::AssetManifestEntry changedLeaf;
    if (!makeEntry(leafDescriptor, 420u, changedLeaf) || !current.upsert(changedLeaf))
    {
        return 10;
    }

    std::vector<l2d::AssetId> rebuildOrder = {"sentinel"};
    if (!l2d::AssetBuildGraph::computeRebuildOrder(restored, current, rebuildOrder, &error) ||
        !error.empty() || rebuildOrder != expectedOrder)
    {
        return 11;
    }

    rebuildOrder = {"sentinel"};
    if (l2d::AssetBuildGraph::computeRebuildOrder(restored, cycleManifest, rebuildOrder, &error) ||
        error.empty() || rebuildOrder != std::vector<l2d::AssetId>({"sentinel"}))
    {
        return 12;
    }

    l2d::AssetCookCache cache;
    std::vector<l2d::AssetId> callbackOrder;
    l2d::AssetCookExecutionResult execution;
    const auto cooker = [&callbackOrder](const l2d::AssetCookRequest& request,
                                         const std::filesystem::path& cookedPath,
                                         std::string*)
    {
        callbackOrder.push_back(request.source.id);
        return !cookedPath.empty();
    };

    if (!l2d::AssetCookExecutor::execute(restored, current, cache, 2u, cooker, execution, &error) ||
        !error.empty() ||
        execution.cooked !=
            std::vector<l2d::AssetId>({"textures/consumer/leaf", "materials/consumer/middle"}) ||
        execution.remaining != std::vector<l2d::AssetId>({"prefabs/consumer/root"}) ||
        callbackOrder != execution.cooked || cache.size() != 2u)
    {
        return 13;
    }

    l2d::AssetCookExecutionResult resumed;
    if (!l2d::AssetCookExecutor::execute(restored, current, cache, 2u, cooker, resumed, &error) ||
        resumed.cooked != std::vector<l2d::AssetId>({"prefabs/consumer/root"}) ||
        !resumed.remaining.empty() || cache.size() != 3u)
    {
        return 14;
    }

    const std::string cacheDocument = cache.serialize();
    l2d::AssetCookCache restoredCache;
    if (!restoredCache.deserialize(cacheDocument, &error) ||
        restoredCache.serialize() != cacheDocument ||
        !restoredCache.contains(changedLeaf.source.id, changedLeaf.cookKey))
    {
        return 15;
    }

    const std::string preservedCache = restoredCache.serialize();
    if (restoredCache.deserialize("broken", &error) || error.empty() ||
        restoredCache.serialize() != preservedCache)
    {
        return 16;
    }

    l2d::AssetCookExecutionResult failed = {{"sentinel"}, {"sentinel"}};
    const auto failingCooker = [](const l2d::AssetCookRequest&, const std::filesystem::path&,
                                  std::string* cookError)
    {
        if (cookError != nullptr)
        {
            *cookError = "expected failure";
        }
        return false;
    };
    l2d::AssetCookCache emptyCache;
    if (l2d::AssetCookExecutor::execute(restored, current, emptyCache, 3u, failingCooker, failed,
                                        &error) ||
        error.empty() || emptyCache.size() != 0u ||
        failed.cooked != std::vector<l2d::AssetId>({"sentinel"}))
    {
        return 17;
    }

    const auto snapshot = registry.descriptors();
    return snapshot.size() == 1u && snapshot.front().id == leafDescriptor.id &&
                   restored.contains(rootDescriptor.id)
               ? 0
               : 18;
}
