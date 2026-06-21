#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "core/filesystem/Path.h"
#include "UiWidget.h"

namespace Unnamed::Gui {
	class UiDocument {
	public:
		UiDocument();
		~UiDocument();

		bool                               Save(const Path& path) const;
		static std::shared_ptr<UiDocument> Load(const Path& path);
		static std::shared_ptr<UiDocument> LoadFromJson(
			const JsonReader& reader, const std::string& sourceLabel
		);

		void               SetName(const std::string& name);
		const std::string& GetName();

		void SetRootWidget(std::unique_ptr<UiWidget> rootWidget);

		[[nodiscard]] UiWidget* GetRootWidget() const;

		void RegisterNamedWidget(const std::string& id, UiWidget* widget);

		UiWidget* FindWidgetById(const std::string& id);

		std::unique_ptr<UiWidget> TakeRootWidget();

	private:
		std::string                                mName;
		std::unique_ptr<UiWidget>                  mRootWidget;
		std::unordered_map<std::string, UiWidget*> mNamedWidgets;

		void RegisterWidgetRecursive(UiWidget* widget);
	};
}
