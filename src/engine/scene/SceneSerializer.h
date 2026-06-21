#pragma once

#include "core/guidgenerator/GuidGenerator.h"

namespace Unnamed {
	class Scene;
	class JsonReader;
	class JsonWriter;

	class SceneSerializer {
	public:
		static bool LoadFromFile(
			Scene& scene, Path path, GuidGenerator& guidGen
		);
		static bool SaveToFile(const Scene& scene, const Path& path);

		static bool Deserialize(
			Scene& scene, const JsonReader& root, GuidGenerator& guidGen
		);
		static void Serialize(const Scene& scene, JsonWriter& writer);

		static bool CloneScene(
			const Scene& src, Scene& dst, GuidGenerator& guidGen
		);
	};
}
