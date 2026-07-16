#include "CourseResultFlowComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <utility>

#include <Windows.h>

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
#include <imgui.h>
#include "engine/ImGui/ImGuiWidgets.h"
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/ImGui/Icons.h"
#include "engine/game/GamePathResolver.h"
#include "engine/game/GameRuntimeContext.h"
#include "engine/game/IGameModule.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiDigitStripComponent.h"
#include "engine/gui/components/UiTextureComponent.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/audio/AudioSourceComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

#include "CourseElapsedTimeFormat.h"
#include "CourseProgressComponent.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel        = "CourseResultFlow";
		constexpr float            kMinDurationSec = 0.01f;
		constexpr size_t           kMaxRankingEntries = 5;

		struct CourseRankingTable {
			std::string        courseId;
			std::vector<float> elapsedSeconds;
		};

		[[nodiscard]] std::string SanitizeStorageName(
			const std::string_view value
		) {
			std::string result;
			result.reserve(value.size());
			for (const char c : value) {
				if (
					(c >= 'a' && c <= 'z') ||
					(c >= 'A' && c <= 'Z') ||
					(c >= '0' && c <= '9') || c == '_' || c == '-'
				) {
					result.push_back(c);
				}
			}
			return result.empty() ? "ParkourGame" : result;
		}

		[[nodiscard]] Path ResolveRankingStoragePath() {
			std::string gameName = "ParkourGame";
			Path        fallbackRoot = Path("saved");
			if (const GameRuntimeContext* runtimeContext =
				ServiceLocator::Get<GameRuntimeContext>()) {
				if (!runtimeContext->modulePaths.gameName.empty()) {
					gameName = runtimeContext->modulePaths.gameName;
				}
				if (!runtimeContext->modulePaths.gameRoot.IsEmpty()) {
					fallbackRoot = runtimeContext->modulePaths.gameRoot / Path("saved");
				}
			}

			std::array<wchar_t, 32768> localAppData = {};
			const DWORD length = GetEnvironmentVariableW(
				L"LOCALAPPDATA",
				localAppData.data(),
				static_cast<DWORD>(localAppData.size())
			);
			if (length > 0 && length < localAppData.size()) {
				return Path::FromNative(std::filesystem::path(localAppData.data())) /
				       Path("UnnamedEngine") /
				       Path(SanitizeStorageName(gameName)) /
				       Path("course_rankings.json");
			}

			return fallbackRoot / Path("course_rankings.json");
		}

		void NormalizeRankingEntries(std::vector<float>& entries) {
			std::erase_if(entries, [](const float elapsedSeconds) {
				return !std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f;
			});
			std::ranges::sort(entries);
			if (entries.size() > kMaxRankingEntries) {
				entries.resize(kMaxRankingEntries);
			}
		}

		[[nodiscard]] std::vector<CourseRankingTable> LoadRankings(
			const Path& path
		) {
			std::vector<CourseRankingTable> tables;
			const JsonReader                reader(path);
			const JsonReader                courses = reader["courses"];
			if (!reader.Valid() || !courses.IsArray()) {
				return tables;
			}

			tables.reserve(courses.Size());
			for (size_t i = 0; i < courses.Size(); ++i) {
				const JsonReader course = courses[i];
				if (!course.IsObject()) {
					continue;
				}

				CourseRankingTable table = {};
				table.courseId = course["courseId"].GetString("");
				const JsonReader times = course["elapsedSeconds"];
				if (table.courseId.empty() || !times.IsArray()) {
					continue;
				}

				table.elapsedSeconds.reserve(times.Size());
				for (size_t timeIndex = 0; timeIndex < times.Size(); ++timeIndex) {
					const JsonReader time = times[timeIndex];
					if (time.IsNumber()) {
						table.elapsedSeconds.emplace_back(time.GetFloat());
					}
				}
				NormalizeRankingEntries(table.elapsedSeconds);
				if (!table.elapsedSeconds.empty()) {
					tables.emplace_back(std::move(table));
				}
			}

			return tables;
		}

		[[nodiscard]] bool SaveRankings(
			const Path& path,
			const std::vector<CourseRankingTable>& tables
		) {
			try {
				JsonWriter writer(path);
				writer.BeginObject();
				writer.Key("version");
				writer.Write(1);
				writer.Key("courses");
				writer.BeginArray();
				for (const CourseRankingTable& table : tables) {
					writer.BeginObject();
					writer.Key("courseId");
					writer.Write(table.courseId);
					writer.Key("elapsedSeconds");
					writer.BeginArray();
					for (const float elapsedSeconds : table.elapsedSeconds) {
						writer.Write(elapsedSeconds);
					}
					writer.EndArray();
					writer.EndObject();
				}
				writer.EndArray();
				writer.EndObject();
				return writer.Save();
			} catch (const std::exception& exception) {
				Warning(
					kChannel,
					"Failed to save course rankings to '{}': {}",
					path,
					exception.what()
				);
				return false;
			}
		}

		[[nodiscard]] std::unique_ptr<Gui::UiWidget> CreateRankingDigitWidget(
			const std::string_view name,
			const Gui::Rect&       rect,
			const int              value,
			const int              minDigits,
			AssetManager*          assetManager,
			const Path&            texturePath,
			Gui::UiDigitStripComponent*& outDigitStrip
		) {
			auto widget = std::make_unique<Gui::UiWidget>();
			widget->SetName(name);
			widget->SetAnchors({.minX = 0.5f, .minY = 0.5f, .maxX = 0.5f, .maxY = 0.5f});
			widget->SetPivot({.x = 0.5f, .y = 0.5f});
			widget->SetLocalRect(rect);
			outDigitStrip = widget->AddComponent<Gui::UiDigitStripComponent>();
			outDigitStrip->SetValue(value);
			outDigitStrip->SetMinDigits(minDigits);
			if (assetManager) {
				(void)outDigitStrip->SetStripTexturePath(
					texturePath.IsEmpty() ? "textures/digits.png" : texturePath.ToGenericUtf8(),
					*assetManager
				);
			}
			return widget;
		}

		[[nodiscard]] std::unique_ptr<Gui::UiWidget> CreateRankingSeparatorWidget(
			const std::string_view name,
			const Gui::Rect&       rect,
			AssetManager*          assetManager,
			const Path&            texturePath,
			const Path&            fallbackTexturePath,
			Gui::UiTextureComponent*& outTexture
		) {
			auto widget = std::make_unique<Gui::UiWidget>();
			widget->SetName(name);
			widget->SetAnchors({.minX = 0.5f, .minY = 0.5f, .maxX = 0.5f, .maxY = 0.5f});
			widget->SetPivot({.x = 0.5f, .y = 0.5f});
			widget->SetLocalRect(rect);
			outTexture = widget->AddComponent<Gui::UiTextureComponent>();
			if (assetManager) {
				(void)outTexture->SetTexturePath(
					texturePath.IsEmpty() ?
						fallbackTexturePath.ToGenericUtf8() :
						texturePath.ToGenericUtf8(),
					*assetManager
				);
			}
			return widget;
		}

		[[nodiscard]] Path ResolveResultContentPath(
			const Path& configuredPath,
			const Path& fallbackRelativePath
		) {
			const auto effectivePath = configuredPath.IsEmpty() ?
				                           fallbackRelativePath :
				                           configuredPath;
			if (effectivePath.IsEmpty()) {
				return {};
			}

			const auto resolveFromModulePaths =
				[&effectivePath](const GameModulePaths& modulePaths) {
					const std::string genericPath =
						effectivePath.ToGenericUtf8();
					if (
						effectivePath.IsRelative() &&
						genericPath.starts_with("content/")
					) {
						// 旧設定の "content/..." 指定はプロジェクトルート基準で解決します。
						return ResolveGameRootPath(modulePaths, effectivePath);
					}
					return ResolveGameContentPath(modulePaths, effectivePath);
				};

			if (const GameRuntimeContext* runtimeContext =
				ServiceLocator::Get<GameRuntimeContext>()) {
				return resolveFromModulePaths(runtimeContext->modulePaths);
			}
			if (
				const IGameModule* gameModule =
					ServiceLocator::Get<IGameModule>()
			) {
				return resolveFromModulePaths(gameModule->GetGameModulePaths());
			}
			return effectivePath.LexicallyNormal();
		}
	}

	void CourseResultFlowComponent::OnAttached() {
		mRankedElapsedSeconds.clear();
		mRankingRows.clear();
		mCurrentRunRankingIndex = -1;
		ResolveBindings();
		HideResultWidgets();
		SetFadeOverlayAlpha(0.0f);
	}

	void CourseResultFlowComponent::OnTick(const float deltaTime) {
		ResolveBindings();

		const float clampedDelta = std::max(0.0f, deltaTime);
		switch (mPhase) {
			case PHASE::WAITING: {
				const bool cleared =
					mCourseProgress &&
					mCourseProgress->GetSnapshot().courseCleared;
				if (cleared && !mWasCourseCleared) {
					BeginResult();
				}
				mWasCourseCleared = cleared;
				break;
			}
			case PHASE::SHOW_RESULT: TickShowResult(clampedDelta);
				break;
			case PHASE::FADE_OUT: TickFadeOut(clampedDelta);
				break;
			case PHASE::TRANSITIONED: UpdateResultWidgets(1.0f);
				SetFadeOverlayAlpha(1.0f);
				break;
			default: break;
		}
	}

	void CourseResultFlowComponent::OnEditorTick(const float deltaTime) {
		(void)deltaTime;
		ResolveBindings();
	}

	void CourseResultFlowComponent::OnDetached() {
		RestoreLockTargets();
		HideResultWidgets();
		SetFadeOverlayAlpha(0.0f);
		mRankingRows.clear();
		BaseComponent::OnDetached();
	}

	std::string_view CourseResultFlowComponent::GetStableName() const {
		return "parkour.CourseResultFlow";
	}

	std::string_view CourseResultFlowComponent::GetComponentName() const {
		return "CourseResultFlow";
	}

	uint32_t CourseResultFlowComponent::GetIcon() const {
		return kIconTimer;
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void CourseResultFlowComponent::DrawInspectorImGui() {
		(void)ImGuiWidgets::InputText<64>("Course Id", mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}

		std::string titleScenePathTemp = mTitleScenePath.ToUtf8();
		(void)ImGuiWidgets::InputText<128>(
			"Title Scene Path", titleScenePathTemp
		);
		mTitleScenePath = Path(titleScenePathTemp);
		(void)ImGuiWidgets::InputText<64>(
			"Result Root Widget", mResultRootWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Clear Image Widget", mClearImageWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Digits Widget (Legacy)", mElapsedDigitsWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Minutes Widget", mElapsedMinutesWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Seconds Widget", mElapsedSecondsWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Fraction Widget", mElapsedFractionWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Comma Widget", mElapsedCommaWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Dot Widget", mElapsedDotWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Fade Overlay Widget", mFadeOverlayWidgetName
		);
		std::string clearTexturePath = mClearTexturePath.ToGenericUtf8();
		(void)ImGuiWidgets::InputText<128>("Clear Texture", clearTexturePath);
		mClearTexturePath            = Path(clearTexturePath);
		std::string digitTexturePath = mDigitTexturePath.ToGenericUtf8();
		(void)ImGuiWidgets::InputText<128>("Digit Texture", digitTexturePath);
		mDigitTexturePath            = Path(digitTexturePath);
		std::string commaTexturePath = mCommaTexturePath.ToGenericUtf8();
		(void)ImGuiWidgets::InputText<128>("Comma Texture", commaTexturePath);
		mCommaTexturePath          = Path(commaTexturePath);
		std::string dotTexturePath = mDotTexturePath.ToGenericUtf8();
		(void)ImGuiWidgets::InputText<128>("Dot Texture", dotTexturePath);
		mDotTexturePath = Path(dotTexturePath);
		ImGui::DragFloat(
			"Result Hold Seconds",
			&mResultHoldSeconds,
			0.05f,
			kMinDurationSec,
			10.0f
		);
		ImGui::DragFloat(
			"Fade Out Seconds",
			&mFadeOutSeconds,
			0.05f,
			kMinDurationSec,
			5.0f
		);
		ImGui::InputScalar(
			"Hud Canvas Entity Guid",
			ImGuiDataType_U64,
			&mHudCanvasEntityGuid
		);
		ImGui::InputScalar(
			"Clear Audio Source Guid",
			ImGuiDataType_U64,
			&mClearAudioSourceGuid
		);
		ImGui::DragInt(
			"Elapsed Min Digits",
			&mElapsedDigitsMinDigits,
			1.0f,
			1,
			12
		);
		ImGui::DragFloat(
			"Elapsed Display Scale",
			&mElapsedDisplayScale,
			1.0f,
			1.0f,
			1000.0f
		);
	}
#endif

	void CourseResultFlowComponent::Deserialize(const JsonReader& reader) {
		mCourseId = reader["courseId"].GetString(mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
		mTitleScenePath = Path(
			reader["titleScenePath"].GetString(mTitleScenePath.ToUtf8())
		);
		if (const JsonReader node = reader["hudCanvasEntityGuid"];
			node.Valid()) {
			mHudCanvasEntityGuid = node.GetUint64();
		}
		if (
			const JsonReader node = reader["clearAudioSourceGuid"];
			node.Valid()
		) {
			mClearAudioSourceGuid = node.GetUint64();
		}

		mResultRootWidgetName =
			reader["resultRootWidgetName"].GetString(mResultRootWidgetName);
		mClearImageWidgetName =
			reader["clearImageWidgetName"].GetString(mClearImageWidgetName);
		mElapsedDigitsWidgetName =
			reader["elapsedDigitsWidgetName"].GetString(
				mElapsedDigitsWidgetName);
		mElapsedMinutesWidgetName =
			reader["elapsedMinutesWidgetName"].GetString(
				mElapsedMinutesWidgetName);
		mElapsedSecondsWidgetName =
			reader["elapsedSecondsWidgetName"].GetString(
				mElapsedSecondsWidgetName);
		mElapsedFractionWidgetName = reader["elapsedFractionWidgetName"].
			GetString(
				mElapsedFractionWidgetName
			);
		mElapsedCommaWidgetName =
			reader["elapsedCommaWidgetName"].GetString(mElapsedCommaWidgetName);
		mElapsedDotWidgetName =
			reader["elapsedDotWidgetName"].GetString(mElapsedDotWidgetName);
		mFadeOverlayWidgetName =
			reader["fadeOverlayWidgetName"].GetString(mFadeOverlayWidgetName);
		mClearTexturePath = Path(
			reader["clearTexturePath"].GetString(
				mClearTexturePath.ToGenericUtf8())
		);
		mDigitTexturePath = Path(
			reader["digitTexturePath"].GetString(
				mDigitTexturePath.ToGenericUtf8())
		);
		mCommaTexturePath = Path(
			reader["commaTexturePath"].GetString(
				mCommaTexturePath.ToGenericUtf8())
		);
		mDotTexturePath = Path(
			reader["dotTexturePath"].GetString(mDotTexturePath.ToGenericUtf8())
		);

		if (const JsonReader node = reader["resultHoldSeconds"];
			node.Valid()) {
			mResultHoldSeconds =
				std::max(kMinDurationSec, node.GetFloat(mResultHoldSeconds));
		}
		if (const JsonReader node = reader["fadeOutSeconds"];
			node.Valid()) {
			mFadeOutSeconds = std::max(kMinDurationSec,
			                           node.GetFloat(mFadeOutSeconds));
		}
		if (const JsonReader node = reader["elapsedDigitsMinDigits"];
			node.Valid()) {
			mElapsedDigitsMinDigits = std::max(
				1, node.GetInt(mElapsedDigitsMinDigits));
		}
		if (const JsonReader node = reader["elapsedDisplayScale"];
			node.Valid()) {
			mElapsedDisplayScale =
				std::max(1.0f, node.GetFloat(mElapsedDisplayScale));
		}

		if (const JsonReader lockNode = reader["lockTargets"];
			lockNode.Valid()) {
			const JsonReader lockArray = lockNode.GetArray();
			mLockTargets.clear();
			mLockTargets.reserve(lockArray.Size());
			for (size_t i = 0; i < lockArray.Size(); ++i) {
				const JsonReader item = lockArray[i];
				if (!item.Valid()) {
					continue;
				}

				LockTargetSpec spec = {};
				if (const JsonReader entityNode = item["entityGuid"];
					entityNode.Valid()) {
					spec.entityGuid = entityNode.GetUint64();
				}
				spec.componentStableName =
					item["componentStableName"].GetString(
						spec.componentStableName);
				if (!spec.componentStableName.empty()) {
					mLockTargets.emplace_back(std::move(spec));
				}
			}
		}
	}

	void CourseResultFlowComponent::Serialize(JsonWriter& writer) const {
		writer.Key("courseId");
		writer.Write(mCourseId);
		writer.Key("titleScenePath");
		writer.Write(mTitleScenePath);
		writer.Key("hudCanvasEntityGuid");
		writer.Write(mHudCanvasEntityGuid);
		writer.Key("clearAudioSourceGuid");
		writer.Write(mClearAudioSourceGuid);
		writer.Key("resultRootWidgetName");
		writer.Write(mResultRootWidgetName);
		writer.Key("clearImageWidgetName");
		writer.Write(mClearImageWidgetName);
		writer.Key("elapsedDigitsWidgetName");
		writer.Write(mElapsedDigitsWidgetName);
		writer.Key("elapsedMinutesWidgetName");
		writer.Write(mElapsedMinutesWidgetName);
		writer.Key("elapsedSecondsWidgetName");
		writer.Write(mElapsedSecondsWidgetName);
		writer.Key("elapsedFractionWidgetName");
		writer.Write(mElapsedFractionWidgetName);
		writer.Key("elapsedCommaWidgetName");
		writer.Write(mElapsedCommaWidgetName);
		writer.Key("elapsedDotWidgetName");
		writer.Write(mElapsedDotWidgetName);
		writer.Key("fadeOverlayWidgetName");
		writer.Write(mFadeOverlayWidgetName);
		writer.Key("clearTexturePath");
		writer.Write(mClearTexturePath.ToGenericUtf8());
		writer.Key("digitTexturePath");
		writer.Write(mDigitTexturePath.ToGenericUtf8());
		writer.Key("commaTexturePath");
		writer.Write(mCommaTexturePath.ToGenericUtf8());
		writer.Key("dotTexturePath");
		writer.Write(mDotTexturePath.ToGenericUtf8());
		writer.Key("resultHoldSeconds");
		writer.Write(mResultHoldSeconds);
		writer.Key("fadeOutSeconds");
		writer.Write(mFadeOutSeconds);
		writer.Key("elapsedDigitsMinDigits");
		writer.Write(mElapsedDigitsMinDigits);
		writer.Key("elapsedDisplayScale");
		writer.Write(mElapsedDisplayScale);

		writer.Key("lockTargets");
		writer.BeginArray();
		for (const LockTargetSpec& spec : mLockTargets) {
			SerializeLockTarget(writer, spec);
		}
		writer.EndArray();
	}

	void CourseResultFlowComponent::BeginResult() {
		if (!mCourseProgress) {
			return;
		}

		const CourseProgressSnapshot& snapshot = mCourseProgress->GetSnapshot();
		mLatchedElapsedSeconds                 =
			snapshot.clearedElapsedSeconds > 0.0f ?
				snapshot.clearedElapsedSeconds :
				snapshot.elapsedSeconds;
		mPhase               = PHASE::SHOW_RESULT;
		mPhaseElapsedSeconds = 0.0f;
		RecordRanking();
		CreateRankingWidgets();

		ApplyLockTargets();
		if (mClearAudio) {
			mClearAudio->Play();
		}

		UpdateResultWidgets(1.0f);
		SetFadeOverlayAlpha(0.0f);
		Msg(
			kChannel,
			"Course result started: course={} elapsed={:.2f}s",
			mCourseId,
			mLatchedElapsedSeconds
		);
	}

	void CourseResultFlowComponent::RecordRanking() {
		mRankedElapsedSeconds.clear();
		mCurrentRunRankingIndex = -1;
		if (
			!std::isfinite(mLatchedElapsedSeconds) ||
			mLatchedElapsedSeconds <= 0.0f
		) {
			Warning(
				kChannel,
				"Skipped invalid course ranking time: course={} elapsed={}",
				mCourseId,
				mLatchedElapsedSeconds
			);
			return;
		}

		const Path rankingPath = ResolveRankingStoragePath();
		std::vector<CourseRankingTable> tables = LoadRankings(rankingPath);
		const std::string courseId =
			mCourseId.empty() ? std::string("default") : mCourseId;
		auto tableIt = std::ranges::find_if(
			tables,
			[&courseId](const CourseRankingTable& table) {
				return table.courseId == courseId;
			}
		);
		if (tableIt == tables.end()) {
			tables.emplace_back(CourseRankingTable{.courseId = courseId});
			tableIt = std::prev(tables.end());
		}

		std::vector<float>& entries = tableIt->elapsedSeconds;
		NormalizeRankingEntries(entries);
		const auto insertionIt = std::ranges::upper_bound(
			entries,
			mLatchedElapsedSeconds
		);
		const size_t insertionIndex = static_cast<size_t>(
			std::distance(entries.begin(), insertionIt)
		);
		entries.insert(insertionIt, mLatchedElapsedSeconds);
		NormalizeRankingEntries(entries);
		mRankedElapsedSeconds = entries;
		if (insertionIndex < mRankedElapsedSeconds.size()) {
			mCurrentRunRankingIndex = static_cast<int>(insertionIndex);
		}

		if (!SaveRankings(rankingPath, tables)) {
			Warning(
				kChannel,
				"Course ranking remains visible for this result, but was not persisted: {}",
				rankingPath
			);
		}
	}

	void CourseResultFlowComponent::CreateRankingWidgets() {
		if (!mResultRootWidget || !mRankingRows.empty()) {
			return;
		}

		AssetManager* const assetManager = GetAssetManager();
		for (size_t index = 0; index < mRankedElapsedSeconds.size(); ++index) {
			const CourseElapsedTimeParts time = SplitCourseElapsedTime(
				mRankedElapsedSeconds[index]
			);
			const float y = 130.0f + static_cast<float>(index) * 31.0f;
			RankingRowWidgets row = {};
			const std::string suffix = std::to_string(index + 1);

			auto rankWidget = CreateRankingDigitWidget(
				"CourseResultRankingRank" + suffix,
				{.x = -140.0f, .y = y, .width = 28.0f, .height = 28.0f},
				static_cast<int>(index + 1),
				1,
				assetManager,
				mDigitTexturePath,
				row.rank
			);
			row.rankWidget = rankWidget.get();
			mResultRootWidget->AddChild(std::move(rankWidget));

			auto minutesWidget = CreateRankingDigitWidget(
				"CourseResultRankingMinutes" + suffix,
				{.x = -70.0f, .y = y, .width = 56.0f, .height = 28.0f},
				time.minutes,
				2,
				assetManager,
				mDigitTexturePath,
				row.minutes
			);
			row.minutesWidget = minutesWidget.get();
			mResultRootWidget->AddChild(std::move(minutesWidget));

			auto commaWidget = CreateRankingSeparatorWidget(
				"CourseResultRankingComma" + suffix,
				{.x = -35.0f, .y = y, .width = 28.0f, .height = 28.0f},
				assetManager,
				mCommaTexturePath,
				Path("textures/colon.png"),
				row.comma
			);
			row.commaWidget = commaWidget.get();
			mResultRootWidget->AddChild(std::move(commaWidget));

			auto secondsWidget = CreateRankingDigitWidget(
				"CourseResultRankingSeconds" + suffix,
				{.x = 0.0f, .y = y, .width = 56.0f, .height = 28.0f},
				time.seconds,
				2,
				assetManager,
				mDigitTexturePath,
				row.seconds
			);
			row.secondsWidget = secondsWidget.get();
			mResultRootWidget->AddChild(std::move(secondsWidget));

			auto dotWidget = CreateRankingSeparatorWidget(
				"CourseResultRankingDot" + suffix,
				{.x = 35.0f, .y = y, .width = 28.0f, .height = 28.0f},
				assetManager,
				mDotTexturePath,
				Path("textures/dot.png"),
				row.dot
			);
			row.dotWidget = dotWidget.get();
			mResultRootWidget->AddChild(std::move(dotWidget));

			auto fractionWidget = CreateRankingDigitWidget(
				"CourseResultRankingFraction" + suffix,
				{.x = 70.0f, .y = y, .width = 56.0f, .height = 28.0f},
				time.fraction,
				2,
				assetManager,
				mDigitTexturePath,
				row.fraction
			);
			row.fractionWidget = fractionWidget.get();
			mResultRootWidget->AddChild(std::move(fractionWidget));

			mRankingRows.emplace_back(row);
		}
	}

	void CourseResultFlowComponent::TickShowResult(const float deltaTime) {
		mPhaseElapsedSeconds += deltaTime;
		UpdateResultWidgets(1.0f);
		SetFadeOverlayAlpha(0.0f);

		if (mPhaseElapsedSeconds < mResultHoldSeconds) {
			return;
		}

		mPhase               = PHASE::FADE_OUT;
		mPhaseElapsedSeconds = 0.0f;
	}

	void CourseResultFlowComponent::TickFadeOut(const float deltaTime) {
		mPhaseElapsedSeconds += deltaTime;
		const float t        = std::clamp(
			mPhaseElapsedSeconds / std::max(kMinDurationSec, mFadeOutSeconds),
			0.0f,
			1.0f
		);
		UpdateResultWidgets(1.0f);
		SetFadeOverlayAlpha(EvaluateEase(t));

		if (t >= 1.0f) {
			CommitTitleTransition();
		}
	}

	void CourseResultFlowComponent::CommitTitleTransition() {
		World* world = GetWorld();
		if (!world) {
			return;
		}

		const Path titleScenePath = ResolveResultContentPath(
			mTitleScenePath,
			Path("scenes/title.json")
		);
		if (titleScenePath.IsEmpty()) {
			Warning(kChannel, "Title scene path is empty.");
			return;
		}

		mPhase = PHASE::TRANSITIONED;
		world->RequestSceneTransition(titleScenePath);
		RestoreLockTargets();
	}

	void CourseResultFlowComponent::ResolveBindings() {
		ClearResolvedBindings();

		Scene* scene = GetScene();
		if (!scene) {
			return;
		}

		const std::string normalizedCourseId =
			mCourseId.empty() ? std::string("default") : mCourseId;
		const auto TryBindCourseProgress = [&](Entity& entity) -> bool {
			bool found = false;
			entity.ForEachComponent(
				[&](BaseComponent& component) {
					auto* progress = dynamic_cast<CourseProgressComponent*>(&
						component);
					if (!progress || progress->GetCourseId() !=
					    normalizedCourseId) {
						return true;
					}
					mCourseProgress = progress;
					found           = true;
					return false;
				}
			);
			return found;
		};

		if (Entity* owner = GetOwner()) {
			(void)TryBindCourseProgress(*owner);
		}
		if (!mCourseProgress) {
			for (const auto& entityPtr : scene->GetEntities()) {
				if (entityPtr && TryBindCourseProgress(*entityPtr)) {
					break;
				}
			}
		}

		Entity* hudEntity = nullptr;
		if (mHudCanvasEntityGuid != 0) {
			hudEntity = scene->FindEntity(mHudCanvasEntityGuid);
		}
		if (!hudEntity) {
			hudEntity = GetOwner();
		}
		if (hudEntity) {
			mHudCanvas = hudEntity->GetComponent<UiCanvasComponent>();
		}

		if (mHudCanvas && mHudCanvas->EnsureRuntimeLoaded()) {
			const Gui::UiRoot* root = mHudCanvas->GetRuntimeRoot();
			Gui::UiWidget* rootWidget = root ? root->GetRootWidget() : nullptr;
			if (rootWidget) {
				mResultRootWidget = FindWidgetByNameRecursive(
					rootWidget,
					mResultRootWidgetName
				);
				mClearImageWidget = FindWidgetByNameRecursive(
					rootWidget,
					mClearImageWidgetName
				);
				mElapsedDigitsWidget = FindWidgetByNameRecursive(
					rootWidget,
					mElapsedDigitsWidgetName
				);
				mElapsedMinutesWidget = FindWidgetByNameRecursive(
					rootWidget,
					mElapsedMinutesWidgetName
				);
				mElapsedSecondsWidget = FindWidgetByNameRecursive(
					rootWidget,
					mElapsedSecondsWidgetName
				);
				mElapsedFractionWidget = FindWidgetByNameRecursive(
					rootWidget,
					mElapsedFractionWidgetName
				);
				mElapsedCommaWidget = FindWidgetByNameRecursive(
					rootWidget,
					mElapsedCommaWidgetName
				);
				mElapsedDotWidget = FindWidgetByNameRecursive(
					rootWidget,
					mElapsedDotWidgetName
				);
				mFadeOverlayWidget = FindWidgetByNameRecursive(
					rootWidget,
					mFadeOverlayWidgetName
				);

				if (mClearImageWidget) {
					mClearImageTexture =
						mClearImageWidget->GetOrAddComponent<
							Gui::UiTextureComponent>();
				}
				if (mElapsedDigitsWidget) {
					mElapsedDigits =
						mElapsedDigitsWidget->GetOrAddComponent<
							Gui::UiDigitStripComponent>();
				}
				if (mElapsedMinutesWidget) {
					mElapsedMinutes =
						mElapsedMinutesWidget->GetOrAddComponent<
							Gui::UiDigitStripComponent>();
				}
				if (mElapsedSecondsWidget) {
					mElapsedSeconds =
						mElapsedSecondsWidget->GetOrAddComponent<
							Gui::UiDigitStripComponent>();
				}
				if (mElapsedFractionWidget) {
					mElapsedFraction =
						mElapsedFractionWidget->GetOrAddComponent<
							Gui::UiDigitStripComponent>();
				}
				if (mElapsedCommaWidget) {
					mElapsedComma =
						mElapsedCommaWidget->GetOrAddComponent<
							Gui::UiTextureComponent>();
				}
				if (mElapsedDotWidget) {
					mElapsedDot =
						mElapsedDotWidget->GetOrAddComponent<
							Gui::UiTextureComponent>();
				}
				if (mFadeOverlayWidget) {
					mFadeOverlayTexture =
						mFadeOverlayWidget->GetOrAddComponent<
							Gui::UiTextureComponent>();
				}
			}
		}

		mClearAudio = ResolveAudioSourceByGuid(mClearAudioSourceGuid);
	}

	void CourseResultFlowComponent::ClearResolvedBindings() {
		mCourseProgress        = nullptr;
		mHudCanvas             = nullptr;
		mClearAudio            = nullptr;
		mResultRootWidget      = nullptr;
		mClearImageWidget      = nullptr;
		mElapsedDigitsWidget   = nullptr;
		mElapsedMinutesWidget  = nullptr;
		mElapsedSecondsWidget  = nullptr;
		mElapsedFractionWidget = nullptr;
		mElapsedCommaWidget    = nullptr;
		mElapsedDotWidget      = nullptr;
		mFadeOverlayWidget     = nullptr;
		mClearImageTexture     = nullptr;
		mElapsedDigits         = nullptr;
		mElapsedMinutes        = nullptr;
		mElapsedSeconds        = nullptr;
		mElapsedFraction       = nullptr;
		mElapsedComma          = nullptr;
		mElapsedDot            = nullptr;
		mFadeOverlayTexture    = nullptr;
	}

	void CourseResultFlowComponent::UpdateResultWidgets(
		const float alpha
	)
	const {
		const float         clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
		AssetManager* const assetManager = GetAssetManager();
		if (mResultRootWidget) {
			mResultRootWidget->SetVisible(clampedAlpha > 0.0f);
			mResultRootWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}

		if (mClearImageWidget) {
			mClearImageWidget->SetVisible(clampedAlpha > 0.0f);
		}
		if (mClearImageTexture) {
			if (assetManager) {
				(void)mClearImageTexture->SetTexturePath(
					mClearTexturePath.IsEmpty() ?
						"textures/clear.png" :
						mClearTexturePath.ToGenericUtf8(),
					*assetManager
				);
			}
			Gui::Color color = mClearImageTexture->GetColor();
			color.a          = clampedAlpha;
			mClearImageTexture->SetColor(color);
		}

		if (mElapsedDigitsWidget) {
			const bool hasStructuredWidgets =
				mElapsedMinutes &&
				mElapsedSeconds &&
				mElapsedFraction &&
				mElapsedComma &&
				mElapsedDot;
			mElapsedDigitsWidget->SetVisible(
				!hasStructuredWidgets && clampedAlpha > 0.0f
			);
		}
		if (mElapsedMinutes &&
		    mElapsedSeconds &&
		    mElapsedFraction &&
		    mElapsedComma &&
		    mElapsedDot) {
			const CourseElapsedTimeParts time =
				SplitCourseElapsedTime(mLatchedElapsedSeconds);

			const auto ApplyDigitStrip = [&](
				Gui::UiWidget*              widget,
				Gui::UiDigitStripComponent* strip,
				const int                   value
			) {
				if (!widget || !strip) {
					return;
				}
				widget->SetVisible(clampedAlpha > 0.0f);
				if (assetManager) {
					(void)strip->SetStripTexturePath(
						mDigitTexturePath.IsEmpty() ?
							"textures/digits.png" :
							mDigitTexturePath.ToGenericUtf8(),
						*assetManager
					);
				}
				strip->SetMinDigits(2);
				strip->SetValue(value);
				Gui::Color color = strip->GetColor();
				color.a          = clampedAlpha;
				strip->SetColor(color);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			ApplyDigitStrip(mElapsedMinutesWidget, mElapsedMinutes,
			                time.minutes);
			ApplyDigitStrip(mElapsedSecondsWidget, mElapsedSeconds,
			                time.seconds);
			ApplyDigitStrip(mElapsedFractionWidget, mElapsedFraction,
			                time.fraction);

			const auto ApplySeparator = [&](
				Gui::UiWidget*           widget,
				Gui::UiTextureComponent* texture,
				const Path&              path,
				const Path&              fallbackPath
			) {
				if (!widget || !texture) {
					return;
				}
				widget->SetVisible(clampedAlpha > 0.0f);
				if (assetManager) {
					(void)texture->SetTexturePath(
						path.IsEmpty() ?
							fallbackPath.ToGenericUtf8() :
							path.ToGenericUtf8(),
						*assetManager
					);
				}
				Gui::Color color = texture->GetColor();
				color.a          = clampedAlpha;
				texture->SetColor(color);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			ApplySeparator(
				mElapsedCommaWidget,
				mElapsedComma,
				mCommaTexturePath,
				Path("textures/colon.png")
			);
			ApplySeparator(
				mElapsedDotWidget,
				mElapsedDot,
				mDotTexturePath,
				Path("textures/dot.png")
			);
		} else if (mElapsedDigits) {
			const int displayValue = static_cast<int>(
				std::lround(std::max(0.0f, mLatchedElapsedSeconds) *
				            mElapsedDisplayScale)
			);
			if (assetManager) {
				(void)mElapsedDigits->SetStripTexturePath(
					mDigitTexturePath.IsEmpty() ?
						"textures/digits.png" :
						mDigitTexturePath.ToGenericUtf8(),
					*assetManager
				);
			}
			mElapsedDigits->SetMinDigits(mElapsedDigitsMinDigits);
			mElapsedDigits->SetValue(displayValue);
			Gui::Color color = mElapsedDigits->GetColor();
			color.a          = clampedAlpha;
			mElapsedDigits->SetColor(color);
		}
		UpdateRankingWidgets(clampedAlpha);
	}

	void CourseResultFlowComponent::UpdateRankingWidgets(
		const float alpha
	) const {
		const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
		for (size_t index = 0; index < mRankingRows.size(); ++index) {
			const RankingRowWidgets& row = mRankingRows[index];
			const bool isCurrentRun =
				static_cast<int>(index) == mCurrentRunRankingIndex;
			const Gui::Color color = isCurrentRun ?
				Gui::Color{.r = 1.0f, .g = 0.9f, .b = 0.62f, .a = clampedAlpha} :
				Gui::Color{.r = 0.76f, .g = 0.84f, .b = 0.96f, .a = clampedAlpha};
			const auto ApplyDigit = [&color, clampedAlpha](
				Gui::UiWidget* widget, Gui::UiDigitStripComponent* digit
			) {
				if (!widget || !digit) {
					return;
				}
				widget->SetVisible(clampedAlpha > 0.0f);
				digit->SetColor(color);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			const auto ApplySeparator = [&color, clampedAlpha](
				Gui::UiWidget* widget, Gui::UiTextureComponent* texture
			) {
				if (!widget || !texture) {
					return;
				}
				widget->SetVisible(clampedAlpha > 0.0f);
				texture->SetColor(color);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			ApplyDigit(row.rankWidget, row.rank);
			ApplyDigit(row.minutesWidget, row.minutes);
			ApplySeparator(row.commaWidget, row.comma);
			ApplyDigit(row.secondsWidget, row.seconds);
			ApplySeparator(row.dotWidget, row.dot);
			ApplyDigit(row.fractionWidget, row.fraction);
		}
	}

	void CourseResultFlowComponent::SetFadeOverlayAlpha(
		const float alpha
	)
	const {
		if (!mFadeOverlayWidget || !mFadeOverlayTexture) {
			return;
		}

		const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
		mFadeOverlayWidget->SetVisible(clampedAlpha > 0.0f);
		Gui::Color color = mFadeOverlayTexture->GetColor();
		color.a          = clampedAlpha;
		mFadeOverlayTexture->SetColor(color);
		mFadeOverlayWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
	}

	void CourseResultFlowComponent::HideResultWidgets() const {
		if (mResultRootWidget) {
			mResultRootWidget->SetVisible(false);
			mResultRootWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
		if (mClearImageWidget) {
			mClearImageWidget->SetVisible(false);
			mClearImageWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
		if (mElapsedDigitsWidget) {
			mElapsedDigitsWidget->SetVisible(false);
			mElapsedDigitsWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
		const auto HideWidget = [](Gui::UiWidget* widget) {
			if (!widget) {
				return;
			}
			widget->SetVisible(false);
			widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		};
		HideWidget(mElapsedMinutesWidget);
		HideWidget(mElapsedSecondsWidget);
		HideWidget(mElapsedFractionWidget);
		HideWidget(mElapsedCommaWidget);
		HideWidget(mElapsedDotWidget);
		for (const RankingRowWidgets& row : mRankingRows) {
			HideWidget(row.rankWidget);
			HideWidget(row.minutesWidget);
			HideWidget(row.commaWidget);
			HideWidget(row.secondsWidget);
			HideWidget(row.dotWidget);
			HideWidget(row.fractionWidget);
		}
	}

	void CourseResultFlowComponent::ApplyLockTargets() {
		RestoreLockTargets();
		std::vector<BaseComponent*> lockedComponents = {};
		lockedComponents.reserve(mLockTargets.size());

		for (const LockTargetSpec& spec : mLockTargets) {
			BaseComponent* target = ResolveLockTarget(spec);
			if (!target || target == this) {
				continue;
			}
			if (std::ranges::find(lockedComponents, target) != lockedComponents.
			    end()) {
				continue;
			}

			mActiveLocks.emplace_back(
				ActiveLockState{
					.component      = target,
					.previousActive = target->IsActive(),
				}
			);
			lockedComponents.emplace_back(target);
			target->SetActive(false);
		}
	}

	void CourseResultFlowComponent::RestoreLockTargets() {
		for (const ActiveLockState& state : mActiveLocks) {
			if (state.component) {
				state.component->SetActive(state.previousActive);
			}
		}
		mActiveLocks.clear();
	}

	BaseComponent* CourseResultFlowComponent::ResolveLockTarget(
		const LockTargetSpec& spec
	) const {
		Scene* scene = GetScene();
		if (!scene || spec.componentStableName.empty()) {
			return nullptr;
		}

		const auto FindInEntity = [&](Entity& entity) -> BaseComponent* {
			BaseComponent* found = nullptr;
			entity.ForEachComponent(
				[&](BaseComponent& component) {
					if (component.GetStableName() != spec.componentStableName) {
						return true;
					}
					found = &component;
					return false;
				}
			);
			return found;
		};

		if (spec.entityGuid != 0) {
			if (Entity* entity = scene->FindEntity(spec.entityGuid)) {
				return FindInEntity(*entity);
			}
			return nullptr;
		}

		if (Entity* owner = GetOwner()) {
			if (BaseComponent* found = FindInEntity(*owner)) {
				return found;
			}
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (entityPtr) {
				if (BaseComponent* found = FindInEntity(*entityPtr)) {
					return found;
				}
			}
		}
		return nullptr;
	}

	AudioSourceComponent* CourseResultFlowComponent::ResolveAudioSourceByGuid(
		const uint64_t componentGuid
	) const {
		if (componentGuid == 0) {
			return nullptr;
		}

		const Scene* scene = GetScene();
		if (!scene) {
			return nullptr;
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}

			AudioSourceComponent* found = nullptr;
			entityPtr->ForEachComponent(
				[&](BaseComponent& component) {
					if (component.GetGuid() != componentGuid) {
						return true;
					}
					found = dynamic_cast<AudioSourceComponent*>(&component);
					return false;
				}
			);
			if (found) {
				return found;
			}
		}
		return nullptr;
	}

	Gui::UiWidget* CourseResultFlowComponent::FindWidgetByNameRecursive(
		Gui::UiWidget*         root,
		const std::string_view widgetName
	) {
		if (!root) {
			return nullptr;
		}
		if (root->GetName() == widgetName) {
			return root;
		}

		for (const auto& child : root->GetChildren()) {
			if (!child) {
				continue;
			}
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(
				child.get(),
				widgetName
			)) {
				return found;
			}
		}

		for (Gui::UiWidget* child : root->GetReferenceChildren()) {
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(
				child, widgetName)) {
				return found;
			}
		}
		return nullptr;
	}

	void CourseResultFlowComponent::SerializeLockTarget(
		JsonWriter&           writer,
		const LockTargetSpec& spec
	) {
		writer.BeginObject();
		writer.Key("entityGuid");
		writer.Write(spec.entityGuid);
		writer.Key("componentStableName");
		writer.Write(spec.componentStableName);
		writer.EndObject();
	}

	float CourseResultFlowComponent::EvaluateEase(const float t) {
		return Math::CubicBezier(
			std::clamp(t, 0.0f, 1.0f),
			0.2f,
			0.0f,
			0.0f,
			1.0f
		);
	}

	REGISTER_COMPONENT(CourseResultFlowComponent);
}
