#pragma once
#include <memory>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../components/base/BaseComponent.h"

#include "core/hash/StableHashBuilder.h"

namespace Unnamed {
	class World;
	class Scene;

	/// @class Entity
	/// @brief エンティティはゲームの基本オブジェクトです。
	/// ゲーム内に登場したり、ロジックを持つオブジェクトを表します。
	/// "ECSとは一切関係ありません"
	class Entity {
	public:
		/// @brief コンストラクタ
		/// @param name エンティティの名前
		/// @param guid エンティティのGUID
		/// @param isEditorOnly エディター専用か?
		explicit Entity(
			const std::string_view& name, uint64_t guid,
			bool                    isEditorOnly = false
		);

		~Entity();

		/// @brief エンティティが登録されたときに呼び出されます。
		void OnRegister();

		/// @brief エンティティが登録された後に呼び出されます。
		void PostRegister();

		/// @brief 物理演算の前に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void                   PrePhysicsTick(float deltaTime) const;
		[[nodiscard]] uint32_t PrePhysicsTick(
			float                     deltaTime,
			BaseComponent::TICK_GROUP group
		) const;

		/// @brief 毎フレーム呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void                   Tick(float deltaTime) const;
		[[nodiscard]] uint32_t Tick(
			float                     deltaTime,
			BaseComponent::TICK_GROUP group
		) const;

		/// @brief 固定シミュレーション前の入力反映フェーズで呼び出されます。
		/// @param frameDeltaTime 描画フレームの経過時間（秒）
		/// @return 呼び出したコンポーネントの数
		[[nodiscard]] uint32_t FrameInputTick(float frameDeltaTime) const;

		/// @brief 物理演算の後に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void PostPhysicsTick(float deltaTime) const;

		/// @brief 指定したTickグループのコンポーネントのPostPhysicsTickを呼び出します。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		/// @param group 対象のTickグループ
		/// @return 呼び出したコンポーネントの数
		[[nodiscard]] uint32_t PostPhysicsTick(
			float                     deltaTime,
			BaseComponent::TICK_GROUP group
		) const;

		/// @brief 描画フレームで見た目更新を行う際に呼び出されます。
		/// @param renderDeltaTime 描画フレームの経過時間 [秒]
		/// @param interpolationAlpha 固定ティック補間係数 [0..1]
		/// @return 呼び出したコンポーネントの数
		[[nodiscard]] uint32_t RenderTick(
			float renderDeltaTime,
			float interpolationAlpha
		) const;

		/// @brief レンダリングの前に呼び出されます。
		void OnPreRender() const;
		/// @brief レンダリング時に呼び出されます。
		void OnRender() const;
		/// @brief レンダリングの後に呼び出されます。
		void OnPostRender() const;

		/// @brief エディターのティック時に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void OnEditorTick(float deltaTime) const;
		/// @brief エディターのレンダリング時に呼び出されます。
		void OnEditorRender() const;

		/// @brief エンティティが破棄されるときに呼び出されます。
		void OnDestroy();

		/// @brief エンティティが破棄される予定かどうかを取得します。
		/// @return 破棄される予定であればtrue、そうでなければfalse
		[[nodiscard]] bool IsPendingDestroy() const noexcept;

		/// @brief エンティティを破棄する予定であることをマークします。
		/// 破棄はシーンの更新ループの最後に行われるため、安全にエンティティを削除できます。
		void MarkPendingDestroy() noexcept;

		/// @brief コンポーネントを追加します。
		/// @tparam ComponentType 追加するコンポーネントの型
		/// @tparam Args コンポーネントのコンストラクタに渡す引数の型
		/// @param args コンポーネントのコンストラクタに渡す引数
		/// @return 追加されたコンポーネントのポインタ
		template <typename ComponentType, typename... Args>
		ComponentType* AddComponent(Args&&... args);

		/// @brief コンポーネントインスタンスを追加します。
		/// @param component 追加するコンポーネントのユニークポインタ
		/// @return 追加されたコンポーネントのポインタ
		BaseComponent* AddComponentInstance(
			std::unique_ptr<BaseComponent> component
		);

		/// @brief 指定した型のコンポーネントを取得します。
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @return 取得したコンポーネントのポインタ。存在しない場合は nullptr。
		template <typename ComponentType>
		[[nodiscard]] ComponentType* GetComponent();

		/// @brief 指定した型のコンポーネントを取得します。(const版)
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @return 取得したコンポーネントのポインタ。存在しない場合は nullptr。
		template <typename ComponentType>
		[[nodiscard]] const ComponentType* GetComponent() const;

		/// @brief 指定した型のすべてのコンポーネントを取得します。
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @param out 取得したコンポーネントのポインタを格納するベクター
		template <typename ComponentType>
		void GetComponents(std::vector<ComponentType*>& out);

