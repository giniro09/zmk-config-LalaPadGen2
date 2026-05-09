# Current Force / Precision / Caret Behavior

対象コミット: `3f14476cbc83b8db8cc1a7d0d97686de8127c25e`

この文書は、現時点のソフトウェアで独自追加されている Force / Precision / Caret の実効仕様をまとめる。キーマップ全体や通常ジェスチャの網羅は対象外とし、特に現在有効な Force の扱いを主対象にする。

## 現在の有効状態

左右どちらの shield 設定でも、Force と Caret は有効、Precision は無効になっている。

```conf
# Left side
CONFIG_INPUT_IQS9151_FORCE_THRESHOLD=18000
CONFIG_INPUT_IQS9151_FORCE_RELEASE_THRESHOLD=11500
CONFIG_INPUT_IQS9151_FORCE_THRESHOLD_2F=19000
CONFIG_INPUT_IQS9151_FORCE_RELEASE_THRESHOLD_2F=12500
CONFIG_INPUT_IQS9151_FORCE_THRESHOLD_3F=20000
CONFIG_INPUT_IQS9151_FORCE_RELEASE_THRESHOLD_3F=13500

# Right side
CONFIG_INPUT_IQS9151_FORCE_THRESHOLD=15000
CONFIG_INPUT_IQS9151_FORCE_RELEASE_THRESHOLD=9500
CONFIG_INPUT_IQS9151_FORCE_THRESHOLD_2F=16000
CONFIG_INPUT_IQS9151_FORCE_RELEASE_THRESHOLD_2F=10500
CONFIG_INPUT_IQS9151_FORCE_THRESHOLD_3F=17000
CONFIG_INPUT_IQS9151_FORCE_RELEASE_THRESHOLD_3F=11500
CONFIG_INPUT_IQS9151_FORCE_USE_ABSOLUTE=y
CONFIG_INPUT_IQS9151_FORCE_ENTER_DEBOUNCE_MS=5
CONFIG_INPUT_IQS9151_FORCE_RELEASE_DEBOUNCE_MS=35
CONFIG_INPUT_IQS9151_FORCE_MOVE_THRESHOLD=36
CONFIG_INPUT_IQS9151_PRECISION_ENABLE=n
CONFIG_INPUT_IQS9151_CARET_ENABLE=y
```

Precision 関連のコード、input processor、設定値は残っているが、現設定では driver が `precision_active` に入らないため実効しない。Caret は有効化されており、1本指 Force の静止長押しから入る。

## 使用デバイス

- Trackpad: IQS9151, I2C address `0x56`
- FSR 読み取り: ADS1115, I2C address `0x48`, channel `0`
- Haptic: DRV2605L, I2C address `0x5a`
- FSR は左右とも ADS1115 経由で読む。`force-gpios` 経由の 0/1 Force 入力は現在の overlay では使っていない。

## FSR 値の読み方

現在は `CONFIG_INPUT_IQS9151_FORCE_USE_ABSOLUTE=y` のため、タッチ開始時の差分ではなく、起動時および無接触時に学習した idle 値からの距離を Force 量として扱う。

1. 起動後 `1500ms` は unloaded / idle の基準値を学習する。
2. その後も、無接触かつ Force 非アクティブ中は idle 基準値をゆっくり追従させる。
3. 現設定では `CONFIG_INPUT_IQS9151_FORCE_INVERT_ANALOG=n` なので、ADS1115 の raw 値が idle より上がった分だけ Force とみなす。
4. raw 値が idle 以下の場合、Force 量は `0` になる。

ADS1115 読み取り値は、通常時は前回安定値と新規値を `3:1` で平滑化する。haptic 再生直後は FSR への機械的な回り込みを避けるため、`25ms` の間は直前の安定値を使う。

## Force の入口条件

Force は、以下の条件を満たすと active になる。

- 指数が 1-3 本の接触中であること
- FSR の Force 量が指本数ごとの enter threshold 以上であること
- 上記状態が `5ms` 以上続くこと
- release 後の再アーム禁止期間 `40ms` 中ではないこと

通常モードでは、接触がない状態で FSR だけを押しても Force には入らない。`CONFIG_INPUT_IQS9151_FSR_DIAG_MODE=n` のため、FSR 単体診断モードも無効。

指本数が増えるほど通常接触だけで FSR に荷重が乗りやすいため、Force 判定は指本数ごとに別しきい値を使う。

| Side / 指本数 | enter threshold | release threshold |
| --- | ---: | ---: |
| Right 1本指 | `15000` | `9500` |
| Right 2本指 | `16000` | `10500` |
| Right 3本指 | `17000` | `11500` |
| Left 1本指 | `18000` | `11500` |
| Left 2本指 | `19000` | `12500` |
| Left 3本指 | `20000` | `13500` |

## Force 中の出力

Force に入ると、driver 内部では `force.active=true` になり、押下時の指本数と Force button が latch される。

