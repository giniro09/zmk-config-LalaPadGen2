# Current Force / Precision / Caret Behavior

対象コミット: 現在の作業ツリー

この文書は、LaLaPad Gen2 の現時点の Force / Precision / Caret の実効仕様をまとめる。キーマップ全体や通常ジェスチャの網羅は対象外とし、独自追加した Force / Precision / Caret と、その触覚フィードバックを中心に記載する。

## 現在の有効状態

- Force: 有効
- Caret: 有効。ただし IQS9151 driver 内のローカル Caret ではなく、`zmk,input-processor-force-caret` で中央処理する
- Precision: 無効。コードと設定値は残っているが、`CONFIG_INPUT_IQS9151_PRECISION_ENABLE=n`
- 通常 tap haptic: 無効
- 起動完了 haptic: 有効

左右どちらも FSR は ADS1115 経由で読み、Force 量は起動時/無接触時の idle 値からの距離として扱う。

## Force Threshold

`CONFIG_INPUT_IQS9151_FORCE_USE_ABSOLUTE=y` のため、Force 判定はタッチ開始時差分ではなく、idle 基準値からの絶対距離で見る。起動直後 `1500ms` は idle 値を学習し、その後も無接触かつ Force 非アクティブ中は idle 値をゆっくり追従させる。

| Side / fingers | enter threshold | release threshold |
| --- | ---: | ---: |
| Right 1本指 | `15000` | `9500` |
| Right 2本指 | `16000` | `10500` |
| Right 3本指 | `17000` | `11500` |
| Left 1本指 | `18000` | `11500` |
| Left 2本指 | `19000` | `12500` |
| Left 3本指 | `20000` | `13500` |

Force enter debounce は `5ms`、release debounce は `35ms`。Force release 後は `40ms` の rearm block を置く。Force-owned hold drag 中は途中で途切れにくくするため、実効 release threshold を通常値の `3 / 4` に下げる。

## Force 出力

| 入力 | 現在の扱い |
| --- | --- |
| 1本指 Force | `INPUT_BTN_0` の Force-owned hold。静止が `450ms` 続くと hold を release して Caret に入る |
| 2本指 Force | `INPUT_BTN_0` の double click。Force-owned hold にはしない |
| 3本指 Force | `INPUT_BTN_5` の Force-owned hold。既存の 3F-up 仮想入力を使う |

2本指 Force は tap との差別化のため、右クリックではなくダブルクリック扱いにした。成立した瞬間に primary button click を2回送る。押しっぱなし右クリックや右ドラッグとしては扱わない。

3本指 Force は現時点では便利寄りの別機能枠として残している。実際の意味は `INPUT_BTN_5` の割り当て先に依存する。

## Haptic

Force click 用 haptic は DRV2605L の RTP mode で短い click 感を再生する。

- kick: `0x7F` を `2300us`
- brake: `0x88` を `1600us`
- zero: `0x00` を `600us`

2本指 Force は、1本指 Force と区別できるように同じ Force click haptic を短い間隔で2回鳴らす。「カカッ」というフィードバックを意図している。

通常 tap click の haptic は無効。tap は指を離す動作自体がフィードバックになるため、触覚フィードバックは Force / Caret / Caret 中の文字送りに寄せる。

起動完了時は、IQS9151 の初期化と IRQ 有効化が終わったタイミングで短い tick haptic を1回鳴らす。その側のトラックパッドが操作可能になったことを知らせる合図として扱う。

Haptic 再生直後は FSR への機械的な回り込みを避けるため、`25ms` の間は直前の安定値を使う。

## Caret

Caret は `zmk,input-processor-force-caret` が中央で処理する。左右のトラックパッド入力を共有状態で見るため、片側で Caret に入った後、反対側のトラックパッドでカーソル移動することもできる。

設定値:

- Caret hold: `450ms`
- Caret deadzone: `80`
- Caret candidate の drag cancel threshold: `18`

1本指 Force が静止文脈で成立すると、まず `INPUT_BTN_0` の Force-owned hold として扱う。そのまま `450ms` 続いた場合、中央 processor が primary hold を release し、Caret active に入る。

Caret 候補中に単発 REL 移動量が `18` 以上出た場合は、Caret 候補をキャンセルし、そのまま primary drag として扱う。小さなセンサー揺れだけでは Caret 候補を維持する。

Caret active 中は pointer REL_X / REL_Y を通常 pointer として出さず、蓄積量が `80` を超えるたびに `LEFT / RIGHT / UP / DOWN` のいずれかを1回 tap する。1イベントあたり最大4ステップまで出す。

Caret haptic:

- Caret に入る瞬間: Caret を成立させた側のトラックパッドで Force double click haptic
- Caret 中に1文字分カーソル移動した瞬間: 実際に移動入力を出した側のトラックパッドで cursor tick haptic

左右別 haptic は `zmk,behavior-iqs9151-haptic` で実行する。behavior 名は split 越しに送れるよう `ihl` / `ihr` の短い名前にしている。locality は `BEHAVIOR_LOCALITY_EVENT_SOURCE` とし、必要な側だけに送る。

## Precision

Precision は現在無効。

`zmk,input-processor-precision-scaler` と関連設定値は残っているが、driver 側で `precision_active` に入らないため実効しない。

残っている主な設定値:

- `CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_MAX_SCALE_X100=78`
- `CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_MIN_SCALE_X100=50`
- `CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_START_FORCE_DELTA=9000`
- `CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_MAX_FORCE_DELTA=12000`

## Tap / TapDrag との関係

Force が通常文脈で入ると、pending になっている tap click は clear され、1本指 tap 状態も reset される。Tap の遅延 click と Force 出力が二重に出ないようにするため。

TapDrag 中に Force threshold を超えた場合は `overlay_only` として扱う。現在は Precision が無効なので、Force 側が追加の pointer slowdown を与えることはない。既存 TapDrag の button ownership との競合を避ける処理は入っているが、この経路は実機確認が必要。

## 実機確認ポイント

- 1本指 Force が primary hold / drag として自然に使えること
- 1本指 Force 静止長押しで Caret に入り、入った側で「カカッ」と鳴ること
- Caret 中、左右どちらのトラックパッドで動かしても矢印入力になり、動かした側だけが tick すること
- 2本指 Force が primary double click として出て、「カカッ」と鳴ること
- 3本指 Force の割り当てが実使用上便利かどうか
- 通常 tap では haptic が鳴らないこと
