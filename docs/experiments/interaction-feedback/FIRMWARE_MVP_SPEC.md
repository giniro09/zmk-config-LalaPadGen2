# FIRMWARE_MVP_SPEC.md

## 目的
LaLaPad Gen2 向けに、以下を追加したファームウェア MVP を作る。

- DRV2605L + LRA によるハプティックフィードバック
- FSR による圧力入力
- Tap と Force Click の共存
- 強押し中の Precision Pointer
- 静止強押しからの Caret Mode
- optional な Piezo 音フィードバック（pin pending）

この MVP は、**トラックパッド周辺のインタラクション改善**を目的とする。

## 対象スコープ
### スコープ内
- DRV2605L 初期化
- 小さなハプティック再生 helper
- D1 の FSR ADC 読み取り
- Force threshold / release threshold の導入
- Tap 系と Force Click 系の差別化
- 1本指 / 2本指 / 3本指 Force Click の導入
- 1本指 Force の文脈分岐
- Precision Pointer
- Caret Mode
- 統一された feedback / state event レイヤ

### optional
- Piezo 出力経路
- Piezo ON/OFF 設定保持

### スコープ外
- 第2 MCU
- 高機能なオーディオスタック
- カスタム波形エディタ
- 複数プロファイルのハプティック調整 UI
- ホスト側ソフトによる文脈認識
- 「今テキスト欄か」の自動判定
- 複雑な多段圧力ジェスチャ設計
- 初期実装での 4 センサ force matrix
- D16 の使用

## 最低限残すべき既存挙動
- 1本指ドラッグによるポインタ移動
- 2本指ドラッグによるスクロール
- 1本指 / 2本指 / 3本指 Tap による click 系入力
- 既存ジェスチャと TapDrag の基本動作

## 追加する主要挙動
### Tap 系
- 1本指 tap -> primary click
- 2本指 tap -> secondary click
- 3本指 tap -> middle click

### Force Click 系
- 1本指 force -> primary click
- 2本指 force -> secondary click
- 3本指 force -> middle click

Force Click は、**しきい値超え瞬間に down**、release で up を送る。

### 1本指 Force の文脈分岐
- 移動文脈から入った場合 -> Precision Pointer
- 静止文脈から入った場合 -> click / Caret Mode 候補
- TapDrag 待機中に Force threshold を超えた場合は、TapDrag 待機をキャンセルして Force 系を優先する
- ただし、すでに TapDrag drag 中であれば button ownership は TapDrag 側に残し、Precision Pointer のみ重ねてよい

### Precision Pointer
- 慣性を弱める
- 移動速度を落とす
- 必要なら加速度も調整する
- 実装はハイブリッドで、文脈判定と慣性制御は driver 側、XY 減速は input processor 側で扱う

### Caret Mode
- 1本指 Force + 静止文脈
- 約 300ms の静止で入る
- ポインタ送出を止める
- 上下左右の文字カーソル入力を生成する
- 最初の移動で 1 発、その後は連続入力へ入る
- Caret Mode に入る瞬間に、Force 由来で保持している button down があれば button up を送って解除する

### Piezo
- optional
- pin pending
- 安全な出力ピンが確定した後に導入する

## イベントの考え方
### click / mode 系
- tap_primary
- tap_secondary
- tap_middle
- force_primary_down
- force_secondary_down
- force_middle_down
- force_primary_up
- force_secondary_up
- force_middle_up
- precision_pointer_enter
- precision_pointer_exit
- caret_mode_enter
- caret_mode_exit

### 補助イベント
- force_threshold_crossed
- force_released
- pointer_motion_detected
- pointer_motion_quiet
- caret_hold_timeout

## ハプティック方針
### 必須
- tap_primary / tap_secondary / tap_middle
- force_primary_down / force_secondary_down / force_middle_down
- precision_pointer_enter
- caret_mode_enter

### optional
- drag_start

### 触感の差
- Tap は軽い
- Force Click はより強い
- Precision Pointer enter は mode cue として軽め
- Caret Mode enter は分かりやすい cue

## Piezo 方針
- 主役ではなく補助
- click 系の輪郭強調を中心に使う
- ただし現時点では optional / pin pending

## FSR 方針
- 最初から連続値として扱う前提で設計する
- ただし初期の実装は閾値と状態遷移で安定化してもよい
- Force 判定は、touch 開始時の FSR 基準値からの増分を使う
- そのため、無負荷の絶対値が高い個体差や機構差をある程度吸収する
- Caret Mode の速度制御では、圧力を主、移動量を補助として使う
- MVP では threshold 類は固定値とし、実行時調整や永続化は入れない
- 現行ファームウェアターゲットでは D1 / P0.03 / AIN1 を FSR の ADC 入力として使う
- 右側では既存の D1 matrix 列を D17 相当の raw GPIO に移し、FSR 用に D1 を空ける
- FSR を LRA と強く結合した機構では、haptic 再生が FSR に機械的に回り込む可能性がある
- そのため、haptic 再生直後の短時間は FSR を直前の安定値で保持し、force の再トリガや誤 release を抑制する

## 完了条件
以下を満たせば初期 MVP として受け入れ可能。

- ファームウェア内で DRV2605L が初期化される
- ハプティック再生をコードから呼べる
- FSR ADC 読み取りをコードから参照できる
- Tap と Force Click の双方が成立する
- 1本指 / 2本指 / 3本指 Force Click が定義される
- 1本指 Force の文脈分岐が入る
- Precision Pointer が動作する
- Caret Mode が動作する
- 明らかなピン競合を導入していない
- 既存トラックパッド挙動を壊していない
- 右側ファームウェアサイズの差分が報告される

Piezo は optional のため、初期 MVP の必須完了条件からは外す。
