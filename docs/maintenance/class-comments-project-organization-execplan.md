# Class comments and project organization remediation ExecPlan

## 1. Authority and status rules

This plan records the remediation requested by the independent final review. The
previous Acceptance Matrix, generated ledger counts, verifier PASS, and prior build
results are not accepted as evidence.

- PASS: independently rechecked after the remediation and no Finding remains.
- FAIL: a Finding remains.
- UNVERIFIED: the required runtime, generated-project, or design evidence has not yet
  been obtained.

The verifier is evidence for objective lexical checks only. It cannot mark semantic
comment quality, Concepts design, or PO-01 through PO-03 as PASS.

## 2. Working-tree boundary

The 18 paths that were staged before this remediation are user-owned and are not edited,
unstaged, discarded, or included as remediation evidence. `git mv` necessarily staged
the two requested source moves; those new index entries are tracked separately from the
pre-existing staged set. No commit, push, branch creation, third-party edit, or external
dependency change is authorized.

Gitのstage、unstage、commitはこの作業の対象外です。Git状態は検証実行時に
取得しますが、変化し続けるworking tree snapshotはJSON台帳へ永続化しません。
必須成果物がnon-ignoredかつ未追跡ならverifierは失敗します。

## 3. Fresh type and comment audit

The repository inventory comes from both `git ls-files` and
`git ls-files --others --exclude-standard`. First-party C++ is every repository C++
source outside explicit vendored/generated roots. The lexical scanner skips comments,
normal and raw strings, forward declarations, and definitely inactive `#if 0` branches;
it handles nested/local/anonymous-namespace/template/attribute/export-macro declarations.
Macro-expanded declarations remain UNVERIFIED because the tool does not run a compiler
preprocessor or AST.

Current source status before final ledger regeneration:

| Item | Result |
|---|---|
| Surviving added/changed type briefs | 397 (the obsolete `GameWorld` definition was removed) |
| Missing comments on detected first-party definitions | 0 |
| Added/changed one-line briefs ending in `。` | 0 |
| Added/changed briefs not ending in `します` | 0 |
| Added/changed exact duplicate briefs | 0 |
| Added/changed forbidden generic briefs | 0 |
| Meaning review | 397件すべてについて型定義、主要メンバ、public API、直接利用箇所を確認し、個別semantic review recordへ根拠を記録します |

The independent review identified 239 classes and 425 structs before removal of obsolete
files. The final counts are regenerated from the current tree and reported in the JSON
ledger and final verification section rather than copied from the previous ledger.

### Semantic-review recording

JSON台帳の`semantic_reviews`は、397件の変更済み定義ごとに責務、重要契約、
修正前後brief、確認根拠を保存します。`syntax_status`と`style_status`はverifierが
字句的に検査しますが、`semantic_status`の`verified`または`rewritten`は実装確認を
行った台帳記録だけに用います。マクロ展開型はcompiler ASTを使用しないため
UNVERIFIEDです。

## 4. Concepts decisions

Concept design remains a manual decision. The tool checks only that each detected class
template has a recorded decision.

| Template | Decision | Rationale |
|---|---|---|
| `RingBuffer<T, Capacity>` | ADDED | The existing `std::array<T, Capacity>` storage requires default construction, while `Push(const T&)` and `Pop(T&)` require assignment from `const T&`; `Capacity > 0` prevents modulo-by-zero. This intentionally does not claim move-only support. |
| `UploadBuffer<T>` | NOT_ADDED | `trivially_copyable` is necessary for byte copying but not sufficient for GPU/HLSL ABI compatibility. There is no current use that establishes a stable public contract; alignment/layout traits are deferred. |
| `TweenInstance<TValue>` | NOT_ADDED | `TweenLerp` is a customization point, but the removed constraint did not fully express construction and assignment and could reject future interpolated types. |
| `UiAnimatedValue<T>` | NOT_ADDED | `copyable` can be stronger than the implementation, default construction was omitted, and operator syntax does not express interpolation semantics. |
| `ConVar<T>` | NOT_ADDED | Supported types remain duplicated across parsing, writing, dynamic casts, and type tests, and `Vec3` persistence is not a completed unified contract. A codec/traits consolidation is a separate task. |

Final expected Concept additions from this remediation: one (`RingBuffer`).

## 5. Project ownership and physical boundaries

| Finding | Resolution | Evidence/status |
|---|---|---|
| `EditorLuaSystem` in runtime targets | Moved with `git mv` to `src/engine/editor/lua`; runtime includes were removed and only editor callers remain | Generated `.vcxproj` membership check pending |
| `src/core/ComponentRegistry.*` depends on Engine `BaseComponent` | Moved with `git mv` to `src/engine/ComponentRegistry.*`; all includes now target the Engine path | Include/cycle search complete; build pending |
| Parkour targets in Engine group | `ParkourGameRuntime` and `ParkourGameRuntimeEditor` now use `Game/Parkour` | Generated Solution group check pending |
| `UnnamedEditorRuntime` in Engine group | Moved to `Editor`; `UnnamedEditorApp` uses `Editor/Applications` | Generated Solution group check pending |
| ImGui/ImGuizmo ownership | Physical sources remain under `src/thirdparty`; `UnnamedEditorRuntime` directly compiles them as its editor integration unit | PO-01 is constrained, not a full separation PASS |

`src/core` is not perfectly Engine-independent: asset/JSON implementations still use
Engine logging, profiling, or service lookup. Moving `ComponentRegistry` removes the
specific Engine component-type dependency but does not prove PO-02/PO-03 globally.

## 6. Previously unassigned C++ files

| File | Decision | References and duplicate analysis |
|---|---|---|
| `src/app/GameProfileLoader.h` | Assigned to both app targets through `AppLaunchFiles()` | Included by `src/app/main.cpp`; header-only implementation |
| `src/app/runtime/TeamGameRuntimeApiEntry.cpp` | Deleted as orphaned | No references; included a TeamGame header absent from this repository after externalization |
| `src/engine/game/GameModuleRegistry.cpp` | Deleted as broken duplicate | `src/app/GameModuleRegistry.cpp` is the compiled canonical implementation; the unused copy called undeclared `NormalizeModuleName` |
| `src/engine/world/GameWorld.h` | Deleted as obsolete duplicate | No references; all runtime worlds are supplied through `IGameWorldFactory`, with Parkour using `ParkourGameWorld` |
| `src/engine/world/GameWorld.cpp` | Deleted with its unused header | Explicitly excluded from both Engine runtime targets and duplicated Parkour world behavior |

No one of these five files remains an unjudged Solution orphan.

## 7. TeamGame compatibility and scope corrections

`tools/generate-teamgame-only.ps1` is restored as a deprecated compatibility stub. It
emits a warning and migration command, infers the game repository from `-ProjectsRoot`,
delegates to that repository's `premake5.exe vs2026`, and exits nonzero when safe
delegation is impossible. `docs/setup.md` records the migration.

The comments added by this change to `tools/newgame.ps1` were confirmed by `git diff`
and removed. No runtime serialization format, asset format, or public ABI was changed.
The removal of four unassigned implementation files is directly tied to FIX-06.

## 8. Verification tool boundary

`tools/verify_repository_requirements.py` was reduced from a comment generator/design
approver to objective checks:

- inventory includes tracked and non-ignored untracked files;
- required artifactsは存在とGit追跡状態を別々に検査し、non-ignoredな未追跡を失敗にする;
- Git path inventoryは`git ls-files -z`および`git ls-files --others --exclude-standard -z`
  を使用し、非ASCII、空白、引用符、バックスラッシュをquote parsingせず処理する;
- fresh type/comment rows and counts are compared with the JSON ledger;
- only `@brief` presence, terminal `。`, forbidden exact generic text, and exact
  duplicates on added/changed type briefs are enforced;
- Concepts are decision records, not design PASS;
- semantic comment quality、Concept設計、PO-01 through PO-03、macro expansion、
  target ownershipは自動PASSの対象外とする;
- Git/subprocess failures produce concise nonzero errors;
- fixtures cover documented/undocumented tracked types, untracked types, attributes,
  export macros, forward declarations, raw strings, `#if 0`, stale ledger, and
  subprocess failure.

## 9. Required final verification

| Check | Status |
|---|---|
| Fresh class/struct audit and JSON comparison | PASS: 238 class, 425 struct, zero missing comments, semantic review record 397件 |
| Added/changed brief policy checks | PASS: 397 surviving briefs; terminal `。` 0, non-`します` 0, forbidden generic 0, exact duplicate 0 |
| Verifier fixtures | PASS: tracked/untracked、必須成果物追跡、非ASCII path、missing tracked、句点、禁止定型、重複、stale ledger、subprocess失敗を検査 |
| Git必須成果物 | UNVERIFIED/FAIL: ExecPlan、JSON台帳、verifier、fixtureは現在未追跡のためverifierは意図どおり非0 |
| `git diff --check`, status, stat, name-status | PASS: all commands executed; `diff --check` emitted no errors |
| Tracked forbidden outputs | PASS: 1,027 tracked files checked, zero forbidden outputs |
| Normal project generation | PASS: `generateallprojects.ps1` |
| Engine-only generation | PASS: only Engine, Editor runtime, tests, and third-party targets in `Unnamed.slnx` |
| Normal Solution restoration | PASS: normal Solution regenerated after engine-only inspection |
| Debug x64 Solution build | PASS: zero warnings/errors |
| Develop x64 Solution build | PASS: zero warnings/errors |
| Release x64 Solution build | PASS: zero warnings/errors |
| Existing tests | PASS: `UnnamedAssetPathContractTests` passed in all three configurations |
| Launcher Debug/Develop/Release startup validation | PASS: each `--validate-startup-only` process exited 0 |
| Editor startup validation | PASS: Debug `--validate-startup-only` exited 0 |
| Generated `.vcxproj` file ownership | PASS: EditorLuaSystem=EditorRuntime only; ComponentRegistry=two Engine runtimes; GameProfileLoader=two apps; deleted orphans absent |
| Generated Solution groups | PASS: Parkour=`Game/Parkour`; Editor runtime=`Editor`; Editor app=`Editor/Applications` |

The first direct Debug invocation stopped before compilation with MSB6001 because the
host environment contained both `Path` and `PATH`. All three reported builds were rerun
with a deduplicated child environment, `/m:1`, and node reuse disabled; those runs are
the build evidence above.

<!-- BEGIN GENERATED REPOSITORY LEDGER -->

## Generated class/struct inventory

