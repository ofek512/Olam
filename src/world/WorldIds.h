#pragma once

#include "core/types/StrongId.h"

namespace olam
{

    struct RiverTag;
    struct LakeTag;

    using RiverId = StrongId<RiverTag>;
    using LakeId = StrongId<LakeTag>;

} // namespace olam
