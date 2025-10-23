#include "ModelCommon.h"

/// @brief ModelCommonクラスの初期化
/// @param d3d12 D3D12レンダラーへのポインタ
void ModelCommon::Init(D3D12* d3d12) {
	this->d3d12_ = d3d12;
}
