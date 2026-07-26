#include "EditorNotification.h"

#ifdef _DEBUG

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <format>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/unnamed/subsystem/console/ConsoleUI.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConCommand.h"

namespace Unnamed {
	/// @brief NotificationStateは、Editor通知の表示開始時刻とfade進行を保持します
	struct EditorNotification::NotificationState {
		Notification notification;
		uint64_t     id             = 0;
		float        opacity        = 0.0f;
		float        slideOffsetX   = 0.0f;
		float        currentY       = 0.0f;
		float        targetY        = 0.0f;
		bool         hasResolvedY   = false;
		bool         isExiting      = false;
		bool         pendingRemoval = false;
		TweenHandle  opacityTween;
		TweenHandle  slideTween;
		TweenHandle  positionTween;
	};

	namespace {
		constexpr std::string_view kChannel = "EditorNotification";
		constexpr char             kNotificationWindowName[] =
			"##EditorNotificationOverlay";
		constexpr float kTitleFontScale       = 1.5f;
		constexpr float kIconFontScale        = 2.5f;
		constexpr float kDescriptionLiftScale = 0.12f;
		constexpr float kRowGapScale          = 0.35f;
		constexpr float kNotificationMarginX  = 16.0f;
		constexpr float kNotificationMarginY  = 16.0f;
		constexpr float kNotificationSpacingY = 8.0f;
		constexpr float kEnterSlideOffsetX    = 56.0f;
		constexpr float kEnterDurationSeconds = 0.28f;
		constexpr float kMoveDurationSeconds  = 0.24f;
		constexpr float kExitDurationSeconds  = 0.18f;
		constexpr float kPositionSnapEpsilon  = 0.5f;
		constexpr Vec2  kP1(0.2f, 0.0f);
		constexpr Vec2  kP2(0.0f, 1.0f);

		/// @brief NotificationLayoutは、Editor通知を積み重ねる画面位置、幅、行間を計算結果として保持します
		struct NotificationLayout {
			float  bodyFontSize            = 0.0f;
			float  titleFontSize           = 0.0f;
			float  iconFontSize            = 0.0f;
			float  iconLineHeight          = 0.0f;
			float  columnGapX              = 0.0f;
			float  titleRenderHeight       = 0.0f;
			float  descriptionRenderHeight = 0.0f;
			float  descriptionLiftY        = 0.0f;
			float  contentWidth            = 0.0f;
			float  contentHeight           = 0.0f;
			ImVec2 titleTextSize           = ImVec2(0.0f, 0.0f);
			ImVec2 descriptionTextSize     = ImVec2(0.0f, 0.0f);
			ImVec2 childSize               = ImVec2(0.0f, 0.0f);
		};

		float CalcLineHeight(ImFont* font, const float size) {
			if (!font) {
				return size;
			}
			const ImFontBaked* baked = font->GetFontBaked(size);
			return baked ? baked->Ascent - baked->Descent : size;
		}

		void StopTween(TweenHandle& handle) {
			if (!handle.IsValid()) {
				return;
			}
			handle.Kill(false);
			handle = TweenHandle();
		}

		ImU32 ApplyAlpha(const ImU32 color, const float alpha) {
			ImVec4 colorVec = ImGui::ColorConvertU32ToFloat4(color);
			colorVec.w      *= std::clamp(alpha, 0.0f, 0.9f);
			return ImGui::ColorConvertFloat4ToU32(colorVec);
		}

		uint32_t GetNotificationIcon(const NOTIFY_TYPE type) {
			switch (type) {
				case NOTIFY_TYPE::INFO: return kIconInfo;
				case NOTIFY_TYPE::WARNING: return kIconWarning;
				case NOTIFY_TYPE::ERR: return kIconError;
				case NOTIFY_TYPE::FATAL: return kIconBomb;
			}
			return kIconQuestionMark;
		}

