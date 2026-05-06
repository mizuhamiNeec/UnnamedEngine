#pragma once

#include <functional>
#include <map>
#include <string>

#include <json.hpp>

namespace Unnamed {
	class BaseComponent;
	class Entity;

	class ReplaySerializerRegistry {
	public:
		using WriteStateFn = std::function<void(
			const BaseComponent& component,
			nlohmann::json&      outState
		)>;
		using ReadStateFn = std::function<void(
			BaseComponent&         component,
			const nlohmann::json&  inState
		)>;
		using HashStateFn = std::function<uint64_t(const BaseComponent& component)>;

		struct ComponentSerializer {
			std::string stableName;
			WriteStateFn writeState = {};
			ReadStateFn  readState  = {};
			HashStateFn  hashState  = {};
		};

		void RegisterComponentSerializer(ComponentSerializer serializer);

		[[nodiscard]] bool SerializeEntity(
			const Entity&      entity,
			nlohmann::json&    outState,
			uint64_t&          outHash
		) const;

		void DeserializeEntity(
			Entity&                entity,
			const nlohmann::json&  inState
		) const;

		[[nodiscard]] static ReplaySerializerRegistry BuildDefault();

	private:
		[[nodiscard]] const BaseComponent* FindComponentByStableName(
			const Entity&       entity,
			const std::string&  stableName
		) const;

		[[nodiscard]] BaseComponent* FindComponentByStableName(
			Entity&             entity,
			const std::string&  stableName
		) const;

		std::map<std::string, ComponentSerializer, std::less<>> mSerializers;
	};
}
