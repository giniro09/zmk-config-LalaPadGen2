# REPO_BASELINE.md

## このファイルの目的
Codex が「何をベースに見ればよいか」を最初に誤解しないようにするための補助ファイルです。

## 参照の優先順位
1. **改造対象として最優先で見るリポジトリ**
   - `https://github.com/giniro09/zmk-config-LalaPadGen2`
2. **公式ハード前提の確認に使うガイド**
   - `https://github.com/ShiniNet/LaLaPadGen2/blob/main/guide/BuildGuide.md`
3. **既存トラックパッド動作のベースライン確認に使うガイド**
   - `https://github.com/ShiniNet/LaLaPadGen2/blob/main/guide/HowtoUse.md`

## 役割分担
### 1) 公式ビルドガイド
- LaLaPad Gen2 というハードの基本構成を確認するための参照元
- どの MCU / 部材 / 組み立て前提なのかを把握するために使う
- 直接改造実装を入れる場所ではない

### 2) 公式 HowtoUse
- 既存トラックパッド挙動の基準を確認するための参照元
- 1本指ドラッグ、2本指ドラッグ、1/2/3本指タップ、3本指スワイプ、TapDrag などの現在仕様を確認するために使う
- 今回の改造は、この既存挙動を壊さずに Force Click 系や新しい mode を追加する前提で考える

### 3) 個人ファームウェアリポジトリ
- 実際に調査・変更・追加実装する主対象
- 今後のハプティクス / FSR / optional な Piezo 対応は、このリポジトリをベースに積み上げる
- 既存のディレクトリ構成、driver の置き方、shield / config / input processor の設計に沿って、最小変更で実装する

## Codex への期待
- まず現行コードベースの構造を把握する
- 特に、トラックパッド関連実装、ピン使用状況、driver の配置先、event の流れを優先して確認する
- 既存トラックパッド挙動を壊さない位置に実装する
- 過剰な全面再設計ではなく、段階的な MVP 実装を行う
- 右側ファームウェアサイズの増加を毎回確認し、増分を報告する
- Piezo については pin pending の optional 要素として扱う
