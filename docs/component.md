# **コンポーネント**

コンポーネントは [Entity](entity.md)
をどう振る舞わせるかを定義するもので、ゲーム内のロジックやデータを実装するための基本的な機能です。

独自のコンポーネントを `C++` で実装でき、ゲームに必要な機能を追加できます。

## **コンポーネントを新規作成**

コンポーネントは `BaseComponent` を継承して実装します。以下の手順で新しいコンポーネントを作成できます。

### 1. ヘッダー・ソース・ファイルの作成

- 任意の場所に `MyComponent.h` と `MyComponent.cpp` を作成します。

### 2. クラス定義

`MyComponent` クラスを `BaseComponent` から継承して定義します。

#### **ヘッダーファイル(`MyComponent.h`)**

```cpp linenums="1" title="MyComponent.h"
#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

// 独自の名前空間を使用する場合は注意
namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	/// @brief サンプル用の独自コンポーネントです。
	class MyComponent final : public BaseComponent {
	public:
		/// @brief デバッグログを出力します。
		void HelloWorld() const;

		// ---- BaseComponent ------------------------------------------------
		/// @brief シーンファイル上で使用する安定名を返します。
		[[nodiscard]] std::string_view GetStableName() const override;

		/// @brief エディター表示名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;

		/// @brief 毎フレーム呼び出される更新処理です。
		void OnTick(float deltaTime) override;

#ifdef _DEBUG
		/// @brief エディターのインスペクターUIを描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief Jsonからコンポーネント値を復元します。
		void Deserialize(const JsonReader& reader) override;

		/// @brief コンポーネント値をJsonへ保存します。
		void Serialize(JsonWriter& writer) const override;

	private:
		float mMoveSpeed = 240.0f;
		bool  mEnableLog = true;
	};
}
```

#### **ソースファイル(`MyComponent.cpp`)**

```cpp linenums="1" title="MyComponent.cpp"
#include "MyComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	namespace {
		float ReadFloatOr(const JsonReader& reader, const char* key, float fallback) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}

		bool ReadBoolOr(const JsonReader& reader, const char* key, bool fallback) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetBool() : fallback;
		}
	}

	void MyComponent::HelloWorld() const {
		Msg(GetComponentName().data(), "Hello, World! speed=%.2f", mMoveSpeed);
	}

	std::string_view MyComponent::GetStableName() const {
		// '.' でエディター上の階層を分けられます。
		return "game.MyComponent";
	}

	std::string_view MyComponent::GetComponentName() const {
		return "MyComponent";
	}

	void MyComponent::OnTick(float deltaTime) {
		if (!mEnableLog) {
			return;
		}

		// 所有エンティティが未設定の場合もあるため安全に確認します。
		const Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		DevMsg(
			GetComponentName().data(),
			"Owner=%s Delta=%.3f Speed=%.2f",
			owner->GetName().data(),
			deltaTime,
			mMoveSpeed
		);
	}

#ifdef _DEBUG
	void MyComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Enable Log", &mEnableLog);
		ImGui::DragFloat("Move Speed", &mMoveSpeed, 1.0f, 0.0f, 10000.0f);
	}
#endif

	void MyComponent::Deserialize(const JsonReader& reader) {
		mMoveSpeed = ReadFloatOr(reader, "moveSpeed", mMoveSpeed);
		mEnableLog = ReadBoolOr(reader, "enableLog", mEnableLog);
	}

	void MyComponent::Serialize(JsonWriter& writer) const {
		writer.Key("moveSpeed");
		writer.Write(mMoveSpeed);
		writer.Key("enableLog");
		writer.Write(mEnableLog);
	}

	REGISTER_COMPONENT(MyComponent);
}
```

!!! danger "重要"
	`REGISTER_COMPONENT(MyComponent);` を忘れずに記述してください。これがないとエンジンがコンポーネントを認識できません。

## **コンポーネントをエンティティーへ追加**

エンティティにコンポーネントを追加する方法は以下の通りです。

### 1. エディターを使用

レベルエディタ内で任意のエンティティを選択し、インスペクタの「Add
Component」ボタンから追加したいコンポーネントを選択します。

TODO: 画像を追加

### 2. コードで追加

コンポーネントから動的にコンポーネントを追加したい場合は、以下のようにコードで追加できます。

```cpp
#include "MyComponent.h" // 追加したいコンポーネントのヘッダをインクルード
#include "engine/unnamed/framework/entity/Entity.h"

void ComponentName::OnAttached() {
	Entity* owner = GetOwner();
	if (!owner) {
		return;
	}

	// オーナー(Entity) に MyComponent を追加
	owner->AddComponent<MyComponent>();
}
```

### 3. シーンファイルに直接記述(※非推奨)

!!! warning "注意"
シーンファイルはJson形式なので、直接編集することも可能ですが、エディタでの管理が推奨されます。直接編集する場合は、以下のようにコンポーネントを記述します。

```json
{
	"entities": [
		{
			"name": "MyEntity",
			"components": [
				{
					"type": "game.MyComponent",
					"data": {
						"moveSpeed": 320.0,
						"enableLog": true
					},
					"guid": "114514"
				}
			]
		}
	]
}
```
