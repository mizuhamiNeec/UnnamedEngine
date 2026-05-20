# **Component**

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
#include "BaseComponent.h"

namespace MyGame {
    class MyComponent : public BaseComponent {
public:
    // -----------------------------------------------------------------------
    // MyComponentの独自のメンバ関数やプロパティをここに定義
    // 例: 独自の関数
    void HelloWorld();
    // -----------------------------------------------------------------------

    // 必要に応じてBaseComponentの仮想関数をオーバーライド
    // void OnAttached() override;
    // void OnTick(float deltaTime) override;
    // void OnDetached() override;

    // -----------------------------------------------------------------------
    // オーバーライド必須関数↓
    // -----------------------------------------------------------------------

    // シーンファイルからのパースに必要です。
    [[nodiscard]] std::string_view GetStableName() const override;

    // エディターやログに表示するコンポーネント名。重複は避けてください。
    [[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
    void DrawInspectorImGui() override;
#endif

	/// @brief コンポーネントの値を読み込む際に使用されます。
	void Deserialize(const JsonReader& reader) override;

	/// @brief コンポーネントの値を書き込む際に使用されます。
	void Serialize(JsonWriter& writer) const override;
};
}
```

```cpp linenums="1" title="MyComponent.cpp"
#include "MyComponent.h"

#ifdef _DEBUG
#include "imgui.h" // ImGui
#endif

namespace MyGame {

    void MyComponent::HelloWorld() {
        Msg(GetComponentName(), "Hello, World!");
    }

    std::string_view MyComponent::GetStableName() const {
        // .でメニュー階層を下げることができます。
        // mygame.foo.bar.MyComponent のように分けられます。
        return "mygame.MyComponent"; 
    }

    std::string_view MyComponent::GetComponentName() const {
        return "MyComponent";
    }

#ifdef _DEBUG
    void MyComponent::DrawInspectorImGui(){
        // インスペクタに表示するImGuiコードをここに記述します。
        ImGui::Text("This is My First Component!");
        if(ImGui::Button("Click Me")) {
            HelloWorld();
        }
    }
#endif

    !!! danger "重要"
        REGISTER_COMPONENT() マクロを忘れずに呼び出してください。これがないとエンジンがコンポーネントを認識できません。
    REGISTER_COMPONENT(MyComponent);
}
```

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

void ComponentName::OnAttached() {
    // オーナー(Entity) に MyComponent を追加
    GetOwner()->AddComponent<MyComponent>();
}
```

### 3. シーンファイルに直接記述(基本的には非推奨)

!!! warning "注意"
シーンファイルはJson形式なので、直接編集することも可能ですが、エディタでの管理が推奨されます。直接編集する場合は、以下のようにコンポーネントを記述します。
もしエディターで何らかの不具合が発生した場合は、管理者にお知らせをお願いいたします。

```json
{
    "entities": [
        {
            "name": "MyEntity",
            "components": [
                {
                    "type": "mygame.MyComponent", // コンポーネントの stableName
                    "data": {
                        // コンポーネントのプロパティをここに記述
                    },
                    "guid": "114514" // コンポーネントのGUID()
                }
            ]
        }
    ]
}
```