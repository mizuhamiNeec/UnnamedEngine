#pragma once

#include "core/guidgenerator/GuidGenerator.h"
#include "SceneLoadOptions.h"

namespace Unnamed {
	class Scene;
	class JsonReader;
	class JsonWriter;

	//-------------------------------------------------------------------------
	// TODO: シーン遷移は一時的なシーン状態にデシリアライズし、検証が成功した後にのみコミットされるべき
	//-------------------------------------------------------------------------

	/// @brief SceneSerializerは、SceneのエンティティーとコンポーネントをJSONへ保存・復元し、シーン複製を仲介します
	class SceneSerializer {
	public:
		/// @brief JSON ファイルからシーンを読み込みます。
		static bool LoadFromFile(
			Scene& scene, Path path, GuidGenerator& guidGen,
			const SceneLoadOptions& options
		);
		static bool SaveToFile(const Scene& scene, const Path& path);

		/// @brief JSON からシーンを構築します。
		static bool Deserialize(
			Scene& scene, const JsonReader& root, GuidGenerator& guidGen,
			const SceneLoadOptions& options, const Path& scenePath
		);
		static void Serialize(const Scene& scene, JsonWriter& writer);

		static bool CloneScene(
			const Scene& src, Scene& dst, GuidGenerator& guidGen
		);
	};
}
