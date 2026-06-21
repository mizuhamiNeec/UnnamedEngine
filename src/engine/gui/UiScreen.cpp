#include "UiScreen.h"

namespace Unnamed::Gui {
	UiScreen::UiScreen(std::shared_ptr<UiDocument> document) : mDocument(
		std::move(document)
	) {
	}

	UiScreen::~UiScreen() = default;

	UiDocument* UiScreen::GetDocument() const {
		return mDocument.get();
	}

	void UiScreen::OnShow() {
	}

	void UiScreen::OnHide() {
	}

	void UiScreen::OnUpdate(const float deltaTime) {
		(void)deltaTime;
	}
}
