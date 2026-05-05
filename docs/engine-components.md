# エンジン内蔵コンポーネント

このページは `src/engine` 配下に実装されている、エンジン標準のコンポーネントをまとめたものです。  
対象は 2 系統です。

- `BaseComponent` 継承の Entity コンポーネント（シーン JSON の `components[].type`）
- `UiComponent` 継承の UI ウィジェット用コンポーネント（UI JSON の `components[].type`）

## 1. 登録の仕組み

### Entity コンポーネント（`BaseComponent`）

- 自動登録: `REGISTER_COMPONENT(...)` マクロによる `ComponentRegistry` 自動登録
- 既定登録: `RegisterDefaultEngineComponents(...)` で標準セットを保証
  - 実装: `src/engine/EngineComponentRegistration.cpp`
- ゲーム固有登録: `IGameModule::RegisterGameComponents(...)` で明示登録
  - 実装例: `<projects-root>/TeamGame/runtime/game/team/runtime/TeamGameComponentRegistration.cpp`
  - 補足: `REGISTER_COMPONENT(...)` のみだとリンク構成次第で静的初期化が到達しない場合があるため、明示登録を正規経路として運用します。

`stableName` は永続化キーなので、シーン JSON の `type` と完全一致が必要です。

### UI コンポーネント（`UiComponent`）

- `UiWidget::CreateComponentByTypeName` で `typeName` 文字列から生成
- 登録済み一覧は `UiWidget::GetRegisteredComponentTypeNames` を参照
  - 実装: `src/engine/gui/UiWidget.cpp`

## 2. Entity コンポーネント一覧（`BaseComponent`）

### 2.1 デフォルト登録される標準セット

| stableName | クラス | 主用途 | Tick Group | 主なシリアライズ項目 |
|---|---|---|---|---|
| `engine.Transform` | `TransformComponent` | 位置・回転・スケール、親子階層、描画補間 | `KINEMATIC_SOURCE` | `position`, `rotation`, `scale`, `parentEntityGuid` |
| `game.Camera` | `CameraComponent` | View/Proj 構築、カレントカメラ切り替え | `GAMEPLAY` | `fovYDegrees`, `nearZ`, `farZ`, `exposureEv`, `cameraActive` |
| `engine.Skybox` | `SkyboxComponent` | スカイボックス設定 | `GAMEPLAY` | `texturePath`, `intensity` |
| `engine.StaticMeshRenderer` | `StaticMeshRendererComponent` | スタティックメッシュ描画 | `GAMEPLAY` | `meshPath`, `materialSlots[]`, `materialInstancePath`(互換) |
| `engine.StaticMeshCollider` | `StaticMeshColliderComponent` | メッシュコライダー同期 | `COLLIDER_SYNC` | `enabled`, `dynamic` |
| `engine.SkeletalMeshRenderer` | `SkeletalMeshRendererComponent` | スケルタルメッシュ描画 | `GAMEPLAY` | `meshPath`, `materialSlots[]`, `materialInstancePath`(互換) |
| `engine.SkeletalAnimation` | `SkeletalAnimationComponent` | 多層アニメ再生、状態遷移、スキニングパレット生成 | `GAMEPLAY` | `layers[]`, `stateMap[]`, 旧形式 `clipName/playOnStart/loop/speed` |
| `engine.UiCanvas` | `UiCanvasComponent` | UI ルートの 2D/3D 配置と実行管理 | `GAMEPLAY` | `uiAssetPath`, `spaceMode`, `billboardDepthMode`, `pixelSize`, `worldSize`, `sortKey`, `receiveInput` |
| `engine.AudioSource` | `AudioSourceComponent` | サウンド再生（Play/Stop/Pause/Resume） | `GAMEPLAY` | `soundPath`, `playOnStart`, `loop`, `volume`, `pitch` |
| `engine.SequenceDirector` | `SequenceDirectorComponent` | Sequence 再生制御、完了時処理、対象コンポーネントロック | `GAMEPLAY` | `sequencePath`, `playOnAttach`, `autoStopWhenCompleted`, `playRate`, `loop`, `completionMode`, `applyComponentLocks`, `lockTargets[]` |

### 2.2 エディタ補助コンポーネント

| stableName | クラス | 用途 |
|---|---|---|
| `engine.EditorCamera` | `EditorCameraComponent` | エディタ時のフリーカメラ移動と視点制御 |

`EditorCameraComponent` は `_DEBUG` 前提の利用を想定したエディタ用コンポーネントです。

## 3. UI コンポーネント一覧（`UiComponent`）

`UiWidget` に登録済みの `typeName` は以下です。

| typeName | クラス | 主用途 |
|---|---|---|
| `Transform` | `UiTransformComponent` | Rect/Anchor/Margin/Pivot/SizePolicy の適用 |
| `VerticalLayout` | `UiVerticalLayoutComponent` | 縦レイアウト（padding/spacing） |
| `HorizontalLayout` | `UiHorizontalLayoutComponent` | 横レイアウト（padding/spacing） |
| `PanelStyle` | `UiPanelStyleComponent` | 背景色、角丸、ボーダー描画 |
| `ButtonBehavior` | `UiButtonBehaviorComponent` | ボタン描画、ホバー/押下状態、クリック通知 |
| `Texture` | `UiTextureComponent` | テクスチャ描画（UV/色/回転対応） |
| `DigitStrip` | `UiDigitStripComponent` | 0-9 ストリップ画像による数値描画 |

補足:

- `UiWidget` は生成時に `Transform` を自動付与します。
- `Transform` は削除不可・先頭維持です（移動先 0 指定時も保護されます）。

## 4. 実装・運用時の注意

- `DrawInspectorImGui` を含む ImGui 依存コードは `#ifdef _DEBUG` で囲むこと。`Develop/Release` では ImGui 実装がリンクされません。
- `stableName`/`typeName` はアセット互換性のキーです。既存データがある状態での変更は避けてください。
- シーン側では `game.*` / `engine.*` が混在します。新規追加時は「どの Runtime が登録責務を持つか」を先に決めると運用が安定します。
- `game_profile.json` は profile/path 解決の仕組みであり、Runtime 実体の自動ロード機構ではありません。App 側 `RegisterGameModule(...)` 登録が未実施だと `DefaultGameModule` が生成され、ゲーム固有 Runtime 実装は使われません。

## 5. 参照実装

- `src/core/ComponentRegistry.h`
- `src/core/ComponentRegistry.cpp`
- `src/engine/EngineComponentRegistration.cpp`
- `src/engine/unnamed/framework/components/`
- `src/engine/gui/components/`
- `src/engine/gui/UiWidget.cpp`