		/// @brief 指定した型のすべてのコンポーネントを取得します。(const版)
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @param out 取得したコンポーネントのポインタを格納するベクター
		template <typename ComponentType>
		void GetComponents(std::vector<const ComponentType*>& out) const;

		/// @brief 指定した型のコンポーネントを取得するか、存在しない場合は新たに追加します。
		/// @tparam ComponentType 取得または追加するコンポーネントの型
		/// @tparam Args コンポーネントのコンストラクタに渡す引数の型
		/// @param args コンポーネントのコンストラクタに渡す引数
		/// @return 取得または追加されたコンポーネントのポインタ
		template <typename ComponentType, typename... Args>
		[[nodiscard]] ComponentType* GetOrAddComponent(Args&&... args);

		/// @brief 指定した型のコンポーネントを検索します。
		/// @tparam ComponentType 検索するコンポーネントの型
		/// @return 見つかったコンポーネントのポインタ。存在しない場合は nullptr。
		template <typename ComponentType>
		[[nodiscard]] ComponentType* FindComponentByBase();

		/// @brief 指定した型のコンポーネントを検索します。(const版)
		/// @tparam ComponentType 検索するコンポーネントの型
		/// @return 見つかったコンポーネントのポインタ。存在しない場合は nullptr。
		template <typename ComponentType>
		[[nodiscard]] const ComponentType* FindComponentByBase() const;

		/// @brief 指定した型のすべてのコンポーネントを検索します。
		/// @tparam ComponentType 検索するコンポーネントの型
		/// @param out 見つかったコンポーネントのポインタを格納するベクター
		template <typename ComponentType>
		void FindComponentsByBase(std::vector<ComponentType*>& out);

		/// @brief 指定した型のすべてのコンポーネントを検索します。(const版)
		/// @tparam ComponentType 検索するコンポーネントの型
		/// @param out 見つかったコンポーネントのポインタを格納するベクター
		template <typename ComponentType>
		void FindComponentsByBase(std::vector<const ComponentType*>& out) const;

		/// @brief コンポーネントを削除します。
		/// @param component 削除するコンポーネントのポインタ
		void               RemoveComponent(BaseComponent* component);
		[[nodiscard]] bool MoveComponentUp(BaseComponent* component);
		[[nodiscard]] bool MoveComponentDown(BaseComponent* component);

		/// @brief エンティティが所属するシーンを取得します。
		/// @return 所属するシーンのポインタ。存在しない場合は nullptr。
		[[nodiscard]] Scene* GetScene() const noexcept;

		/// @brief エンティティが所属するシーンを設定します。
		/// @param scene 所属するシーンのポインタ
		void SetScene(Scene* scene) noexcept;

		/// @brief エンティティが所属するワールドを取得します。
		/// @return 所属するワールドのポインタ。存在しない場合は nullptr。
		[[nodiscard]] World* GetWorld() const noexcept;

		/// @brief エンティティの名前を取得します。
		/// @return エンティティの名前
		[[nodiscard]] std::string_view GetName() const;

		/// @brief エンティティの名前を設定します。
		/// @param name エンティティの新しい名前
		void SetName(const std::string_view& name);

		/// @brief エンティティがエディター専用かどうかを取得します。
		/// @return エディター専用の場合は true、そうでない場合は false
		[[nodiscard]] bool IsEditorOnly() const noexcept;

		/// @brief エンティティがアクティブかどうかを取得します。
		/// @return アクティブな場合は true、非アクティブな場合は false
		[[nodiscard]] bool IsActive() const noexcept;

		/// @brief エンティティをアクティブまたは非アクティブに設定します。
		/// @param isActive アクティブにする場合は true、非アクティブにする場合は false
		void               SetActive(bool isActive) noexcept;
		[[nodiscard]] bool IsVisible() const noexcept;
		void               SetVisible(bool isVisible) noexcept;

		/// @brief エンティティのGUIDを取得します。
		/// @return エンティティのGUID
		[[nodiscard]] uint64_t GetGuid() const noexcept;

		[[nodiscard]] std::string_view GetFolderPath() const noexcept;
		void SetFolderPath(std::string_view folderPath);

		/// @brief 全てのコンポーネントに対して関数を実行します。
		/// @tparam Func 呼び出し可能オブジェクト型（`void(BaseComponent&)` 等）
		/// @param func 各コンポーネントに対して呼ばれる関数
		/// @return 全て走査した場合 true。途中で中断された場合 false。
		template <typename Func>
		bool ForEachComponent(Func&& func);

		/// @brief 全てのコンポーネントに対して関数を実行します。(const版)
		/// @tparam Func 呼び出し可能オブジェクト型（`void(const BaseComponent&)` 等）
		/// @param func 各コンポーネントに対して呼ばれる関数
		/// @return 全て走査した場合 true。途中で中断された場合 false。
		template <typename Func>
		bool ForEachComponent(Func&& func) const;