		ImU32 GetNotificationIconColor(const NOTIFY_TYPE type) {
			switch (type) {
				case NOTIFY_TYPE::INFO: return ImGui::ColorConvertFloat4ToU32(
						ImVec4(0.21f, 0.45f, 0.94f, 1.0f)
					);
				case NOTIFY_TYPE::WARNING: return
						ImGui::ColorConvertFloat4ToU32(
							ToImVec4(kConTextColorWarn)
						);
				case NOTIFY_TYPE::ERR: return ImGui::ColorConvertFloat4ToU32(
						ToImVec4(kConTextColorError)
					);
				case NOTIFY_TYPE::FATAL: return ImGui::ColorConvertFloat4ToU32(
						ToImVec4(kConTextColorFatal)
					);
			}
			return ImGui::GetColorU32(ImGuiCol_Text);
		}

		std::optional<NOTIFY_TYPE> TryParseNotifyType(
			const std::string_view value
		) {
			const std::string lower = StrUtil::ToLowerCase(std::string(value));
			if (lower == "info" || lower == "i") {
				return NOTIFY_TYPE::INFO;
			}
			if (lower == "warning" || lower == "warn" || lower == "w") {
				return NOTIFY_TYPE::WARNING;
			}
			if (lower == "error" || lower == "err" || lower == "e") {
				return NOTIFY_TYPE::ERR;
			}
			if (lower == "fatal" || lower == "f") {
				return NOTIFY_TYPE::FATAL;
			}
			return std::nullopt;
		}

		std::string JoinArgs(
			const std::vector<std::string>& args, const size_t beginIndex,
			const size_t                    endIndex
		) {
			if (beginIndex >= endIndex || beginIndex >= args.size()) {
				return {};
			}

			std::string result;
			for (size_t i = beginIndex; i < endIndex && i < args.size(); ++i) {
				if (!result.empty()) {
					result += ' ';
				}
				result += args[i];
			}
			return result;
		}

		bool TryParseNotifyArgs(
			const std::vector<std::string>& args,
			NOTIFY_TYPE&                    outType,
			float&                          outDurationSeconds,
			std::string&                    outTitle,
			std::string&                    outDescription
		) {
			outType            = NOTIFY_TYPE::INFO;
			outDurationSeconds = 5.0f;
			outTitle.clear();
			outDescription.clear();

			size_t index = 0;
			if (index < args.size()) {
				if (const auto parsedType = TryParseNotifyType(args[index])) {
					outType = *parsedType;
					++index;
				}
			}

			if (index < args.size() && StrUtil::IsFloat(args[index])) {
				outDurationSeconds = std::stof(args[index]);
				++index;
			}

			if (index >= args.size()) {
				return false;
			}

			size_t separatorIndex = args.size();
			for (size_t i = index; i < args.size(); ++i) {
				if (args[i] == "|") {
					separatorIndex = i;
					break;
				}
			}

			if (separatorIndex < args.size()) {
				outTitle       = JoinArgs(args, index, separatorIndex);
				outDescription =
					JoinArgs(args, separatorIndex + 1, args.size());
				if (outDescription.empty()) {
					outDescription = outTitle;
					outTitle.clear();
				}
			} else {
				outDescription = JoinArgs(args, index, args.size());
			}

			return !outDescription.empty();
		}