| File | Line | Kind | Name | Declaration | Source | Brief | Syntax | Style | Concept decision |
|---|---:|---|---|---|---|---|---|---|---|
| projects/ParkourGame/runtime/game/core/collision/kinematic/BoxKinematicCollisionResolver.h | 8 | class | Unnamed::BoxKinematicCollisionResolver | definition | tracked | BoxKinematicCollisionResolverは、box sweepとpenetration queryから接触・補正結果を解決します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/collision/kinematic/BoxKinematicCollisionResolver.h | 21 | struct | Unnamed::BoxKinematicCollisionResolver::CollisionDebugState | definition | tracked | CollisionDebugStateは、衝突解決の接触点と補正ベクトルをdebug描画用に保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h | 18 | struct | Unnamed::KinematicContact | definition | tracked | KinematicContactは、接触点、法線、貫通量、衝突対象を1件のkinematic接触結果として保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h | 26 | struct | Unnamed::KinematicMoveQuery | definition | tracked | KinematicMoveQueryは、キネマティック衝突解決照会の入力条件を一単位として呼び出し先へ渡します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h | 40 | struct | Unnamed::KinematicMoveResult | definition | tracked | KinematicMoveResultは、解決後の位置・速度、接地・衝突状態、移動時間、接触列を返します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h | 53 | class | Unnamed::BaseKinematicCollisionResolver | definition | tracked | 衝突解決の基底クラスです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/AudioFxControllerComponent.h | 11 | class | Unnamed::AudioSourceComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/AudioFxControllerComponent.h | 12 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/AudioFxControllerComponent.h | 13 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/AudioFxControllerComponent.h | 16 | class | Unnamed::AudioFxControllerComponent | definition | tracked | AudioFxControllerComponentは、効果音presetの再生要求をAudioSourceへ仲介し、同時再生状態を管理します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/AudioFxControllerComponent.h | 19 | struct | Unnamed::AudioFxControllerComponent::OneShotPreset | definition | tracked | OneShotPresetは、単発効果音のsound asset、音量、pitch、同時再生制限を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.cpp | 30 | struct | Unnamed::<anonymous-namespace@28>::EaseNamePair | definition | tracked | EaseNamePairは、easing列挙値とEditor表示名の対応を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 16 | class | Unnamed::CameraComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 17 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 18 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 19 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 22 | class | Unnamed::CameraFxControllerComponent | definition | tracked | CameraFxControllerComponentは、camera shake、FOV、回転演出を合成してCameraへ適用します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 25 | struct | Unnamed::CameraFxControllerComponent::ShakePreset | definition | tracked | ShakePresetは、camera shakeの振幅、周波数、duration、easeを保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 34 | struct | Unnamed::CameraFxControllerComponent::FovPreset | definition | tracked | FovPresetは、FOV演出の目標角度、duration、easeを保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 41 | struct | Unnamed::CameraFxControllerComponent::RotationPreset | definition | tracked | RotationPresetは、camera回転演出の目標姿勢、duration、easeを保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 77 | struct | Unnamed::CameraFxControllerComponent::ActiveShake | definition | tracked | ActiveShakeは、再生中camera shakeのpreset、経過時間、乱数位相を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 89 | struct | Unnamed::CameraFxControllerComponent::ActiveFovAnim | definition | tracked | ActiveFovAnimは、再生中FOV演出の開始値、目標値、経過時間を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | 95 | struct | Unnamed::CameraFxControllerComponent::ActiveRotationAnim | definition | tracked | ActiveRotationAnimは、再生中camera回転演出の開始姿勢、目標姿勢、経過時間を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/CameraRotatorComponent.h | 14 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraRotatorComponent.h | 15 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraRotatorComponent.h | 16 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraRotatorComponent.h | 17 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraRotatorComponent.h | 18 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/CameraRotatorComponent.h | 21 | class | Unnamed::CameraRotatorComponent | definition | tracked | CameraRotatorComponentは、mouse・gamepad入力をyawとpitchへ変換し、Camera姿勢を更新します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/ViewmodelSway.h | 9 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/ViewmodelSway.h | 12 | class | Unnamed::ViewmodelSway | definition | tracked | ViewmodelSwayは、視点移動と回転から一人称モデルの位置・姿勢オフセットを計算します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.h | 12 | class | Unnamed::Physics::Engine | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.h | 16 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.h | 17 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.h | 18 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.h | 20 | class | Unnamed::ConVar | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.h | 23 | class | Unnamed::GameMovementComponent | definition | tracked | プレイヤーの移動を処理するコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.h | 115 | struct | Unnamed::GameMovementComponent::SupportCache | definition | tracked | SupportCacheは、接地追従に使う支持entity、支持面の速度・step変位、接地状態をframe間で保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/character/base/BaseCharacterComponent.h | 11 | class | Unnamed::GameMovementStateMachine | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/base/BaseCharacterComponent.h | 12 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/base/BaseCharacterComponent.h | 13 | class | Unnamed::BaseKinematicCollisionResolver | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/base/BaseCharacterComponent.h | 16 | struct | Unnamed::MovementFrameInput | definition | tracked | MovementFrameInputは、固定step中に使う移動方向、jump、crouch入力をsnapshotとして保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/character/base/BaseCharacterComponent.h | 34 | struct | Unnamed::DeterministicInputPacket | definition | tracked | 再現性のある入力パケット | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/base/BaseCharacterComponent.h | 41 | class | Unnamed::BaseCharacterComponent | definition | tracked | 衝突付きでの移動を処理するコンポーネントの基底クラスです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTransitionRouter.h | 7 | class | Unnamed::MovementTransitionRouter | definition | tracked | 遷移要求バッファから最終遷移先を解決します。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTypes.h | 10 | class | Unnamed::GameMovementComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTypes.h | 11 | class | Unnamed::BaseKinematicCollisionResolver | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTypes.h | 44 | struct | Unnamed::MovementCapabilitySet | definition | tracked | Ability有効/無効の設定です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTypes.h | 56 | struct | Unnamed::MovementTransitionRequest | definition | tracked | 遷移要求1件を表します。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTypes.h | 65 | struct | Unnamed::MovementModeState | definition | tracked | Modeの実行状態です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTypes.h | 72 | struct | Unnamed::MovementContext | definition | tracked | Mode/Ability実行で共有するコンテキストです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/state/GameMovementStateMachine.h | 12 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/state/GameMovementStateMachine.h | 15 | class | Unnamed::GameMovementStateMachine | definition | tracked | Locomotion ModeとAbilityを統合管理する状態機械です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/state/GameMovementStateMachine.h | 43 | struct | Unnamed::GameMovementStateMachine::AbilityEntry | definition | tracked | AbilityEntryは、移動abilityのshared ownershipと現在の有効状態を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/character/state/ability/CoreMovementAbilities.h | 7 | class | Unnamed::JumpMovementAbility | definition | tracked | 地上ジャンプを処理する基本Abilityです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/state/ability/CoreMovementAbilities.h | 21 | class | Unnamed::CrouchMovementAbility | definition | tracked | しゃがみ入力を管理する基本Abilityです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/state/interface/IMovementAbility.h | 6 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/state/interface/IMovementAbility.h | 9 | class | Unnamed::IMovementAbility | definition | tracked | 移動Abilityインターフェースです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/state/interface/IMovementMode.h | 6 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/state/interface/IMovementMode.h | 9 | class | Unnamed::IMovementMode | definition | tracked | Locomotion Modeインターフェースです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/AirMovementMode.h | 7 | struct | Unnamed::Physics::Hit | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/AirMovementMode.h | 11 | class | Unnamed::AirMovementMode | definition | tracked | 基本の空中移動Modeです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/GroundMovementMode.h | 7 | struct | Unnamed::Physics::Hit | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/GroundMovementMode.h | 11 | class | Unnamed::GroundMovementMode | definition | tracked | 基本の地上移動Modeです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/NoclipMovementMode.h | 7 | class | Unnamed::NoclipMovementMode | definition | tracked | デバッグ用のノークリップ移動Modeです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/common/PatrolPointComponent.h | 8 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/common/PatrolPointComponent.h | 9 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/common/PatrolPointComponent.h | 10 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/common/PatrolPointComponent.h | 13 | class | Unnamed::PatrolPointComponent | definition | tracked | PatrolPointComponentは、巡回点の次接続と待機設定をシーンへ保存・復元します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/common/RotatorComponent.h | 8 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/common/RotatorComponent.h | 9 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/common/RotatorComponent.h | 10 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/common/RotatorComponent.h | 13 | class | Unnamed::RotatorComponent | definition | tracked | RotatorComponentは、設定した回転軸と角速度で所有Transformを毎フレーム回転します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/controller/PlayerCharacterController.h | 12 | class | Unnamed::CameraRotatorComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/controller/PlayerCharacterController.h | 15 | class | Unnamed::PlayerCharacterController | definition | tracked | プレイヤーがキャラクターを制御するためのコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/controller/base/BaseCharacterController.h | 5 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/controller/base/BaseCharacterController.h | 6 | class | Unnamed::BaseCharacterComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/controller/base/BaseCharacterController.h | 10 | class | Unnamed::BaseCharacterController | definition | tracked | BaseCharacterComponentを制御するためのコントローラーの基底クラスです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/inventory/InventorySystemComponent.h | 17 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/inventory/InventorySystemComponent.h | 18 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/inventory/InventorySystemComponent.h | 19 | class | Unnamed::WorldItemComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/inventory/InventorySystemComponent.h | 22 | class | Unnamed::InventorySystemComponent | definition | tracked | アイテムの所持/装備/拾得/ドロップを管理するコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/inventory/InventorySystemComponent.h | 124 | struct | Unnamed::InventorySystemComponent::DeterministicActionInputPacket | definition | tracked | deterministic 入力を 1 ティック単位で保持するパケットです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/inventory/InventorySystemComponent.h | 131 | struct | Unnamed::InventorySystemComponent::InventoryEntry | definition | tracked | インベントリ 1 スロット分の保持データです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/inventory/WorldItemComponent.h | 10 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/inventory/WorldItemComponent.h | 11 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/inventory/WorldItemComponent.h | 14 | class | Unnamed::WorldItemComponent | definition | tracked | フィールド上に存在する拾得可能アイテムを表すコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.cpp | 45 | struct | Unnamed::EventPresentationGraphEditorState | definition | tracked | EventPresentationGraphEditorStateは、presentation graph editorの選択nodeと編集中linkを保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 17 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 18 | class | Unnamed::ComponentRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 19 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 20 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 21 | class | Unnamed::AudioFxControllerComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 22 | class | Unnamed::CameraFxControllerComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 23 | class | Unnamed::SkeletalAnimationComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 25 | struct | Unnamed::EventPresentationGraphEditorState | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | 29 | class | Unnamed::EventPresentationComponent | definition | tracked | GameplayCue に反応して高レベル Action を実行する v2 プレゼンテーションコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.cpp | 123 | class | Unnamed::<anonymous-namespace@22>::MeleeWeaponActionModule | definition | tracked | 近接アクション用のモジュールです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.cpp | 218 | class | Unnamed::<anonymous-namespace@22>::HitscanWeaponActionModule | definition | tracked | ヒットスキャン向けモジュールです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.cpp | 307 | class | Unnamed::<anonymous-namespace@22>::ProjectileWeaponActionModule | definition | tracked | プロジェクタイル発生向けモジュールです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.cpp | 367 | class | Unnamed::<anonymous-namespace@22>::DeployableWeaponActionModule | definition | tracked | 設置型（地雷等）アクション向けモジュールです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.cpp | 443 | class | Unnamed::<anonymous-namespace@22>::UtilityWeaponActionModule | definition | tracked | ユーティリティ用途モジュールです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 17 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 18 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 19 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 20 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 21 | class | Unnamed::InventorySystemComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 24 | struct | Unnamed::WeaponModuleRuntime | definition | tracked | 武器モジュールの実行時状態です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 30 | struct | Unnamed::WeaponActionRuntimeContext | definition | tracked | 武器モジュールが参照する実行コンテキストです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 44 | class | Unnamed::IWeaponActionModule | definition | tracked | 武器挙動差し替え用モジュールIFです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 79 | class | Unnamed::WeaponSystemComponent | definition | tracked | 武器実行専任のコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 133 | struct | Unnamed::WeaponSystemComponent::DeterministicActionInputPacket | definition | tracked | deterministic 入力を 1 ティック分保持するパケットです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | 140 | struct | Unnamed::WeaponSystemComponent::WeaponRuntimeSlot | definition | tracked | アイテムインスタンスごとの武器モジュール実行状態です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/input/CharacterActionFrameInput.h | 7 | struct | Unnamed::ActionTriggerInput | definition | tracked | 汎用アクションのトリガー状態を保持します。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/input/CharacterActionFrameInput.h | 15 | struct | Unnamed::WeaponActionInput | definition | tracked | 武器/ツール共通の入力状態を保持します。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/input/CharacterActionFrameInput.h | 25 | struct | Unnamed::CharacterActionFrameInput | definition | tracked | キャラクターが固定ティックで消費するアクション入力パケットです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/input/CharacterActionFrameInput.h | 30 | class | Unnamed::ICharacterActionInputReceiver | definition | tracked | キャラクター固有アクション入力の受け口です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/item/ItemTypes.h | 9 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/item/ItemTypes.h | 10 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/item/ItemTypes.h | 29 | struct | Unnamed::WeaponItemDefinition | definition | tracked | 武器動作向けの定義データです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/item/ItemTypes.h | 40 | struct | Unnamed::ItemDefinition | definition | tracked | 静的なアイテム定義データです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/item/ItemTypes.h | 53 | struct | Unnamed::ItemInstance | definition | tracked | 動的なアイテムインスタンスです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationExecutor.h | 12 | class | Unnamed::AudioFxControllerComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationExecutor.h | 13 | class | Unnamed::CameraFxControllerComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationExecutor.h | 14 | class | Unnamed::SkeletalAnimationComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationExecutor.h | 17 | class | Unnamed::EventPresentationExecutor | definition | tracked | Event Presentation v2 の Action 実行器です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationExecutor.h | 28 | struct | Unnamed::EventPresentationExecutor::ExecutionContext | definition | tracked | 1回の Cue 処理に必要な実行コンテキストです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationTypes.h | 30 | struct | Unnamed::EventPresentationValuePipeline | definition | tracked | Event Presentation v2 の値加工パイプラインです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationTypes.h | 42 | struct | Unnamed::EventPresentationCondition | definition | tracked | Event Presentation v2 のトリガー条件です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationTypes.h | 52 | struct | Unnamed::EventPresentationAction | definition | tracked | Event Presentation v2 の Action 実行設定です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationTypes.h | 62 | struct | Unnamed::EventPresentationTrigger | definition | tracked | Event Presentation v2 の Cue 反応定義です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraph.h | 46 | struct | Unnamed::EventPresentationPinData | definition | tracked | EventPresentation 専用グラフのピン定義です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraph.h | 56 | struct | Unnamed::EventPresentationLinkData | definition | tracked | EventPresentation 専用グラフのリンク定義です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraph.h | 63 | struct | Unnamed::EventPresentationNodeData | definition | tracked | EventPresentation 専用ノードの編集データです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraph.h | 105 | class | Unnamed::EventPresentationEditorGraph | definition | tracked | EventPresentation 専用グラフ編集データ本体です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphCodec.h | 10 | class | Unnamed::EventPresentationEditorGraphCodec | definition | tracked | EventPresentation アセットと編集グラフの相互変換を行うユーティリティです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphUi.cpp | 456 | struct | Unnamed::<local@456>::NodeRect | definition | tracked | NodeRectは、ゲーム演出要素の位置と寸法を同じ座標系で表します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphUi.h | 13 | struct | ImVec2 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphUi.h | 17 | class | Unnamed::EventPresentationEditorGraphUi | definition | tracked | EventPresentation 専用グラフ UI の最小描画・編集クラスです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphUi.h | 20 | struct | Unnamed::EventPresentationEditorGraphUi::GraphSnapshot | definition | tracked | Undo/Redo 用に保持するグラフ編集スナップショットです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphUi.h | 192 | struct | Unnamed::EventPresentationEditorGraphUi::NodeRuntimeState | definition | tracked | NodeRuntimeStateは、presentation nodeの開始時刻と完了状態を再生中だけ保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphValidator.h | 17 | struct | Unnamed::EventPresentationValidationIssue | definition | tracked | EventPresentation グラフ検証メッセージです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphValidator.h | 26 | class | Unnamed::EventPresentationEditorGraphValidator | definition | tracked | EventPresentation 専用グラフの静的バリデーションを行うクラスです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryFormat.h | 48 | struct | Unnamed::DemoBinaryFileHeader | definition | tracked | DemoBinaryFileHeaderは、replay fileのmagic、version、header size、chunk数を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryFormat.h | 56 | struct | Unnamed::DemoBinaryChunkHeader | definition | tracked | DemoBinaryChunkHeaderは、replay chunkの種別、payload size、tick範囲を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryFormat.h | 62 | struct | Unnamed::DemoBinaryPackedPlayerCommand | definition | tracked | DemoBinaryPackedPlayerCommandは、replayへ保存するplayer commandを固定幅fieldへpackして保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryReader.h | 12 | class | Unnamed::DemoBinaryReader | definition | tracked | DemoBinaryReaderは、リプレイ記録・再生のバイト列を境界検査しながら型付き値へ読み出します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryWriter.h | 14 | class | Unnamed::DemoBinaryWriter | definition | tracked | DemoBinaryWriterは、リプレイ記録・再生の型付き値を決められたバイナリ形式へ書き出します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoManager.h | 17 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/replay/DemoManager.h | 20 | class | Unnamed::DemoManager | definition | tracked | DemoManagerは、replay recording・playback sessionとframe packetの進行を管理します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoTypes.h | 24 | struct | Unnamed::DemoPlayerInputPayload | definition | tracked | DemoPlayerInputPayloadは、リプレイ記録・再生イベントで送受信する値を一単位として保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoTypes.h | 32 | struct | Unnamed::DemoTickCommand | definition | tracked | DemoTickCommandは、simulation tickとそのtickで適用するplayer commandを保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoTypes.h | 40 | struct | Unnamed::EntitySnapshotRecord | definition | tracked | EntitySnapshotRecordは、リプレイ記録・再生で保存・復元する一件分の記録を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoTypes.h | 49 | struct | Unnamed::FrameSnapshot | definition | tracked | FrameSnapshotは、リプレイ記録・再生の特定時点を再現する状態値を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/DemoTypes.h | 55 | struct | Unnamed::DemoFileV2 | definition | tracked | DemoFileV2は、version 2 replayのheader、初期scene、frame packet列を所有します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/ReplaySerializerRegistry.h | 10 | class | Unnamed::BaseComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/replay/ReplaySerializerRegistry.h | 11 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/core/replay/ReplaySerializerRegistry.h | 14 | class | Unnamed::ReplaySerializerRegistry | definition | tracked | ReplaySerializerRegistryは、リプレイ記録・再生の実装を安定キーで登録し、利用側へ解決します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/replay/ReplaySerializerRegistry.h | 27 | struct | Unnamed::ReplaySerializerRegistry::ComponentSerializer | definition | tracked | ComponentSerializerは、component型に対応するreplay serialize・deserialize callbackを保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/core/script/ConsoleScriptComponent.h | 10 | class | Unnamed::ConsoleScriptComponent | definition | tracked | Attach/Detach時にコンソールコマンドを実行するコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/character/GrappleMotor.h | 8 | struct | Unnamed::GrappleState | definition | tracked | GrappleStateは、grapple接続先、rope長、接続中フラグを移動更新間で保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 12 | struct | Unnamed::MovementContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 15 | class | Unnamed::ParkourMovementComponent | definition | tracked | パルクール向けの拡張移動コンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 23 | struct | Unnamed::ParkourMovementComponent::WallRunRuntime | definition | tracked | WallRunRuntimeは、接触wall、走行side、経過時間、離脱抑止状態をwall-run中だけ保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 34 | struct | Unnamed::ParkourMovementComponent::SlideRuntime | definition | tracked | SlideRuntimeは、slide開始速度、経過時間、stance復帰待ち状態をslide中だけ保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 41 | struct | Unnamed::ParkourMovementComponent::BlinkRuntime | definition | tracked | BlinkRuntimeは、blink開始位置、目標位置、補間進行を瞬間移動中だけ保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 50 | struct | Unnamed::ParkourMovementComponent::VaultRuntime | definition | tracked | VaultRuntimeは、vault軌道、開始姿勢、補間進行を障害物通過中だけ保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 61 | struct | Unnamed::ParkourMovementComponent::ParkourRuntime | definition | tracked | ParkourRuntimeは、現在ability、入力buffer、移動補助cacheをparkour更新間で保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 164 | struct | Unnamed::ParkourMovementComponent::HullOccupancyDebugInfo | definition | tracked | 1回のハル占有判定で収集したデバッグ情報です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 182 | struct | Unnamed::ParkourMovementComponent::DuckStandDebugFrame | definition | tracked | Duck/UnDuck判定を可視化するフレームデータです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | 285 | struct | Unnamed::ParkourMovementComponent::DeterministicActionInputPacket | definition | tracked | DeterministicActionInputPacketは、movement abilityを再現するtick、固定step、action入力を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 165 | struct | Unnamed::<anonymous-namespace@23>::SpeedVaultTrajectory | definition | tracked | SpeedVaultTrajectoryは、speed vaultの開始点、通過点、着地点と補間進行を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 173 | struct | Unnamed::<anonymous-namespace@23>::WallRunCandidate | definition | tracked | WallRunCandidateは、wall hit、走行方向、左右sideをwall-run開始候補として保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 1040 | class | Unnamed::<anonymous-namespace@23>::ParkourCrouchAbility | definition | tracked | ParkourCrouchAbilityは、crouch入力に応じて立ち・しゃがみ姿勢を切り替え、天井干渉時の解除を抑止します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 1103 | class | Unnamed::<anonymous-namespace@23>::ParkourDoubleJumpAbility | definition | tracked | ParkourDoubleJumpAbilityは、空中jump入力を残り回数と接地状態で判定し、追加上向き速度を適用します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 1199 | class | Unnamed::<anonymous-namespace@23>::ParkourBlinkAbility | definition | tracked | ParkourBlinkAbilityは、blink入力の移動先を衝突照会し、安全な位置へ瞬間移動します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 1355 | class | Unnamed::<anonymous-namespace@23>::ParkourWallRunAbility | definition | tracked | ParkourWallRunAbilityは、走行可能な壁面を検出し、接触中の速度と重力補正を更新します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 1754 | class | Unnamed::<anonymous-namespace@23>::ParkourSlideAbility | definition | tracked | ParkourSlideAbilityは、slide開始条件を判定し、低姿勢中の速度・摩擦・終了条件を更新します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 1991 | class | Unnamed::<anonymous-namespace@23>::ParkourSpeedVaultAbility | definition | tracked | ParkourSpeedVaultAbilityは、低障害物のvault軌道を検証し、補間移動と着地遷移を制御します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | 2166 | class | Unnamed::<anonymous-namespace@23>::ParkourGrappleAbility | definition | tracked | ParkourGrappleAbilityは、grapple対象を照会し、rope距離と牽引加速度を更新します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.h | 4 | class | Unnamed::GameMovementStateMachine | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseElapsedTimeFormat.h | 10 | struct | Unnamed::CourseElapsedTimeParts | definition | tracked | CourseElapsedTimePartsは、パルクールゲームプレイで表示・計算しやすい単位へ分解した値を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseHudProjection.h | 10 | struct | Unnamed::CourseHudProjectionResult | definition | tracked | コース誘導HUDの画面投影結果です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.h | 17 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.h | 18 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.h | 19 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.h | 28 | struct | Unnamed::CourseTriggerSnapshot | definition | tracked | 1フレームで収集したコーストリガーのスナップショットです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.h | 39 | struct | Unnamed::CourseProgressSnapshot | definition | tracked | 外部参照用のコース進行スナップショットです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.h | 60 | class | Unnamed::CourseProgressComponent | definition | tracked | チェックポイント/ゴール進行を管理するプレイヤー側コンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.h | 108 | struct | Unnamed::CourseProgressComponent::DeterministicActionInputPacket | definition | tracked | DeterministicActionInputPacketは、course進行を再計算するtick、固定step、action入力を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | 13 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | 14 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | 17 | class | Unnamed::Gui::UiWidget | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | 18 | class | Unnamed::Gui::UiTextureComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | 19 | class | Unnamed::Gui::UiTransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | 22 | class | Unnamed::CourseProgressComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | 23 | class | Unnamed::UiCanvasComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | 26 | class | Unnamed::CourseProgressHudComponent | definition | tracked | CourseProgress のスナップショットを UiCanvas に反映する HUD コンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.cpp | 50 | struct | Unnamed::<anonymous-namespace@44>::CourseRankingTable | definition | tracked | CourseRankingTableは、course result画面へ表示する順位行とplayerの挿入位置を保持します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 13 | class | Unnamed::AudioSourceComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 14 | class | Unnamed::CourseProgressComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 15 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 16 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 17 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 18 | class | Unnamed::UiCanvasComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 21 | class | Unnamed::Gui::UiDigitStripComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 22 | class | Unnamed::Gui::UiTextureComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 23 | class | Unnamed::Gui::UiWidget | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 27 | class | Unnamed::CourseResultFlowComponent | definition | tracked | コースクリア後のリザルト表示とタイトル復帰を制御するコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 64 | struct | Unnamed::CourseResultFlowComponent::LockTargetSpec | definition | tracked | LockTargetSpecは、result演出中に無効化するcomponentをentity GUIDとstable nameで指定します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 70 | struct | Unnamed::CourseResultFlowComponent::ActiveLockState | definition | tracked | 実行中のロック復帰情報です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | 76 | struct | Unnamed::CourseResultFlowComponent::RankingRowWidgets | definition | tracked | リザルト画面へ動的に追加したランキング1行の参照です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 14 | class | Unnamed::AudioSourceComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 15 | class | Unnamed::CameraComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 16 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 17 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 18 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 19 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 20 | class | Unnamed::UiCanvasComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 23 | class | Unnamed::Gui::UiTextureComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 24 | class | Unnamed::Gui::UiTransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 25 | class | Unnamed::Gui::UiWidget | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 29 | class | Unnamed::GameStartCutsceneComponent | definition | tracked | ゲーム開始演出（カメラツアー/カウントダウン/開始解放）を制御するコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 64 | struct | Unnamed::GameStartCutsceneComponent::ShotSpec | definition | tracked | カメラショット定義です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 73 | struct | Unnamed::GameStartCutsceneComponent::LockTargetSpec | definition | tracked | LockTargetSpecは、開始cutscene中に無効化するcomponentをentity GUIDとstable nameで指定します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | 79 | struct | Unnamed::GameStartCutsceneComponent::ActiveLockState | definition | tracked | 実行中のロック復帰情報です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/title/TitleFlowComponent.h | 11 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/title/TitleFlowComponent.h | 12 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/title/TitleFlowComponent.h | 13 | class | Unnamed::UiCanvasComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/title/TitleFlowComponent.h | 16 | class | Unnamed::Gui::UiWidget | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/title/TitleFlowComponent.h | 17 | class | Unnamed::Gui::UiTextureComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/title/TitleFlowComponent.h | 21 | class | Unnamed::TitleFlowComponent | definition | tracked | タイトル演出（デモ再生/フェード/開始遷移）を制御するコンポーネントです。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/CheckpointComponent.h | 8 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/CheckpointComponent.h | 9 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/CheckpointComponent.h | 12 | class | Unnamed::CheckpointComponent | definition | tracked | CheckpointComponentは、player進入時にcourse checkpoint到達を記録します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/GoalComponent.h | 8 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/GoalComponent.h | 9 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/GoalComponent.h | 12 | class | Unnamed::GoalComponent | definition | tracked | GoalComponentは、player進入時にcourse完走を確定してresult flowを開始します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/JumpPadComponent.h | 6 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/JumpPadComponent.h | 7 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/JumpPadComponent.h | 10 | class | Unnamed::JumpPadComponent | definition | tracked | JumpPadComponentは、進入したcharacterへ設定方向・強度の跳躍速度を適用します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/SpeedBoostAreaComponent.h | 6 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/SpeedBoostAreaComponent.h | 7 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/SpeedBoostAreaComponent.h | 10 | class | Unnamed::SpeedBoostAreaComponent | definition | tracked | SpeedBoostAreaComponentは、領域内characterへ設定倍率の移動速度補正を適用します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/TriggerVolumeComponentBase.h | 8 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/TriggerVolumeComponentBase.h | 9 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/TriggerVolumeComponentBase.h | 10 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/components/trigger/TriggerVolumeComponentBase.h | 13 | class | Unnamed::TriggerVolumeComponentBase | definition | tracked | TriggerVolumeComponentBaseは、ボックス領域への進入・離脱を追跡し、派生トリガーへ通知します | pass | pass | - |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourComponentRegistration.h | 4 | class | Unnamed::ComponentRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourGameModule.h | 7 | class | Unnamed::ParkourGameModule | definition | tracked | Parkour ゲーム向けの GameModule 実装です。 | pass | out-of-scope | - |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourGameWorld.h | 7 | class | Unnamed::ParkourGameWorld | definition | tracked | Parkour ゲーム向けのランタイムワールド実装です。 | pass | out-of-scope | - |
| src/app/GameRuntimeModuleRegistration.h | 4 | class | Unnamed::GameModuleRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/app/LaunchDesc.h | 19 | struct | Unnamed::LaunchDesc | definition | tracked | App 起動引数から抽出した共通オプションです。 | pass | out-of-scope | - |
| src/app/LoadedGameModule.h | 10 | class | Unnamed::Engine | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/app/LoadedGameModule.h | 11 | class | Unnamed::GameModuleRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/app/LoadedGameModule.h | 12 | class | Unnamed::IGameModule | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/app/LoadedGameModule.h | 13 | class | Unnamed::IGameWorldFactory | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/app/LoadedGameModule.h | 14 | struct | Unnamed::GameRuntimeContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/app/LoadedGameModule.h | 17 | class | Unnamed::LoadedGameModule | definition | tracked | App/Launcher 層で GameModule の所有と lifecycle を管理します。 | pass | out-of-scope | - |
| src/core/assets/AssetManager.h | 18 | class | Unnamed::ContentPathResolver | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/AssetManager.h | 19 | class | Unnamed::IAssetLoader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/AssetManager.h | 22 | class | Unnamed::AssetManager | definition | tracked | アセットのロード、依存関係、ホットリロード、およびキャッシュ寿命を統括します | pass | pass | - |
| src/core/assets/AssetManager.h | 27 | struct | Unnamed::AssetManager::DebugStats | definition | tracked | DebugStatsは、アセット管理の診断用カウンターと集計値を保持します | pass | pass | - |
| src/core/assets/AssetManager.h | 323 | struct | Unnamed::AssetManager::SourceWatchState | definition | tracked | SourceWatchStateは、asset sourceの監視時刻とreload判定状態を保持します | pass | pass | - |
| src/core/assets/AssetManager.h | 329 | struct | Unnamed::AssetManager::Node | definition | tracked | アセット依存グラフの隣接関係と走査状態を保持します | pass | pass | - |
| src/core/assets/AssetMetaData.h | 12 | struct | Unnamed::AssetMetaData | definition | tracked | アセットのメタデータ | pass | out-of-scope | - |
| src/core/assets/FileStamp.h | 9 | struct | Unnamed::FileStamp | definition | tracked | ファイル更新検知に使う更新時刻とサイズのスナップショットを保持します | pass | pass | - |
| src/core/assets/LoadResult.h | 45 | struct | Unnamed::LoadResult | definition | tracked | アセットのロード結果を表す構造体 | pass | out-of-scope | - |
| src/core/assets/loader/EditorGuiLoader.h | 6 | class | Unnamed::EditorGuiLoader | definition | tracked | EditorGuiLoaderは、Lua editor GUI sourceを読み込み、実行用EditorGuiDataへ変換します | pass | pass | - |
| src/core/assets/loader/EventPresentationLoader.h | 7 | class | Unnamed::EventPresentationLoader | definition | tracked | Event Presentation v2 アセットを読み込むローダーです。 | pass | out-of-scope | - |
| src/core/assets/loader/MaterialAssetLoader.h | 5 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/loader/MaterialAssetLoader.h | 8 | class | Unnamed::MaterialAssetLoader | definition | tracked | MaterialAssetLoaderは、material定義のshader、parameter、texture参照を検証してruntime materialへ変換します | pass | pass | - |
| src/core/assets/loader/MaterialInstanceAssetLoader.h | 5 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/loader/MaterialInstanceAssetLoader.h | 8 | class | Unnamed::MaterialInstanceAssetLoader | definition | tracked | MaterialInstanceAssetLoaderは、親material参照とparameter overrideからruntime instanceを構築します | pass | pass | - |
| src/core/assets/loader/MeshAssetLoader.cpp | 41 | struct | Unnamed::<anonymous-namespace@28>::MeshCacheHeader | definition | tracked | バイナリなメッシュキャッシュファイルのヘッダ構造体。 | pass | out-of-scope | - |
| src/core/assets/loader/MeshAssetLoader.h | 6 | class | Unnamed::MeshAssetLoader | definition | tracked | MeshAssetLoaderは、mesh fileをvertex、index、submesh、skeleton dataへdecodeします | pass | pass | - |
| src/core/assets/loader/PostFxChainLoader.h | 5 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/loader/PostFxChainLoader.h | 8 | class | Unnamed::PostFxChainLoader | definition | tracked | PostFxChainLoaderは、post-process pass列とmaterial参照から実行順付きchainを構築します | pass | pass | - |
| src/core/assets/loader/SequenceAssetLoader.h | 7 | class | Unnamed::SequenceAssetLoader | definition | tracked | Sequenceアセットを読み込むローダーです。 | pass | out-of-scope | - |
| src/core/assets/loader/SequenceFileIO.h | 10 | struct | Unnamed::SequenceFileLoadResult | definition | tracked | Sequenceファイル読み込み結果です。 | pass | out-of-scope | - |
| src/core/assets/loader/SequenceFileIO.h | 19 | class | Unnamed::SequenceFileIO | definition | tracked | Sequence編集データの保存/読み込みを行うIOユーティリティです。 | pass | out-of-scope | - |
| src/core/assets/loader/SequenceMigrator.h | 9 | class | Unnamed::SequenceMigrator | definition | tracked | Sequence JSONのバージョン移行を行うユーティリティです。 | pass | out-of-scope | - |
| src/core/assets/loader/ShaderProgramLoader.cpp | 17 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/loader/ShaderProgramLoader.h | 5 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/loader/ShaderProgramLoader.h | 8 | class | Unnamed::ShaderProgramLoader | definition | tracked | ShaderProgramLoaderは、shader stage、entry point、defineを検証してcompile要求へ変換します | pass | pass | - |
| src/core/assets/loader/ShaderSourceLoader.h | 5 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/loader/ShaderSourceLoader.h | 8 | class | Unnamed::ShaderSourceLoader | definition | tracked | シェーダーソースローダークラス | pass | out-of-scope | - |
| src/core/assets/loader/SoundAssetLoader.h | 7 | class | Unnamed::SoundAssetLoader | definition | tracked | SoundAssetLoaderは、音声fileをPCM formatとsample byte列へdecodeします | pass | pass | - |
| src/core/assets/loader/TextureLoaderDirectXTex.h | 6 | class | Unnamed::TextureLoaderDirectXTex | definition | tracked | DirectXTexを使用したテクスチャローダークラス | pass | out-of-scope | - |
| src/core/assets/loader/UiDocumentAssetLoader.h | 7 | class | Unnamed::UiDocumentAssetLoader | definition | tracked | UiDocumentAssetLoaderは、UI JSONのnode階層、style、asset参照をUiDocumentDataへ変換します | pass | pass | - |
| src/core/assets/loader/interface/IAssetLoader.h | 9 | struct | Unnamed::AssetLoadContext | definition | tracked | AssetManagerからLoaderへ渡すロード元情報です。 | pass | out-of-scope | - |
| src/core/assets/loader/interface/IAssetLoader.h | 15 | class | Unnamed::IAssetLoader | definition | tracked | アセットローダーインターフェース | pass | out-of-scope | - |
| src/core/assets/shader/ShaderIncludeParser.h | 11 | class | Unnamed::ShaderIncludeParser | definition | tracked | HLSLソースからinclude directiveを抽出します。 | pass | out-of-scope | - |
| src/core/assets/shader/ShaderIncludeResolver.h | 8 | class | Unnamed::ContentPathResolver | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/assets/shader/ShaderIncludeResolver.h | 11 | class | Unnamed::ShaderIncludeResolver | definition | tracked | Shader includeを親ShaderSourceと同じmount内で解決します。 | pass | out-of-scope | - |
| src/core/assets/shader/ShaderIncludeTypes.h | 19 | struct | Unnamed::ShaderIncludeReference | definition | tracked | HLSLソースから抽出したinclude参照です。 | pass | out-of-scope | - |
| src/core/assets/shader/ShaderIncludeTypes.h | 27 | struct | Unnamed::ResolvedShaderInclude | definition | tracked | mount内で解決されたShader includeです。 | pass | out-of-scope | - |
| src/core/assets/shader/ShaderIncludeTypes.h | 36 | struct | Unnamed::UnresolvedShaderInclude | definition | tracked | 解決できなかったShader includeの再試行情報です。 | pass | out-of-scope | - |
| src/core/assets/types/EditorGuiData.h | 7 | struct | Unnamed::EditorGuiData | definition | tracked | EditorGuiDataは、EditorGui assetのdecode結果をruntime生成処理へ渡す中間表現として保持します | pass | pass | - |
| src/core/assets/types/EventPresentationAssetData.h | 9 | struct | Unnamed::EventPresentationValueInputAssetData | definition | tracked | Event Presentation v2 の値入力を定義します。 | pass | out-of-scope | - |
| src/core/assets/types/EventPresentationAssetData.h | 19 | struct | Unnamed::EventPresentationConditionAssetData | definition | tracked | Event Presentation v2 の条件式を定義します。 | pass | out-of-scope | - |
| src/core/assets/types/EventPresentationAssetData.h | 27 | struct | Unnamed::EventPresentationActionAssetData | definition | tracked | Event Presentation v2 の Action 設定を定義します。 | pass | out-of-scope | - |
| src/core/assets/types/EventPresentationAssetData.h | 35 | struct | Unnamed::EventPresentationTriggerAssetData | definition | tracked | Event Presentation v2 の Cue 反応定義を表します。 | pass | out-of-scope | - |
| src/core/assets/types/EventPresentationAssetData.h | 43 | struct | Unnamed::EventPresentationAssetData | definition | tracked | Event Presentation v2 アセットのルートデータです。 | pass | out-of-scope | - |
| src/core/assets/types/MaterialAssetData.h | 51 | struct | Unnamed::MaterialRenderStateData | definition | tracked | マテリアルの描画状態を表す構造体 | pass | out-of-scope | - |
| src/core/assets/types/MaterialAssetData.h | 66 | struct | Unnamed::MaterialAssetData | definition | tracked | マテリアルアセットのデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/MaterialInstanceAssetData.h | 11 | struct | Unnamed::MatTextureOverride | definition | tracked | Material Instanceのテクスチャオーバーライドです。 | pass | out-of-scope | - |
| src/core/assets/types/MaterialInstanceAssetData.h | 17 | struct | Unnamed::MaterialInstanceAssetData | definition | tracked | マテリアルインスタンスアセットのデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/MeshAssetData.h | 17 | struct | Unnamed::MeshVertex | definition | tracked | メッシュの頂点データ構造体 | pass | out-of-scope | - |
| src/core/assets/types/MeshAssetData.h | 27 | struct | Unnamed::SkeletonBoneAssetData | definition | tracked | スケルトンのボーンデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/MeshAssetData.h | 37 | struct | Unnamed::AnimationKeyVec3AssetData | definition | tracked | AnimationKeyVec3AssetDataは、AnimationKeyVec3Asset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します | pass | pass | - |
| src/core/assets/types/MeshAssetData.h | 43 | struct | Unnamed::AnimationKeyQuatAssetData | definition | tracked | AnimationKeyQuatAssetDataは、AnimationKeyQuatAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します | pass | pass | - |
| src/core/assets/types/MeshAssetData.h | 49 | struct | Unnamed::SkeletonBoneTrackAssetData | definition | tracked | SkeletonBoneTrackAssetDataは、SkeletonBoneTrackAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します | pass | pass | - |
| src/core/assets/types/MeshAssetData.h | 57 | struct | Unnamed::AnimationClipAssetData | definition | tracked | AnimationClipAssetDataは、AnimationClipAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します | pass | pass | - |
| src/core/assets/types/MeshAssetData.h | 64 | struct | Unnamed::SubMeshAssetData | definition | tracked | メッシュのサブメッシュデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/MeshAssetData.h | 71 | struct | Unnamed::MeshAssetData | definition | tracked | メッシュアセットのデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/PostFxChainAssetData.h | 12 | struct | Unnamed::PostFxPassAssetData | definition | tracked | ポストエフェクトパスのデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/PostFxChainAssetData.h | 24 | struct | Unnamed::PostFxChainAssetData | definition | tracked | ポストエフェクトチェーンアセットのデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 37 | struct | Unnamed::SequenceFloatKeyAssetData | definition | tracked | Floatキーフレームです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 48 | struct | Unnamed::SequenceBoolKeyAssetData | definition | tracked | Boolキーフレームです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 55 | struct | Unnamed::SequenceVec3KeyAssetData | definition | tracked | Vec3キーフレームです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 62 | struct | Unnamed::SequenceCameraCutKeyAssetData | definition | tracked | カメラカットキーフレームです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 69 | struct | Unnamed::SequenceEventKeyAssetData | definition | tracked | イベントキーフレームです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 80 | struct | Unnamed::SequenceRichCurveAssetData | definition | tracked | Floatカーブチャンネルです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 86 | struct | Unnamed::SequenceSkeletalControlData | definition | tracked | セクション内で扱うスケルタル制御データです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 97 | struct | Unnamed::SequenceSectionAssetData | definition | tracked | トラック上の時間範囲を表すセクションです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 126 | struct | Unnamed::SequenceBindingAssetData | definition | tracked | シーケンス対象のバインディング情報です。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 134 | struct | Unnamed::SequenceTrackAssetData | definition | tracked | 1本のトラック情報です。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAssetData.h | 145 | struct | Unnamed::SequenceAssetData | definition | tracked | シーケンスアセットのルートデータです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAuthoringData.h | 11 | struct | Unnamed::SequenceEditorMetadata | definition | tracked | Sequenceエディタ専用メタデータです。 | pass | out-of-scope | - |
| src/core/assets/types/SequenceAuthoringData.h | 17 | struct | Unnamed::SequenceAuthoringData | definition | tracked | Sequence編集用ルートデータです。 | pass | out-of-scope | - |
| src/core/assets/types/ShaderProgramAssetData.h | 12 | struct | Unnamed::ShaderProgramStage | definition | tracked | シェーダープログラムのステージごとの情報を表す構造体 | pass | out-of-scope | - |
| src/core/assets/types/ShaderProgramAssetData.h | 21 | struct | Unnamed::ShaderProgramAssetData | definition | tracked | シェーダープログラムアセットのデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/ShaderSourceAssetData.h | 12 | struct | Unnamed::ShaderSourceAssetData | definition | tracked | シェーダーソースアセットのデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/SoundAssetData.h | 8 | struct | Unnamed::SoundAssetData | definition | tracked | SoundAssetDataは、SoundAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します | pass | pass | - |
| src/core/assets/types/TextureAssetData.h | 17 | struct | Unnamed::TextureMip | definition | tracked | TextureMipは、1 mip levelの幅、高さ、row pitch、pixel byte列を所有します | pass | pass | - |
| src/core/assets/types/TextureAssetData.h | 26 | struct | Unnamed::TextureSubresource | definition | tracked | TextureSubresourceは、texture array sliceとmip levelをGPU upload対象として識別します | pass | pass | - |
| src/core/assets/types/TextureAssetData.h | 37 | struct | Unnamed::TextureAssetData | definition | tracked | テクスチャアセットのデータ構造体 | pass | out-of-scope | - |
| src/core/assets/types/UiDocumentAssetData.h | 9 | struct | Unnamed::UiDocumentAssetData | definition | tracked | UiDocumentAssetDataは、UiDocumentAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します | pass | pass | - |
| src/core/containers/RingBuffer.h | 18 | class | Unnamed::RingBuffer | definition | tracked | スレッドセーフなリングバッファクラス | pass | out-of-scope | ADDED: Capacity > 0, default_initializable<T>, and assignable_from<T&, const T&> match std::array storage plus Push/Pop copy assignment. |
| src/core/containers/RingBuffer.h | 139 | class | Unnamed::RingBuffer::Iterator | definition | tracked | リングバッファのイテレータクラス | pass | out-of-scope | - |
| src/core/content/ContentPathResolver.h | 14 | struct | Unnamed::ContentDirectoryMount | definition | tracked | コンテンツディレクトリのマウント情報 | pass | out-of-scope | - |
| src/core/content/ContentPathResolver.h | 23 | struct | Unnamed::ResolvedContentFile | definition | tracked | 論理コンテンツパスの物理パスへの解決結果 | pass | out-of-scope | - |
| src/core/content/ContentPathResolver.h | 33 | class | Unnamed::ContentPathResolver | definition | tracked | 仮想パスをマウント済みのコンテンツディレクトリから解決するためのクラス。 | pass | out-of-scope | - |
| src/core/filesystem/Path.h | 11 | class | Unnamed::Path | definition | tracked | OS上の物理ファイルパスを表す。 | pass | out-of-scope | - |
| src/core/filesystem/Path.h | 171 | struct | std::formatter | definition | tracked | Pathを汎用文字列フォーマッターへ橋渡しします | pass | pass | EXISTING_SPECIALIZATION: this is not an open primary template contract. |
| src/core/filesystem/VirtualPath.h | 7 | class | Unnamed::VirtualPath | definition | tracked | content をルートとした仮想パス | pass | out-of-scope | - |
| src/core/guidgenerator/GuidGenerator.h | 9 | class | Unnamed::GuidGenerator | definition | tracked | GUIDを生成するクラス | pass | out-of-scope | - |
| src/core/hash/HashBuilder.h | 11 | class | Unnamed::HashBuilder | definition | tracked | ランタイム用の汎用ハッシュ生成クラス | pass | out-of-scope | - |
| src/core/hash/StableHashBuilder.h | 32 | class | Unnamed::StableHashBuilder | definition | tracked | 順序付き入力から実行間で安定したハッシュ値を構築します | pass | pass | - |
| src/core/io/binary/BinaryReader.h | 11 | class | Unnamed::BinaryReader | definition | tracked | BinaryReaderは、バイナリ入出力のバイト列を境界検査しながら型付き値へ読み出します | pass | pass | - |
| src/core/io/binary/BinaryWriter.h | 13 | class | Unnamed::BinaryWriter | definition | tracked | BinaryWriterは、バイナリ入出力の型付き値を決められたバイナリ形式へ書き出します | pass | pass | - |
| src/core/io/ini/IniParser.h | 8 | class | Unnamed::IniParser | definition | tracked | INIファイルパーサークラス | pass | out-of-scope | - |
| src/core/io/json/JsonReader.h | 19 | class | Unnamed::JsonReader | definition | tracked | JSON読み込みクラス | pass | out-of-scope | - |
| src/core/io/json/JsonWriter.h | 9 | struct | Vec4 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/io/json/JsonWriter.h | 10 | struct | Vec3 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/io/json/JsonWriter.h | 16 | class | Unnamed::JsonWriter | definition | tracked | JSON書き込みクラス | pass | out-of-scope | - |
| src/core/math/Mat4.h | 6 | struct | Quaternion | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/math/Mat4.h | 7 | struct | Vec3 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/math/Mat4.h | 8 | struct | Vec4 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/math/Mat4.h | 16 | struct | Mat4 | definition | tracked | 行優先の4x4行列を保持し、ビュー・射影・アフィン変換を提供します | pass | pass | - |
| src/core/math/Quaternion.h | 3 | struct | Vec3 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/math/Quaternion.h | 8 | struct | Quaternion | definition | tracked | クォータニオン（四元数）構造体 | pass | out-of-scope | - |
| src/core/math/Vec2.h | 5 | struct | Vec2 | definition | tracked | 2次元ベクトル構造体 | pass | out-of-scope | - |
| src/core/math/Vec3.h | 5 | struct | Vec4 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/math/Vec3.h | 6 | struct | Quaternion | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/math/Vec3.h | 12 | struct | Vec3 | definition | tracked | 3次元ベクトル構造体 | pass | out-of-scope | - |
| src/core/math/Vec4.h | 5 | struct | Mat4 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/math/Vec4.h | 11 | struct | Vec4 | definition | tracked | 4次元ベクトル構造体 | pass | out-of-scope | - |
| src/core/math/random/Random.h | 5 | struct | Vec3 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/math/random/Random.h | 11 | class | Random | definition | tracked | エンジン共通の疑似乱数生成器と分布変換を提供します | pass | pass | - |
| src/core/memory/MemUtil.h | 7 | class | Unnamed::MemUtil | definition | tracked | メモリユーティリティクラス | pass | out-of-scope | - |
| src/core/string/StrUtil.h | 7 | class | Unnamed::Path | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/core/string/StrUtil.h | 133 | struct | Unnamed::StrUtil::LinkSpan | definition | tracked | 文字列中のリンク範囲をバイトオフセットで表します | pass | pass | - |
| src/core/string/TextEncoding.h | 5 | class | Unnamed::TextEncoding | definition | tracked | UTF-8とWindowsワイド文字列の境界変換を提供します | pass | pass | - |
| src/engine/Animation/Animation.h | 7 | struct | Animation | definition | tracked | Animationは、animation clipのduration、tick rate、node channel列を所有します | pass | pass | - |
| src/engine/Animation/KeyFrame.h | 6 | struct | Keyframe | definition | tracked | Keyframeは、animation sampleの時刻と補間対象値を1 keyとして保持します | pass | pass | NOT_ADDED: storage alone has no additional stable operation contract. |
| src/engine/Animation/Node.h | 9 | struct | aiNode | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Animation/Node.h | 13 | struct | AnimationCurve | definition | tracked | AnimationCurveは、時間順keyframe列を所有し、指定時刻の値を補間します | pass | pass | NOT_ADDED: consumers, rather than storage, impose the relevant operations. |
| src/engine/Animation/Node.h | 18 | struct | NodeAnimation | definition | tracked | NodeAnimationは、skeleton nodeごとのtranslation、rotation、scale curveを保持します | pass | pass | - |
| src/engine/Animation/Node.h | 25 | struct | Node | definition | tracked | アニメーション階層の子ノードとローカル変換を保持します | pass | pass | - |
| src/engine/Animation/Node.h | 27 | struct | Node::Transform | definition | tracked | アニメーションキーから得た平行移動、回転、拡縮を一組で保持します | pass | pass | - |
| src/engine/ComponentRegistry.h | 14 | class | Unnamed::ComponentRegistry | definition | tracked | コンポーネントレジストリ | pass | out-of-scope | - |
| src/engine/ComponentRegistry.h | 19 | struct | Unnamed::ComponentRegistry::Entry | definition | tracked | Entryは、コンポーネント生成関数、表示名、型識別子を登録済みcomponentごとに保持します | pass | pass | - |
| src/engine/ComponentRegistry.h | 27 | struct | Unnamed::ComponentRegistry::RegisteredComponentInfo | definition | tracked | RegisteredComponentInfoは、Editorのcomponent追加UIへ公開する安定名と表示名を保持します | pass | pass | - |
| src/engine/ComponentRegistry.h | 93 | struct | Unnamed::Detail::AutoComponentRegister | definition | tracked | AutoComponentRegisterは、static初期化時にcomponent factoryをComponentRegistryへ登録します | pass | pass | - |
| src/engine/Engine.cpp | 71 | class | Unnamed::Rhi::D3D12CommandContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.cpp | 72 | class | Unnamed::Rhi::D3D12Device | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 13 | class | IPostProcess | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 14 | class | SrvManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 17 | class | Unnamed::Engine | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 18 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 19 | class | Unnamed::AudioSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 20 | class | Unnamed::ComponentRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 21 | class | Unnamed::EditorRuntime | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 22 | class | Unnamed::ImGuiLayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 23 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 24 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 25 | class | Unnamed::Profiler | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 26 | class | Unnamed::WindowManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 27 | class | Unnamed::ConCommand | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 28 | class | Unnamed::IDemoService | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 29 | class | Unnamed::IGameWorldFactory | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 30 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 31 | struct | Unnamed::GameRuntimeContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 32 | struct | Unnamed::WorldServices | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 35 | class | Unnamed::Render::RenderModule | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 36 | struct | Unnamed::Render::RenderFrameContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 40 | class | Unnamed::Rhi::IRhiDevice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 44 | struct | Unnamed::EngineRuntimeBindings | definition | tracked | Engine へ注入するゲームランタイム依存情報です。 | pass | out-of-scope | - |
| src/engine/Engine.h | 58 | struct | Unnamed::EngineRunCallbacks | definition | tracked | Engine 実行時フックです。 | pass | out-of-scope | - |
| src/engine/Engine.h | 66 | class | Unnamed::Engine | definition | tracked | 各サブシステムの初期化、フレーム実行、終了順序を統括します | pass | pass | - |
| src/engine/Engine.h | 152 | class | Unnamed::Engine::PlatformEventsImpl | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 155 | class | Unnamed::Engine::TerminalSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/Engine.h | 156 | class | Unnamed::Engine::TimeSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/EngineComponentRegistration.h | 4 | class | Unnamed::ComponentRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/EngineConfig.h | 21 | struct | Unnamed::EngineConfig | definition | tracked | EngineConfigは、Engine機能の生成時に適用する有効化条件と調整値を保持します | pass | pass | - |
| src/engine/EngineConfig.h | 25 | struct | Unnamed::EngineConfig::Window | definition | tracked | Windowは、engine起動時のwindow title、client幅、高さ、表示設定を保持します | pass | pass | - |
| src/engine/EngineServices.h | 4 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/EngineServices.h | 5 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/EngineServices.h | 6 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/EngineServices.h | 7 | class | Unnamed::Profiler | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/EngineServices.h | 8 | class | Unnamed::WindowManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/EngineServices.h | 9 | class | Unnamed::IDemoService | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/EngineServices.h | 12 | struct | Unnamed::EngineServices | definition | tracked | ゲームモジュール初期化時に渡す Engine サービス群です。 | pass | out-of-scope | - |
| src/engine/IWin32MsgListener.h | 13 | struct | Unnamed::IWin32MsgListener | definition | tracked | Win32のメッセージを受け取るリスナーのインターフェース | pass | out-of-scope | - |
| src/engine/ImGui/ImGuiUtil.h | 5 | struct | Vec3 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/ImGui/ImGuiUtil.h | 6 | struct | Vec4 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/ImGui/ImGuiWidgets.h | 16 | struct | Vec3 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/ContentBrowser.cpp | 27 | struct | Unnamed::EditorContentBrowser::<anonymous-namespace@23>::BrowserEntry | definition | tracked | BrowserEntryは、Content Browserに表示するpath、表示名、directory・asset種別を保持します | pass | pass | - |
| src/engine/editor/ContentBrowser.h | 19 | struct | Unnamed::EditorContentBrowser::AssetDragDropPayload | definition | tracked | AssetDragDropPayloadは、エディターイベントで送受信する値を一単位として保持します | pass | pass | - |
| src/engine/editor/ContentBrowser.h | 25 | struct | Unnamed::EditorContentBrowser::BrowserViewState | definition | tracked | BrowserViewStateは、Content Browserの現在directory、選択、表示modeを保持します | pass | pass | - |
| src/engine/editor/EditorGuiScriptPanel.h | 11 | class | Unnamed::EditorGuiScriptPanel | definition | tracked | luaでエディタを拡張するためのGUIパネル | pass | out-of-scope | - |
| src/engine/editor/EditorNotification.cpp | 23 | struct | EditorNotification::NotificationState | definition | tracked | NotificationStateは、Editor通知の表示開始時刻とfade進行を保持します | pass | pass | - |
| src/engine/editor/EditorNotification.cpp | 58 | struct | Unnamed::<anonymous-namespace@38>::NotificationLayout | definition | tracked | NotificationLayoutは、Editor通知を積み重ねる画面位置、幅、行間を計算結果として保持します | pass | pass | - |
| src/engine/editor/EditorNotification.h | 12 | class | Unnamed::ConCommand | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorNotification.h | 22 | struct | Unnamed::Notification | definition | tracked | Notificationは、Editorへ表示する本文、severity、表示時間を保持します | pass | pass | - |
| src/engine/editor/EditorNotification.h | 34 | class | Unnamed::EditorNotification | definition | tracked | エディターノーティフィケーションクラス | pass | out-of-scope | - |
| src/engine/editor/EditorNotification.h | 63 | struct | Unnamed::EditorNotification::NotificationState | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorProperties.h | 9 | struct | Unnamed::EditorUIProperties | definition | tracked | EditorUIPropertiesは、Editor UIで共有する色、余白、寸法のstyle値を保持します | pass | pass | - |
| src/engine/editor/EditorRuntime.h | 8 | class | Unnamed::WindowManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 9 | class | Unnamed::ImGuiLayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 10 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 11 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 12 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 13 | class | Unnamed::IDemoService | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 14 | class | Unnamed::IGameWorldFactory | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 15 | class | Unnamed::Profiler | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 18 | class | Unnamed::Render::RenderModule | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 19 | struct | Unnamed::Render::RenderFrameInputs | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorRuntime.h | 24 | class | Unnamed::EditorRuntime | definition | tracked | エディタランタイム | pass | out-of-scope | - |
| src/engine/editor/EditorToolHost.h | 12 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 13 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 14 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 15 | class | Unnamed::IDemoService | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 16 | class | Unnamed::IGameWorldFactory | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 17 | class | Unnamed::Profiler | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 18 | class | Unnamed::WindowManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 19 | class | Unnamed::ImGuiLayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 22 | class | Unnamed::Render::RenderModule | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 23 | struct | Unnamed::Render::RenderFrameInputs | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 24 | struct | Unnamed::Render::SceneOutputView | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 27 | class | Unnamed::GuiEditorTool | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 30 | class | Unnamed::EditorToolHost | definition | tracked | EditorToolHostは、登録済みEditor toolの選択、フレーム更新、UI描画の呼び出し順序を管理します | pass | pass | - |
| src/engine/editor/EditorToolHost.h | 76 | class | Unnamed::EditorToolHost::EditorNotification | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 77 | class | Unnamed::EditorToolHost::EditorLuaSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorToolHost.h | 78 | class | Unnamed::EditorToolHost::EditorGuiScriptPanel | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorUiMetrics.h | 5 | class | Unnamed::EditorUiMetrics | definition | tracked | EditorUiMetricsは、DPI倍率に応じたEditor UIの寸法と間隔を算出します | pass | pass | - |
| src/engine/editor/EditorViewportCameraManager.h | 12 | class | Unnamed::EditorWorld | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/EditorViewportCameraManager.h | 21 | struct | Unnamed::ViewportCameraBinding | definition | tracked | ViewportCameraBindingは、ViewportCameraの論理識別子とruntime resource参照の対応を保持します | pass | pass | - |
| src/engine/editor/EditorViewportCameraManager.h | 28 | class | Unnamed::EditorViewportCameraManager | definition | tracked | EditorViewportCameraManagerは、viewportごとのcamera bindingとactive camera解決を管理します | pass | pass | - |
| src/engine/editor/EditorViewportCameraManager.h | 31 | struct | Unnamed::EditorViewportCameraManager::ResolvedCamera | definition | tracked | ResolvedCameraは、viewportが使用するcamera componentとtransformの非所有参照を保持します | pass | pass | - |
| src/engine/editor/GuiEditorTool.h | 23 | class | Unnamed::ImGuiLayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/GuiEditorTool.h | 26 | struct | Unnamed::Render::SceneOutputView | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/GuiEditorTool.h | 30 | class | Unnamed::GuiEditorTool | definition | tracked | GuiEditorToolは、GUI documentのnode選択、property編集、preview描画をEditor toolとして提供します | pass | pass | - |
| src/engine/editor/GuiEditorTool.h | 59 | struct | Unnamed::GuiEditorTool::ViewOutputCache | definition | tracked | GUI プレビュー出力の SRV と有効領域を UI 構築まで保持します。 | pass | out-of-scope | - |
| src/engine/editor/IEditorTool.h | 13 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 14 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 15 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 16 | class | Unnamed::IDemoService | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 17 | class | Unnamed::IGameWorldFactory | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 18 | class | Unnamed::Profiler | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 19 | class | Unnamed::ImGuiLayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 20 | class | Unnamed::WindowManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 21 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 24 | class | Unnamed::Render::RenderModule | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 25 | struct | Unnamed::Render::RenderFrameInputs | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 26 | struct | Unnamed::Render::SceneOutputView | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/IEditorTool.h | 30 | struct | Unnamed::EditorToolServices | definition | tracked | EditorToolServicesは、Editor toolへ注入するWorld、Renderer、各subsystemの非所有参照を保持します | pass | pass | - |
| src/engine/editor/IEditorTool.h | 43 | struct | Unnamed::EditorToolFrameContext | definition | tracked | EditorToolFrameContextは、EditorToolFrame処理中に共有する非所有参照と一時的なframe入力を保持します | pass | pass | - |
| src/engine/editor/IEditorTool.h | 49 | class | Unnamed::IEditorTool | definition | tracked | IEditorToolは、Editor toolの識別、open状態、frame更新、UI描画契約を定義します | pass | pass | - |
| src/engine/editor/ImGuizmoConfigLoader.h | 6 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/ImGuizmoConfigLoader.h | 9 | class | Unnamed::ImGuizmoConfigLoader | definition | tracked | ImGuizmoConfigLoaderは、ImGuizmo設定ファイルを読み込み、コンソール変数へ反映します | pass | pass | - |
| src/engine/editor/LevelEditorTool.h | 24 | class | Unnamed::WindowManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 25 | class | Unnamed::EditorWorld | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 26 | class | Unnamed::ImGuiLayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 27 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 28 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 29 | class | Unnamed::SequenceEditorController | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 30 | class | Unnamed::SequenceTimelinePanel | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 31 | class | Unnamed::SequenceCurvePanel | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 34 | struct | Unnamed::Render::SceneOutputView | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/LevelEditorTool.h | 57 | class | Unnamed::LevelEditorTool | definition | tracked | LevelEditorToolは、World hierarchy、viewport、component propertyを編集するlevel UIを提供します | pass | pass | - |
| src/engine/editor/LevelEditorTool.h | 97 | struct | Unnamed::LevelEditorTool::ViewOutputCache | definition | tracked | ViewOutputCacheは、view textureのSRV、revision、texture ID、表示寸法とUV範囲を再利用用に保持します | pass | pass | - |
| src/engine/editor/LevelEditorToolHierarchy.cpp | 32 | struct | Unnamed::<anonymous-namespace@30>::OutlinerFolderNode | definition | tracked | OutlinerFolderNodeは、Outliner folder名、親子関係、展開状態を保持します | pass | pass | - |
| src/engine/editor/LevelEditorToolHierarchy.cpp | 38 | struct | Unnamed::<anonymous-namespace@30>::ComponentMenuNode | definition | tracked | ComponentMenuNodeは、add-component menuのcategory階層と登録component項目を保持します | pass | pass | - |
| src/engine/editor/LevelEditorToolViewport.cpp | 28 | struct | Unnamed::<anonymous-namespace@26>::ViewportFitResult | definition | tracked | ViewportFitResultは、panel内へaspect比を保って収めた描画位置、寸法、aspect比を返します | pass | pass | - |
| src/engine/editor/lua/EditorLuaSystem.h | 10 | class | Unnamed::EditorLuaSystem | definition | tracked | EditorLuaSystemは、Editor専用Lua stateを所有し、tool scriptの実行と関数呼び出しを仲介します | pass | pass | - |
| src/engine/editor/sequence/SequenceEditorController.h | 15 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/sequence/SequenceEditorController.h | 16 | class | Unnamed::SequencePlayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/sequence/SequenceEditorController.h | 17 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/sequence/SequenceEditorController.h | 20 | class | Unnamed::SequenceEditorController | definition | tracked | Sequence Editor全体のドキュメントと再生ヘッドを管理するコントローラです。 | pass | out-of-scope | - |
| src/engine/editor/sequence/SequenceEditorDocument.h | 13 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/editor/sequence/SequenceEditorDocument.h | 16 | class | Unnamed::SequenceEditorDocument | definition | tracked | Sequenceアセット1件分の編集状態を保持するドキュメントです。 | pass | out-of-scope | - |
| src/engine/editor/sequence/SequenceEditorTypes.h | 30 | struct | Unnamed::SequenceEditorSelection | definition | tracked | Sequence Editorの選択状態です。 | pass | out-of-scope | - |
| src/engine/game/GameModulePaths.h | 11 | struct | Unnamed::GameModulePaths | definition | tracked | ゲームのルート情報と既定起動情報をまとめた構造体です。 | pass | out-of-scope | - |
| src/engine/game/GameModuleRegistry.h | 10 | class | Unnamed::IGameModule | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/GameModuleRegistry.h | 13 | class | Unnamed::GameModuleRegistry | definition | tracked | 文字列名から GameModule を生成する静的リンク向けファクトリレジストリです。 | pass | out-of-scope | - |
| src/engine/game/GamePathResolver.cpp | 27 | struct | Unnamed::<anonymous-namespace@10>::ContentMountRoot | definition | tracked | ContentMountRootは、content mount名と解決済みphysical root pathの対応を保持します | pass | pass | - |
| src/engine/game/GamePathResolver.h | 10 | struct | Unnamed::MountedContentResolution | definition | tracked | マウント付き content 解決の詳細結果です。 | pass | out-of-scope | - |
| src/engine/game/GameRuntimeContext.h | 10 | struct | Unnamed::GameRuntimeContext | definition | tracked | ランタイム選択済みゲーム情報を Engine へ渡すコンテキストです。 | pass | out-of-scope | - |
| src/engine/game/IDemoService.h | 7 | class | Unnamed::Path | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IDemoService.h | 8 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IDemoService.h | 9 | struct | Unnamed::DemoTickCommand | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IDemoService.h | 13 | class | Unnamed::IDemoService | definition | tracked | デモ録画・再生サービスの抽象インターフェースです。 | pass | out-of-scope | - |
| src/engine/game/IGameModule.h | 14 | class | Unnamed::Engine | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IGameModule.h | 15 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IGameModule.h | 16 | class | Unnamed::IDemoService | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IGameModule.h | 17 | class | Unnamed::ComponentRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IGameModule.h | 18 | struct | Unnamed::EngineServices | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IGameModule.h | 19 | struct | Unnamed::WorldServices | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IGameModule.h | 22 | class | Unnamed::IGameModule | definition | tracked | ゲーム側から Engine へ機能注入するためのモジュール抽象です。 | pass | out-of-scope | - |
| src/engine/game/IGameModule.h | 72 | struct | Unnamed::GameRuntimeApiV1 | definition | tracked | Runtime DLL が公開する C ABI 関数テーブル（v1）です。 | pass | out-of-scope | - |
| src/engine/game/IGameWorldFactory.h | 6 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IGameWorldFactory.h | 7 | struct | Unnamed::WorldServices | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/game/IGameWorldFactory.h | 10 | class | Unnamed::IGameWorldFactory | definition | tracked | PIE 向けにゲームワールドを生成する抽象ファクトリです。 | pass | out-of-scope | - |
| src/engine/gui/Rect.h | 6 | struct | Unnamed::Gui::Rect | definition | tracked | UI空間の左上座標と幅・高さをピクセル単位で表します | pass | pass | - |
| src/engine/gui/Rect.h | 27 | struct | Unnamed::Gui::Anchors | definition | tracked | アンカー構造体 | pass | out-of-scope | - |
| src/engine/gui/Rect.h | 37 | struct | Unnamed::Gui::Margins | definition | tracked | マージン構造体 | pass | out-of-scope | - |
| src/engine/gui/Rect.h | 51 | struct | Unnamed::Gui::Pivot | definition | tracked | ピボット構造体 | pass | out-of-scope | - |
| src/engine/gui/UiButton.h | 8 | class | Unnamed::Gui::UiButtonBehaviorComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiButton.h | 11 | class | Unnamed::Gui::UiButton | definition | tracked | UiButtonは、押下状態とクリック通知をUiWidgetの入力処理へ追加します | pass | pass | - |
| src/engine/gui/UiCanvasRuntime.h | 10 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiCanvasRuntime.h | 11 | class | Unnamed::Scene | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiCanvasRuntime.h | 12 | struct | Unnamed::Ray | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiCanvasRuntime.h | 15 | struct | Unnamed::Render::RenderCameraInput | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiCanvasRuntime.h | 16 | struct | Unnamed::Render::ScreenSpriteInput | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiDeserializeContext.h | 6 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiDeserializeContext.h | 11 | struct | Unnamed::Gui::UiDeserializeContext | definition | tracked | UI Documentのデシリアライズに必要なサービスと方針です。 | pass | out-of-scope | - |
| src/engine/gui/UiDocument.h | 10 | class | Unnamed::Gui::UiDocument | definition | tracked | UiDocumentは、GUIウィジェット階層のルートとドキュメント固有状態を所有します | pass | pass | - |
| src/engine/gui/UiDocumentManager.h | 13 | class | Unnamed::Gui::UiDocument | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiDocumentManager.h | 17 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiDocumentManager.h | 22 | class | Unnamed::Gui::UiDocumentManager | definition | tracked | UiDocumentManagerは、UiDocumentをIDで所有し、screenへのattach・detach寿命を管理します | pass | pass | - |
| src/engine/gui/UiDocumentManager.h | 44 | struct | Unnamed::Gui::UiDocumentManager::ManagedDocument | definition | tracked | ManagedDocumentは、document ID、UiDocument所有権、screenへのattach状態を保持します | pass | pass | - |
| src/engine/gui/UiDrawCommand.h | 10 | struct | Unnamed::Gui::Color | definition | tracked | Colorは、GUI描画命令へ渡す非premultiplied RGBA成分を保持します | pass | pass | - |
| src/engine/gui/UiDrawCommand.h | 24 | struct | Unnamed::Gui::UiDrawCommandRect | definition | tracked | UiDrawCommandRectは、GUI要素の位置と寸法を同じ座標系で表します | pass | pass | - |
| src/engine/gui/UiDrawCommand.h | 33 | struct | Unnamed::Gui::UiDrawCommandText | definition | tracked | UiDrawCommandTextは、描画文字列、font、位置、色を1件のtext描画命令として保持します | pass | pass | - |
| src/engine/gui/UiDrawCommand.h | 41 | struct | Unnamed::Gui::UiDrawCommandImage | definition | tracked | UiDrawCommandImageは、texture、描画矩形、UV、tintを1件のimage描画命令として保持します | pass | pass | - |
| src/engine/gui/UiDrawCommand.h | 52 | struct | Unnamed::Gui::UiDrawCommand | definition | tracked | UiDrawCommandは、GUIで順序付き実行する命令と引数を保持します | pass | pass | - |
| src/engine/gui/UiPanel.h | 6 | class | Unnamed::Gui::UiPanelStyleComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiPanel.h | 9 | class | Unnamed::Gui::UiPanel | definition | tracked | UiPanelは、子ウィジェットの配置領域と背景描画を提供します | pass | pass | - |
| src/engine/gui/UiRoot.h | 10 | class | Unnamed::Gui::UiWidget | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiRoot.h | 13 | class | Unnamed::Gui::UiRoot | definition | tracked | UiRootは、GUI階層の最上位でレイアウト、入力配送、描画コマンド生成を統括します | pass | pass | - |
| src/engine/gui/UiScreen.h | 8 | class | Unnamed::Gui::UiScreen | definition | tracked | UiScreenは、画面単位のUiRootと表示ライフサイクルを所有します | pass | pass | - |
| src/engine/gui/UiScreenStack.h | 9 | class | Unnamed::Gui::UiScreenStack | definition | tracked | UiScreenStackは、画面の積み重ね順に入力対象と描画順を管理します | pass | pass | - |
| src/engine/gui/UiTextureReference.h | 10 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiTextureReference.h | 14 | struct | Unnamed::Gui::UiDeserializeContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/UiTextureReference.h | 17 | struct | Unnamed::Gui::UiTextureReference | definition | tracked | UIが保持する解決済みテクスチャ参照です。 | pass | out-of-scope | - |
| src/engine/gui/UiWidget.h | 38 | struct | Unnamed::Gui::UiSizePolicy | definition | tracked | UiSizePolicyは、UiWidgetが親の余剰幅・高さを伸縮へ使う方針を保持します | pass | pass | - |
| src/engine/gui/UiWidget.h | 44 | struct | Unnamed::Gui::UiSizeConstraints | definition | tracked | UiSizeConstraintsは、UiWidgetの最小・最大幅と高さをlayout制約として保持します | pass | pass | - |
| src/engine/gui/UiWidget.h | 52 | class | Unnamed::Gui::UiWidget | definition | tracked | UIウィジェットの基本クラス | pass | out-of-scope | - |
| src/engine/gui/components/UiButtonBehaviorComponent.h | 13 | class | Unnamed::Gui::UiButtonBehaviorComponent | definition | tracked | UiButtonBehaviorComponentは、UiWidgetのhover・press・click状態を入力から更新します | pass | pass | - |
| src/engine/gui/components/UiComponent.h | 7 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/components/UiComponent.h | 8 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/components/UiComponent.h | 9 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/components/UiComponent.h | 13 | struct | Unnamed::Gui::UiDrawCommand | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/components/UiComponent.h | 14 | struct | Unnamed::Gui::UiDeserializeContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/components/UiComponent.h | 15 | class | Unnamed::Gui::UiWidget | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/components/UiComponent.h | 18 | class | Unnamed::Gui::UiComponent | definition | tracked | UiComponentは、entity上のGUI widget参照と有効状態をcomponent lifecycleへ接続します | pass | pass | - |
| src/engine/gui/components/UiDigitStripComponent.h | 14 | class | Unnamed::Gui::UiDigitStripComponent | definition | tracked | UiDigitStripComponentは、数値を桁ごとのtexture領域へ変換して連続描画します | pass | pass | - |
| src/engine/gui/components/UiLayoutComponents.cpp | 62 | struct | Unnamed::Gui::<local@62>::ChildInfo | definition | tracked | ChildInfoは、layout対象widget、固定extent、余剰領域を受け取る指定をchild一件分保持します | pass | pass | - |
| src/engine/gui/components/UiLayoutComponents.h | 7 | struct | Unnamed::Gui::LayoutPadding | definition | tracked | LayoutPaddingは、layout内容領域の上下左右paddingをpixel単位で保持します | pass | pass | - |
| src/engine/gui/components/UiLayoutComponents.h | 15 | class | Unnamed::Gui::UiLinearLayoutComponent | definition | tracked | UiLinearLayoutComponentは、子UiWidgetを指定軸、間隔、alignmentに従って配置します | pass | pass | - |
| src/engine/gui/components/UiLayoutComponents.h | 41 | class | Unnamed::Gui::UiVerticalLayoutComponent | definition | tracked | UiVerticalLayoutComponentは、子UiWidgetを上から下へ間隔付きで配置します | pass | pass | - |
| src/engine/gui/components/UiLayoutComponents.h | 54 | class | Unnamed::Gui::UiHorizontalLayoutComponent | definition | tracked | UiHorizontalLayoutComponentは、子UiWidgetを左から右へ間隔付きで配置します | pass | pass | - |
| src/engine/gui/components/UiPanelStyleComponent.h | 9 | class | Unnamed::Gui::UiPanelStyleComponent | definition | tracked | UiPanelStyleComponentは、panelの背景色、border、paddingを描画とlayoutへ提供します | pass | pass | - |
| src/engine/gui/components/UiTextureComponent.h | 14 | class | Unnamed::Gui::UiTextureComponent | definition | tracked | UiTextureComponentは、asset textureとtintをUiWidgetのimage描画へ提供します | pass | pass | - |
| src/engine/gui/components/UiTransformComponent.h | 10 | class | Unnamed::Gui::UiTransformComponent | definition | tracked | UiTransformComponentは、UiWidgetの位置、寸法、anchorをlayout可能な状態として保持します | pass | pass | - |
| src/engine/gui/editor/GuiEditor.cpp | 35 | struct | Unnamed::Gui::<anonymous-namespace@31>::PaletteTemplate | definition | tracked | PaletteTemplateは、GUI editor paletteに表示する名称、分類、widget生成callbackを保持します | pass | pass | - |
| src/engine/gui/editor/GuiEditor.h | 11 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/editor/GuiEditor.h | 12 | class | Unnamed::ImGuiLayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/editor/GuiEditor.h | 15 | struct | Unnamed::Render::SceneOutputView | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/editor/GuiEditor.h | 20 | class | Unnamed::Gui::UiWidget | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/editor/GuiEditor.h | 21 | class | Unnamed::Gui::UiRoot | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/editor/GuiEditor.h | 24 | struct | Unnamed::Gui::GuiEditorContext | definition | tracked | GuiEditorContextは、GuiEditor処理中に共有する非所有参照と一時的なframe入力を保持します | pass | pass | - |
| src/engine/gui/layout/UiHorizontalLayout.h | 6 | class | Unnamed::Gui::UiHorizontalLayout | definition | tracked | UiHorizontalLayoutは、子widgetを左から右へspacingとalignmentに従って配置します | pass | pass | - |
| src/engine/gui/layout/UiVerticalLayout.h | 6 | class | Unnamed::Gui::UiVerticalLayout | definition | tracked | UiVerticalLayoutは、子widgetを上から下へspacingとalignmentに従って配置します | pass | pass | - |
| src/engine/gui/layout/base/UiLayout.h | 7 | class | Unnamed::Gui::UiLinearLayoutComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/gui/layout/base/UiLayout.h | 10 | class | Unnamed::Gui::UiLayout | definition | tracked | UiLayoutは、親領域と子widgetのsize policyから配置矩形を計算する基底契約を提供します | pass | pass | - |
| src/engine/physics/core/Physics.cpp | 799 | struct | Unnamed::Physics::<local@799>::EntityOverlapAggregate | definition | tracked | EntityOverlapAggregateは、同一entityに属する複数shapeのoverlap結果をentity単位へ集約します | pass | pass | - |
| src/engine/physics/core/Physics.h | 16 | class | Unnamed::Physics::Engine | definition | tracked | コライダー登録、空間照会、形状キャストを提供する物理ワールドを管理します | pass | pass | - |
| src/engine/physics/core/Physics.h | 186 | struct | Unnamed::Physics::Engine::<local@186>::StackItem | definition | tracked | StackItemは、debug path復元用parent indexを伴うBVH nodeを走査stackへ保持します | pass | pass | - |
| src/engine/physics/core/Physics.h | 191 | struct | Unnamed::Physics::Engine::<local@191>::PathEntry | definition | tracked | PathEntryは、Pathを検索・生成するためのkeyと対象参照をregistry内で保持します | pass | pass | - |
| src/engine/physics/core/Physics.h | 203 | struct | Unnamed::Physics::Engine::<local@203>::StackItem | definition | tracked | StackItemは、Release BVH走査で後から評価するnode indexだけを保持します | pass | pass | - |
| src/engine/platform/IPlatformEvents.h | 12 | struct | Unnamed::IWin32MsgListener | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/platform/IPlatformEvents.h | 15 | struct | Unnamed::IPlatformEvents | definition | tracked | プラットフォームイベントインターフェースクラス | pass | out-of-scope | - |
| src/engine/platform/PlatformEventsImpl.h | 8 | class | Unnamed::PlatformEventsImpl | definition | tracked | プラットフォームイベント実装クラス | pass | out-of-scope | - |
| src/engine/platform/Window.h | 12 | struct | Unnamed::WindowId | definition | tracked | WindowIdは、Windowsプラットフォームの対象を値型として安定して識別します | pass | pass | - |
| src/engine/platform/Window.h | 25 | struct | Unnamed::WindowDesc | definition | tracked | WindowDescは、Windowsプラットフォームオブジェクトの生成と初期化に必要な設定を保持します | pass | pass | - |
| src/engine/platform/Window.h | 35 | struct | Unnamed::WindowResizeEvent | definition | tracked | WindowResizeEventは、Windowsプラットフォームで発生した事象と配送に必要な付随値を保持します | pass | pass | - |
| src/engine/platform/Window.h | 42 | class | Unnamed::Window | definition | tracked | Win32ウィンドウの生成、メッセージ処理、およびネイティブハンドル寿命を管理します | pass | pass | - |
| src/engine/platform/WindowManager.h | 15 | class | Unnamed::WindowManager | definition | tracked | WindowManagerは、Win32 Window instanceを所有し、message pumpと終了要求を管理します | pass | pass | - |
| src/engine/platform/WindowsUtils.h | 8 | class | WindowsUtils | definition | tracked | Windowsユーティリティクラス | pass | out-of-scope | - |
| src/engine/platform/WindowsUtils.h | 27 | struct | WindowsUtils::ProcessResult | definition | tracked | ProcessResultは、子processのexit code、標準出力・標準error、timeoutとWin32起動errorを返します | pass | pass | - |
| src/engine/platform/WindowsUtils.h | 73 | struct | WindowsUtils::ProcessHandle | definition | tracked | ProcessHandleは、子プロセス、主スレッド、標準出力パイプのWin32ハンドルをCloseProcessHandleまで所有・管理します | pass | pass | - |
| src/engine/profiler/Profiler.h | 12 | class | Unnamed::Profiler | definition | tracked | パフォーマンスプロファイラー。フレームごとのサンプルを記録し、履歴を保持します。 | pass | out-of-scope | - |
| src/engine/profiler/Profiler.h | 15 | struct | Unnamed::Profiler::SampleView | definition | tracked | SampleViewは、profiler sampleの名称、経過時間、nest深度を表示用に参照します | pass | pass | - |
| src/engine/profiler/Profiler.h | 26 | class | Unnamed::Profiler::ScopeTimer | definition | tracked | ScopeTimerは、構築から破棄までの経過時間を計測し、対応するProfilerへRAIIで記録します | pass | pass | - |
| src/engine/profiler/Profiler.h | 68 | struct | Unnamed::Profiler::SampleData | definition | tracked | SampleDataは、named profiling scopeのframe内時間、履歴、平均・最大時間を保持します | pass | pass | - |
| src/engine/render/RenderDevice.h | 10 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/RenderDevice.h | 14 | class | Unnamed::Rhi::IRhiDevice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/RenderDevice.h | 19 | class | Unnamed::Render::RenderDevice | definition | tracked | RenderDeviceは、レンダリングデバイスの状態取得とネイティブ資源寿命をカプセル化します | pass | pass | - |
| src/engine/render/RenderModule.h | 13 | class | Unnamed::Rhi::IRhiDevice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/RenderModule.h | 16 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/RenderModule.h | 20 | struct | Unnamed::Render::RenderFrameInputs | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/RenderModule.h | 21 | class | Unnamed::Render::RenderDevice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/RenderModule.h | 24 | class | Unnamed::Render::RenderModule | definition | tracked | RenderModuleは、RendererとRHIの初期化・終了順序をmodule境界として集約します | pass | pass | - |
| src/engine/render/RenderStartupOptions.h | 13 | struct | Unnamed::Render::RenderStartupOptions | definition | tracked | Renderer起動時の検証オプションです。 | pass | out-of-scope | - |
| src/engine/render/Renderer.h | 30 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/Renderer.h | 34 | class | Unnamed::Rhi::D3D12Device | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/Renderer.h | 38 | struct | Unnamed::Render::RenderFrameInputs | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/Renderer.h | 39 | class | Unnamed::Render::RenderPassContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/Renderer.h | 40 | class | Unnamed::Render::RenderDevice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/Renderer.h | 43 | struct | Unnamed::Render::SceneOutputView | definition | tracked | UI などがシーン出力をサンプリングするための SRV と有効 UV 範囲です。 | pass | out-of-scope | - |
| src/engine/render/Renderer.h | 63 | struct | Unnamed::Render::MaterialTextureSet | definition | tracked | 解決済み Material texture の RgTextureId セット。 | pass | out-of-scope | - |
| src/engine/render/Renderer.h | 73 | class | Unnamed::Render::Renderer | definition | tracked | フレーム入力から RenderGraph を構築し、シーンおよび UI 描画を記録するレンダラーです。 | pass | out-of-scope | - |
| src/engine/render/Renderer.h | 122 | struct | Unnamed::Render::Renderer::MaterialBinding | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/Renderer.h | 214 | struct | Unnamed::Render::Renderer::FullscreenPassRes | definition | tracked | FullscreenPassResは、Fullscreen描画passが再利用するroot signatureとpipeline stateを保持します | pass | pass | - |
| src/engine/render/Renderer.h | 220 | struct | Unnamed::Render::Renderer::ComputePassRes | definition | tracked | ComputePassResは、Compute描画passが再利用するroot signatureとpipeline stateを保持します | pass | pass | - |
| src/engine/render/Renderer.h | 226 | struct | Unnamed::Render::Renderer::GeometryPassRes | definition | tracked | GeometryPassResは、Geometry描画passが再利用するroot signatureとpipeline stateを保持します | pass | pass | - |
| src/engine/render/Renderer.h | 247 | struct | Unnamed::Render::Renderer::MaterialBinding | definition | tracked | MaterialBindingは、Materialの論理識別子とruntime resource参照の対応を保持します | pass | pass | - |
| src/engine/render/Renderer.h | 263 | struct | Unnamed::Render::Renderer::PostFxRuntimePass | definition | tracked | PostFxRuntimePassは、post-process passのmaterial、入出力resource、実行順を保持します | pass | pass | - |
| src/engine/render/Renderer.h | 272 | struct | Unnamed::Render::Renderer::SpritePassRes | definition | tracked | SpritePassResは、Sprite描画passが再利用するroot signatureとpipeline stateを保持します | pass | pass | - |
| src/engine/render/Renderer.h | 279 | struct | Unnamed::Render::Renderer::BillboardPassRes | definition | tracked | BillboardPassResは、Billboard描画passが再利用するroot signatureとpipeline stateを保持します | pass | pass | - |
| src/engine/render/Renderer.h | 285 | struct | Unnamed::Render::Renderer::SkyboxPassRes | definition | tracked | SkyboxPassResは、Skybox描画passが再利用するroot signatureとpipeline stateを保持します | pass | pass | - |
| src/engine/render/Renderer.h | 290 | struct | Unnamed::Render::Renderer::DebugLineVertex | definition | tracked | DebugLineVertexは、debug line shaderへ渡すworld位置と色の頂点layoutを定義します | pass | pass | - |
| src/engine/render/Renderer.h | 301 | struct | Unnamed::Render::Renderer::LinePassRes | definition | tracked | LinePassResは、Line描画passが再利用するroot signatureとpipeline stateを保持します | pass | pass | - |
| src/engine/render/Renderer.h | 313 | struct | Unnamed::Render::Renderer::DirectionalShadowRuntimeState | definition | tracked | DirectionalShadowRuntimeStateは、directional shadowの有効cascadeとframe resource参照を保持します | pass | pass | - |
| src/engine/render/Renderer.h | 327 | struct | Unnamed::Render::Renderer::ViewRuntimeState | definition | tracked | ViewRuntimeStateは、描画viewごとの履歴textureと前frame camera情報を保持します | pass | pass | - |
| src/engine/render/RendererGeometry.cpp | 168 | struct | Unnamed::Render::<anonymous-namespace@26>::VertexGeom | definition | tracked | VertexGeomは、geometry passへ渡すposition、normal、tangent、UVの頂点layoutを定義します | pass | pass | - |
| src/engine/render/RendererGeometry.cpp | 178 | struct | Unnamed::Render::<anonymous-namespace@26>::QuadVertex | definition | tracked | QuadVertexは、fullscreen quad shaderへ渡すclip位置とUVの頂点layoutを定義します | pass | pass | - |
| src/engine/render/RendererGraph.cpp | 54 | struct | Unnamed::Render::<anonymous-namespace@25>::DirectionalShadowMatrices | definition | tracked | DirectionalShadowMatricesは、directional light cascadeごとのview・projection行列を保持します | pass | pass | - |
| src/engine/render/RendererGraph.cpp | 94 | struct | Unnamed::Render::<anonymous-namespace@25>::PostFxParamsConstants | definition | tracked | PostFxParamsConstantsは、post-process material parameterをshader定数layoutで保持します | pass | pass | - |
| src/engine/render/RendererGraph.cpp | 102 | struct | Unnamed::Render::<anonymous-namespace@25>::BloomPyramidConstants | definition | tracked | BloomPyramidConstantsは、bloom pyramidのsource寸法、mip index、thresholdをshader定数として保持します | pass | pass | - |
| src/engine/render/RendererGraph.cpp | 110 | struct | Unnamed::Render::<anonymous-namespace@25>::FitRect | definition | tracked | FitRectは、レンダリング要素の位置と寸法を同じ座標系で表します | pass | pass | - |
| src/engine/render/RendererPipelineCatalog.h | 9 | class | Unnamed::Render::RendererPipelineCatalog | definition | tracked | レンダラ向けパイプライン定義のプリセット生成を担います。 | pass | out-of-scope | - |
| src/engine/render/TextureResourceCache.h | 9 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/TextureResourceCache.h | 13 | class | Unnamed::Render::RgResourceRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/TextureResourceCache.h | 16 | struct | Unnamed::Render::TextureResourceCacheDebugStats | definition | tracked | TextureResourceCache のデバッグ統計情報です。 | pass | out-of-scope | - |
| src/engine/render/TextureResourceCache.h | 30 | class | Unnamed::Render::TextureResourceCache | definition | tracked | AssetID から RgTextureId への解決と寿命管理を行います。 | pass | out-of-scope | - |
| src/engine/render/TextureResourceCache.h | 63 | struct | Unnamed::Render::TextureResourceCache::CacheEntry | definition | tracked | CacheEntryは、レンダリングキャッシュ内の資源と最終利用情報を同じ寿命で保持します | pass | pass | - |
| src/engine/render/foundation/AdvancedRenderFoundation.h | 10 | class | Unnamed::Render::RenderGraph | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/foundation/AdvancedRenderFoundation.h | 20 | struct | Unnamed::Render::RtInstanceDesc | definition | tracked | RtInstanceDescは、TLAS instanceのtransform、BLAS address、mask、instance IDを指定します | pass | pass | - |
| src/engine/render/foundation/AdvancedRenderFoundation.h | 28 | struct | Unnamed::Render::RtFrameState | definition | tracked | RtFrameStateは、ray tracing機能のframe有効状態と診断値を保持します | pass | pass | - |
| src/engine/render/foundation/AdvancedRenderFoundation.h | 35 | struct | Unnamed::Render::GiFrameState | definition | tracked | GiFrameStateは、global illumination機能のframe有効状態と診断値を保持します | pass | pass | - |
| src/engine/render/foundation/AdvancedRenderFoundation.h | 42 | struct | Unnamed::Render::VirtualGeometryConfig | definition | tracked | VirtualGeometryConfigは、VirtualGeometry機能の生成時に適用する有効化条件と調整値を保持します | pass | pass | - |
| src/engine/render/foundation/AdvancedRenderFoundation.h | 49 | struct | Unnamed::Render::VirtualGeometryFrameState | definition | tracked | VirtualGeometryFrameStateは、virtual geometry機能のframe有効状態と診断値を保持します | pass | pass | - |
| src/engine/render/foundation/AdvancedRenderFoundation.h | 55 | class | Unnamed::Render::AdvancedRenderFoundation | definition | tracked | AdvancedRenderFoundationは、レイトレーシング、GI、仮想ジオメトリ機能の利用可否とフレーム状態を集約します | pass | pass | - |
| src/engine/render/frame/RenderFrameContext.h | 11 | struct | Unnamed::Render::RenderOverlayFrameData | definition | tracked | オーバーレイ描画用のフレーム投入データです。 | pass | out-of-scope | - |
| src/engine/render/frame/RenderFrameContext.h | 20 | struct | Unnamed::Render::RenderFrameContext | definition | tracked | フレーム単位のレンダー投入データを集約します。 | pass | out-of-scope | - |
| src/engine/render/frame/RenderFrameInputs.h | 28 | struct | Unnamed::Render::SpriteTextureRef | definition | tracked | SpriteTextureRefは、sprite textureのasset IDと解決済みGPU resource参照を保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 44 | struct | Unnamed::Render::SceneViewRenderMode | definition | tracked | SceneViewRenderModeは、scene viewで有効にするlighting、wireframe、debug描画modeを保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 70 | struct | Unnamed::Render::PostFxPassOverride | definition | tracked | PostFxPassOverrideは、名前で指定したpost-process passの有効状態とparameter overrideを保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 89 | struct | Unnamed::Render::RenderViewOutputDesc | definition | tracked | RenderViewOutputDescは、render viewの出力texture、format、幅、高さを指定します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 99 | struct | Unnamed::Render::RenderCameraInput | definition | tracked | RenderCameraInputは、camera位置、view・projection行列、viewportを1 view分の描画入力として保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 112 | struct | Unnamed::Render::SkyboxInput | definition | tracked | SkyboxInputは、skybox texture、world回転、露出値を背景描画入力として保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 119 | struct | Unnamed::Render::DirectionalLightInput | definition | tracked | DirectionalLightInputは、方向光の向き、色、強度、shadow設定をframe描画入力として保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 130 | struct | Unnamed::Render::EnvironmentLightInput | definition | tracked | EnvironmentLightInputは、ambient色、IBL texture、露出値を環境照明入力として保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 138 | struct | Unnamed::Render::VisibleRenderObject | definition | tracked | VisibleRenderObjectは、cullingを通過したrender object、world transform、material参照を保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 152 | struct | Unnamed::Render::SkinningPaletteInput | definition | tracked | SkinningPaletteInputは、skinned meshが参照するbone行列範囲とpalette IDを保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 158 | struct | Unnamed::Render::ScreenSpriteInput | definition | tracked | ScreenSpriteInputは、screen-space spriteの矩形、UV、色、texture参照を保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 172 | struct | Unnamed::Render::WorldBillboardInput | definition | tracked | WorldBillboardInputは、camera正対quadのworld位置、寸法、色、texture参照を保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 184 | struct | Unnamed::Render::WorldSpriteInput | definition | tracked | WorldSpriteInputは、world-space spriteのtransform、UV、色、texture参照を保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 197 | struct | Unnamed::Render::DebugLineInput | definition | tracked | DebugLineInputは、world-space線分の2端点と色をdebug描画入力として保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 204 | struct | Unnamed::Render::DebugDrawFrameInput | definition | tracked | DebugDrawFrameInputは、1frameに提出するdebug線分列と描画有効状態を保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 209 | struct | Unnamed::Render::RenderViewInput | definition | tracked | RenderViewInputは、1 viewのcamera、出力先、render mode、可視object範囲を保持します | pass | pass | - |
| src/engine/render/frame/RenderFrameInputs.h | 229 | struct | Unnamed::Render::RenderFrameInputs | definition | tracked | RenderFrameInputsは、WorldからRendererへ渡すview、light、visible object、UIを1frame snapshotとして所有します | pass | pass | - |
| src/engine/render/rendergraph/IDescriptorResolver.h | 8 | class | Unnamed::Render::IDescriptorResolver | definition | tracked | IDescriptorResolverは、RenderGraph resource handleからframe有効なSRV・UAV descriptorを解決します | pass | pass | - |
| src/engine/render/rendergraph/RegistryDescriptorResolver.h | 6 | class | Unnamed::Render::RgResourceRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RegistryDescriptorResolver.h | 9 | class | Unnamed::Render::RegistryDescriptorResolver | definition | tracked | RegistryDescriptorResolverは、RgResourceRegistryを参照してtexture handleのdescriptorを解決します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraph.cpp | 45 | struct | Unnamed::Render::<anonymous-namespace@43>::PassResourceAccess | definition | tracked | PassResourceAccessは、RenderGraph passがresourceへ行うread・write種別とpipeline stageを保持します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraph.cpp | 62 | struct | Unnamed::Render::<anonymous-namespace@43>::ResourceDependencyState | definition | tracked | ResourceDependencyStateは、RenderGraph resourceの直前accessと依存passをDAG構築中に保持します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraph.h | 13 | class | Unnamed::Rhi::D3D12CommandContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderGraph.h | 14 | class | Unnamed::Rhi::IRhiDevice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderGraph.h | 19 | struct | Unnamed::Render::RgUse | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderGraph.h | 20 | class | Unnamed::Render::RenderGraphBuilder | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderGraph.h | 21 | class | Unnamed::Render::RenderPassContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderGraph.h | 24 | struct | Unnamed::Render::RgPass | definition | tracked | RgPassは、RenderGraph passの名称、setup結果、実行callback、resource use列を所有します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraph.h | 37 | struct | Unnamed::Render::CompiledTransition | definition | tracked | CompiledTransitionは、resource barrierに必要な遷移前後stateと対象resourceを保持します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraph.h | 44 | struct | Unnamed::Render::CompiledPass | definition | tracked | CompiledPassは、実行順が確定したpass参照と事前barrier列を保持します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraph.h | 67 | class | Unnamed::Render::RenderGraph | definition | tracked | パスのリソース契約から実行順と D3D12 バリアをコンパイルします。 | pass | out-of-scope | - |
| src/engine/render/rendergraph/RenderGraphBuilder.h | 7 | struct | Unnamed::Render::RgTextureDesc | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderGraphBuilder.h | 8 | class | Unnamed::Render::RgResourceRegistry | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderGraphBuilder.h | 21 | struct | Unnamed::Render::RgUse | definition | tracked | RgUseは、RenderGraph resource handleと要求accessを1件のpass useとして保持します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraphBuilder.h | 27 | struct | Unnamed::Render::Color | definition | tracked | Colorは、render target clear命令へ渡すRGBA値を保持します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraphBuilder.h | 32 | struct | Unnamed::Render::RgClearCmd | definition | tracked | RgClearCmdは、render target handleとclear colorをpass開始時のclear命令として保持します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraphBuilder.h | 38 | struct | Unnamed::Render::RgDepthClearCmd | definition | tracked | RgDepthClearCmdは、depth target handleとdepth・stencil値をclear命令として保持します | pass | pass | - |
| src/engine/render/rendergraph/RenderGraphBuilder.h | 45 | class | Unnamed::Render::RenderGraphBuilder | definition | tracked | RenderGraphBuilderは、RenderGraphの入力記述から実行用データ構造を構築します | pass | pass | - |
| src/engine/render/rendergraph/RenderPassContext.h | 11 | class | Unnamed::Rhi::D3D12CommandContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderPassContext.h | 15 | class | Unnamed::Render::RenderGraph | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RenderPassContext.h | 20 | class | Unnamed::Render::RenderPassContext | definition | tracked | RenderGraph が確定したパス状態の上で描画コマンドを記録するためのコンテキストです。 | pass | out-of-scope | - |
| src/engine/render/rendergraph/RenderPassContext.h | 100 | class | Unnamed::Render::RenderPassContext::RenderGraph | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RgResourceRegistry.h | 15 | class | Unnamed::Rhi::D3D12Device | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RgResourceRegistry.h | 19 | struct | Unnamed::TextureAssetData | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/rendergraph/RgResourceRegistry.h | 29 | struct | Unnamed::Render::RgTextureDesc | definition | tracked | RgTextureDescは、RenderGraphオブジェクトの生成と初期化に必要な設定を保持します | pass | pass | - |
| src/engine/render/rendergraph/RgResourceRegistry.h | 56 | struct | Unnamed::Render::RgRegistryDebugStats | definition | tracked | RgRegistryDebugStatsは、RenderGraphの診断用カウンターと集計値を保持します | pass | pass | - |
| src/engine/render/rendergraph/RgResourceRegistry.h | 69 | struct | Unnamed::Render::RgSrvDescriptorTable | definition | tracked | RgSrvDescriptorTableは、RenderGraph SRV tableのdescriptor先頭、要素数、frame寿命を保持します | pass | pass | - |
| src/engine/render/rendergraph/RgResourceRegistry.h | 79 | class | Unnamed::Render::RgResourceRegistry | definition | tracked | RgResourceRegistryは、RenderGraphの実装を安定キーで登録し、利用側へ解決します | pass | pass | - |
| src/engine/render/rendergraph/RgResourceRegistry.h | 153 | struct | Unnamed::Render::RgResourceRegistry::TexEntry | definition | tracked | TexEntryは、Texを検索・生成するためのkeyと対象参照をregistry内で保持します | pass | pass | - |
| src/engine/render/rendergraph/RgResourceRegistry.h | 174 | struct | Unnamed::Render::RgResourceRegistry::RetiredTextureResource | definition | tracked | RetiredTextureResourceは、置換済みtexture resourceと安全に解放できるframe番号を保持します | pass | pass | - |
| src/engine/render/shaders/MountAwareDxcIncludeHandler.h | 12 | class | Unnamed::Render::MountAwareDxcIncludeHandler | definition | tracked | 事前解決済みShaderCompileUnitだけをDXCへ公開するinclude handlerです。 | pass | out-of-scope | - |
| src/engine/render/shaders/PipelineCache.h | 15 | class | Unnamed::Render::ShaderLibrary | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/shaders/PipelineCache.h | 18 | struct | Unnamed::Render::GraphicsPsoKey | definition | tracked | GraphicsPsoKeyは、graphics PSOを一意に決めるshader、root signature、raster・depth・blend状態を保持します | pass | pass | - |
| src/engine/render/shaders/PipelineCache.h | 99 | struct | Unnamed::Render::ComputePipelineKey | definition | tracked | ComputePipelineKeyは、compute PSOを一意に決めるshaderとroot signatureを保持します | pass | pass | - |
| src/engine/render/shaders/PipelineCache.h | 127 | struct | Unnamed::Render::GraphicsPipelineKeyHash | definition | tracked | GraphicsPipelineKeyHashは、シェーダー、ルートシグネチャー、描画状態、頂点レイアウトからGraphicsPsoKeyのハッシュを計算します | pass | pass | - |
| src/engine/render/shaders/PipelineCache.h | 175 | struct | Unnamed::Render::GraphicsPipelineKeyEqual | definition | tracked | GraphicsPipelineKeyEqualは、GraphicsPsoKeyの全パイプライン状態を比較して同一PSOを表すか判定します | pass | pass | - |
| src/engine/render/shaders/PipelineCache.h | 289 | struct | Unnamed::Render::ComputePipelineKeyHash | definition | tracked | ComputePipelineKeyHashは、コンピュートシェーダーとルートシグネチャーからComputePipelineKeyのハッシュを計算します | pass | pass | - |
| src/engine/render/shaders/PipelineCache.h | 299 | struct | Unnamed::Render::BuiltInputLayout | definition | tracked | BuiltInputLayoutは、PSO作成まで文字列を生存させながらD3D12入力要素配列を所有します | pass | pass | - |
| src/engine/render/shaders/PipelineCache.h | 308 | class | Unnamed::Render::PipelineCache | definition | tracked | PipelineCacheは、描画・コンピュート設定をキーにD3D12 PSOを生成して再利用します | pass | pass | - |
| src/engine/render/shaders/PipelineRegistry.h | 10 | class | Unnamed::Render::RenderDevice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/shaders/PipelineRegistry.h | 31 | struct | Unnamed::Render::PipelineHandle | definition | tracked | パイプライン登録エントリを識別するハンドル。 | pass | out-of-scope | - |
| src/engine/render/shaders/ShaderCompileUnit.h | 13 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/shaders/ShaderCompileUnit.h | 18 | struct | Unnamed::Render::ShaderCompileIncludeEntry | definition | tracked | DXCへ渡す事前解決済みinclude sourceです。 | pass | out-of-scope | - |
| src/engine/render/shaders/ShaderCompileUnit.h | 27 | struct | Unnamed::Render::ShaderCompileUnit | definition | tracked | 1回のShader compileで使用するroot sourceとinclude tableです。 | pass | out-of-scope | - |
| src/engine/render/shaders/ShaderCompileUnit.h | 36 | class | Unnamed::Render::ShaderCompileUnitBuilder | definition | tracked | AssetManagerの確定済み依存グラフからShader compile unitを構築します。 | pass | out-of-scope | - |
| src/engine/render/shaders/ShaderKey.h | 9 | struct | Unnamed::Render::ShaderKey | definition | tracked | ShaderKeyは、shader compile結果を一意に決めるpath、entry point、profile、define列を保持します | pass | pass | - |
| src/engine/render/shaders/ShaderKey.h | 25 | struct | Unnamed::Render::ShaderKeyHash | definition | tracked | ShaderKeyHashは、シェーダーパス、エントリーポイント、プロファイル、define列からShaderKeyのハッシュを計算します | pass | pass | - |
| src/engine/render/shaders/ShaderLibrary.h | 12 | class | Unnamed::Rhi::DxcShaderCompiler | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/shaders/ShaderLibrary.h | 15 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/render/shaders/ShaderLibrary.h | 20 | struct | Unnamed::Render::ShaderDxil | definition | tracked | ShaderDxilは、compile済みDXIL bytecodeとshader cache識別情報を所有します | pass | pass | - |
| src/engine/render/shaders/ShaderLibrary.h | 25 | class | Unnamed::Render::ShaderLibrary | definition | tracked | ShaderLibraryは、コンパイル済みDXILと依存ファイル指紋をキー別にキャッシュします | pass | pass | - |
| src/engine/render/shaders/ShaderLibrary.h | 43 | struct | Unnamed::Render::ShaderLibrary::ShaderDependencyFingerprint | definition | tracked | ShaderDependencyFingerprintは、shader依存fileのpath、更新時刻、content hashを保持します | pass | pass | - |
| src/engine/rhi/Buffer.h | 12 | struct | Unnamed::Render::MeshSubMeshRange | definition | tracked | MeshSubMeshRangeは、submeshのindex開始位置、index数、material slotを保持します | pass | pass | - |
| src/engine/rhi/Buffer.h | 19 | struct | Unnamed::Render::MeshBuffer | definition | tracked | MeshBufferは、vertex・index GPU bufferとsubmesh範囲列を所有します | pass | pass | - |
| src/engine/rhi/Constants.h | 8 | struct | Unnamed::Rhi::FrameConstants | definition | tracked | FrameConstantsは、frame時刻、camera、viewportなど全描画で共有するshader定数を保持します | pass | pass | - |
| src/engine/rhi/Constants.h | 27 | struct | Unnamed::Rhi::ObjectConstants | definition | tracked | ObjectConstantsは、1 render objectのworld行列とobject識別値をshader定数として保持します | pass | pass | - |
| src/engine/rhi/Constants.h | 44 | struct | Unnamed::Rhi::SkinningPaletteConstants | definition | tracked | SkinningPaletteConstantsは、skinning shaderへ転送するbone変換行列列を保持します | pass | pass | - |
| src/engine/rhi/Constants.h | 55 | struct | Unnamed::Rhi::MaterialConstants | definition | tracked | MaterialConstantsは、PBR materialの色、roughness、metallic等をshader定数として保持します | pass | pass | - |
| src/engine/rhi/Constants.h | 76 | struct | Unnamed::Rhi::ShadowConstants | definition | tracked | ShadowConstantsは、shadow map変換、cascade境界、bias値をshader定数として保持します | pass | pass | - |
| src/engine/rhi/Constants.h | 96 | struct | Unnamed::Rhi::EnvironmentLightingConstants | definition | tracked | EnvironmentLightingConstantsは、ambient・IBL強度とenvironment parameterをshader定数として保持します | pass | pass | - |
| src/engine/rhi/DxcShaderCompiler.h | 12 | class | Unnamed::Rhi::DxcShaderCompiler | definition | tracked | DxcShaderCompilerは、RHIのソースと設定を実行可能な中間表現へコンパイルします | pass | pass | - |
| src/engine/rhi/PipelineKey.h | 23 | struct | Unnamed::Rhi::VertexElementDesc | definition | tracked | VertexElementDescは、1頂点属性のsemantic、format、byte offset、input slotを指定します | pass | pass | - |
| src/engine/rhi/PipelineKey.h | 35 | struct | Unnamed::Rhi::VertexLayoutDesc | definition | tracked | VertexLayoutDescは、PSO作成へ渡す頂点属性列とstrideを指定します | pass | pass | - |
| src/engine/rhi/RhiTypes.h | 6 | struct | Unnamed::Rhi::DeviceDesc | definition | tracked | DeviceDescは、D3D12 device生成時のadapter選択とdebug機能設定を指定します | pass | pass | - |
| src/engine/rhi/RhiTypes.h | 17 | struct | Unnamed::Rhi::SwapChainDesc | definition | tracked | SwapChainDescは、swap chainのwindow、幅、高さ、format、buffer数を指定します | pass | pass | - |
| src/engine/rhi/RhiTypes.h | 29 | struct | Unnamed::Rhi::ClearColor | definition | tracked | ClearColorは、描画色のRGBA成分を同一色空間の値として保持します | pass | pass | - |
| src/engine/rhi/UploadBuffer.h | 16 | class | Unnamed::Rhi::UploadBuffer | definition | tracked | UploadBufferは、永続mapしたD3D12 upload heapを所有し、256 byte strideで要素を書き込みます | pass | pass | NOT_ADDED: trivially_copyable is necessary for byte copying but insufficient for GPU/HLSL ABI compatibility; no stable used public contract exists. |
| src/engine/rhi/d3d12/D3D12CommandContext.h | 9 | class | Unnamed::Rhi::D3D12CommandContext | definition | tracked | D3D12CommandContextは、コマンドリストへの描画・ディスパッチ・資源遷移記録を仲介します | pass | pass | - |
| src/engine/rhi/d3d12/D3D12Device.h | 15 | class | Unnamed::Render::PipelineCache | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/rhi/d3d12/D3D12Device.h | 16 | class | Unnamed::Render::ShaderLibrary | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/rhi/d3d12/D3D12Device.h | 20 | class | Unnamed::Rhi::DxcShaderCompiler | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/rhi/d3d12/D3D12Device.h | 24 | class | Unnamed::Rhi::D3D12Device | definition | tracked | D3D12Deviceは、ID3D12Device、queue、descriptor allocator、upload contextを所有します | pass | pass | - |
| src/engine/rhi/d3d12/D3D12Device.h | 43 | class | Unnamed::Rhi::D3D12Device::D3D12SwapChain | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/rhi/d3d12/D3D12Device.h | 83 | class | Unnamed::Rhi::D3D12Device::UploadContext | definition | tracked | UploadContextは、一時アップロード資源とコピーコマンドをフェンス完了まで所有します | pass | pass | - |
| src/engine/rhi/d3d12/D3D12Device.h | 158 | struct | Unnamed::Rhi::D3D12Device::FrameContext | definition | tracked | FrameContextは、GPU fence完了まで再利用しないcommand allocatorとframe upload allocatorを所有します | pass | pass | - |
| src/engine/rhi/d3d12/D3D12FrameUploadAllocator.h | 9 | class | Unnamed::Rhi::D3D12FrameUploadAllocator | definition | tracked | D3D12FrameUploadAllocatorは、フレームごとにリセットするUpload Heapから整列済み領域を払い出します | pass | pass | - |
| src/engine/rhi/d3d12/D3D12SwapChain.h | 12 | class | Unnamed::Rhi::D3D12SwapChain | definition | tracked | D3D12SwapChainは、DXGI swap chainとback buffer viewの生成、resize、presentを管理します | pass | pass | - |
| src/engine/rhi/interface/IRhiCommandContext.h | 6 | class | Unnamed::Rhi::IRhiCommandContext | definition | tracked | IRhiCommandContextは、描画・dispatch・copy・resource barrierを記録するRHI command契約を定義します | pass | pass | - |
| src/engine/rhi/interface/IRhiDevice.h | 11 | class | Unnamed::Rhi::IRhiDevice | definition | tracked | IRhiDeviceは、buffer・texture・pipeline生成とcommand context取得のRHI device契約を定義します | pass | pass | - |
| src/engine/rhi/interface/IRhiSwapChain.h | 8 | class | Unnamed::Rhi::IRhiSwapChain | definition | tracked | IRhiSwapChainは、back buffer取得、resize、presentを提供するswap chain契約を定義します | pass | pass | - |
| src/engine/scene/Scene.h | 10 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/Scene.h | 11 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/Scene.h | 12 | class | Unnamed::BaseComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/Scene.h | 13 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/Scene.h | 14 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/Scene.h | 19 | struct | Unnamed::SceneEntityHandle | definition | tracked | SceneEntityHandleは、エンジンの所有者が管理する資源を世代付きまたは型付きIDで参照します | pass | pass | - |
| src/engine/scene/Scene.h | 24 | class | Unnamed::Scene | definition | tracked | Sceneは、シリアライズ対象のエンティティー集合とシーン識別情報を所有します | pass | pass | - |
| src/engine/scene/SceneLoadOptions.h | 9 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/SceneLoadOptions.h | 18 | struct | Unnamed::SceneLoadOptions | definition | tracked | シーン読込時の動作を指定します。 | pass | out-of-scope | - |
| src/engine/scene/SceneLoadOptions.h | 24 | struct | Unnamed::SceneDeserializeContext | definition | tracked | コンポーネントのシーンデシリアライズに必要な文脈です。 | pass | out-of-scope | - |
| src/engine/scene/SceneSerializer.h | 7 | class | Unnamed::Scene | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/SceneSerializer.h | 8 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/SceneSerializer.h | 9 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/scene/SceneSerializer.h | 16 | class | Unnamed::SceneSerializer | definition | tracked | SceneSerializerは、SceneのエンティティーとコンポーネントをJSONへ保存・復元し、シーン複製を仲介します | pass | pass | - |
| src/engine/sequence/CompiledSequence.h | 10 | struct | Unnamed::CompiledSequenceSection | definition | tracked | コンパイル済みセクション参照です。 | pass | out-of-scope | - |
| src/engine/sequence/CompiledSequence.h | 15 | struct | Unnamed::CompiledSequenceTrack | definition | tracked | コンパイル済みトラック参照です。 | pass | out-of-scope | - |
| src/engine/sequence/CompiledSequence.h | 21 | class | Unnamed::CompiledSequence | definition | tracked | 実行時評価用に前処理したシーケンスです。 | pass | out-of-scope | - |
| src/engine/sequence/PreAnimatedStateStore.h | 12 | struct | Unnamed::SequenceTransformSnapshot | definition | tracked | Transformの退避値です。 | pass | out-of-scope | - |
| src/engine/sequence/PreAnimatedStateStore.h | 19 | struct | Unnamed::SequenceCameraSnapshot | definition | tracked | カメラ選択状態の退避値です。 | pass | out-of-scope | - |
| src/engine/sequence/PreAnimatedStateStore.h | 25 | class | Unnamed::PreAnimatedStateStore | definition | tracked | Sequence適用前の値を保持するストアです。 | pass | out-of-scope | - |
| src/engine/sequence/SequencePlayer.h | 13 | struct | Unnamed::SequenceFrameRange | definition | tracked | シーケンス再生区間です。 | pass | out-of-scope | - |
| src/engine/sequence/SequencePlayer.h | 20 | class | Unnamed::SequencePlayer | definition | tracked | シーケンス再生を保持するランタイムプレイヤーです。 | pass | out-of-scope | - |
| src/engine/sequence/SequencePropertyAccessorRegistry.h | 11 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/sequence/SequencePropertyAccessorRegistry.h | 14 | struct | Unnamed::SequenceFloatAccessor | definition | tracked | Floatプロパティのアクセサです。 | pass | out-of-scope | - |
| src/engine/sequence/SequencePropertyAccessorRegistry.h | 20 | struct | Unnamed::SequenceBoolAccessor | definition | tracked | Boolプロパティのアクセサです。 | pass | out-of-scope | - |
| src/engine/sequence/SequencePropertyAccessorRegistry.h | 26 | struct | Unnamed::SequenceVec3Accessor | definition | tracked | Vec3プロパティのアクセサです。 | pass | out-of-scope | - |
| src/engine/sequence/SequencePropertyAccessorRegistry.h | 32 | class | Unnamed::SequencePropertyAccessorRegistry | definition | tracked | プロパティアクセスのレジストリです。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 19 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/sequence/SequenceRuntime.h | 20 | class | Unnamed::SequencePlayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/sequence/SequenceRuntime.h | 21 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/sequence/SequenceRuntime.h | 24 | class | Unnamed::SequenceRuntime | definition | tracked | シーケンス再生をワールド単位で管理するランタイムです。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 52 | struct | Unnamed::SequenceRuntime::FloatContribution | definition | tracked | Float寄与情報です。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 63 | struct | Unnamed::SequenceRuntime::BoolContribution | definition | tracked | Bool寄与情報です。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 73 | struct | Unnamed::SequenceRuntime::Vec3Contribution | definition | tracked | Vec3寄与情報です。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 84 | struct | Unnamed::SequenceRuntime::TransformContribution | definition | tracked | Transform寄与情報です。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 98 | struct | Unnamed::SequenceRuntime::EntityBoolContribution | definition | tracked | Entity bool寄与情報です。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 106 | struct | Unnamed::SequenceRuntime::CameraContribution | definition | tracked | カメラカット寄与情報です。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 113 | struct | Unnamed::SequenceRuntime::SkeletalContribution | definition | tracked | スケルタル制御寄与情報です。 | pass | out-of-scope | - |
| src/engine/sequence/SequenceRuntime.h | 130 | struct | Unnamed::SequenceRuntime::EventDispatch | definition | tracked | EventDispatchは、sequence eventの発火時刻、event名、payloadを保持します | pass | pass | - |
| src/engine/sequence/SequenceRuntime.h | 140 | struct | Unnamed::SequenceRuntime::FloatTargetMeta | definition | tracked | FloatTargetMetaは、sequenceのFloatTarget対象を実行時に解決する識別情報を保持します | pass | pass | - |
| src/engine/sequence/SequenceRuntime.h | 147 | struct | Unnamed::SequenceRuntime::BoolTargetMeta | definition | tracked | BoolTargetMetaは、sequenceのBoolTarget対象を実行時に解決する識別情報を保持します | pass | pass | - |
| src/engine/sequence/SequenceRuntime.h | 154 | struct | Unnamed::SequenceRuntime::Vec3TargetMeta | definition | tracked | Vec3TargetMetaは、sequenceのVec3Target対象を実行時に解決する識別情報を保持します | pass | pass | - |
| src/engine/sequence/SequenceRuntimeTypes.h | 33 | struct | Unnamed::SequenceTraversalRange | definition | tracked | 1ティック内でのシーケンス走査区間です。 | pass | out-of-scope | - |
| src/engine/tween/ITweenPlayable.h | 5 | class | Unnamed::ITweenPlayable | definition | tracked | ITweenPlayableは、Tween実装が満たす操作契約とライフタイム境界を定義します | pass | pass | - |
| src/engine/tween/TweenEase.h | 7 | class | Unnamed::TweenEase | definition | tracked | 正規化時間へイージング曲線を適用する純粋関数群を提供します | pass | pass | - |
| src/engine/tween/TweenHandle.h | 8 | class | Unnamed::TweenHandle | definition | tracked | TweenHandleは、再生中Tweenへのweak_ptrを保持し、対象の破棄後は無効な非所有ハンドルとして動作します | pass | pass | - |
| src/engine/tween/TweenInstance.h | 16 | class | Unnamed::TweenInstance | definition | tracked | TweenInstanceは、開始値から終了値までの補間、遅延、ループ、完了通知を再生状態として管理します | pass | pass | NOT_ADDED: TweenLerp is a customization point, while the removed constraint omitted required construction/assignment and could reject future interpolated types. |
| src/engine/tween/TweenLerp.h | 3 | struct | Vec4 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/tween/TweenLerp.h | 4 | struct | Vec2 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/tween/TweenLerp.h | 5 | struct | Vec3 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/tween/TweenLerp.h | 6 | struct | Quaternion | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/tween/TweenLerp.h | 10 | struct | Unnamed::TweenLerp | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/tween/TweenLerp.h | 14 | struct | Unnamed::TweenLerp | definition | tracked | float値を線形補間してTweenInstanceへ返します | pass | pass | EXISTING_SPECIALIZATION: explicit specializations are the current customization set. |
| src/engine/tween/TweenLerp.h | 22 | struct | Unnamed::TweenLerp | definition | tracked | double値を線形補間してTweenInstanceへ返します | pass | pass | EXISTING_SPECIALIZATION: explicit specializations are the current customization set. |
| src/engine/tween/TweenLerp.h | 30 | struct | Unnamed::TweenLerp | definition | tracked | Vec2の各成分を線形補間してTweenInstanceへ返します | pass | pass | EXISTING_SPECIALIZATION: explicit specializations are the current customization set. |
| src/engine/tween/TweenLerp.h | 38 | struct | Unnamed::TweenLerp | definition | tracked | Vec3の各成分を線形補間してTweenInstanceへ返します | pass | pass | EXISTING_SPECIALIZATION: explicit specializations are the current customization set. |
| src/engine/tween/TweenLerp.h | 46 | struct | Unnamed::TweenLerp | definition | tracked | Vec4の各成分を線形補間してTweenInstanceへ返します | pass | pass | EXISTING_SPECIALIZATION: explicit specializations are the current customization set. |
| src/engine/tween/TweenLerp.h | 54 | struct | Unnamed::TweenLerp | definition | tracked | Quaternionを球面線形補間してTweenInstanceへ返します | pass | pass | EXISTING_SPECIALIZATION: explicit specializations are the current customization set. |
| src/engine/tween/TweenManager.h | 9 | class | Unnamed::TweenManager | definition | tracked | TweenManagerは、active Tweenをshared ownershipで保持し、更新・完了・一括停止を管理します | pass | pass | - |
| src/engine/ui/ImGuiLayer.h | 11 | struct | ImGui_ImplDX12_InitInfo | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/ui/ImGuiLayer.h | 15 | class | Unnamed::Rhi::D3D12Device | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/ui/ImGuiLayer.h | 19 | class | Unnamed::Render::RenderPassContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/ui/ImGuiLayer.h | 23 | class | Unnamed::ImGuiLayer | definition | tracked | ImGuiLayerは、ImGuiコンテキスト、フレーム開始・描画、GPU descriptor寿命を管理します | pass | pass | - |
| src/engine/ui/ImGuiLayer.h | 61 | struct | Unnamed::ImGuiLayer::FrameTextureSlot | definition | tracked | FrameTextureSlotは、ImGui textureのSRV descriptorと最終参照frameを保持します | pass | pass | - |
| src/engine/ui/ImGuiLayer.h | 86 | struct | Unnamed::ImGuiLayer::TextureSlots | definition | tracked | TextureSlotsは、frameごとのImGui texture slot集合と再利用cursorを保持します | pass | pass | - |
| src/engine/ui/retained/IUiBackend.h | 11 | struct | Unnamed::UI::Retained::UiBackendFrameContext | definition | tracked | UiBackendFrameContextは、backend描画時に使用するviewport矩形をframe単位で渡します | pass | pass | - |
| src/engine/ui/retained/IUiBackend.h | 16 | struct | Unnamed::UI::Retained::UiRectPaintData | definition | tracked | UiRectPaintDataは、clip範囲を伴う角丸矩形の描画位置と色をbackendへ渡します | pass | pass | - |
| src/engine/ui/retained/IUiBackend.h | 26 | struct | Unnamed::UI::Retained::UiImagePaintData | definition | tracked | UiImagePaintDataは、clip範囲、texture、UV、tintを1件のimage描画命令としてbackendへ渡します | pass | pass | - |
| src/engine/ui/retained/IUiBackend.h | 39 | class | Unnamed::UI::Retained::IUiBackend | definition | tracked | IUiBackendは、retained UIを描画・入力バックエンドへ接続する契約を定義します | pass | pass | - |
| src/engine/ui/retained/UiAnimatedValue.h | 16 | class | Unnamed::UI::Retained::UiAnimatedValue | definition | tracked | UiAnimatedValueは、retained UI値の現在値、目標値、補間時間を保持してフレーム更新します | pass | pass | NOT_ADDED: copyable may be excessive, default construction was omitted, and operator syntax does not express interpolation semantics. |
| src/engine/ui/retained/UiDocument.h | 6 | struct | Unnamed::UI::Retained::UiNode | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/ui/retained/UiDocument.h | 9 | class | Unnamed::UI::Retained::UiDocument | definition | tracked | UiDocumentは、retained UIノード木と世代付きノードIDの割り当てを所有します | pass | pass | - |
| src/engine/ui/retained/UiNode.h | 11 | struct | Unnamed::UI::Retained::UiButtonVisualStyle | definition | tracked | UiButtonVisualStyleは、buttonの通常・hover・pressed状態ごとの背景色とtintを保持します | pass | pass | - |
| src/engine/ui/retained/UiNode.h | 24 | struct | Unnamed::UI::Retained::UiButtonStyle | definition | tracked | UiButtonStyleは、buttonのvisual style、padding、animation durationを保持します | pass | pass | - |
| src/engine/ui/retained/UiNode.h | 32 | struct | Unnamed::UI::Retained::UiButtonRuntimeState | definition | tracked | UiButtonRuntimeStateは、retained buttonのhover、press、animation値をframe間で保持します | pass | pass | - |
| src/engine/ui/retained/UiNode.h | 44 | struct | Unnamed::UI::Retained::UiButtonData | definition | tracked | UiButtonDataは、buttonのstyle、frame間の操作状態、有効・無効状態をnode内で保持します | pass | pass | - |
| src/engine/ui/retained/UiNode.h | 52 | struct | Unnamed::UI::Retained::UiImageData | definition | tracked | UiImageDataは、nodeが描画するtexture、UV範囲、tint色を保持します | pass | pass | - |
| src/engine/ui/retained/UiNode.h | 64 | struct | Unnamed::UI::Retained::UiNode | definition | tracked | UiNodeは、retained UIの階層またはグラフにおける接続関係とノード固有値を保持します | pass | pass | - |
| src/engine/ui/retained/UiSystem.h | 10 | struct | Unnamed::UI::Retained::UiFrameContext | definition | tracked | UiFrameContextは、UiFrame処理中に共有する非所有参照と一時的なframe入力を保持します | pass | pass | - |
| src/engine/ui/retained/UiSystem.h | 16 | class | Unnamed::UI::Retained::UiSystem | definition | tracked | UiSystemは、retained UIの初期化、フレーム更新、および終了順序を統括します | pass | pass | - |
| src/engine/ui/retained/UiSystem.h | 28 | struct | Unnamed::UI::Retained::UiSystem::TraversalContext | definition | tracked | TraversalContextは、Traversal処理中に共有する非所有参照と一時的なframe入力を保持します | pass | pass | - |
| src/engine/ui/retained/UiTypes.h | 13 | struct | Unnamed::UI::Retained::UiNodeId | definition | tracked | retained UIノードを世代付き整数で安定して識別します | pass | pass | - |
| src/engine/ui/retained/UiTypes.h | 28 | struct | Unnamed::UI::Retained::UiNodeHandle | definition | tracked | UIノードのハンドル | pass | out-of-scope | - |
| src/engine/ui/retained/UiTypes.h | 43 | struct | Unnamed::UI::Retained::UiRect | definition | tracked | retained UIレイアウトの位置と寸法をピクセル単位で表します | pass | pass | - |
| src/engine/ui/retained/UiTypes.h | 80 | struct | Unnamed::UI::Retained::UiInteractionResult | definition | tracked | UiInteractionResultは、retained UI nodeのhover、hold、click判定を当該frameの結果として返します | pass | pass | - |
| src/engine/ui/retained/UiTypes.h | 95 | struct | Unnamed::UI::Retained::UiEvent | definition | tracked | UiEventは、retained UIで発生した事象と配送に必要な付随値を保持します | pass | pass | - |
| src/engine/unnamed/framework/components/CameraComponent.h | 8 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/CameraComponent.h | 9 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/CameraComponent.h | 10 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/CameraComponent.h | 13 | class | Unnamed::CameraComponent | definition | tracked | CameraComponentは、view・projection行列とviewport設定をWorldの描画cameraとして提供します | pass | pass | - |
| src/engine/unnamed/framework/components/DirectionalLightComponent.h | 11 | class | Unnamed::DirectionalLightComponent | definition | tracked | DirectionalLightComponentは、方向光の向き、色、強度、shadow設定を描画入力へ提供します | pass | pass | - |
| src/engine/unnamed/framework/components/SkyLightComponent.h | 11 | class | Unnamed::SkyLightComponent | definition | tracked | SkyLightComponentは、環境光の色と強度をscene lighting入力へ提供します | pass | pass | - |
| src/engine/unnamed/framework/components/SkyboxComponent.h | 12 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/SkyboxComponent.h | 13 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/SkyboxComponent.h | 14 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/SkyboxComponent.h | 17 | class | Unnamed::SkyboxComponent | definition | tracked | SkyboxComponentは、skybox textureと描画設定をWorld rendererへ提供します | pass | pass | - |
| src/engine/unnamed/framework/components/TransformComponent.h | 11 | class | Unnamed::Scene | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/TransformComponent.h | 14 | class | Unnamed::TransformComponent | definition | tracked | Entity の位置・回転・スケールと親子階層を管理するコンポーネントです。 | pass | out-of-scope | - |
| src/engine/unnamed/framework/components/audio/AudioSourceComponent.h | 13 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/audio/AudioSourceComponent.h | 14 | class | Unnamed::AudioSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/audio/AudioSourceComponent.h | 15 | class | Unnamed::AudioVoice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/audio/AudioSourceComponent.h | 16 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/audio/AudioSourceComponent.h | 17 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/audio/AudioSourceComponent.h | 20 | class | Unnamed::AudioSourceComponent | definition | tracked | AudioSourceComponentは、sound assetとXAudio2 voiceの再生・停止・音量状態を管理します | pass | pass | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 6 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 7 | class | Unnamed::Scene | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 8 | class | Unnamed::Entity | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 9 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 10 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 11 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 12 | class | Unnamed::IDemoService | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 13 | class | Unnamed::AudioSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 14 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 15 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 16 | struct | Unnamed::SceneDeserializeContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/base/BaseComponent.h | 20 | class | Unnamed::BaseComponent | definition | tracked | Component は Entity に取り付けられるオブジェクトです。 | pass | out-of-scope | - |
| src/engine/unnamed/framework/components/collider/StaticMeshColliderComponent.h | 9 | class | Unnamed::StaticMeshColliderComponent | definition | tracked | StaticMeshColliderComponentは、static meshから物理colliderを登録し、Transform変更へ追従します | pass | pass | - |
| src/engine/unnamed/framework/components/editor/EditorCameraComponent.h | 8 | class | Unnamed::TransformComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/editor/EditorCameraComponent.h | 9 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/editor/EditorCameraComponent.h | 10 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/editor/EditorCameraComponent.h | 13 | class | Unnamed::EditorCameraComponent | definition | tracked | EditorCameraComponentは、Editor viewport入力から自由cameraの移動・回転を更新します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.cpp | 21 | struct | Unnamed::<anonymous-namespace@19>::LocalBonePose | definition | tracked | LocalBonePoseは、bone local poseのtranslation、rotation、scaleを保持します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.cpp | 28 | struct | Unnamed::<anonymous-namespace@19>::BlendAccumVec3 | definition | tracked | BlendAccumVec3は、Vec3 animation channelの加重和と総weightをblend中だけ保持します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.cpp | 34 | struct | Unnamed::<anonymous-namespace@19>::BlendAccumQuat | definition | tracked | BlendAccumQuatは、Quaternion animation channelの加重和と基準向きをblend中だけ保持します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | 13 | struct | Unnamed::MeshAssetData | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | 14 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | 15 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | 18 | class | Unnamed::SkeletalAnimationComponent | definition | tracked | SkeletalAnimationComponentは、animation clip、layer、transitionを評価してskeleton poseを更新します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | 21 | struct | Unnamed::SkeletalAnimationComponent::AnimationLayerDesc | definition | tracked | AnimationLayerDescは、animation layerの名前、weight、blend mode、初期stateを指定します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | 30 | struct | Unnamed::SkeletalAnimationComponent::AnimationStateDesc | definition | tracked | AnimationStateDescは、animation stateのclip、loop、speed、transition設定を指定します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | 102 | struct | Unnamed::SkeletalAnimationComponent::RuntimeLayerState | definition | tracked | RuntimeLayerStateは、animation layerの再生clip、時刻、weightを評価間で保持します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | 110 | struct | Unnamed::SkeletalAnimationComponent::RuntimeLayerState::TransitionState | definition | tracked | TransitionStateは、animation transitionのsource、destination、blend進行を保持します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h | 12 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h | 13 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h | 14 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h | 16 | struct | Unnamed::SkeletalMaterialSlotReference | definition | tracked | Skeletal MeshのMaterial slot参照です。 | pass | out-of-scope | - |
| src/engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h | 23 | class | Unnamed::SkeletalMeshRendererComponent | definition | tracked | SkeletalMeshRendererComponentは、skeletal mesh、material、bone paletteをrendererへ提出します | pass | pass | - |
| src/engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h | 11 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h | 12 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h | 13 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h | 16 | struct | Unnamed::MaterialSlot | definition | tracked | マテリアルスロットのデータ構造体 | pass | out-of-scope | - |
| src/engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h | 22 | class | Unnamed::StaticMeshRendererComponent | definition | tracked | StaticMeshRendererComponentは、static meshとmaterialの描画instanceをrendererへ提出します | pass | pass | - |
| src/engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h | 14 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h | 15 | class | Unnamed::JsonWriter | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h | 16 | class | Unnamed::SequencePlayer | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h | 19 | class | Unnamed::SequenceDirectorComponent | definition | tracked | Sequence再生要求とロック対象管理を行うコンポーネントです。 | pass | out-of-scope | - |
| src/engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h | 66 | struct | Unnamed::SequenceDirectorComponent::LockTargetSpec | definition | tracked | LockTargetSpecは、エンジンで対象を選択・拘束するための宣言的条件を保持します | pass | pass | - |
| src/engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h | 73 | struct | Unnamed::SequenceDirectorComponent::ActiveLockState | definition | tracked | ActiveLockStateは、sequenceが現在拘束する対象と解除条件を保持します | pass | pass | - |
| src/engine/unnamed/framework/components/ui/NewUICanvas.h | 17 | class | Unnamed::UI::UIFontAtlas | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/ui/NewUICanvas.h | 22 | class | Unnamed::NewUICanvas | definition | tracked | NewUICanvasは、エンジン要素のルート配置、入力、および描画提出を管理します | pass | pass | - |
| src/engine/unnamed/framework/components/ui/UiCanvasComponent.h | 18 | class | Unnamed::Gui::UiRoot | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/ui/UiCanvasComponent.h | 22 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/components/ui/UiCanvasComponent.h | 61 | class | Unnamed::UiCanvasComponent | definition | tracked | UiCanvasComponentは、entity上のGUI canvasをWorldの入力・描画更新へ登録します | pass | pass | - |
| src/engine/unnamed/framework/entity/Entity.h | 14 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/entity/Entity.h | 15 | class | Unnamed::Scene | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/framework/entity/Entity.h | 21 | class | Unnamed::Entity | definition | tracked | エンティティはゲームの基本オブジェクトです。 | pass | out-of-scope | - |
| src/engine/unnamed/physics/BVH.h | 9 | struct | <anonymous@9> | definition | tracked | 三角形群を階層化した境界ボリュームと葉インデックスを所有します | pass | pass | - |
| src/engine/unnamed/physics/BVH.h | 20 | struct | RegisteredBVH | definition | tracked | 登録されたBVH構造体 | pass | out-of-scope | - |
| src/engine/unnamed/physics/BVHBuilder.cpp | 96 | struct | Unnamed::Physics::<local@96>::Bucket | definition | tracked | Bucketは、BVH SAH分割bucketの境界boxとprimitive数を保持します | pass | pass | - |
| src/engine/unnamed/physics/BVHBuilder.h | 11 | struct | Unnamed::Physics::FlatNode | definition | tracked | FlatNodeは、物理照会の階層またはグラフにおける接続関係とノード固有値を保持します | pass | pass | - |
| src/engine/unnamed/physics/BVHBuilder.h | 19 | class | Unnamed::Physics::BVHBuilder | definition | tracked | BVHビルダークラス | pass | out-of-scope | - |
| src/engine/unnamed/physics/BoxCast.h | 8 | struct | Unnamed::Physics::BoxCast | definition | tracked | ボックスキャスト構造体 | pass | out-of-scope | - |
| src/engine/unnamed/physics/CollisionDetection.h | 3 | struct | Vec3 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/physics/CollisionDetection.h | 6 | struct | Unnamed::Box | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/physics/CollisionDetection.h | 7 | struct | Unnamed::Triangle | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/physics/CollisionDetection.h | 8 | struct | Unnamed::<anonymous@8> | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/physics/CollisionDetection.h | 9 | struct | Unnamed::Ray | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/physics/PhysicsTypes.h | 8 | struct | Unnamed::Physics::Hit | definition | tracked | 形状照会の衝突位置、法線、距離、および対象を呼び出し元へ返します | pass | pass | - |
| src/engine/unnamed/physics/PhysicsTypes.h | 21 | struct | Unnamed::Physics::TriInfo | definition | tracked | 衝突判定に使う三角形の頂点と面情報を保持します | pass | pass | - |
| src/engine/unnamed/physics/RayCast.h | 6 | struct | Unnamed::Physics::RayCast | definition | tracked | レイと三角形群の最近接ヒットを評価します | pass | pass | - |
| src/engine/unnamed/physics/ShapeCast.h | 7 | struct | Unnamed::Physics::ShapeCast | definition | tracked | 形状キャストインターフェース | pass | out-of-scope | - |
| src/engine/unnamed/physics/SphereCast.h | 6 | struct | Unnamed::Physics::SphereCast | definition | tracked | スフィアキャスト構造体 | pass | out-of-scope | - |
| src/engine/unnamed/primitive/Primitives.h | 5 | struct | Unnamed::Ray | definition | tracked | 始点と正規化方向で半直線を表します | pass | pass | - |
| src/engine/unnamed/primitive/Primitives.h | 14 | struct | Unnamed::Line | definition | tracked | 2端点で有限線分を表します | pass | pass | - |
| src/engine/unnamed/primitive/Primitives.h | 20 | struct | Unnamed::Triangle | definition | tracked | 3頂点とそこから導出される面法線を表します | pass | pass | - |
| src/engine/unnamed/primitive/Primitives.h | 29 | struct | Unnamed::Plane | definition | tracked | 単位法線と原点からの符号付き距離で平面を表します | pass | pass | - |
| src/engine/unnamed/primitive/Primitives.h | 40 | struct | Unnamed::Box | definition | tracked | 中心、半径、および姿勢で有向ボックスを表します | pass | pass | - |
| src/engine/unnamed/primitive/Primitives.h | 46 | struct | Unnamed::Frustum | definition | tracked | ビュー錐台を構成する6平面を保持します | pass | pass | - |
| src/engine/unnamed/primitive/Primitives.h | 51 | struct | Unnamed::Sphere | definition | tracked | 中心と半径で球を表します | pass | pass | - |
| src/engine/unnamed/primitive/Primitives.h | 62 | struct | Unnamed::<anonymous@62> | definition | tracked | Axis Aligned Bounding Box | pass | out-of-scope | - |
| src/engine/unnamed/primitive/Primitives.h | 98 | struct | Unnamed::Capsule | definition | tracked | 線分の両端と半径でカプセル形状を表します | pass | pass | - |
| src/engine/unnamed/subsystem/audio/Audio.h | 8 | struct | Unnamed::SoundAssetData | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/audio/Audio.h | 11 | class | Unnamed::AudioVoice | definition | tracked | サウンド再生インスタンス（1ボイス） | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/audio/AudioSystem.h | 7 | struct | IXAudio2MasteringVoice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/audio/AudioSystem.h | 8 | struct | IXAudio2 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/audio/AudioSystem.h | 11 | class | Unnamed::AudioVoice | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/audio/AudioSystem.h | 12 | struct | Unnamed::SoundAssetData | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/audio/AudioSystem.h | 15 | class | Unnamed::AudioSystem | definition | tracked | AudioSystemは、XAudio2 engineとmaster voiceを所有し、sound voiceの生成・停止を仲介します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 8 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 11 | class | Unnamed::ConVarHelper | definition | tracked | コンソール変数ヘルパークラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 53 | struct | Unnamed::ConVarHelper::Empty | definition | tracked | Emptyは、ConVar UI layout variantで内容を持たない空要素を表します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 58 | struct | Unnamed::ConVarHelper::Label | definition | tracked | ラベルの構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 67 | struct | Unnamed::ConVarHelper::Button | definition | tracked | ボタンの構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 77 | struct | Unnamed::ConVarHelper::ExecutableButton | definition | tracked | 実行可能ボタンの構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 87 | struct | Unnamed::ConVarHelper::GridElement | definition | tracked | グリッド要素の構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 92 | struct | Unnamed::ConVarHelper::Page | definition | tracked | Pageは、ConVar helper UIのpage名と所属control列を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | 96 | struct | Unnamed::ConVarHelper::Page::Grid | definition | tracked | Gridは、ConVar helper UIを行列配置する列数、間隔、control列を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConVarWriter.h | 5 | class | Unnamed::ConVarWriter | definition | tracked | ConVarWriterは、永続化対象のコンソール変数を名前と値のテキスト行としてファイルへ書き出します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConsoleFileLogSink.h | 19 | class | Unnamed::ConsoleFileLogSink | definition | tracked | コンソールログをファイルへ非同期に書き込むためのシンク | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConsoleFileLogSink.h | 22 | struct | Unnamed::ConsoleFileLogSink::Event | definition | tracked | Eventは、開発者コンソールで発生した事象と配送に必要な付随値を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConsoleFileLogSink.h | 33 | struct | Unnamed::ConsoleFileLogSink::Config | definition | tracked | Configは、機能の生成時に適用する有効化条件と調整値を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConsoleScriptParser.h | 6 | class | Unnamed::ConsoleScriptParser | definition | tracked | コンソールスクリプトパーサークラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConsoleSystem.h | 20 | class | Unnamed::ConCommandBase | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleSystem.h | 21 | class | Unnamed::ConCommand | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleSystem.h | 23 | class | Unnamed::ConVar | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleSystem.h | 33 | struct | Unnamed::ConsoleLogText | definition | tracked | コンソールログテキスト構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConsoleSystem.h | 70 | class | Unnamed::ConsoleSystem | definition | tracked | コンソールシステムクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.cpp | 26 | struct | Unnamed::<anonymous-namespace@24>::ConsoleUIData | definition | tracked | コンソールUIのデータ構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.cpp | 28 | struct | Unnamed::<anonymous-namespace@24>::ConsoleUIData::SuggestionEntry | definition | tracked | SuggestionEntryは、Suggestionを検索・生成するためのkeyと対象参照をregistry内で保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.cpp | 35 | struct | Unnamed::<anonymous-namespace@24>::ConsoleUIData::SuggestionState | definition | tracked | SuggestionStateは、console補完候補、選択index、popup表示状態を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 8 | struct | ImVec2 | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 9 | struct | ImGuiInputTextCallbackData | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 12 | struct | Unnamed::ConsoleLogText | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 13 | class | Unnamed::ConVarHelper | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 14 | class | Unnamed::ConCommand | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 15 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 33 | struct | Unnamed::InputTextWithComboItems | definition | tracked | InputTextWithComboItemsは、console入力欄と補完popupへ渡す候補列と選択状態を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 40 | class | Unnamed::ConsoleUI | definition | tracked | コンソールUIクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 60 | struct | Unnamed::ConsoleUI::SuggestionContext | definition | tracked | SuggestionContextは、入力行内の補完対象tokenの範囲、文字列、確定状態を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | 69 | struct | Unnamed::ConsoleUI::SuggestionItem | definition | tracked | SuggestionItemは、console補完候補の表示文字列、挿入文字列、command種別を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/console/concommand/ConCommand.h | 8 | class | Unnamed::ConCommand | definition | tracked | コンソールコマンドクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/concommand/ConVar.h | 13 | class | Unnamed::ConVar | definition | tracked | コンソール変数クラス | pass | out-of-scope | NOT_ADDED: supported types remain distributed across parsing, writing, dynamic casts, and type checks; Vec3 persistence is not a stable unified contract. |
| src/engine/unnamed/subsystem/console/concommand/base/ConCommandBase.h | 8 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/console/concommand/base/ConCommandBase.h | 17 | class | Unnamed::ConCommandBase | definition | tracked | コンソールコマンド/変数基底クラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/console/interface/IConsole.h | 33 | class | Unnamed::IConsole | definition | tracked | コンソールインターフェースクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/InputSystem.h | 14 | struct | std::hash | definition | tracked | InputKeyのデバイス種別とコードを連想コンテナ用ハッシュへ変換します | pass | pass | EXISTING_SPECIALIZATION: this is not an open primary template contract. |
| src/engine/unnamed/subsystem/input/InputSystem.h | 22 | class | Unnamed::ConCommand | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/input/InputSystem.h | 25 | struct | Unnamed::InputActionState | definition | tracked | 入力アクション状態構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/InputSystem.h | 32 | struct | Unnamed::InputAxisState1D | definition | tracked | 1D入力軸状態構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/InputSystem.h | 41 | struct | Unnamed::InputAxisState2D | definition | tracked | 2D入力軸状態構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/InputSystem.h | 52 | struct | Unnamed::InputBinding | definition | tracked | 入力バインディング構造体 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/InputSystem.h | 63 | class | Unnamed::InputSystem | definition | tracked | InputSystemは、入力の初期化、フレーム更新、および終了順序を統括します | pass | pass | - |
| src/engine/unnamed/subsystem/input/InputSystem.h | 257 | struct | Unnamed::InputSystem::MouseLockAnchor | definition | tracked | MouseLockAnchorは、mouse lock開始時のscreen座標とclient座標を復帰位置として保持します | pass | pass | - |
| src/engine/unnamed/subsystem/input/KeyNameTable.h | 11 | struct | Unnamed::KeyHash | definition | tracked | 入力キー構造体のハッシュ関数 | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/KeyNameTable.h | 18 | class | Unnamed::KeyNameTable | definition | tracked | キー名変換テーブルクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/device/base/BaseInputDevice.h | 13 | struct | Unnamed::InputKey | definition | tracked | 入力デバイス種別とデバイス内キーコードの組を識別します | pass | pass | - |
| src/engine/unnamed/subsystem/input/device/base/BaseInputDevice.h | 23 | class | Unnamed::BaseInputDevice | definition | tracked | 入力デバイス基底クラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h | 52 | class | Unnamed::GamepadDevice | definition | tracked | GamepadDeviceは、XInput gamepadのbutton・axis状態と時間指定rumble要求を更新します | pass | pass | - |
| src/engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h | 75 | struct | Unnamed::GamepadDevice::DirectInputPad | definition | tracked | DirectInputPadは、DirectInput gamepad device、識別子、接続状態を所有します | pass | pass | - |
| src/engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h | 80 | struct | Unnamed::GamepadDevice::RumbleEvent | definition | tracked | RumbleEventは、入力で発生した事象と配送に必要な付随値を保持します | pass | pass | - |
| src/engine/unnamed/subsystem/input/device/keyboard/KeyboardDevice.h | 15 | class | Unnamed::KeyboardDevice | definition | tracked | キーボードデバイスクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/input/device/mouse/MouseDevice.h | 29 | class | Unnamed::MouseDevice | definition | tracked | マウスデバイスクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/interface/ISubsystem.h | 6 | class | ISubsystem | definition | tracked | サブシステムのインターフェース | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/interface/ServiceLocator.h | 6 | class | ServiceLocatorMap | definition | tracked | サービスロケーターマップクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/interface/ServiceLocator.h | 21 | class | ServiceLocator | definition | tracked | サービスロケーター | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/terminal/TerminalSystem.h | 9 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/terminal/TerminalSystem.h | 12 | class | Unnamed::TerminalSystem | definition | tracked | TerminalSystemは、Windows terminalの入出力をConsoleSystemのcommand実行へ接続します | pass | pass | - |
| src/engine/unnamed/subsystem/time/FrameLimiter.h | 5 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/time/FrameLimiter.h | 8 | class | GameTime | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/subsystem/time/FrameLimiter.h | 11 | class | FrameLimiter | definition | tracked | フレームレートリミッタークラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/time/GameTime.h | 6 | class | GameTime | definition | tracked | ゲームの時間を管理するクラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/time/SystemClock.h | 8 | class | SystemClock | definition | tracked | システムクロッククラス | pass | out-of-scope | - |
| src/engine/unnamed/subsystem/time/TimeSystem.h | 11 | class | Unnamed::TimeSystem | definition | tracked | 時間管理システムクラス | pass | out-of-scope | - |
| src/engine/unnamed/time/DateTime.h | 4 | struct | DateTime | definition | tracked | ログや表示に使うローカル暦日時の各要素を保持します | pass | pass | - |
| src/engine/unnamed/ui/UIContext.h | 16 | class | Unnamed::UI::UIContext | definition | tracked | UIContextは、即時UIの入力、レイアウトスタック、操作状態、描画リストをフレーム単位で管理します | pass | pass | - |
| src/engine/unnamed/ui/UIContext.h | 77 | struct | Unnamed::UI::UIContext::UILayoutState | definition | tracked | UILayoutStateは、即時UIのcursor位置、利用可能幅、行高さをlayout stack内で保持します | pass | pass | - |
| src/engine/unnamed/ui/UIContext.h | 84 | struct | Unnamed::UI::UIContext::UIButtonAnimationState | definition | tracked | UIButtonAnimationStateは、即時buttonのhover・press補間値をwidget IDごとに保持します | pass | pass | - |
| src/engine/unnamed/ui/UIContext.h | 90 | struct | Unnamed::UI::UIContext::UICheckBoxAnimationState | definition | tracked | UICheckBoxAnimationStateは、即時checkboxのhover・check補間値をwidget IDごとに保持します | pass | pass | - |
| src/engine/unnamed/ui/UIContext.h | 96 | struct | Unnamed::UI::UIContext::UISliderAnimationState | definition | tracked | UISliderAnimationStateは、即時sliderのhover・grab補間値をwidget IDごとに保持します | pass | pass | - |
| src/engine/unnamed/ui/UIDrawCommandSprite.h | 11 | class | Unnamed::UI::UIFontAtlas | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/ui/UIDrawCommandSprite.h | 13 | struct | Unnamed::UI::UIDrawCommandSpriteStats | definition | tracked | UIDrawCommandSpriteStatsは、即時UIの診断用カウンターと集計値を保持します | pass | pass | - |
| src/engine/unnamed/ui/UIFontAtlas.cpp | 41 | struct | Unnamed::UI::<anonymous-namespace@37>::PendingGlyph | definition | tracked | PendingGlyphは、font atlasへ追加予定のcodepoint、metrics、bitmapを保持します | pass | pass | - |
| src/engine/unnamed/ui/UIFontAtlas.h | 16 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/ui/UIFontAtlas.h | 21 | struct | Unnamed::UI::UIFontAtlasKey | definition | tracked | UIフォントアトラスを一意に識別するキーです。 | pass | out-of-scope | - |
| src/engine/unnamed/ui/UIFontAtlas.h | 40 | struct | Unnamed::UI::UIGlyph | definition | tracked | 1文字分のグリフ情報です。 | pass | out-of-scope | - |
| src/engine/unnamed/ui/UIFontAtlas.h | 50 | class | Unnamed::UI::UIFontAtlas | definition | tracked | 最小UI向けフォントアトラスです（ASCII固定）。 | pass | out-of-scope | - |
| src/engine/unnamed/ui/UIFontAtlas.h | 96 | class | Unnamed::UI::UIFontAtlas::UIFontAtlasCache | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/unnamed/ui/UIFontAtlas.h | 125 | struct | Unnamed::UI::UIFontAtlasCacheDebugInfo | definition | tracked | UIFontAtlasCache のデバッグ情報です。 | pass | out-of-scope | - |
| src/engine/unnamed/ui/UIFontAtlas.h | 136 | class | Unnamed::UI::UIFontAtlasCache | definition | tracked | UIFontAtlas を設定キー単位で再利用する最小キャッシュです。 | pass | out-of-scope | - |
| src/engine/unnamed/ui/UIFontAtlas.h | 153 | struct | Unnamed::UI::UIFontAtlasCache::CacheEntry | definition | tracked | CacheEntryは、即時UIキャッシュ内の資源と最終利用情報を同じ寿命で保持します | pass | pass | - |
| src/engine/unnamed/ui/UITheme.h | 9 | struct | Unnamed::UI::UITheme | definition | tracked | UI描画の最小テーマ定義です。 | pass | out-of-scope | - |
| src/engine/unnamed/ui/UnnamedUIDrawList.h | 15 | struct | Unnamed::UI::UIDrawCommand | definition | tracked | UIDrawCommandは、即時UIで順序付き実行する命令と引数を保持します | pass | pass | - |
| src/engine/unnamed/ui/UnnamedUIDrawList.h | 31 | class | Unnamed::UI::UIDrawList | definition | tracked | UIDrawListは、即時UIが生成したスプライト描画命令を提出順に蓄積します | pass | pass | - |
| src/engine/unnamed/ui/UnnamedUIInput.h | 6 | struct | Unnamed::UI::UnnamedUiInputState | definition | tracked | UnnamedUiInputStateは、mouse位置、button遷移、wheel量を即時UIの1frame入力として保持します | pass | pass | - |
| src/engine/unnamed/ui/UnnamedUITypes.h | 6 | struct | Unnamed::UI::UIRect | definition | tracked | 即時UIの位置と寸法をピクセル単位で表します | pass | pass | - |
| src/engine/unnamed/ui/UnnamedUITypes.h | 22 | struct | Unnamed::UI::UIColor | definition | tracked | UIの色構造体 | pass | out-of-scope | - |
| src/engine/world/EditorWorld.h | 9 | struct | Unnamed::Render::SceneViewRenderMode | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/EditorWorld.h | 12 | class | Unnamed::EditorCameraComponent | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/EditorWorld.h | 13 | class | Unnamed::IGameWorldFactory | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/EditorWorld.h | 16 | class | Unnamed::EditorWorld | definition | tracked | EditorWorldは、編集scene、selection、Editor cameraを保持し、simulationを開始せずに更新します | pass | pass | - |
| src/engine/world/GameplayCueBus.h | 15 | struct | Unnamed::GameplayCuePayloadBag | definition | tracked | GameplayCue の型付き named payload コンテナです。 | pass | out-of-scope | - |
| src/engine/world/GameplayCueBus.h | 93 | struct | Unnamed::GameplayCue | definition | tracked | GameplayCueは、cue tag、送信元、payloadを1件のgameplay通知として保持します | pass | pass | - |
| src/engine/world/GameplayCueBus.h | 149 | struct | Unnamed::GameplayCueFilter | definition | tracked | GameplayCueFilterは、ワールドイベントまたは要素を通過させる条件を保持します | pass | pass | - |
| src/engine/world/GameplayCueBus.h | 155 | class | Unnamed::GameplayCueBus | definition | tracked | GameplayCueBusは、GameplayCueをフィルター条件に一致するlistenerへ同期配送します | pass | pass | - |
| src/engine/world/GameplayCueBus.h | 182 | struct | Unnamed::GameplayCueBus::Listener | definition | tracked | Listenerは、GameplayCue listener ID、filter、callbackとowner lifetime tokenを保持します | pass | pass | - |
| src/engine/world/World.cpp | 130 | struct | Unnamed::<anonymous-namespace@50>::UiCanvasRuntimeEntry | definition | tracked | UiCanvasRuntimeEntryは、UiCanvasRuntimeを検索・生成するためのkeyと対象参照をregistry内で保持します | pass | pass | - |
| src/engine/world/World.cpp | 137 | struct | Unnamed::<anonymous-namespace@50>::NewUiCanvasRuntimeEntry | definition | tracked | NewUiCanvasRuntimeEntryは、NewUiCanvasRuntimeを検索・生成するためのkeyと対象参照をregistry内で保持します | pass | pass | - |
| src/engine/world/World.h | 21 | class | Unnamed::Physics::Engine | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 24 | class | Unnamed::Scene | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 25 | class | Unnamed::AssetManager | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 26 | class | Unnamed::ConsoleSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 27 | class | Unnamed::InputSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 28 | class | Unnamed::Profiler | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 29 | class | Unnamed::AudioSystem | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 30 | class | Unnamed::IDemoService | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 31 | class | Unnamed::JsonReader | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 34 | struct | Unnamed::Render::RenderFrameContext | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 35 | struct | Unnamed::Render::RenderFrameInputs | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 36 | struct | Unnamed::Render::RenderCameraInput | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/World.h | 40 | struct | Unnamed::WorldTime | definition | tracked | ワールド固有の経過時間、デルタ時間、および時間倍率を保持します | pass | pass | - |
| src/engine/world/World.h | 51 | struct | Unnamed::WorldServices | definition | tracked | World が参照する外部サービス群です。 | pass | out-of-scope | - |
| src/engine/world/World.h | 61 | class | Unnamed::World | definition | tracked | エンティティー、物理、描画入力、およびシーン遷移のフレーム寿命を統括します | pass | pass | - |
| src/engine/world/World.h | 268 | struct | Unnamed::World::PostFxPassOverrides | definition | tracked | PostFxPassOverridesは、Worldがpost-process pass名ごとに適用する有効状態とparameter値を保持します | pass | pass | - |
| src/engine/world/WorldCameraManager.h | 8 | class | Unnamed::World | forward declaration | tracked | - | not-applicable | not-applicable | - |
| src/engine/world/WorldCameraManager.h | 11 | class | Unnamed::WorldCameraManager | definition | tracked | WorldCameraManagerは、World内cameraの優先順位と現在の描画camera解決を管理します | pass | pass | - |
| src/engine/world/WorldCameraManager.h | 14 | struct | Unnamed::WorldCameraManager::CurrentCameraInfo | definition | tracked | CurrentCameraInfoは、選択中camera entityのGUID、解決済み描画camera、有効性を直近frameのsnapshotとして保持します | pass | pass | - |
| src/engine/world/WorldDebugDraw.h | 12 | class | Unnamed::WorldDebugDraw | definition | tracked | WorldDebugDrawは、ワールド空間のデバッグ線分を寿命付きで蓄積して描画入力へ提出します | pass | pass | - |
| src/engine/world/WorldDebugDraw.h | 145 | struct | Unnamed::WorldDebugDraw::PendingLine | definition | tracked | PendingLineは、debug線分の両端、色、残り表示時間を保持します | pass | pass | - |

