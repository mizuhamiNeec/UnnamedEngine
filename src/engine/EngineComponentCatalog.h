#pragma once

#include <type_traits>

#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/components/SkyboxComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/audio/AudioSourceComponent.h"
#include "engine/unnamed/framework/components/collider/StaticMeshColliderComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h"
#include "engine/unnamed/framework/components/ui/NewUICanvas.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"

namespace Unnamed {
	/// @brief エンジン標準コンポーネント型を列挙します。
	/// @details 追加時はこの関数へ追記すれば登録経路へ反映されます。
	/// @tparam Fn std::type_identity<T> を受け取る呼び出し可能オブジェクト
	/// @param fn 各コンポーネント型に対して呼び出される関数オブジェクト
	template <typename Fn>
	void ForEachEngineComponentType(Fn&& fn) {
		fn(std::type_identity<TransformComponent>{});
		fn(std::type_identity<CameraComponent>{});
		fn(std::type_identity<SkyboxComponent>{});
		fn(std::type_identity<StaticMeshRendererComponent>{});
		fn(std::type_identity<StaticMeshColliderComponent>{});
		fn(std::type_identity<SkeletalMeshRendererComponent>{});
		fn(std::type_identity<SkeletalAnimationComponent>{});
		fn(std::type_identity<UiCanvasComponent>{});
		fn(std::type_identity<NewUICanvas>{});
		fn(std::type_identity<AudioSourceComponent>{});
		fn(std::type_identity<SequenceDirectorComponent>{});
	}
}