		NotificationLayout BuildNotificationLayout(
			const Notification& notification, const ImGuiStyle& style,
			ImFont*             font
		) {
			NotificationLayout layout;
			layout.bodyFontSize       = ImGui::GetFontSize();
			layout.titleFontSize      = layout.bodyFontSize * kTitleFontScale;
			layout.iconFontSize       = layout.bodyFontSize * kIconFontScale;
			layout.columnGapX         = style.ItemInnerSpacing.x;
			const bool hasTitle       = !notification.title.empty();
			const bool hasDescription = !notification.description.empty();

			const float rowGapY = style.ItemSpacing.y * kRowGapScale;
			layout.iconLineHeight = CalcLineHeight(font, layout.iconFontSize);
			const float titleLineHeight = CalcLineHeight(
				font,
				layout.titleFontSize
			);
			const float bodyLineHeight = CalcLineHeight(
				font,
				layout.bodyFontSize
			);
			layout.titleTextSize = hasTitle ?
				                       font->CalcTextSizeA(
					                       layout.titleFontSize,
					                       FLT_MAX,
					                       -1.0f,
					                       notification.title.c_str()
				                       ) :
				                       ImVec2(0.0f, 0.0f);
			layout.descriptionTextSize = hasDescription ?
				                             font->CalcTextSizeA(
					                             layout.bodyFontSize,
					                             FLT_MAX,
					                             -1.0f,
					                             notification.description.
					                             c_str()
				                             ) :
				                             ImVec2(0.0f, 0.0f);
			layout.titleRenderHeight = hasTitle ?
				                           ImMax(
					                           layout.titleTextSize.y,
					                           titleLineHeight
				                           ) :
				                           0.0f;
			layout.descriptionRenderHeight = hasDescription ?
				                                 ImMax(
					                                 layout.descriptionTextSize.
					                                 y, bodyLineHeight
				                                 ) :
				                                 0.0f;
			layout.descriptionLiftY = hasTitle ?
				                          bodyLineHeight *
				                          kDescriptionLiftScale :
				                          0.0f;

			float textBlockHeight = 0.0f;
			if (hasTitle) {
				textBlockHeight += layout.titleRenderHeight;
			}
			if (hasDescription) {
				if (hasTitle) {
					textBlockHeight += rowGapY;
				}
				textBlockHeight += layout.descriptionRenderHeight;
			}

			layout.contentHeight = ImMax(
				layout.iconLineHeight, textBlockHeight
			);
			const float textColumnWidth = ImMax(
				layout.titleTextSize.x,
				layout.descriptionTextSize.x
			);
			layout.contentWidth =
				layout.iconFontSize + layout.columnGapX + textColumnWidth;
			layout.childSize = ImVec2(
				style.WindowPadding.x * 2.0f + layout.contentWidth,
				style.WindowPadding.y * 2.0f + layout.contentHeight
			);
			return layout;
		}

		void DrawNotificationCard(
			const Notification&       notification,
			const uint64_t            notificationId,
			ImFont*                   font,
			const NotificationLayout& layout,
			const float               alpha
		) {
			if (!font) {
				return;
			}
			const bool  hasTitle       = !notification.title.empty();
			const bool  hasDescription = !notification.description.empty();
			const float clampedAlpha   = std::clamp(alpha, 0.0f, 0.75f);

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, clampedAlpha);
			ImGui::BeginChild(
				std::format("##notify{}", notificationId).c_str(),
				layout.childSize,
				ImGuiChildFlags_FrameStyle |
				ImGuiChildFlags_AutoResizeX,
				ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs
			);

			ImDrawList*  drawList   = ImGui::GetWindowDrawList();
			const ImVec2 contentMin = ImGui::GetCursorScreenPos();
			const float  iconPosY   =
				contentMin.y + (layout.contentHeight - layout.iconLineHeight) *
				0.5f;
			const float textPosX =
				contentMin.x + layout.iconFontSize + layout.columnGapX;
			const std::string iconText =
				StrUtil::ConvertToUtf8(GetNotificationIcon(notification.type));
			const ImU32 iconColor = ApplyAlpha(
				GetNotificationIconColor(notification.type), clampedAlpha
			);
			const ImU32 textColor = ApplyAlpha(
				ImGui::GetColorU32(ImGuiCol_Text), 1.0f
			);

			drawList->AddText(
				font,
				layout.iconFontSize,
				ImVec2(contentMin.x, iconPosY),
				iconColor,
				iconText.c_str()
			);

			if (hasTitle) {
				drawList->AddText(
					font,
					layout.titleFontSize,
					ImVec2(textPosX, contentMin.y),
					textColor,
					notification.title.c_str()
				);
			}

