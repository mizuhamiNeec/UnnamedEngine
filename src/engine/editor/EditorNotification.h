#pragma once
#ifdef _DEBUG
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/tween/TweenManager.h"

namespace Unnamed {
	class ConCommand;

	enum class NOTIFY_TYPE {
		INFO,    // 通常の通知
		WARNING, // 注意が必要な通知
		ERR,     // 重大な問題を示す通知
		FATAL,   // 致命的な問題を示す通知。あんまり見たくないです。ははっ。
	};

	/// @brief Notificationは、Editorへ表示する本文、severity、表示時間を保持します
	struct Notification {
		std::string title;       // 通知のタイトル
		std::string description; // タイトルの下に表示される説明文

		NOTIFY_TYPE type = NOTIFY_TYPE::INFO; // 通知の種類 (INFO, WARNING, ERROR)

		float durationSeconds  = 5.0f; // 通知の表示時間 [sec] 0で以下で閉じるまで表示
		float remainingSeconds = 0.0f; // 通知の残り表示時間 [sec]。0以下の場合は閉じるまで表示
	};

	/// @brief エディターノーティフィケーションクラス
	/// @details コンソールなどのシステムから通知を受け取り、エディターに表示するためのクラス。
	class EditorNotification {
	public:
		/// @brief コンストラクタ
		EditorNotification();

		/// @brief デストラクタ
		~EditorNotification();

		/// @brief 更新処理。通知の更新と表示を行います。ImGuiコンテキスト内で呼び出してください。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void Update(float deltaTime);

		/// @brief 通知を追加します。
		/// @param title 通知のタイトル
		/// @param description タイトルの下に表示される説明文
		/// @param type 通知の種類（INFO、WARNING、ERROR）
		/// @param durationSeconds 通知の表示時間 [sec] 0で以下で閉じるまで表示
		void PushNotification(
			std::string_view title,
			std::string_view description,
			NOTIFY_TYPE      type            = NOTIFY_TYPE::INFO,
			float            durationSeconds = 5.0f
		);

		/// @brief 現在の通知数を取得します。
		/// @return 現在の通知数
		[[nodiscard]] size_t GetNotifySize() const;

	private:
		struct NotificationState;

		void UpdateNotificationLifetimes(float deltaTime) const;
		void CleanupFinishedNotifications();
		void StartExitAnimation(
			const std::shared_ptr<NotificationState>& notification
		) const;

		std::vector<std::shared_ptr<NotificationState>> mNotifications;
		std::unique_ptr<TweenManager>                   mTweenManager = nullptr;
		std::unique_ptr<ConCommand>                     mNotifyCommand;
		uint64_t                                        mNextNotificationId = 0;
	};
}
#endif
