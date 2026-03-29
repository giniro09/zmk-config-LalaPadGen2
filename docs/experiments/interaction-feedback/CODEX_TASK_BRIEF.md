# CODEX_TASK_BRIEF.md

以下を、Codex に最初に渡すタスク指示として使ってください。

---

このリポジトリを、**LaLaPad Gen2 のトラックパッド周辺に対する interaction feedback / force interaction 改造案件**として扱ってください。

## まず理解してほしい前提
- 対象デバイスは LaLaPad Gen2 という左右分割キーボード
- 改造の中心はトラックパッド周辺
- 公式の組み立て前提は ShiniNet の LaLaPad Gen2 BuildGuide
- 既存トラックパッドの標準動作は ShiniNet の HowtoUse をベースラインとする
- ただし、実際に調査・変更するベースリポジトリは **giniro09/zmk-config-LalaPadGen2**
- 今後の改良は、この個人リポジトリを起点に積み上げる

## 最初に読むファイル
- AGENTS.md
- docs/PROJECT_CONTEXT.md
- docs/REPO_BASELINE.md
- docs/CURRENT_TRACKPAD_BEHAVIOR_BASELINE.md
- docs/HARDWARE_AND_PIN_PLAN.md
- docs/FIRMWARE_MVP_SPEC.md
- docs/TRACKPAD_BEHAVIOR_SPEC.md
- docs/TRACKPAD_STATE_EVENT_SPEC.md

## まず調べること
- 現在のピン使用状況
- トラックパッド関連実装がどこにあるか
- driver, shield, config, input processor のどこに追加するのが自然か
- 既存の Tap / TapDrag / gesture flow に event を差し込める場所があるか
- 右側ファームウェアサイズが今どの程度で、今回の差分をどのように監視すべきか

## タスク
現在のファームウェア構造を調査し、以下を加える first-pass MVP を実装してください。

### 低レベル
- DRV2605L ベースのハプティックフィードバック
- D14 の FSR ADC 入力

### 入力 / 状態機械
- Force threshold / release threshold
- 1本指 / 2本指 / 3本指 Force Click
- Tap と Force Click のハプティック差別化
- 1本指 Force の文脈分岐
- Precision Pointer
- Caret Mode
- Force Hold + Move による drag

### optional
- Piezo 出力経路
- Piezo ON/OFF 設定保持

## 現時点の前提
- D4 = SDA
- D5 = SCL
- D14 = FSR ADC に使用
- DRV2605L は既存 SDA / SCL を使って I2C 接続する
- DRV2605L IN は GND に固定する
- MCLR は新規ハードウェアに使わない
- D16 は使わない
- Piezo は optional / pin pending
- ジェスチャ / スクロールは主改造対象ではないので、既存仕様を壊さない

## 実装ゴール
1. 現在のピン使用状況とファームウェア構造を調べる
2. Force interaction layer を置くのに適切な場所を見つける
3. DRV2605L 初期化と基本再生 helper を追加する
4. D14 の FSR 読み取り経路を追加する
5. Tap / Force / mode 系イベントを定義する
6. 1本指 Force の文脈分岐を実装する
7. Precision Pointer を実装する
8. Caret Mode を実装する
9. 可能な範囲で既存トラックパッド flow に最小差分で接続する
10. Piezo は安全な別ピン候補が見つかった場合のみ optional として扱う

## 制約
- 第2 MCU を導入しない
- 別のハプティックドライバ対応を追加しない
- MCLR を使わない
- ホスト側ソフトを前提にしない
- D16 を使わない
- 既存トラックパッド動作を壊さない
- 変更は増分的で読みやすく保つ
- 不明点は推測で固定せず、仮定として明記する
- 右側ファームウェアサイズの増加を毎回確認する

## あわせて確認・報告してほしいこと
- 正確な LRA 型番がコードや設定に影響するか
- FSR は 1 個前提で進めてよいか
- 3本指 Force Drag の実用性はどの程度ありそうか
- 実機でしか確認できない部分はどこか
- バイナリサイズ / RAM 使用量の増分はどの程度か
- Piezo に使える安全な別ピン候補があるか

## 返してほしいもの
- 変更ファイル一覧
- アーキテクチャ上の判断を短くまとめた説明
- 未解決のピン競合やハード前提
- 可能なら build / test 結果
- 実機でしか確認できない項目の明示
- 変更前後のサイズ差分