			if (hasDescription) {
				const float descriptionPosY = hasTitle ?
					                              contentMin.y + layout.
					                              contentHeight -
					                              layout.descriptionRenderHeight
					                              -
					                              layout.descriptionLiftY :
					                              contentMin.y +
					                              (layout.contentHeight - layout
					                               .descriptionRenderHeight) *
					                              0.5f;
				drawList->AddText(
					font,
					layout.bodyFontSize,
					ImVec2(textPosX, descriptionPosY),
					textColor,
					notification.description.c_str()
				);
			}

			ImGui::Dummy(ImVec2(layout.contentWidth, layout.contentHeight));
			ImGui::EndChild();
			ImGui::PopStyleVar();
		}
	}

	EditorNotification::EditorNotification() {
		mNotifyCommand = std::make_unique<ConCommand>(
			"notify",
			[this](const std::vector<std::string>& args) {
				NOTIFY_TYPE type;
				float       durationSeconds = 0.0f;
				std::string title;
				std::string description;
				if (!TryParseNotifyArgs(
					args,
					type,
					durationSeconds,
					title,
					description
				)) {
					return false;
				}

				PushNotification(
					title,
					description,
					type,
					durationSeconds
				);
				return true;
			},
			"Usage: notify [info|warning|error|fatal] [seconds] <message...>\n"
			"       notify [type] [seconds] <title...> | <description...>"
		);

		mTweenManager = std::make_unique<TweenManager>();
	}

	EditorNotification::~EditorNotification() {
		mNotifyCommand.reset();
		if (mTweenManager) {
			mTweenManager->KillAll(false);
		}
	}

	void EditorNotification::Update(const float deltaTime) {
		if (!ImGui::GetCurrentContext()) {
			Warning(kChannel, "ImGuiコンテキストが無効です。");
			return;
		}

		if (mTweenManager) {
			mTweenManager->Update(deltaTime);
		}

		UpdateNotificationLifetimes(deltaTime);
		CleanupFinishedNotifications();
		if (mNotifications.empty()) {
			return;
		}

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		const bool isOpen = ImGui::Begin(
			kNotificationWindowName,
			nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoMouseInputs |
			ImGuiWindowFlags_NoBackground
		);

		if (isOpen) {
			if (ImFont* font = ImGui::GetFont()) {
				const ImGuiStyle&               style = ImGui::GetStyle();
				std::vector<NotificationLayout> layouts;
				layouts.reserve(mNotifications.size());

				// 全カードの配置先を先に決め、追加・削除時もスタックを滑らかに移動する
				float totalHeight = 0.0f;
				for (const std::shared_ptr<NotificationState>& state :
				     mNotifications) {
					layouts.emplace_back(
						BuildNotificationLayout(
							state->notification, style, font
						)
					);
					totalHeight += layouts.back().childSize.y;
				}
				if (!layouts.empty()) {
					totalHeight +=
						kNotificationSpacingY * static_cast<float>(
							layouts.size() - 1);
				}

				const ImVec2 windowPos  = ImGui::GetWindowPos();
				const ImVec2 windowSize = ImGui::GetWindowSize();
				const float  rightEdge  =
					windowPos.x + windowSize.x - style.WindowPadding.x -
					kNotificationMarginX;
				float cursorY =
					windowPos.y + windowSize.y - style.WindowPadding.y -
					kNotificationMarginY - totalHeight;

				for (size_t i = 0; i < mNotifications.size(); ++i) {
					const std::shared_ptr<NotificationState>& state =
						mNotifications[i];
					const NotificationLayout& layout = layouts[i];

					if (!state->hasResolvedY) {
						state->currentY     = cursorY;
						state->targetY      = cursorY;
						state->hasResolvedY = true;
					} else if (
						std::fabs(state->targetY - cursorY) >
						kPositionSnapEpsilon
					) {
						state->targetY = cursorY;
						StopTween(state->positionTween);

						if (mTweenManager) {
							auto positionTween = mTweenManager->CreateTo(
								state->currentY,
								state->targetY,
								kMoveDurationSeconds
							);
							positionTween->SetEaseCubicBezier(
								kP1, kP2
							);
							state->positionTween = TweenHandle(positionTween);
						} else {
							state->currentY = state->targetY;
						}
					} else {
						state->currentY = state->targetY;
					}

					ImGui::SetCursorScreenPos(
						ImVec2(
							rightEdge - layout.childSize.x +
							state->slideOffsetX,
							state->currentY
						)
					);
					DrawNotificationCard(
						state->notification,
						state->id,
						font,
						layout,
						state->opacity
					);
					cursorY += layout.childSize.y + kNotificationSpacingY;
				}
			}
		}

		ImGui::End();
	}

	void EditorNotification::PushNotification(
		const std::string_view title,
		const std::string_view description,
		const NOTIFY_TYPE      type,
		const float            durationSeconds
	) {
		auto notification          = std::make_shared<NotificationState>();
		notification->notification = Notification{
			.title            = std::string(title),
			.description      = std::string(description),
			.type             = type,
			.durationSeconds  = durationSeconds,
			.remainingSeconds = durationSeconds
		};
		notification->id           = ++mNextNotificationId;
		notification->opacity      = 0.0f;
		notification->slideOffsetX = kEnterSlideOffsetX;

		if (mTweenManager) {
			const auto fadeTween = mTweenManager->CreateTo(
				notification->opacity, 1.0f, kEnterDurationSeconds
			);
			fadeTween->SetEaseCubicBezier(kP1, kP2);
			notification->opacityTween = TweenHandle(fadeTween);

			const auto slideTween = mTweenManager->CreateTo(
				notification->slideOffsetX, 0.0f, kEnterDurationSeconds
			);
			slideTween->SetEaseCubicBezier(kP1, kP2);
			notification->slideTween = TweenHandle(slideTween);
		} else {
			notification->opacity      = 1.0f;
			notification->slideOffsetX = 0.0f;
		}

		mNotifications.emplace_back(std::move(notification));
	}

	size_t EditorNotification::GetNotifySize() const {
		return mNotifications.size();
	}

	void EditorNotification::UpdateNotificationLifetimes(
		const float deltaTime
	) const {
		if (deltaTime <= 0.0f) {
			return;
		}

		for (const std::shared_ptr<NotificationState>& state : mNotifications) {
			if (!state || state->isExiting) {
				continue;
			}

			if (state->notification.durationSeconds <= 0.0f) {
				continue;
			}

			state->notification.remainingSeconds -= deltaTime;
			if (state->notification.remainingSeconds <= 0.0f) {
				StartExitAnimation(state);
			}
		}
	}

	void EditorNotification::CleanupFinishedNotifications() {
		std::erase_if(
			mNotifications,
			[](const std::shared_ptr<NotificationState>& state) {
				if (!state) {
					return true;
				}
				if (!state->pendingRemoval) {
					return false;
				}

				StopTween(state->opacityTween);
				StopTween(state->slideTween);
				StopTween(state->positionTween);
				return true;
			}
		);
	}

	void EditorNotification::StartExitAnimation(
		const std::shared_ptr<NotificationState>& notification
	) const {
		if (!notification || notification->isExiting) {
			return;
		}

		notification->isExiting = true;
		StopTween(notification->opacityTween);
		StopTween(notification->slideTween);

		if (!mTweenManager) {
			notification->opacity        = 0.0f;
			notification->pendingRemoval = true;
			return;
		}

		// 完了コールバックが通知の寿命を延長しないよう弱参照だけを保持する
		const std::weak_ptr weakNotification = notification;
		const auto          fadeTween        = mTweenManager->CreateTo(
			notification->opacity, 0.0f, kExitDurationSeconds
		);
		fadeTween->SetEaseCubicBezier(kP1, kP2).OnComplete(
			[weakNotification] {
				if (const auto locked = weakNotification.lock()) {
					locked->pendingRemoval = true;
				}
			}
		);
		notification->opacityTween = TweenHandle(fadeTween);
	}
}
#endif
