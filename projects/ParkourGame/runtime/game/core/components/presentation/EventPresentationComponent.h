#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/assets/AssetID.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/world/GameplayCueBus.h"
#include "game/core/presentation/EventPresentationTypes.h"

namespace Unnamed {
	class ComponentRegistry;
	class JsonReader;
	class JsonWriter;
	class AudioFxControllerComponent;
	class CameraFxControllerComponent;
	class SkeletalAnimationComponent;
#ifdef _DEBUG
	struct EventPresentationGraphEditorState;
#endif

	/// @brief GameplayCue に反応して高レベル Action を実行する v2 プレゼンテーションコンポーネントです。
	class EventPresentationComponent final : public BaseComponent {
	public:
		/// @brief コンポーネントがワールドへ接続されたときの初期化処理です。
		void OnAttached() override;

		/// @brief コンポーネントがワールドから切り離されるときの終了処理です。
		void OnDetached() override;

		/// @brief レンダーティックでアセット更新とターゲット再解決を行います。
		/// @param renderDeltaTime レンダー更新の秒単位デルタ時間
		/// @param interpolationAlpha 補間係数
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;

		/// @brief コンポーネントの安定名を取得します。
		/// @return 安定名
		[[nodiscard]] std::string_view GetStableName() const override;

		/// @brief コンポーネント表示名を取得します。
		/// @return 表示名
		[[nodiscard]] std::string_view GetComponentName() const override;

		/// @brief エディタ表示アイコンを取得します。
		/// @return Material icon codepoint
		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		/// @brief デバッグ用の Inspector UI を描画します。
		void DrawInspectorImGui() override;

		/// @brief cueSourceEntityGuid と主要 publisher の owner GUID 整合を監査します。
		void AuditSourceGuidBindings() const;

		/// @brief EventPresentation 専用グラフエディタウィンドウを描画します。
		void DrawGraphEditorWindow();
#endif

		/// @brief JSON から設定を復元します。
		/// @param reader JSON リーダー
		void Deserialize(const JsonReader& reader) override;

		/// @brief JSON へ設定を保存します。
		/// @param writer JSON ライター
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief アセットパスを正規化して設定します。
		/// @param path アセットパス
		void SetAssetPath(const std::string& path);

		/// @brief EventPresentation アセットをロードして実行用データを再構築します。
		/// @return ロード成功時 true
		bool LoadAsset();

		/// @brief アセットのバージョン変化を監視し必要に応じてリロードします。
		void RefreshAssetIfNeeded();

		/// @brief Trigger に含まれる CueId へまとめて購読します。
		void SubscribeAll();

		/// @brief 現在の購読をすべて解除します。
		void UnsubscribeAll();

		/// @brief GameplayCue を受信して Trigger を評価します。
		/// @param cue 受信 Cue
		void HandleCue(const GameplayCue& cue);

		/// @brief Cue 受信元 GUID を解決します。
		/// @return 解決済み GUID
		[[nodiscard]] uint64_t ResolveCueSourceEntityGuid() const;

		/// @brief Audio ターゲットエンティティ GUID を解決します。
		/// @details 明示 GUID が 0 以外ならそれを優先し、0 の場合は Owner Entity GUID を使用します。
		/// @return 解決済み GUID（Owner 不在時は 0）
		[[nodiscard]] uint64_t ResolveAudioTargetEntityGuid() const;

		/// @brief CameraFx ターゲットエンティティ GUID を解決します。
		/// @details 明示 GUID が 0 以外ならそれを優先し、0 の場合は Owner Entity GUID を使用します。
		/// @return 解決済み GUID（Owner 不在時は 0）
		[[nodiscard]] uint64_t ResolveCameraFxTargetEntityGuid() const;

		/// @brief Animation ターゲットエンティティ GUID を解決します。
		/// @details 明示 GUID が 0 以外ならそれを優先し、0 の場合は Owner Entity GUID を使用します。
		/// @return 解決済み GUID（Owner 不在時は 0）
		[[nodiscard]] uint64_t ResolveAnimationTargetEntityGuid() const;

		/// @brief AudioFxController を解決します。
		/// @return 解決結果（失敗時は nullptr）
		[[nodiscard]] AudioFxControllerComponent* ResolveAudioFx();

		/// @brief CameraFxController を解決します。
		/// @return 解決結果（失敗時は nullptr）
		[[nodiscard]] CameraFxControllerComponent* ResolveCameraFx();

		/// @brief SkeletalAnimation を解決します。
		/// @return 解決結果（失敗時は nullptr）
		[[nodiscard]] SkeletalAnimationComponent* ResolveAnimation();

		std::string                      mAssetPath;
		AssetID                          mAssetId = kInvalidAssetID;
		uint64_t                         mLoadedAssetVersion = 0;
		std::string                      mLoadedAssetName;
		std::vector<EventPresentationTrigger> mTriggers;
		std::vector<GameplayCueBus::Handle> mCueHandles;

		/// @brief Cue 購読フィルタの source GUID です。0 の場合は Owner GUID を利用します。
		uint64_t mCueSourceEntityGuid = 0;
		/// @brief Audio Action の対象 GUID です。0 の場合は Owner GUID を利用します。
		uint64_t mAudioFxEntityGuid = 0;
		/// @brief Camera Action の対象 GUID です。0 の場合は Owner GUID を利用します。
		uint64_t mCameraFxEntityGuid = 0;
		/// @brief Animation Action の対象 GUID です。0 の場合は Owner GUID を利用します。
		uint64_t mAnimationEntityGuid = 0;

		AudioFxControllerComponent*  mAudioFx = nullptr;
		CameraFxControllerComponent* mCameraFx = nullptr;
		SkeletalAnimationComponent*  mAnimation = nullptr;

		float mElapsedSeconds = 0.0f;
		bool  mVerboseLog     = false;

#ifdef _DEBUG
		std::string mDebugPublishCueId = "movement.land";
		float       mDebugPublishValue = 1.0f;
		float       mDebugPublishValue2 = 400.0f;
		uint64_t    mDebugHandledCueCount = 0;
		std::unique_ptr<EventPresentationGraphEditorState> mGraphEditorState;
#endif
	};

	/// @brief EventPresentationComponent の明示登録ヘルパーです。
	/// @details 前方宣言型を含む実装都合のため、この関数経由で登録します。
	void RegisterEventPresentationComponent(ComponentRegistry& componentRegistry);
}
