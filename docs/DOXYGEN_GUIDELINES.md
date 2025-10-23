# Doxygen コメント作成ガイドライン

このドキュメントは、UnnamedEngineのソースコードにDoxygenコメントを追加する際のガイドラインです。

## 基本方針

### コメントを追加する場所
- **クラス宣言**: `.h` ファイルのクラス定義の直前
- **関数定義**: `.cpp` ファイルの関数実装の直前
- **ヘッダーオンリー関数**: `.h` ファイルの関数定義の直前

### コメント形式
日本語でDoxygenフォーマットを使用します。

## 完了済みファイル例

参考のために、既にコメントが追加されたファイル：
- `src/core/UnnamedMacro.h/cpp`
- `src/core/containers/RingBuffer.h`
- `src/core/guidgenerator/GUIDGenerator.h/cpp`
- `src/runtime/core/math/Vec2.h`
- `src/runtime/core/math/Vec3.h`
- `src/runtime/core/math/Quaternion.h`
- `src/engine/Components/Base/Component.h/cpp`
- `src/engine/Entity/Entity.h`
- `src/game/components/CameraRotator.h/cpp`

これらのファイルを参考にして、一貫性のあるコメントスタイルを維持してください。