		/// @brief 指定した型の全てのコンポーネントに対して関数を実行します。
		/// @tparam ComponentType 対象コンポーネント型
		/// @tparam Func 呼び出し可能オブジェクト型（`void(ComponentType&)` 等）
		/// @param func 各コンポーネントに対して呼ばれる関数
		/// @return 全て走査した場合 true。途中で中断された場合 false。
		template <typename ComponentType, typename Func>
		bool ForEachComponent(Func&& func);

		/// @brief 指定した型の全てのコンポーネントに対して関数を実行します。(const版)
		/// @tparam ComponentType 対象コンポーネント型
		/// @tparam Func 呼び出し可能オブジェクト型（`void(const ComponentType&)` 等）
		/// @param func 各コンポーネントに対して呼ばれる関数
		/// @return 全て走査した場合 true。途中で中断された場合 false。
		template <typename ComponentType, typename Func>
		bool ForEachComponent(Func&& func) const;

	protected:
		void RebuildComponentTypeIndex();

		// 所有しているコンポーネント
		std::vector<std::unique_ptr<BaseComponent>> mComponents;

		std::unordered_map<uint64_t, std::vector<BaseComponent*>>
		mComponentsByType;

		std::string mName = "unnamed";         // 名前
		std::string mFolderPath;               // エディタ階層上のフォルダパス
		Scene*      mScene          = nullptr; // 所属しているシーン
		uint64_t    mGuid           = 0;       // GUID
		bool        mIsEditorOnly   = false;   // エディター専用か?
		bool        mIsActive       = true;    // アクティブか?
		bool        mIsVisible      = true;    // レンダリング上の可視状態
		bool        mDestroyed      = false;   // OnDestroy二重呼び出し防止
		bool        mPendingDestroy = false;   // 破棄予定か?
	};

	template <typename ComponentType, typename... Args>
	ComponentType* Entity::AddComponent(Args&&... args) {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"T は BaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		auto ptr = std::make_unique<ComponentType>(
			std::forward<Args>(args)...
		);

		auto* raw = ptr.get();
		raw->SetOwner(this);

		// 先に所有へ入れる
		mComponents.emplace_back(std::move(ptr));

		// 型索引へ登録
		StableHashBuilder hashBuilder;
		hashBuilder.AddString(ComponentType{}.GetStableName());
		const uint64_t typeId = hashBuilder.Value();
		mComponentsByType[typeId].emplace_back(raw);

