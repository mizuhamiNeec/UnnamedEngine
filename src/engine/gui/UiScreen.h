#pragma once
#include <memory>

#include "UiDocument.h"

namespace Unnamed::Gui {
	/// @brief UiScreenは、画面単位のUiRootと表示ライフサイクルを所有します
	class UiScreen {
	public:
		UiScreen(std::shared_ptr<UiDocument> document);

		virtual ~UiScreen();

		[[nodiscard]] UiDocument* GetDocument() const;

		virtual void OnShow();
		virtual void OnHide();
		virtual void OnUpdate(float deltaTime);

	private:
		std::shared_ptr<UiDocument> mDocument;
	};
}
