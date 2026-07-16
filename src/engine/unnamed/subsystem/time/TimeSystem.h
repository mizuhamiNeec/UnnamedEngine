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
		~TimeSystem() override;

		bool Init() override;

		void BeginFrame() const;
		/// @brief フレームを終了します。
		/// @param advanceGameTime false の場合はフレームレート制限だけを適用し、ゲーム時間は進めません。
		void EndFrame(bool advanceGameTime = true) const;

		[[nodiscard]] const std::string_view GetName() const override;

		[[nodiscard]] GameTime*     GetGameTime() const;
		[[nodiscard]] FrameLimiter* GetFrameLimiter() const;

	private:
		std::unique_ptr<GameTime>     mGameTime;
		std::unique_ptr<FrameLimiter> mFrameLimiter;
		std::unique_ptr<SystemClock>  mSystemClock;
	};
}
