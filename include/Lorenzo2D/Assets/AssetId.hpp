#pragma once

#include <string>

namespace l2d
{
    // Stable serialized identifier. Runtime asset handles are resolved only
    // when content is instantiated.
    using AssetId = std::string;
}