現在の実効挙動は次の通り。

| 入力 | 現在の実効扱い |
| --- | --- |
| 1本指 Force | `INPUT_BTN_0` の Force-owned hold。静止が `450ms` 続くと button up を送って Caret に入る |
| 2本指 Force | `INPUT_BTN_1` の Force-owned hold |
| 3本指 Force | `INPUT_BTN_5` の Force-owned hold。既存の 3F-up 仮想入力を使い、Task View / Overview 系の操作に割り当てる |

Force では、Force-owned hold として button down を送り、Force release まで保持する。したがって、単発 click というより「押し込みで button を押し下げ、圧が抜けるか指が離れたら release する」挙動になる。1本指 Force だけは Caret 候補にもなり、Force 入口後に静止が続いた場合は保持中の `INPUT_BTN_0` を release してから Caret へ移行する。

## Force release

Force は以下のいずれかで release される。

- 指が離れる
- active 中に latch された指本数と現在の有効指本数が合わなくなる
- Force 量が release threshold 以下になる

通常の release threshold は指本数ごとの値を使う。ただし Force-owned hold drag 中は途中で途切れにくくするため、実効 release threshold がその指本数の release threshold の `3 / 4` に下がる。

しきい値 release には `35ms` の debounce がある。release 後は `40ms` の再アーム禁止期間が入る。

## Haptic

Force click 用 haptic は DRV2605L の RTP mode で短い強めの click を再生する。

- kick: `0x7F` を `2300us`
- brake: `0x88` を `1600us`
- zero: `0x00` を `600us`

2本指 Force は 1本指 Force と区別しやすいように、同じ Force click haptic を短い間隔で2回鳴らす。

通常 tap click の haptic は無効。tap は指を離す動作自体がフィードバックになるため、触覚フィードバックは Force / Caret / Caret 中の文字送りに寄せる。

起動完了時は、IQS9151 の初期化とIRQ有効化が終わったタイミングで短い tick haptic を1回鳴らす。これは、そのトラックパッド側が操作可能になったことを知らせるための合図として扱う。

ただし、1本指 Force が「移動中の文脈」または TapDrag 中の文脈から入った場合、Force haptic は鳴らさない。これは本来 Precision / overlay 用の入口として扱うための条件だが、現在は Precision が無効なので、haptic 抑制だけが残るケースがある。

## Tap / TapDrag との関係

Force が TapDrag ではない通常文脈で入ると、pending になっている tap click は clear され、1本指 tap 状態も reset される。Tap の遅延 click と Force-owned hold が二重に出ないようにするため。

TapDrag 中に Force threshold を超えた場合は `overlay_only` として扱われる。現在は Precision が無効なので、Force 側が追加の pointer slowdown を与えることはない。既存 TapDrag の button ownership との競合を避ける処理は入っているが、この経路は実機で挙動確認が必要。

## Precision の現在の扱い

Precision は `CONFIG_INPUT_IQS9151_PRECISION_ENABLE=n` のため無効。

コード上は、1本指 Force が移動中の文脈から入った場合に `precision_active` を立て、`zmk,input-processor-precision-scaler` が REL_X / REL_Y を FSR 量に応じて減速する設計が残っている。現在は driver 側が `precision_active` に入らないため、precision scaler は listener に存在していても実効しない。

設定値としては以下が残っているが、現状は休眠中。

- `CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_MAX_SCALE_X100=78`
- `CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_MIN_SCALE_X100=50`
- `CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_START_FORCE_DELTA=9000`
- `CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_MAX_FORCE_DELTA=12000`

## Caret の現在の扱い

Caret は `CONFIG_INPUT_IQS9151_CARET_ENABLE=y` のため有効。

静止文脈の 1本指 Force が `450ms` 続いた場合に caret mode へ入り、通常 pointer 出力を止めて `INPUT_KEY_LEFT / RIGHT / UP / DOWN` を生成する。

Caret に入る前は Force-owned `INPUT_BTN_0` hold として扱う。Caret に入る瞬間にその hold を release し、Force click と同じ click 感のある haptic を2回繰り返す。以後は指の中心位置からの移動量を矢印キー入力に変換する。Caret 中は1文字ぶんの矢印キー入力を出すたびに短い cursor tick haptic を返す。Caret 候補中に明確な移動が出た場合は Caret 候補をキャンセルし、そのまま primary drag として扱う。

## 実機確認が必要な点

- 1本指 Force の press / drag / release が、意図通り primary button hold として動くか。
- 2本指 Force が secondary button hold / drag として自然に使えるか。
- 3本指 Force の Task View / Overview 系割り当てが、Windows / macOS の両レイヤーで期待通りか。
- moving context からの Force で haptic が抑制されることが、Precision 無効中でも自然に感じられるか。
- TapDrag 中に Force threshold を超えた場合の ownership / release が実機操作で破綻しないか。
