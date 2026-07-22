#include "UiDocument.h"

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Gui {
	namespace {
		constexpr std::string_view kChannel = "UiDocument";
	}

	UiDocument::UiDocument() = default;

	UiDocument::~UiDocument() = default;

	bool UiDocument::Save(const Path& path) const {
		JsonWriter writer(path);

		writer.BeginObject();
		writer.Key("version");
		writer.Write(2);
		writer.Key("name");
		writer.Write(mName);

		if (mRootWidget) {
			writer.Key("root");
			mRootWidget->SaveToJson(writer);
		}

		writer.EndObject();

		return writer.Save();
	}

	std::shared_ptr<UiDocument> UiDocument::Load(
		const Path& path, const UiDeserializeContext& context
	) {
		const JsonReader reader(path);
		if (!reader.Valid()) {
			Error(kChannel, "Failed to open UI document '{}'.", path);
			return nullptr;
		}
		return LoadFromJson(reader, path.ToGenericUtf8(), context);
	}

	std::shared_ptr<UiDocument> UiDocument::LoadFromJson(
		const JsonReader&  reader,
		const std::string& sourceLabel,
		const UiDeserializeContext& context
	) {
		if (!reader.Valid()) {
			Error(kChannel, "Invalid UI document source: '{}'.", sourceLabel);
			return nullptr;
		}

		const JsonReader versionNode = reader["version"];
		if (!versionNode.Valid() || versionNode.GetInt() != 2) {
			Error(
				kChannel,
				"UI document '{}' is not supported. version=2 only.",
				sourceLabel
			);
			return nullptr;
		}

		auto doc = std::make_shared<UiDocument>();
		if (reader.Has("name")) {
			doc->SetName(reader["name"].GetString());
		}

		if (!reader.Has("root")) {
			Error(kChannel, "UI root is missing: '{}'.", sourceLabel);
			return nullptr;
		}

		const JsonReader rootNode   = reader["root"];
		auto rootWidget = UiWidget::CreateFromJson(rootNode, context);
		if (!rootWidget) {
			Error(kChannel, "UI root parse failed: '{}'.", sourceLabel);
			return nullptr;
		}

		doc->SetRootWidget(std::move(rootWidget));
		return doc;
	}

	void UiDocument::SetName(const std::string& name) {
		mName = name;
	}

	const std::string& UiDocument::GetName() {
		return mName;
	}

	void UiDocument::SetRootWidget(std::unique_ptr<UiWidget> rootWidget) {
		mRootWidget = std::move(rootWidget);
		mNamedWidgets.clear();
		RegisterWidgetRecursive(mRootWidget.get());
	}

	UiWidget* UiDocument::GetRootWidget() const {
		return mRootWidget.get();
	}

	void UiDocument::RegisterNamedWidget(
		const std::string& id, UiWidget* widget
	) {
		mNamedWidgets[id] = widget;
	}

	UiWidget* UiDocument::FindWidgetById(const std::string& id) {
		const auto it = mNamedWidgets.find(id);
		if (it != mNamedWidgets.end()) {
			return it->second;
		}
		return nullptr;
	}

	std::unique_ptr<UiWidget> UiDocument::TakeRootWidget() {
		return std::move(mRootWidget);
	}

	void UiDocument::RegisterWidgetRecursive(UiWidget* widget) {
		if (!widget) {
			return;
		}

		const auto name = widget->GetName();
		if (!name.empty()) {
			RegisterNamedWidget(std::string(name), widget);
		}

		for (const auto& child : widget->GetChildren()) {
			RegisterWidgetRecursive(child.get());
		}
	}
}
