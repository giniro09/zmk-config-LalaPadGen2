# AGENTS.md

## この案件の位置づけ
これは LaLaPad Gen2 ファームウェア全体の一部を対象とした、interaction feedback 改造実験である。
リポジトリ全体を voice-control 向けに再構成する案件ではない。

## この案件で最初に読む文書
- PROJECT_CONTEXT.md
- REPO_BASELINE.md
- HARDWARE_AND_PIN_PLAN.md
- FIRMWARE_MVP_SPEC.md
- CODEX_TASK_BRIEF.md

## この案件の基本制約
- XIAO BLE Plus を唯一の MCU / firmware host とする。
- 既存のトラックパッド挙動を壊さない。
- D4 / D5 は既存 I2C を前提に確認する。
- D14 は FSR デジタル入力候補、D16 は piezo 出力候補として扱う。
- DRV2605L は既存 I2C に追加する前提で調査する。
- 推測で大規模な抽象化や全面整理をしない。
- まず既存コードベースに沿った増分実装を優先する。

## この案件で避けること
- 第二 MCU の導入
- 不要な大規模リファクタ
- 代替ハプティックドライバ対応の過剰抽象化
- 実機未確認なのに断定的に完了扱いすること

## 報告時に必ず含めること
- 変更ファイル一覧
- 実装した経路とイベント接続点
- ビルド確認結果
- 実機確認が必要な項目
- ピン競合やバス競合の懸念