## Generated C++ file coverage

| File | Origin | Source | Scanned | Definitions | Forward declarations |
|---|---|---|---|---:|---:|
| projects/ParkourGame/runtime/game/core/collision/kinematic/BoxKinematicCollisionResolver.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/collision/kinematic/BoxKinematicCollisionResolver.h | first-party | tracked | yes | 2 | 0 |
| projects/ParkourGame/runtime/game/core/collision/kinematic/base/BaseKinematicCollisionResolver.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h | first-party | tracked | yes | 4 | 0 |
| projects/ParkourGame/runtime/game/core/components/AudioFxControllerComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/AudioFxControllerComponent.h | first-party | tracked | yes | 2 | 3 |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.cpp | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/core/components/CameraFxControllerComponent.h | first-party | tracked | yes | 7 | 4 |
| projects/ParkourGame/runtime/game/core/components/CameraRotatorComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/CameraRotatorComponent.h | first-party | tracked | yes | 1 | 5 |
| projects/ParkourGame/runtime/game/core/components/ViewmodelSway.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/ViewmodelSway.h | first-party | tracked | yes | 1 | 1 |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/GameMovementComponent.h | first-party | tracked | yes | 2 | 5 |
| projects/ParkourGame/runtime/game/core/components/character/base/BaseCharacterComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/base/BaseCharacterComponent.h | first-party | tracked | yes | 3 | 3 |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTransitionRouter.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTransitionRouter.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTypes.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/movement/MovementTypes.h | first-party | tracked | yes | 4 | 2 |
| projects/ParkourGame/runtime/game/core/components/character/state/GameMovementStateMachine.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/state/GameMovementStateMachine.h | first-party | tracked | yes | 2 | 1 |
| projects/ParkourGame/runtime/game/core/components/character/state/ability/CoreMovementAbilities.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/state/ability/CoreMovementAbilities.h | first-party | tracked | yes | 2 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/state/interface/IMovementAbility.h | first-party | tracked | yes | 1 | 1 |
| projects/ParkourGame/runtime/game/core/components/character/state/interface/IMovementMode.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/state/interface/IMovementMode.h | first-party | tracked | yes | 1 | 1 |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/AirMovementMode.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/AirMovementMode.h | first-party | tracked | yes | 1 | 1 |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/GroundMovementMode.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/GroundMovementMode.h | first-party | tracked | yes | 1 | 1 |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/NoclipMovementMode.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/character/state/mode/NoclipMovementMode.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/core/components/common/PatrolPointComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/common/PatrolPointComponent.h | first-party | tracked | yes | 1 | 3 |
| projects/ParkourGame/runtime/game/core/components/common/RotatorComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/common/RotatorComponent.h | first-party | tracked | yes | 1 | 3 |
| projects/ParkourGame/runtime/game/core/components/controller/PlayerCharacterController.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/controller/PlayerCharacterController.h | first-party | tracked | yes | 1 | 1 |
| projects/ParkourGame/runtime/game/core/components/controller/base/BaseCharacterController.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/controller/base/BaseCharacterController.h | first-party | tracked | yes | 1 | 2 |
| projects/ParkourGame/runtime/game/core/components/inventory/InventorySystemComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/inventory/InventorySystemComponent.h | first-party | tracked | yes | 3 | 3 |
| projects/ParkourGame/runtime/game/core/components/inventory/WorldItemComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/components/inventory/WorldItemComponent.h | first-party | tracked | yes | 1 | 2 |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.cpp | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/core/components/presentation/EventPresentationComponent.h | first-party | tracked | yes | 1 | 8 |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.cpp | first-party | tracked | yes | 5 | 0 |
| projects/ParkourGame/runtime/game/core/components/weapon/WeaponSystemComponent.h | first-party | tracked | yes | 6 | 5 |
| projects/ParkourGame/runtime/game/core/input/CharacterActionFrameInput.h | first-party | tracked | yes | 4 | 0 |
| projects/ParkourGame/runtime/game/core/item/ItemEventIds.h | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/item/ItemTypes.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/item/ItemTypes.h | first-party | tracked | yes | 3 | 2 |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationExecutor.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationExecutor.h | first-party | tracked | yes | 2 | 3 |
| projects/ParkourGame/runtime/game/core/presentation/EventPresentationTypes.h | first-party | tracked | yes | 4 | 0 |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraph.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraph.h | first-party | tracked | yes | 4 | 0 |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphCodec.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphCodec.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphUi.cpp | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphUi.h | first-party | tracked | yes | 3 | 1 |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphValidator.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/presentation/editor/EventPresentationEditorGraphValidator.h | first-party | tracked | yes | 2 | 0 |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryFormat.h | first-party | tracked | yes | 3 | 0 |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryReader.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryReader.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryWriter.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/replay/DemoBinaryWriter.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/core/replay/DemoManager.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/replay/DemoManager.h | first-party | tracked | yes | 1 | 1 |
| projects/ParkourGame/runtime/game/core/replay/DemoTypes.h | first-party | tracked | yes | 5 | 0 |
| projects/ParkourGame/runtime/game/core/replay/ReplayHash.h | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/replay/ReplayJson.h | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/replay/ReplaySerializerRegistry.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/replay/ReplaySerializerRegistry.h | first-party | tracked | yes | 2 | 2 |
| projects/ParkourGame/runtime/game/core/script/ConsoleScriptComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/core/script/ConsoleScriptComponent.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/character/GrappleMotor.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/character/ParkourMovementComponent.h | first-party | tracked | yes | 9 | 1 |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.cpp | first-party | tracked | yes | 9 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/character/ability/ParkourMovementAbilities.h | first-party | tracked | yes | 0 | 1 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseElapsedTimeFormat.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseHudProjection.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseHudProjection.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressComponent.h | first-party | tracked | yes | 4 | 3 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseProgressHudComponent.h | first-party | tracked | yes | 1 | 7 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.cpp | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/course/CourseResultFlowComponent.h | first-party | tracked | yes | 4 | 9 |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/cutscene/GameStartCutsceneComponent.h | first-party | tracked | yes | 4 | 10 |
| projects/ParkourGame/runtime/game/parkour/components/title/TitleFlowComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/title/TitleFlowComponent.h | first-party | tracked | yes | 1 | 5 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/CheckpointComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/CheckpointComponent.h | first-party | tracked | yes | 1 | 2 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/GoalComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/GoalComponent.h | first-party | tracked | yes | 1 | 2 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/JumpPadComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/JumpPadComponent.h | first-party | tracked | yes | 1 | 2 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/SpeedBoostAreaComponent.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/SpeedBoostAreaComponent.h | first-party | tracked | yes | 1 | 2 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/TriggerVolumeComponentBase.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/components/trigger/TriggerVolumeComponentBase.h | first-party | tracked | yes | 1 | 3 |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourComponentRegistration.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourComponentRegistration.h | first-party | tracked | yes | 0 | 1 |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourFlowRuntimeState.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourFlowRuntimeState.h | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourGameModule.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourGameModule.h | first-party | tracked | yes | 1 | 0 |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourGameWorld.cpp | first-party | tracked | yes | 0 | 0 |
| projects/ParkourGame/runtime/game/parkour/runtime/ParkourGameWorld.h | first-party | tracked | yes | 1 | 0 |
| src/app/GameModuleRegistry.cpp | first-party | tracked | yes | 0 | 0 |
| src/app/GameProfileLoader.h | first-party | tracked | yes | 0 | 0 |
| src/app/GameRuntimeModuleRegistration.cpp | first-party | tracked | yes | 0 | 0 |
| src/app/GameRuntimeModuleRegistration.h | first-party | tracked | yes | 0 | 1 |
| src/app/LaunchDesc.h | first-party | tracked | yes | 1 | 0 |
| src/app/LoadedGameModule.cpp | first-party | tracked | yes | 0 | 0 |
| src/app/LoadedGameModule.h | first-party | tracked | yes | 1 | 5 |
| src/app/main.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/UnnamedMacro.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/UnnamedMacro.h | first-party | tracked | yes | 0 | 0 |
| src/core/assets/AssetID.h | first-party | tracked | yes | 0 | 0 |
| src/core/assets/AssetManager.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/AssetManager.h | first-party | tracked | yes | 4 | 2 |
| src/core/assets/AssetMetaData.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/AssetType.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/AssetType.h | first-party | tracked | yes | 0 | 0 |
| src/core/assets/FileStamp.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/LoadResult.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/AssimpConversions.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/AssimpConversions.h | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/DirectXTexConversions.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/DirectXTexConversions.h | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/EditorGuiLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/EditorGuiLoader.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/EventPresentationLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/EventPresentationLoader.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/MaterialAssetLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/MaterialAssetLoader.h | first-party | tracked | yes | 1 | 1 |
| src/core/assets/loader/MaterialInstanceAssetLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/MaterialInstanceAssetLoader.h | first-party | tracked | yes | 1 | 1 |
| src/core/assets/loader/MeshAssetLoader.cpp | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/MeshAssetLoader.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/PostFxChainLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/PostFxChainLoader.h | first-party | tracked | yes | 1 | 1 |
| src/core/assets/loader/SequenceAssetLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/SequenceAssetLoader.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/SequenceFileIO.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/SequenceFileIO.h | first-party | tracked | yes | 2 | 0 |
| src/core/assets/loader/SequenceMigrator.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/SequenceMigrator.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/ShaderProgramLoader.cpp | first-party | tracked | yes | 0 | 1 |
| src/core/assets/loader/ShaderProgramLoader.h | first-party | tracked | yes | 1 | 1 |
| src/core/assets/loader/ShaderSourceLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/ShaderSourceLoader.h | first-party | tracked | yes | 1 | 1 |
| src/core/assets/loader/SoundAssetLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/SoundAssetLoader.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/TextureLoaderDirectXTex.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/TextureLoaderDirectXTex.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/UiDocumentAssetLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/loader/UiDocumentAssetLoader.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/loader/interface/IAssetLoader.h | first-party | tracked | yes | 2 | 0 |
| src/core/assets/shader/ShaderIncludeParser.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/shader/ShaderIncludeParser.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/shader/ShaderIncludeResolver.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/shader/ShaderIncludeResolver.h | first-party | tracked | yes | 1 | 1 |
| src/core/assets/shader/ShaderIncludeTypes.h | first-party | tracked | yes | 3 | 0 |
| src/core/assets/types/EditorGuiData.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/types/EventPresentationAssetData.h | first-party | tracked | yes | 5 | 0 |
| src/core/assets/types/MaterialAssetData.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/types/MaterialAssetData.h | first-party | tracked | yes | 2 | 0 |
| src/core/assets/types/MaterialInstanceAssetData.h | first-party | tracked | yes | 2 | 0 |
| src/core/assets/types/MeshAssetData.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/assets/types/MeshAssetData.h | first-party | tracked | yes | 8 | 0 |
| src/core/assets/types/PostFxChainAssetData.h | first-party | tracked | yes | 2 | 0 |
| src/core/assets/types/SequenceAssetData.h | first-party | tracked | yes | 11 | 0 |
| src/core/assets/types/SequenceAuthoringData.h | first-party | tracked | yes | 2 | 0 |
| src/core/assets/types/ShaderProgramAssetData.h | first-party | tracked | yes | 2 | 0 |
| src/core/assets/types/ShaderSourceAssetData.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/types/SoundAssetData.h | first-party | tracked | yes | 1 | 0 |
| src/core/assets/types/TextureAssetData.h | first-party | tracked | yes | 3 | 0 |
| src/core/assets/types/UiDocumentAssetData.h | first-party | tracked | yes | 1 | 0 |
| src/core/containers/RingBuffer.h | first-party | tracked | yes | 2 | 0 |
| src/core/content/ContentPathResolver.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/content/ContentPathResolver.h | first-party | tracked | yes | 3 | 0 |
| src/core/filesystem/Path.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/filesystem/Path.h | first-party | tracked | yes | 2 | 0 |
| src/core/filesystem/VirtualPath.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/filesystem/VirtualPath.h | first-party | tracked | yes | 1 | 0 |
| src/core/guidgenerator/GuidGenerator.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/guidgenerator/GuidGenerator.h | first-party | tracked | yes | 1 | 0 |
| src/core/hash/HashBuilder.h | first-party | tracked | yes | 1 | 0 |
| src/core/hash/StableHashBuilder.h | first-party | tracked | yes | 1 | 0 |
| src/core/io/binary/BinaryReader.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/io/binary/BinaryReader.h | first-party | tracked | yes | 1 | 0 |
| src/core/io/binary/BinaryWriter.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/io/binary/BinaryWriter.h | first-party | tracked | yes | 1 | 0 |
| src/core/io/ini/IniParser.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/io/ini/IniParser.h | first-party | tracked | yes | 1 | 0 |
| src/core/io/json/JsonReader.h | first-party | tracked | yes | 1 | 0 |
| src/core/io/json/JsonWriter.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/io/json/JsonWriter.h | first-party | tracked | yes | 1 | 2 |
| src/core/math/Mat4.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/math/Mat4.h | first-party | tracked | yes | 1 | 3 |
| src/core/math/Math.h | first-party | tracked | yes | 0 | 0 |
| src/core/math/MathLib.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/math/Quaternion.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/math/Quaternion.h | first-party | tracked | yes | 1 | 1 |
| src/core/math/Vec2.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/math/Vec2.h | first-party | tracked | yes | 1 | 0 |
| src/core/math/Vec3.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/math/Vec3.h | first-party | tracked | yes | 1 | 2 |
| src/core/math/Vec4.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/math/Vec4.h | first-party | tracked | yes | 1 | 1 |
| src/core/math/random/Random.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/math/random/Random.h | first-party | tracked | yes | 1 | 1 |
| src/core/memory/MemUtil.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/memory/MemUtil.h | first-party | tracked | yes | 1 | 0 |
| src/core/string/StrUtil.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/string/StrUtil.h | first-party | tracked | yes | 1 | 1 |
| src/core/string/TextEncoding.cpp | first-party | tracked | yes | 0 | 0 |
| src/core/string/TextEncoding.h | first-party | tracked | yes | 1 | 0 |
| src/engine/Animation/Animation.h | first-party | tracked | yes | 1 | 0 |
| src/engine/Animation/KeyFrame.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/Animation/KeyFrame.h | first-party | tracked | yes | 1 | 0 |
| src/engine/Animation/Node.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/Animation/Node.h | first-party | tracked | yes | 4 | 1 |
| src/engine/ComponentRegistry.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/ComponentRegistry.h | first-party | tracked | yes | 4 | 0 |
| src/engine/Engine.cpp | first-party | tracked | yes | 0 | 2 |
| src/engine/Engine.h | first-party | tracked | yes | 3 | 24 |
| src/engine/EngineComponentCatalog.h | first-party | tracked | yes | 0 | 0 |
| src/engine/EngineComponentRegistration.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/EngineComponentRegistration.h | first-party | tracked | yes | 0 | 1 |
| src/engine/EngineConfig.h | first-party | tracked | yes | 2 | 0 |
| src/engine/EngineServices.h | first-party | tracked | yes | 1 | 6 |
| src/engine/IWin32MsgListener.h | first-party | tracked | yes | 1 | 0 |
| src/engine/ImGui/Icons.h | first-party | tracked | yes | 0 | 0 |
| src/engine/ImGui/ImGuiUtil.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/ImGui/ImGuiUtil.h | first-party | tracked | yes | 0 | 2 |
| src/engine/ImGui/ImGuiWidgets.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/ImGui/ImGuiWidgets.h | first-party | tracked | yes | 0 | 1 |
| src/engine/Properties.h | first-party | tracked | yes | 0 | 0 |
| src/engine/content/AssetReferenceValidationPolicy.h | first-party | tracked | yes | 0 | 0 |
| src/engine/content/ContentMountDefinitions.h | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/ContentBrowser.cpp | first-party | tracked | yes | 1 | 0 |
| src/engine/editor/ContentBrowser.h | first-party | tracked | yes | 2 | 0 |
| src/engine/editor/EditorGuiScriptPanel.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/EditorGuiScriptPanel.h | first-party | tracked | yes | 1 | 0 |
| src/engine/editor/EditorNotification.cpp | first-party | tracked | yes | 2 | 0 |
| src/engine/editor/EditorNotification.h | first-party | tracked | yes | 2 | 2 |
| src/engine/editor/EditorProperties.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/EditorProperties.h | first-party | tracked | yes | 1 | 0 |
| src/engine/editor/EditorRuntime.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/EditorRuntime.h | first-party | tracked | yes | 1 | 10 |
| src/engine/editor/EditorToolHost.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/EditorToolHost.h | first-party | tracked | yes | 1 | 15 |
| src/engine/editor/EditorUiMetrics.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/EditorUiMetrics.h | first-party | tracked | yes | 1 | 0 |
| src/engine/editor/EditorViewportCameraManager.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/EditorViewportCameraManager.h | first-party | tracked | yes | 3 | 1 |
| src/engine/editor/GuiEditorTool.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/GuiEditorTool.h | first-party | tracked | yes | 2 | 2 |
| src/engine/editor/IEditorTool.h | first-party | tracked | yes | 3 | 12 |
| src/engine/editor/ImGuizmoConfigLoader.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/ImGuizmoConfigLoader.h | first-party | tracked | yes | 1 | 1 |
| src/engine/editor/LevelEditorTool.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/LevelEditorTool.h | first-party | tracked | yes | 2 | 9 |
| src/engine/editor/LevelEditorToolChrome.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/LevelEditorToolHierarchy.cpp | first-party | tracked | yes | 2 | 0 |
| src/engine/editor/LevelEditorToolProfiler.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/LevelEditorToolViewport.cpp | first-party | tracked | yes | 1 | 0 |
| src/engine/editor/lua/EditorLuaSystem.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/lua/EditorLuaSystem.h | first-party | tracked | yes | 1 | 0 |
| src/engine/editor/sequence/SequenceEditorController.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/sequence/SequenceEditorController.h | first-party | tracked | yes | 1 | 3 |
| src/engine/editor/sequence/SequenceEditorDocument.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/editor/sequence/SequenceEditorDocument.h | first-party | tracked | yes | 1 | 1 |
| src/engine/editor/sequence/SequenceEditorTypes.h | first-party | tracked | yes | 1 | 0 |
| src/engine/game/GameModulePaths.h | first-party | tracked | yes | 1 | 0 |
| src/engine/game/GameModuleRegistry.h | first-party | tracked | yes | 1 | 1 |
| src/engine/game/GamePathResolver.cpp | first-party | tracked | yes | 1 | 0 |
| src/engine/game/GamePathResolver.h | first-party | tracked | yes | 1 | 0 |
| src/engine/game/GameRuntimeContext.h | first-party | tracked | yes | 1 | 0 |
| src/engine/game/IDemoService.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/game/IDemoService.h | first-party | tracked | yes | 1 | 3 |
| src/engine/game/IGameModule.h | first-party | tracked | yes | 2 | 6 |
| src/engine/game/IGameWorldFactory.h | first-party | tracked | yes | 1 | 2 |
| src/engine/gui/Rect.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/Rect.h | first-party | tracked | yes | 4 | 0 |
| src/engine/gui/UiButton.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiButton.h | first-party | tracked | yes | 1 | 1 |
| src/engine/gui/UiCanvasRuntime.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiCanvasRuntime.h | first-party | tracked | yes | 0 | 5 |
| src/engine/gui/UiDeserializeContext.h | first-party | tracked | yes | 1 | 1 |
| src/engine/gui/UiDocument.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiDocument.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/UiDocumentManager.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiDocumentManager.h | first-party | tracked | yes | 2 | 2 |
| src/engine/gui/UiDrawCommand.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiDrawCommand.h | first-party | tracked | yes | 5 | 0 |
| src/engine/gui/UiPanel.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiPanel.h | first-party | tracked | yes | 1 | 1 |
| src/engine/gui/UiRoot.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiRoot.h | first-party | tracked | yes | 1 | 1 |
| src/engine/gui/UiScreen.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiScreen.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/UiScreenStack.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiScreenStack.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/UiSerializationHelpers.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiSerializationHelpers.h | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiTextureReference.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiTextureReference.h | first-party | tracked | yes | 1 | 2 |
| src/engine/gui/UiWidget.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/UiWidget.h | first-party | tracked | yes | 3 | 0 |
| src/engine/gui/components/UiButtonBehaviorComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/components/UiButtonBehaviorComponent.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/components/UiComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/components/UiComponent.h | first-party | tracked | yes | 1 | 6 |
| src/engine/gui/components/UiDigitStripComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/components/UiDigitStripComponent.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/components/UiLayoutComponents.cpp | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/components/UiLayoutComponents.h | first-party | tracked | yes | 4 | 0 |
| src/engine/gui/components/UiPanelStyleComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/components/UiPanelStyleComponent.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/components/UiTextureComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/components/UiTextureComponent.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/components/UiTransformComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/components/UiTransformComponent.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/editor/GuiEditor.cpp | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/editor/GuiEditor.h | first-party | tracked | yes | 1 | 5 |
| src/engine/gui/layout/UiHorizontalLayout.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/layout/UiHorizontalLayout.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/layout/UiVerticalLayout.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/layout/UiVerticalLayout.h | first-party | tracked | yes | 1 | 0 |
| src/engine/gui/layout/base/UiLayout.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/gui/layout/base/UiLayout.h | first-party | tracked | yes | 1 | 1 |
| src/engine/physics/core/Physics.cpp | first-party | tracked | yes | 1 | 0 |
| src/engine/physics/core/Physics.h | first-party | tracked | yes | 4 | 0 |
| src/engine/platform/IPlatformEvents.h | first-party | tracked | yes | 1 | 1 |
| src/engine/platform/PlatformEventsImpl.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/platform/PlatformEventsImpl.h | first-party | tracked | yes | 1 | 0 |
| src/engine/platform/Window.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/platform/Window.h | first-party | tracked | yes | 4 | 0 |
| src/engine/platform/WindowManager.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/platform/WindowManager.h | first-party | tracked | yes | 1 | 0 |
| src/engine/platform/WindowsUtils.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/platform/WindowsUtils.h | first-party | tracked | yes | 3 | 0 |
| src/engine/profiler/Profiler.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/profiler/Profiler.h | first-party | tracked | yes | 4 | 0 |
| src/engine/render/RenderDevice.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/RenderDevice.h | first-party | tracked | yes | 1 | 2 |
| src/engine/render/RenderHandles.h | first-party | tracked | yes | 0 | 0 |
| src/engine/render/RenderModule.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/RenderModule.h | first-party | tracked | yes | 1 | 4 |
| src/engine/render/RenderStartupOptions.h | first-party | tracked | yes | 1 | 0 |
| src/engine/render/Renderer.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/Renderer.h | first-party | tracked | yes | 15 | 6 |
| src/engine/render/RendererGeometry.cpp | first-party | tracked | yes | 2 | 0 |
| src/engine/render/RendererGraph.cpp | first-party | tracked | yes | 4 | 0 |
| src/engine/render/RendererInit.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/RendererPipelineCatalog.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/RendererPipelineCatalog.h | first-party | tracked | yes | 1 | 0 |
| src/engine/render/TextureResourceCache.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/TextureResourceCache.h | first-party | tracked | yes | 3 | 2 |
| src/engine/render/foundation/AdvancedRenderFoundation.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/foundation/AdvancedRenderFoundation.h | first-party | tracked | yes | 6 | 1 |
| src/engine/render/frame/RenderFrameContext.h | first-party | tracked | yes | 2 | 0 |
| src/engine/render/frame/RenderFrameInputs.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/frame/RenderFrameInputs.h | first-party | tracked | yes | 17 | 0 |
| src/engine/render/rendergraph/IDescriptorResolver.h | first-party | tracked | yes | 1 | 0 |
| src/engine/render/rendergraph/RegistryDescriptorResolver.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/rendergraph/RegistryDescriptorResolver.h | first-party | tracked | yes | 1 | 1 |
| src/engine/render/rendergraph/RenderGraph.cpp | first-party | tracked | yes | 2 | 0 |
| src/engine/render/rendergraph/RenderGraph.h | first-party | tracked | yes | 4 | 5 |
| src/engine/render/rendergraph/RenderGraphBuilder.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/rendergraph/RenderGraphBuilder.h | first-party | tracked | yes | 5 | 2 |
| src/engine/render/rendergraph/RenderPassContext.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/rendergraph/RenderPassContext.h | first-party | tracked | yes | 1 | 3 |
| src/engine/render/rendergraph/RgResourceRegistry.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/rendergraph/RgResourceRegistry.h | first-party | tracked | yes | 6 | 2 |
| src/engine/render/shaders/MountAwareDxcIncludeHandler.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/shaders/MountAwareDxcIncludeHandler.h | first-party | tracked | yes | 1 | 0 |
| src/engine/render/shaders/PipelineCache.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/shaders/PipelineCache.h | first-party | tracked | yes | 7 | 1 |
| src/engine/render/shaders/PipelineRegistry.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/shaders/PipelineRegistry.h | first-party | tracked | yes | 1 | 1 |
| src/engine/render/shaders/RootSignatureSlots.h | first-party | tracked | yes | 0 | 0 |
| src/engine/render/shaders/ShaderCompileUnit.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/shaders/ShaderCompileUnit.h | first-party | tracked | yes | 3 | 1 |
| src/engine/render/shaders/ShaderKey.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/shaders/ShaderKey.h | first-party | tracked | yes | 2 | 0 |
| src/engine/render/shaders/ShaderLibrary.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/render/shaders/ShaderLibrary.h | first-party | tracked | yes | 3 | 2 |
| src/engine/rhi/Buffer.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/Buffer.h | first-party | tracked | yes | 2 | 0 |
| src/engine/rhi/Constants.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/Constants.h | first-party | tracked | yes | 6 | 0 |
| src/engine/rhi/DxcShaderCompiler.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/DxcShaderCompiler.h | first-party | tracked | yes | 1 | 0 |
| src/engine/rhi/PipelineKey.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/PipelineKey.h | first-party | tracked | yes | 2 | 0 |
| src/engine/rhi/RhiTypes.h | first-party | tracked | yes | 3 | 0 |
| src/engine/rhi/UploadBuffer.h | first-party | tracked | yes | 1 | 0 |
| src/engine/rhi/d3d12/D3D12CommandContext.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/d3d12/D3D12CommandContext.h | first-party | tracked | yes | 1 | 0 |
| src/engine/rhi/d3d12/D3D12Device.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/d3d12/D3D12Device.h | first-party | tracked | yes | 3 | 4 |
| src/engine/rhi/d3d12/D3D12FrameUploadAllocator.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/d3d12/D3D12FrameUploadAllocator.h | first-party | tracked | yes | 1 | 0 |
| src/engine/rhi/d3d12/D3D12SwapChain.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/d3d12/D3D12SwapChain.h | first-party | tracked | yes | 1 | 0 |
| src/engine/rhi/d3d12/D3D12Util.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/d3d12/D3D12Util.h | first-party | tracked | yes | 0 | 0 |
| src/engine/rhi/interface/IRhiCommandContext.h | first-party | tracked | yes | 1 | 0 |
| src/engine/rhi/interface/IRhiDevice.h | first-party | tracked | yes | 1 | 0 |
| src/engine/rhi/interface/IRhiSwapChain.h | first-party | tracked | yes | 1 | 0 |
| src/engine/scene/Scene.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/scene/Scene.h | first-party | tracked | yes | 2 | 5 |
| src/engine/scene/SceneFolderPath.h | first-party | tracked | yes | 0 | 0 |
| src/engine/scene/SceneLoadOptions.h | first-party | tracked | yes | 2 | 1 |
| src/engine/scene/SceneSerializer.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/scene/SceneSerializer.h | first-party | tracked | yes | 1 | 3 |
| src/engine/sequence/CompiledSequence.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/sequence/CompiledSequence.h | first-party | tracked | yes | 3 | 0 |
| src/engine/sequence/PreAnimatedStateStore.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/sequence/PreAnimatedStateStore.h | first-party | tracked | yes | 3 | 0 |
| src/engine/sequence/SequencePlayer.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/sequence/SequencePlayer.h | first-party | tracked | yes | 2 | 0 |
| src/engine/sequence/SequencePropertyAccessorRegistry.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/sequence/SequencePropertyAccessorRegistry.h | first-party | tracked | yes | 4 | 1 |
| src/engine/sequence/SequenceRuntime.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/sequence/SequenceRuntime.h | first-party | tracked | yes | 12 | 3 |
| src/engine/sequence/SequenceRuntimeTypes.h | first-party | tracked | yes | 1 | 0 |
| src/engine/tween/ITweenPlayable.h | first-party | tracked | yes | 1 | 0 |
| src/engine/tween/TweenEase.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/tween/TweenEase.h | first-party | tracked | yes | 1 | 0 |
| src/engine/tween/TweenHandle.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/tween/TweenHandle.h | first-party | tracked | yes | 1 | 0 |
| src/engine/tween/TweenInstance.h | first-party | tracked | yes | 1 | 0 |
| src/engine/tween/TweenLerp.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/tween/TweenLerp.h | first-party | tracked | yes | 6 | 5 |
| src/engine/tween/TweenManager.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/tween/TweenManager.h | first-party | tracked | yes | 1 | 0 |
| src/engine/tween/TweenTypes.h | first-party | tracked | yes | 0 | 0 |
| src/engine/ui/ImGuiLayer.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/ui/ImGuiLayer.h | first-party | tracked | yes | 3 | 3 |
| src/engine/ui/retained/IUiBackend.h | first-party | tracked | yes | 4 | 0 |
| src/engine/ui/retained/UiAnimatedValue.h | first-party | tracked | yes | 1 | 0 |
| src/engine/ui/retained/UiDocument.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/ui/retained/UiDocument.h | first-party | tracked | yes | 1 | 1 |
| src/engine/ui/retained/UiNode.h | first-party | tracked | yes | 6 | 0 |
| src/engine/ui/retained/UiSystem.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/ui/retained/UiSystem.h | first-party | tracked | yes | 3 | 0 |
| src/engine/ui/retained/UiTypes.h | first-party | tracked | yes | 5 | 0 |
| src/engine/unnamed/framework/components/CameraComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/CameraComponent.h | first-party | tracked | yes | 1 | 3 |
| src/engine/unnamed/framework/components/DirectionalLightComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/DirectionalLightComponent.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/framework/components/SkyLightComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/SkyLightComponent.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/framework/components/SkyboxComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/SkyboxComponent.h | first-party | tracked | yes | 1 | 3 |
| src/engine/unnamed/framework/components/TransformComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/TransformComponent.h | first-party | tracked | yes | 1 | 1 |
| src/engine/unnamed/framework/components/audio/AudioSourceComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/audio/AudioSourceComponent.h | first-party | tracked | yes | 1 | 5 |
| src/engine/unnamed/framework/components/base/BaseComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/base/BaseComponent.h | first-party | tracked | yes | 1 | 11 |
| src/engine/unnamed/framework/components/collider/StaticMeshColliderComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/collider/StaticMeshColliderComponent.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/framework/components/editor/EditorCameraComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/editor/EditorCameraComponent.h | first-party | tracked | yes | 1 | 3 |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.cpp | first-party | tracked | yes | 3 | 0 |
| src/engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h | first-party | tracked | yes | 5 | 3 |
| src/engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h | first-party | tracked | yes | 2 | 3 |
| src/engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h | first-party | tracked | yes | 2 | 3 |
| src/engine/unnamed/framework/components/sequence/SequenceDirectorComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/sequence/SequenceDirectorComponent.h | first-party | tracked | yes | 3 | 3 |
| src/engine/unnamed/framework/components/ui/NewUICanvas.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/ui/NewUICanvas.h | first-party | tracked | yes | 1 | 1 |
| src/engine/unnamed/framework/components/ui/UiCanvasComponent.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/components/ui/UiCanvasComponent.h | first-party | tracked | yes | 1 | 2 |
| src/engine/unnamed/framework/entity/Entity.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/framework/entity/Entity.h | first-party | tracked | yes | 1 | 2 |
| src/engine/unnamed/physics/BVH.h | first-party | tracked | yes | 2 | 0 |
| src/engine/unnamed/physics/BVHBuilder.cpp | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/physics/BVHBuilder.h | first-party | tracked | yes | 2 | 0 |
| src/engine/unnamed/physics/BoxCast.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/physics/BoxCast.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/physics/CollisionDetection.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/physics/CollisionDetection.h | first-party | tracked | yes | 0 | 5 |
| src/engine/unnamed/physics/PhysicsTypes.h | first-party | tracked | yes | 2 | 0 |
| src/engine/unnamed/physics/RayCast.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/physics/RayCast.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/physics/ShapeCast.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/physics/SphereCast.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/physics/SphereCast.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/primitive/Primitives.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/primitive/Primitives.h | first-party | tracked | yes | 9 | 0 |
| src/engine/unnamed/subsystem/audio/Audio.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/audio/Audio.h | first-party | tracked | yes | 1 | 1 |
| src/engine/unnamed/subsystem/audio/AudioSystem.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/audio/AudioSystem.h | first-party | tracked | yes | 1 | 4 |
| src/engine/unnamed/subsystem/console/ConVarHelper.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/ConVarHelper.h | first-party | tracked | yes | 8 | 1 |
| src/engine/unnamed/subsystem/console/ConVarWriter.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/ConVarWriter.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/console/ConsoleFileLogSink.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/ConsoleFileLogSink.h | first-party | tracked | yes | 3 | 0 |
| src/engine/unnamed/subsystem/console/ConsoleFlags.h | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/ConsoleScriptParser.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/ConsoleScriptParser.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/console/ConsoleSystem.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/ConsoleSystem.h | first-party | tracked | yes | 2 | 3 |
| src/engine/unnamed/subsystem/console/ConsoleUI.cpp | first-party | tracked | yes | 3 | 0 |
| src/engine/unnamed/subsystem/console/ConsoleUI.h | first-party | tracked | yes | 4 | 6 |
| src/engine/unnamed/subsystem/console/Log.h | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/builtin/BuiltInCommands.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/builtin/BuiltInCommands.h | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/builtin/BuiltInConVars.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/builtin/BuiltInConVars.h | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/concommand/ConCommand.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/concommand/ConCommand.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/console/concommand/ConVar.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/concommand/ConVar.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/console/concommand/base/ConCommandBase.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/concommand/base/ConCommandBase.h | first-party | tracked | yes | 1 | 1 |
| src/engine/unnamed/subsystem/console/interface/IConsole.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/console/interface/IConsole.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/input/InputSystem.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/input/InputSystem.h | first-party | tracked | yes | 7 | 1 |
| src/engine/unnamed/subsystem/input/KeyNameTable.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/input/KeyNameTable.h | first-party | tracked | yes | 2 | 0 |
| src/engine/unnamed/subsystem/input/device/base/BaseInputDevice.h | first-party | tracked | yes | 2 | 0 |
| src/engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h | first-party | tracked | yes | 3 | 0 |
| src/engine/unnamed/subsystem/input/device/keyboard/KeyboardDevice.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/input/device/keyboard/KeyboardDevice.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/input/device/mouse/MouseDevice.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/input/device/mouse/MouseDevice.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/interface/ISubsystem.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/interface/ServiceLocator.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/interface/ServiceLocator.h | first-party | tracked | yes | 2 | 0 |
| src/engine/unnamed/subsystem/terminal/TerminalSystem.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/terminal/TerminalSystem.h | first-party | tracked | yes | 1 | 1 |
| src/engine/unnamed/subsystem/time/FrameLimiter.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/time/FrameLimiter.h | first-party | tracked | yes | 1 | 2 |
| src/engine/unnamed/subsystem/time/GameTime.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/time/GameTime.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/time/SystemClock.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/time/SystemClock.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/subsystem/time/TimeSystem.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/subsystem/time/TimeSystem.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/time/DateTime.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/ui/UIContext.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/ui/UIContext.h | first-party | tracked | yes | 5 | 0 |
| src/engine/unnamed/ui/UIDrawCommandSprite.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/ui/UIDrawCommandSprite.h | first-party | tracked | yes | 1 | 1 |
| src/engine/unnamed/ui/UIFontAtlas.cpp | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/ui/UIFontAtlas.h | first-party | tracked | yes | 6 | 2 |
| src/engine/unnamed/ui/UITheme.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/ui/UnnamedUIDrawList.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/ui/UnnamedUIDrawList.h | first-party | tracked | yes | 2 | 0 |
| src/engine/unnamed/ui/UnnamedUIInput.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/ui/UnnamedUIInput.h | first-party | tracked | yes | 1 | 0 |
| src/engine/unnamed/ui/UnnamedUITypes.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/unnamed/ui/UnnamedUITypes.h | first-party | tracked | yes | 2 | 0 |
| src/engine/world/EditorWorld.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/world/EditorWorld.h | first-party | tracked | yes | 1 | 3 |
| src/engine/world/GameplayCueBus.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/world/GameplayCueBus.h | first-party | tracked | yes | 5 | 0 |
| src/engine/world/World.cpp | first-party | tracked | yes | 2 | 0 |
| src/engine/world/World.h | first-party | tracked | yes | 4 | 12 |
| src/engine/world/WorldCameraManager.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/world/WorldCameraManager.h | first-party | tracked | yes | 2 | 1 |
| src/engine/world/WorldDebugDraw.cpp | first-party | tracked | yes | 0 | 0 |
| src/engine/world/WorldDebugDraw.h | first-party | tracked | yes | 2 | 0 |
| src/pch.cpp | first-party | tracked | yes | 0 | 0 |
| src/pch.h | first-party | tracked | yes | 0 | 0 |
| src/tests/AssetPathContractTests.cpp | first-party | tracked | yes | 0 | 0 |
| src/thirdparty/DirectXTex/BC.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/BC.h | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/BC4BC5.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/BC6HBC7.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/BCDirectCompute.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/BCDirectCompute.h | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DDS.h | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTex.h | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTex.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexCompress.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexCompressGPU.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexConvert.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexD3D11.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexD3D12.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexDDS.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexFlipRotate.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexHDR.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexImage.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexMipmaps.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexMisc.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexNormalMaps.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexP.h | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexPMAlpha.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexResize.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexTGA.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexUtil.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/DirectXTexWIC.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/d3dx12.h | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/filters.h | external | tracked | no | 0 | 0 |
| src/thirdparty/DirectXTex/scoped.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imconfig.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_demo.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_draw.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_impl_dx12.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_impl_dx12.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_impl_win32.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_impl_win32.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_internal.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_tables.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imgui_widgets.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imstb_rectpack.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imstb_textedit.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGui/imstb_truetype.h | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGuizmo/ImGuizmo.cpp | external | tracked | no | 0 | 0 |
| src/thirdparty/ImGuizmo/ImGuizmo.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/AssertHandler.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Base64.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/BaseImporter.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Bitmap.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/BlobIOSystem.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/ByteSwapper.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/ColladaMetaData.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Compiler/poppack1.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Compiler/pstdint.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Compiler/pushpack1.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/CreateAnimMesh.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/DefaultIOStream.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/DefaultIOSystem.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/DefaultLogger.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Exceptional.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Exporter.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/GenericProperty.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/GltfMaterial.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Hash.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/IOStream.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/IOStreamBuffer.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/IOSystem.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Importer.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/LineSplitter.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/LogAux.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/LogStream.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Logger.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/MathFunctions.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/MemoryIOWrapper.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/NullLogger.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/ObjMaterial.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/ParsingUtils.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Profiler.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/ProgressHandler.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/RemoveComments.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/SGSpatialSort.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/SceneCombiner.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/SkeletonMeshBuilder.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/SmallVector.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/SmoothingGroups.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/SmoothingGroups.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/SpatialSort.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/StandardShapes.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/StreamReader.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/StreamWriter.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/StringComparison.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/StringUtils.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Subdivision.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/TinyFormatter.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/Vertex.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/XMLTools.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/XmlParser.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/ZipArchiveIOSystem.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/aabb.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/ai_assert.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/anim.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/camera.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/cexport.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/cfileio.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/cimport.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/color4.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/color4.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/commonMetaData.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/config.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/defs.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/fast_atof.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/importerdesc.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/light.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/material.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/material.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/matrix3x3.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/matrix3x3.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/matrix4x4.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/matrix4x4.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/mesh.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/metadata.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/pbrmaterial.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/port/AndroidJNI/AndroidJNIIOSystem.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/port/AndroidJNI/BundledAssetIOSystem.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/postprocess.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/qnan.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/quaternion.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/quaternion.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/scene.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/texture.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/types.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/vector2.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/vector2.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/vector3.h | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/vector3.inl | external | tracked | no | 0 | 0 |
| src/thirdparty/assimp/include/assimp/version.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lapi.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lauxlib.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lcode.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lctype.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/ldebug.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/ldo.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lfunc.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lgc.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/ljumptab.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/llex.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/llimits.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lmem.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lobject.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lopcodes.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lopnames.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lparser.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lprefix.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lstate.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lstring.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/ltable.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/ltm.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lua.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lua.hpp | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/luaconf.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lualib.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lundump.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lvm.h | external | tracked | no | 0 | 0 |
| src/thirdparty/lua/lzio.h | external | tracked | no | 0 | 0 |
| src/thirdparty/nlohmann/json.hpp | external | tracked | no | 0 | 0 |

<!-- END GENERATED REPOSITORY LEDGER -->
