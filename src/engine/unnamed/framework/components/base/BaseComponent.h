#pragma once
#include <cstdint>
#include <string_view>

namespace Unnamed {
	class World;
	class Scene;
	class Entity;
	class ConsoleSystem;
	class InputSystem;
	class AssetManager;
	class IDemoService;
	class AudioSystem;
	class JsonWriter;
	class JsonReader;
	struct SceneDeserializeContext;

	/// @brief Component は Entity に取り付けられるオブジェクトです。
	/// @details 取り付けられた Entity にどのように振る舞わせるかを定義します。
	class BaseComponent {
	public:
		// Tickの実行順序
		enum class TICK_GROUP : uint8_t {
			EARLY = 0, // 物理演算の前に処理したいものをここに入れます。
			KINEMATIC_SOURCE = 1, // 物理演算の前に、Kinematicなオブジェクトの移動をここで反映させます。
			COLLIDER_SYNC = 2, // 物理演算の前に、コライダーの状態をTransformから同期させる処理をここに入れます。
			GAMEPLAY = 3, // ほとんどのゲーム処理はここで行います。(デフォルト)
			LATE = 4, // 物理演算の後に処理したいものをここに入れます。
		};

		explicit BaseComponent();
		virtual  ~BaseComponent();

		/// @brief コンポーネントがエンティティに取り付けられたときに呼び出されます。
		virtual void OnAttached();
		/// @brief コンポーネントがエンティティから取り外されたときに呼び出されます。
		virtual void OnDetached();

		/// @brief 物理演算の前に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		virtual void PrePhysicsTick(float deltaTime);
		/// @brief 毎フレーム呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		virtual void OnTick(float deltaTime);
		/// @brief 固定シミュレーション前の入力反映フェーズで呼び出されます。
		/// @param frameDeltaTime 描画フレームの経過時間 [秒]
		virtual void OnFrameInputTick(float frameDeltaTime);
		/// @brief 描画フレームで見た目更新を行う際に呼び出されます。
		/// @param renderDeltaTime 描画フレームの経過時間 [秒]
		/// @param interpolationAlpha 固定ティック補間係数 [0..1]
		virtual void OnRenderTick(
			float renderDeltaTime, float interpolationAlpha
		);
		/// @brief 物理演算の後に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		virtual void PostPhysicsTick(float deltaTime);

		/// @brief レンダリングの前に呼び出されます。
		virtual void OnPreRender() const;
		/// @brief レンダリング時に呼び出されます。
		virtual void OnRender() const;
		/// @brief レンダリングの後に呼び出されます。
		virtual void OnPostRender() const;

		/// @brief エディターのティック時に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		virtual void OnEditorTick(float deltaTime);
		/// @brief エディターのレンダリング時に呼び出されます。
		virtual void OnEditorRender() const;

		/// @brief コンポーネントの安定した名前を取得します。オーバーライド必須
		[[nodiscard]] virtual std::string_view GetStableName() const = 0;

		/// @brief エディタでの表示やコンソールのログに使用されます。オーバーライド必須
		[[nodiscard]] virtual std::string_view GetComponentName() const = 0;

		/// @brief エディタのアイコン（フォントアイコン）を取得します。オーバーライド任意
		[[nodiscard]] virtual uint32_t GetIcon() const;

		/// @brief エディタのインスペクターでこのコンポーネントのプロパティを描画します。
		/// @details ImGui呼び出しは実装側で `_DEBUG`/`UNNAMED_WITH_EDITOR` を必ず確認してください。
		virtual void DrawInspectorImGui();

		/// @brief コンポーネントの値を読み込む際に使用されます。オーバーライド必須
		virtual void Deserialize(const JsonReader& reader) = 0;

		/// @brief シーン読込文脈を使用してコンポーネントの値を読み込みます。
		/// @return 読込を継続できる場合は true、シーン読込を失敗させる場合は false。
		[[nodiscard]] virtual bool Deserialize(
			const JsonReader& reader, const SceneDeserializeContext& context
		);

		/// @brief コンポーネントの値を書き込む際に使用されます。オーバーライド必須
		virtual void Serialize(JsonWriter& writer) const = 0;

		/// @brief このコンポーネントを所有しているエンティティを取得します。
		/// @return 所有しているエンティティのポインタ
		[[nodiscard]] Entity* GetOwner() const;

		/// @brief このコンポーネントを所有しているエンティティを設定します。
		/// @param owner 所有するエンティティのポインタ
		void SetOwner(Entity* owner);

		/// @brief このコンポーネントが所属するシーンを取得します。
		/// @return 所属するシーンのポインタ
		[[nodiscard]] Scene* GetScene() const noexcept;

		/// @brief このコンポーネントが所属するワールドを取得します。
		/// @return 所属するワールドのポインタ
		[[nodiscard]] World* GetWorld() const noexcept;

		/// @brief このコンポーネントに紐づく ConsoleSystem を取得します。
		/// @return ConsoleSystem へのポインタ。未設定時は nullptr
		[[nodiscard]] ConsoleSystem* GetConsoleSystem() const noexcept;

		/// @brief このコンポーネントに紐づく InputSystem を取得します。
		/// @return InputSystem へのポインタ。未設定時は nullptr
		[[nodiscard]] InputSystem* GetInputSystem() const noexcept;

		/// @brief このコンポーネントに紐づく AssetManager を取得します。
		/// @return AssetManager へのポインタ。未設定時は nullptr
		[[nodiscard]] AssetManager* GetAssetManager() const noexcept;

		/// @brief このコンポーネントに紐づく DemoService を取得します。
		/// @return DemoService へのポインタ。未設定時は nullptr
		[[nodiscard]] IDemoService* GetDemoService() const noexcept;

		/// @brief このコンポーネントに紐づく AudioSystem を取得します。
		/// @return AudioSystem へのポインタ。未設定時は nullptr
		[[nodiscard]] AudioSystem* GetAudioSystem() const noexcept;

		/// @brief コンポーネントがアクティブかどうかを取得します。
		/// @return アクティブな場合は true、非アクティブな場合は false
		[[nodiscard]] bool IsActive() const noexcept;

		/// @brief コンポーネントをアクティブまたは非アクティブに設定します。
		/// @param isActive アクティブにする場合は true、非アクティブにする場合は false
		void SetActive(bool isActive) noexcept;

		/// @brief コンポーネントの一意の識別子 (GUID) を取得します。
		/// @return コンポーネントの GUID
		[[nodiscard]] uint64_t GetGuid() const noexcept;

		/// @brief コンポーネントの一意の識別子 (GUID) を設定します。
		/// @param guid コンポーネントの新しい GUID
		void SetGuid(uint64_t guid) noexcept;

		/// @brief コンポーネントの型識別子を取得します。
		/// @return コンポーネントの型識別子
		[[nodiscard]] uint64_t GetTypeId() const;

		/// @brief Tick実行グループを取得します。
		/// @return Tickグループ
		[[nodiscard]] virtual TICK_GROUP GetTickGroup() const;

	protected:
		Entity*  mOwner = nullptr; // 所有しているエンティティ
		uint64_t mGuid  = 0;       // GUID

		bool mIsActive = true; // アクティブか?
	};
}
