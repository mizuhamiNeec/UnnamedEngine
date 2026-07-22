#include "MeshAssetData.h"

#include <algorithm>

namespace Unnamed {
	uint32_t ComputeRequiredMaterialSlotCount(const MeshAssetData& meshAsset) {
		if (meshAsset.submeshes.empty()) {
			return 1;
		}

		uint32_t maxMaterialIndex = 0;
		for (const auto& submesh : meshAsset.submeshes) {
			maxMaterialIndex = std::max(maxMaterialIndex, submesh.materialIndex);
		}

		return maxMaterialIndex + 1;
	}
}
