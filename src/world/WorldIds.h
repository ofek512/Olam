#pragma once

#include "core/types/StrongId.h"

namespace olam
{

    struct RiverTag;
    struct LakeTag;
    struct DepositTag;

    using RiverId = StrongId<RiverTag>;
    using LakeId = StrongId<LakeTag>;
    using DepositId = StrongId<DepositTag>;

} // namespace olam
