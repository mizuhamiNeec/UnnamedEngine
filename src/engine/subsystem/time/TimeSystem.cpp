#include <engine/subsystem/time/TimeSystem.h>
#include <engine/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	/// @brief デストラクタ
	TimeSystem::~TimeSystem() = default;

	/// @brief 初期化
	/// @return 初期化成功ならtrue
	bool TimeSystem::Init() {
		ServiceLocator::Register<TimeSystem>(this);

		mGameTime     = std::make_unique<GameTime>();
		mFrameLimiter = std::make_unique<FrameLimiter>(mGameTime.get());
		mSystemClock  = std::make_unique<SystemClock>();

		return mGameTime && mFrameLimiter;
	}

	/// @brief フレーム開始処理
	void TimeSystem::BeginFrame() const {
		mFrameLimiter->BeginFrame();
	}

	/// @brief フレーム終了処理
	void TimeSystem::EndFrame() const {
		mFrameLimiter->Limit();
		mGameTime->EndFrame();
	}

	/// @brief 名前を取得します
	/// @return サブシステムの名前
	const std::string_view TimeSystem::GetName() const {
		return "Time";
	}

	/// @brief ゲームタイムを取得します
	/// @return ゲームタイムのポインタ
	GameTime* TimeSystem::GetGameTime() const {
		return mGameTime.get();
	}

	/// @brief フレームリミッターを取得します
	/// @return フレームリミッターのポインタ
	FrameLimiter* TimeSystem::GetFrameLimiter() const {
		return mFrameLimiter.get();
	}
}
