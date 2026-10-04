#pragma once

#include "core/types/StrongId.h"

namespace olam
{

    struct RiverTag;
    struct LakeTag;
    struct DepositTag;
    struct WatershedTag;

    using RiverId = StrongId<RiverTag>;
    using LakeId = StrongId<LakeTag>;
    using DepositId = StrongId<DepositTag>;
    using WatershedId = StrongId<WatershedTag>;

} // namespace olam
