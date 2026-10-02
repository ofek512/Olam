#pragma once

#include "core/containers/Layer.h"
#include "core/serialization/ByteWriter.h"

#include <cstdint>

namespace olam
{

    // Hash of a layer's dimensions and element values; identical on every platform.
    template <typename T>
    std::uint64_t hashLayer(const Layer<T> &layer)
    {
        ByteWriter writer;
        writer.reserve(8 + layer.size() * sizeof(T));
        writer.write(static_cast<std::int32_t>(layer.width()));
        writer.write(static_cast<std::int32_t>(layer.height()));
        for (const T &value : layer.values())
            writer.write(value);
        return writer.hash();
    }

} // namespace olam
