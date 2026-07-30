#pragma once
#include <memory>

#include <engine/unnamed/subsystem/interface/ISubsystem.h>
#include <engine/unnamed/subsystem/time/FrameLimiter.h>
#include <engine/unnamed/subsystem/time/GameTime.h>
#include <engine/unnamed/subsystem/time/SystemClock.h>

namespace Unnamed {
	/// @brief 時間管理システムクラス
	class TimeSystem : public ISubsystem {
	public:
		/// @brief 初期化
		/// @return 初期化成功ならtrue
		bool Init() override;

		/// @brief フレーム開始処理
		void BeginFrame() const;

		/// @brief フレーム終了処理
		/// @param advanceGameTime false の場合はフレームレート制限だけを適用し、ゲーム時間は進めません。
		void EndFrame(bool advanceGameTime = true) const;

		/// @brief 名前を取得します
		/// @return サブシステムの名前
		[[nodiscard]] const std::string_view GetName() const override;

		/// @brief ゲームタイムを取得します
		/// @return ゲームタイムのポインタ
		[[nodiscard]] GameTime* GetGameTime() const;

		/// @brief フレームリミッターを取得します
		/// @return フレームリミッターのポインタ
		[[nodiscard]] FrameLimiter* GetFrameLimiter() const;

	private:
		std::unique_ptr<GameTime>     mGameTime;
		std::unique_ptr<FrameLimiter> mFrameLimiter;
		std::unique_ptr<SystemClock>  mSystemClock;
	};
}