		raw->OnAttached();
		return raw;
	}

	template <typename ComponentType>
	ComponentType* Entity::GetComponent() {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"ComponentType は BaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		StableHashBuilder hashBuilder;
		hashBuilder.AddString(ComponentType{}.GetStableName());
		const uint64_t typeId = hashBuilder.Value();

		const auto it = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end() || it->second.empty()) {
			return nullptr;
		}
		return static_cast<ComponentType*>(it->second.front());
	}

	template <typename ComponentType>
	const ComponentType* Entity::GetComponent() const {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"T は BaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		StableHashBuilder hashBuilder;
		hashBuilder.AddString(ComponentType{}.GetStableName());
		const uint64_t typeId = hashBuilder.Value();

		const auto it = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end() || it->second.empty()) {
			return nullptr;
		}
		return static_cast<const ComponentType*>(it->second.front());
	}

	template <typename ComponentType>
	void Entity::GetComponents(std::vector<ComponentType*>& out) {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"T は BaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		out.clear();
		const uint64_t typeId = HashTypeName(ComponentType{}.GetStableName());
		const auto     it     = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end()) {
			return;
		}

		out.reserve(it->second.size());
		for (BaseComponent* c : it->second) {
			out.push_back(static_cast<ComponentType*>(c));
		}
	}

	template <typename ComponentType>
	void Entity::GetComponents(std::vector<const ComponentType*>& out) const {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"T は BaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		out.clear();
		const uint64_t typeId = HashTypeName(ComponentType{}.GetStableName());
		const auto     it     = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end()) {
			return;
		}

		out.reserve(it->second.size());
		for (BaseComponent* c : it->second) {
			out.push_back(static_cast<const ComponentType*>(c));
		}
	}

	template <typename ComponentType, typename... Args>
	ComponentType* Entity::GetOrAddComponent(Args&&... args) {
		if (auto* component = GetComponent<ComponentType>()) {
			return component;
		}
		return AddComponent<ComponentType>(std::forward<Args>(args)...);
	}

	template <typename ComponentType>
	ComponentType* Entity::FindComponentByBase() {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"ComponentType は BaseComponent の派生クラスでなければなりません。"
		);

		for (const auto& uptr : mComponents) {
			BaseComponent* c = uptr.get();
			if (!c) {
				continue;
			}
			if (auto* casted = dynamic_cast<ComponentType*>(c)) {
				return casted;
			}
		}
		return nullptr;
	}

	template <typename ComponentType>
	const ComponentType* Entity::FindComponentByBase() const {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"ComponentType は BaseComponent の派生クラスでなければなりません。"
		);

		for (const auto& uptr : mComponents) {
			const BaseComponent* c = uptr.get();
			if (!c) {
				continue;
			}
			if (auto* casted = dynamic_cast<const ComponentType*>(c)) {
				return casted;
			}
		}
		return nullptr;
	}

	template <typename ComponentType>
	void Entity::FindComponentsByBase(std::vector<ComponentType*>& out) {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"ComponentType は BaseComponent の派生クラスでなければなりません。"
		);

		out.clear();
		for (const auto& uptr : mComponents) {
			BaseComponent* c = uptr.get();
			if (!c) {
				continue;
			}
			if (auto* casted = dynamic_cast<ComponentType*>(c)) {
				out.push_back(casted);
			}
		}
	}

	template <typename ComponentType>
	void Entity::FindComponentsByBase(
		std::vector<const ComponentType*>& out
	) const {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"ComponentType は BaseComponent の派生クラスでなければなりません。"
		);

		out.clear();
		for (const auto& uptr : mComponents) {
			const BaseComponent* c = uptr.get();
			if (!c) {
				continue;
			}
			if (auto* casted = dynamic_cast<const ComponentType*>(c)) {
				out.push_back(casted);
			}
		}
	}

	template <typename Func>
	bool Entity::ForEachComponent(Func&& func) {
		// RemoveComponent 等で mComponents が変更される可能性に備えてスナップショットを取る
		std::vector<BaseComponent*> snapshot;
		snapshot.reserve(mComponents.size());
		for (auto& uptr : mComponents) {
			snapshot.push_back(uptr.get());
		}

		for (BaseComponent* c : snapshot) {
			if (!c) {
				continue;
			}
			if constexpr (std::is_same_v<
				std::invoke_result_t<Func, BaseComponent&>, bool>) {
				if (!std::forward<Func>(func)(*c)) {
					return false;
				}
			} else {
				std::forward<Func>(func)(*c);
			}
		}
		return true;
	}

	template <typename Func>
	bool Entity::ForEachComponent(Func&& func) const {
		std::vector<const BaseComponent*> snapshot;
		snapshot.reserve(mComponents.size());
		for (const auto& uptr : mComponents) {
			snapshot.push_back(uptr.get());
		}

		for (const BaseComponent* c : snapshot) {
			if (!c) {
				continue;
			}
			if constexpr (std::is_same_v<
				std::invoke_result_t<Func, const BaseComponent&>, bool>) {
				if (!std::forward<Func>(func)(*c)) {
					return false;
				}
			} else {
				std::forward<Func>(func)(*c);
			}
		}
		return true;
	}

	template <typename ComponentType, typename Func>
	bool Entity::ForEachComponent(Func&& func) {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"ComponentType は BaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		const uint64_t typeId = HashTypeName(ComponentType{}.GetStableName());
		const auto     it     = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end() || it->second.empty()) {
			return true;
		}

		// RemoveComponent 等で vector が変わる可能性に備えてスナップショットを取る
		const std::vector<BaseComponent*> snapshot = it->second;
		for (BaseComponent* base : snapshot) {
			if (!base) {
				continue;
			}
			auto* c = static_cast<ComponentType*>(base);
			if constexpr (std::is_same_v<
				std::invoke_result_t<Func, ComponentType&>, bool>) {
				if (!std::forward<Func>(func)(*c)) {
					return false;
				}
			} else {
				std::forward<Func>(func)(*c);
			}
		}
		return true;
	}

	template <typename ComponentType, typename Func>
	bool Entity::ForEachComponent(Func&& func) const {
		static_assert(
			std::is_base_of_v<BaseComponent, ComponentType>,
			"ComponentType は BaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		const uint64_t typeId = HashTypeName(ComponentType{}.GetStableName());
		const auto     it     = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end() || it->second.empty()) {
			return true;
		}

		const std::vector<BaseComponent*> snapshot = it->second;
		for (BaseComponent* base : snapshot) {
			if (!base) {
				continue;
			}
			const auto* c = static_cast<const ComponentType*>(base);
			if constexpr (std::is_same_v<
				std::invoke_result_t<Func, const ComponentType&>, bool>) {
				if (!std::forward<Func>(func)(*c)) {
					return false;
				}
			} else {
				std::forward<Func>(func)(*c);
			}
		}
		return true;
	}
}
