#pragma once
#include <cstdint>

//-----------------------------------------------------------------------------
// Material Icons: https://fonts.google.com/icons
//-----------------------------------------------------------------------------

// ---- System / Window -------------------------------------------------------
constexpr uint32_t kIconPower    = 0xE8AC; // 電源
constexpr uint32_t kIconSettings = 0xE8B8; // 設定
constexpr uint32_t kIconReset    = 0xE166; // リセット
constexpr uint32_t kIconClose    = 0xE5CD; // 閉じる

// ---- Navigation / App ------------------------------------------------------
constexpr uint32_t kIconArrowBack = 0xE5C4; // 戻る/ロゴ(仮)
constexpr uint32_t kIconAdd       = 0xE145; // 追加
constexpr uint32_t kIconMoreHoriz = 0xE5D3; // その他（水平）
constexpr uint32_t kIconRefresh   = 0xE5D5; // 更新/リフレッシュ

// ---- Help / Info -----------------------------------------------------------
constexpr uint32_t kIconInfo         = 0xE88E; // 情報
constexpr uint32_t kIconWarning      = 0xE002; // 警告
constexpr uint32_t kIconError        = 0xE000; // エラー
constexpr uint32_t kIconBomb         = 0xF568; // 爆弾 (致命的なエラー用)
constexpr uint32_t kIconQuestionMark = 0xEB8b; // クエスチョンマーク
constexpr uint32_t kIconHelp         = 0xE887; // ヘルプ
constexpr uint32_t kIconAvgTime      = 0xF813; // 平均時間 (プロファイラー用)

// ---- Console ---------------------------------------------------------------
constexpr uint32_t kIconTerminal = 0xEB8E; // ターミナル

// ---- Labels / UI elements --------------------------------------------------
constexpr uint32_t kIconLabel     = 0xE892; // ラベル
constexpr uint32_t kIconRectangle = 0xEB54; // 四角形

// ---- File / IO -------------------------------------------------------------
constexpr uint32_t kIconSave   = 0xE161; // セーブ
constexpr uint32_t kIconSaveAs = 0xEB60; // 名前を付けて保存

constexpr uint32_t kIconCopy     = 0xE14D; // コピー
constexpr uint32_t kIconDownload = 0xF090; // インポート
constexpr uint32_t kIconUpload   = 0xF09B; // エクスポート

// ---- Visibility ------------------------------------------------------------
constexpr uint32_t kIconVisibility    = 0xE8F4; // On
constexpr uint32_t kIconVisibilityOff = 0xE8F5; // Off

// ---- CheckBox --------------------------------------------------------------
constexpr uint32_t kIconCheckBoxOn  = 0xE834; // チェックボックスオン
constexpr uint32_t kIconCheckBoxOff = 0xE835; // チェックボックスオフ

// ---- Playback / Time -------------------------------------------------------
constexpr uint32_t kIconArrowBack2   = 0xF43A; // arrow_back_2
constexpr uint32_t kIconPlay         = 0xE037; // プレイ
constexpr uint32_t kIconPause        = 0xE034; // ポーズ
constexpr uint32_t kIconStop         = 0xE047; // ストップ
constexpr uint32_t kIconSkipPrevious = 0xE045; // skip_previous
constexpr uint32_t kIconSkipNext     = 0xE044; // skip_next
constexpr uint32_t kIconTimer        = 0xE425; // タイマー
constexpr uint32_t kIconSpeed        = 0xE9E4; // スピード

// ---- Content Browser -------------------------------------------------------
constexpr uint32_t kIconFolder      = 0xE2C7; // フォルダ
constexpr uint32_t kIconArrowUpward = 0xE5D8; // 上向き矢印 (親フォルダに移動)

// ---- Scene / Data types ----------------------------------------------------
constexpr uint32_t kIconEntity           = 0xEA24; // エンティティ
constexpr uint32_t kIconObject           = 0xE3B4; // オブジェクト
constexpr uint32_t kIconGroup            = 0xE574; // グループ
constexpr uint32_t kIconMesh             = 0xE3EC; // メッシュ
constexpr uint32_t kIconVertex           = 0xEBC7; // 頂点
constexpr uint32_t kIconEdge             = 0xE922; // エッジ
constexpr uint32_t kIconFace             = 0xE86B; // 面
constexpr uint32_t kIconAudioFile        = 0xEB82; // オーディオファイル
constexpr uint32_t kIconArticle          = 0xEF42; // 記事 (生ファイルに使用)
constexpr uint32_t kIconJson             = 0xF3BB; // JSONファイル
constexpr uint32_t kIconDesktopLandscape = 0xF45E; // UIファイルに使用

// ---- Gizmo / Transform -----------------------------------------------------
constexpr uint32_t kIconSelect  = 0xF82F; // 選択
constexpr uint32_t kIconDragPan = 0xF71E; // 移動
constexpr uint32_t kIconRotate  = 0xE042; // 回転
constexpr uint32_t kIconScale   = 0xF707; // 拡縮
constexpr uint32_t kIconPivot   = 0xE147; // 原点

// ---- Primitives / Assets ---------------------------------------------------
constexpr uint32_t kIconBox          = 0xF720; // ボックス
constexpr uint32_t kIconTexture      = 0xE421; // テクスチャ
constexpr uint32_t kIconDeployedCode = 0xF720; // 3Dモデル(仮)のアイコンに使用
constexpr uint32_t kIconCode         = 0xE86F; // コード
constexpr uint32_t kIconEvShadow     = 0xEF8F; // シャドウ(マテリアルのアイコンに使用)

// ---- UI helpers ------------------------------------------------------------
constexpr uint32_t kIconFilter   = 0xEF4F; // フィルター
constexpr uint32_t kIconDropDown = 0xE5C5; // ドロップダウン

// ---- Misc ------------------------------------------------------------------
constexpr uint32_t kIconDirectionsWalk     = 0xE536; // 歩く
constexpr uint32_t kIconDirectionsRun      = 0xE566; // 走る
constexpr uint32_t kIconSprint             = 0xF81F; // 全力疾走
constexpr uint32_t kIconJoystick           = 0xF5EE; // ジョイスティック
constexpr uint32_t kIcon3DRotation         = 0xE84D; // 3D回転
constexpr uint32_t kIconSiren              = 0xF3A7; // サイレン
constexpr uint32_t kIconPanoramaHorizontal = 0xE40D; // パノラマ水平
constexpr uint32_t kIconSpeaker            = 0xE32D; // スピーカー
constexpr uint32_t kIconExplosion          = 0xF685; // 爆発
constexpr uint32_t kIconAccessibility      = 0xE84E; // アクセシビリティ
constexpr uint32_t kIconEmojiPeople        = 0xEA1D; // 人の絵文字
constexpr uint32_t kIconMonitor            = 0xEF5B; // モニター
constexpr uint32_t kIconVideoCam           = 0xE04B; // ビデオカメラ
