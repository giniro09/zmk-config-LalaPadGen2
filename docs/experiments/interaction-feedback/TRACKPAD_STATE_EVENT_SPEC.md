# TRACKPAD_STATE_EVENT_SPEC.md

## 1. 目的
この文書は、LaLaPad Gen2 のトラックパッド改造における
状態遷移とイベントの骨格を定義する。

本仕様は、以下を両立することを目的とする。

- Tap と Force Click の共存
- 強押し中の精密ポインタ操作
- 強押し静止からの Caret Mode
- ファームウェアのみで完結する振る舞い設計

Piezo は optional / pin pending であり、
本状態機械の成立条件には含めない。

## 2. 既存入力との関係
現行の LaLaPad Gen2 は少なくとも以下を持つ。

- 1本指ドラッグ: カーソル移動
- 2本指ドラッグ: スクロール
- 1本指タップ: 左クリック
- 2本指タップ: 右クリック
- 3本指タップ: 中クリック
- 3本指スワイプ: 戻る / 進む / タスクビュー / デスクトップ表示
- TapDrag: タップ後ホールドでドラッグ継続

本仕様では、それらを壊さずに
**Force Click 系入力を追加する**。

## 3. 基本イベント定義
### 3.1 Tap 系イベント
- tap_primary
- tap_secondary
- tap_middle

### 3.2 Force 系イベント
- force_primary_down
- force_secondary_down
- force_middle_down
- force_primary_up
- force_secondary_up
- force_middle_up

### 3.3 モードイベント
- precision_pointer_enter
- precision_pointer_exit
- caret_mode_enter
- caret_mode_exit

### 3.4 補助イベント
- force_threshold_crossed
- force_released
- pointer_motion_detected
- pointer_motion_quiet
- caret_hold_timeout
- direction_changed
- recenter_detected

## 4. 指本数と click 種別
### Tap
- 1本指 tap -> primary click
- 2本指 tap -> secondary click
- 3本指 tap -> middle click

### Force Click
- 1本指 force -> primary click
- 2本指 force -> secondary click
- 3本指 force -> middle click

Force Click は、しきい値を超えた瞬間に down を発火する。
これにより drag の起点としても使える。

## 5. 上位状態定義
### S0: IDLE
非接触状態。

### S1: TOUCH_TRACKING
接触中だが、Force threshold 未満。
通常のトラックパッド追跡状態。

### S2: FORCE_HELD
Force threshold を超えた状態。
button down 発火済みで、押し込み継続中。

### S3: PRECISION_POINTER
1本指 Force のうち、移動文脈から入った精密ポインタ状態。

### S4: CARET_CANDIDATE
1本指 Force のうち、静止文脈から入った状態。
click は発火済みで、caret mode へ移行するかを待つ。

### S5: CARET_MODE
文字カーソル移動状態。
通常ポインタ出力を止め、上下左右の caret 入力を生成する。

### S6: FORCE_DRAG
Force hold 中に move を伴う drag 状態。
1本指 / 2本指 / 3本指で click 種別に対応する。

## 6. 状態遷移
### 6.1 IDLE -> TOUCH_TRACKING
条件:
- 指接触開始

### 6.2 TOUCH_TRACKING -> IDLE
条件:
- 全指離脱

### 6.3 TOUCH_TRACKING -> FORCE_HELD
条件:
- FSR が force threshold を超える

動作:
- 指本数に応じて click down 発火
  - 1本指 -> force_primary_down
  - 2本指 -> force_secondary_down
  - 3本指 -> force_middle_down
- Force Click 用ハプティックを返す

### 6.4 FORCE_HELD -> PRECISION_POINTER
条件:
- 1本指 Force
- 強押し直前または直後に移動量が一定以上ある

動作:
- precision_pointer_enter
- 精密カーソルパラメータへ切替
- 専用ハプティックを返す

注記:
- この押下中は Caret Mode に入らない

### 6.5 FORCE_HELD -> CARET_CANDIDATE
条件:
- 1本指 Force
- 強押し前後の移動量が少ない

動作:
- 300ms 前後の静止待ちを開始

### 6.6 CARET_CANDIDATE -> CARET_MODE
条件:
- caret_hold_timeout 到達
- その間の移動量が小さい

動作:
- caret_mode_enter
- 通常ポインタ出力停止
- 中心座標を記録
- 専用ハプティックを返す

### 6.7 CARET_CANDIDATE -> FORCE_DRAG
条件:
- button down 後に move が発生
- caret timeout より前に drag 文脈に入った

動作:
- drag 継続

### 6.8 PRECISION_POINTER -> FORCE_DRAG
条件:
- button down 維持中に move 継続

動作:
- drag 継続
- precision パラメータを適用したままでもよい

### 6.9 FORCE_HELD / PRECISION_POINTER / CARET_CANDIDATE / CARET_MODE / FORCE_DRAG -> IDLE
条件:
- FSR が release threshold 未満に戻る
  または指が離れる

動作:
- 対応する click up を発火
- 必要に応じて mode exit を発火
  - precision_pointer_exit
  - caret_mode_exit

## 7. Precision Pointer 仕様
### 7.1 目的
通常カーソルでは細かく狙いづらい場面で、
精密なポインタ操作を可能にする。

### 7.2 パラメータ方針
優先順位:
1. 慣性を弱める
2. 移動速度を落とす
3. 必要なら加速度を調整する

### 7.3 入口条件
- 1本指 Force
- 移動文脈あり

### 7.4 Exit 条件
- Force release
- 指離脱

## 8. Force Drag 仕様
### 8.1 割当
- 1本指 Force hold + move -> primary drag
- 2本指 Force hold + move -> secondary drag
- 3本指 Force hold + move -> middle drag

### 8.2 備考
3本指 middle drag は初期段階では残すが、
実機評価で有用性を再確認する。

## 9. Caret Mode 仕様
### 9.1 入口
- 1本指 Force
- 静止文脈
- 300ms 前後の静止継続

### 9.2 基本動作
Caret Mode に入ったら、通常ポインタ移動は停止する。
以後の移動は、文字カーソル移動へ変換する。

### 9.3 入力生成
- 最初の移動で 1 発入力
- 同方向へ寄せ続けると連続入力へ入る
- 中心付近へ戻すと停止
- 方向が変われば新方向へ切替

### 9.4 方向
- left
- right
- up
- down

### 9.5 方向判定
方向は、Caret Mode 開始時に記録した中心位置からの
相対移動で判定する。

### 9.6 速度制御
連続入力速度は以下で決める。
- 主: FSR 圧力
- 補: パッド上の移動量

理由:
- パッド面積が小さいため、移動量だけへの依存は避ける

## 10. ハプティックイベント
### 必須
- tap_primary / tap_secondary / tap_middle
- force_primary_down / force_secondary_down / force_middle_down
- precision_pointer_enter
- caret_mode_enter

### optional
- drag_start

### 方針
- Tap は軽い
- Force Click は強い
- Precision Pointer enter は軽い mode cue
- Caret Mode enter は明確な mode cue

## 11. 音
- optional
- pin pending
- 状態機械の成立条件には含めない

## 12. 実機調整対象
- force threshold / release threshold
- precision pointer の慣性・速度・加速度
- caret mode の静止時間
- caret mode の中心復帰判定
- caret 連続入力速度カーブ
- 3本指 force drag の実用性
- Piezo を使う場合の安全な別ピン候補
