# Current Force / Precision / Caret Behavior

対象コミット: `3f14476cbc83b8db8cc1a7d0d97686de8127c25e`

この文書は、現時点のソフトウェアで独自追加されている Force / Precision / Caret の実効仕様をまとめる。キーマップ全体や通常ジェスチャの網羅は対象外とし、特に現在有効な Force の扱いを主対象にする。

## 現在の有効状態

左右どちらの shield 設定でも、Force は有効、Precision と Caret は無効になっている。

```conf
CONFIG_INPUT_IQS9151_FORCE_THRESHOLD=11200
CONFIG_INPUT_IQS9151_FORCE_RELEASE_THRESHOLD=7000
CONFIG_INPUT_IQS9151_FORCE_USE_ABSOLUTE=y
CONFIG_INPUT_IQS9151_FORCE_ENTER_DEBOUNCE_MS=5
CONFIG_INPUT_IQS9151_FORCE_RELEASE_DEBOUNCE_MS=35
CONFIG_INPUT_IQS9151_FORCE_MOVE_THRESHOLD=36
CONFIG_INPUT_IQS9151_PRECISION_ENABLE=n
CONFIG_INPUT_IQS9151_CARET_ENABLE=n
```

Precision / Caret 関連のコード、input processor、設定値は残っているが、現設定では driver が `precision_active` や `caret_active` に入らないため、実動作としては Force 判定と Force-owned button hold が中心になる。

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
- FSR の Force 量が `11200` 以上であること
- 上記状態が `5ms` 以上続くこと
- release 後の再アーム禁止期間 `40ms` 中ではないこと

通常モードでは、接触がない状態で FSR だけを押しても Force には入らない。`CONFIG_INPUT_IQS9151_FSR_DIAG_MODE=n` のため、FSR 単体診断モードも無効。

## Force 中の出力

Force に入ると、driver 内部では `force.active=true` になり、押下時の指本数と Force button が latch される。

現実装では、Force 入口で `2本指 Force` が `1本指 Force` として latch される。

```c
const uint8_t latched_force_finger_count =
    (force_finger_count == 2U) ? 1U : force_finger_count;
```

そのため、現在の実効挙動は次の通り。

| 入力 | 現在の実効扱い |
| --- | --- |
| 1本指 Force | `INPUT_BTN_0` の Force-owned hold |
| 2本指 Force | 1本指 Force に丸められ、`INPUT_BTN_0` の Force-owned hold |
| 3本指 Force | Force 状態には入るが、現在の `CONFIG_INPUT_IQS9151_CARET_ENABLE=n` 分岐では Force-owned button down は送られない |

1本指 / 2本指 Force では、Force-owned hold として button down を送り、Force release まで保持する。したがって、単発 click というより「押し込みで primary button を押し下げ、圧が抜けるか指が離れたら release する」挙動になる。

## Force release

Force は以下のいずれかで release される。

- 指が離れる
- active 中に latch された指本数と現在の有効指本数が合わなくなる
- Force 量が release threshold 以下になる

通常の release threshold は `7000`。ただし Force-owned hold drag 中は途中で途切れにくくするため、実効 release threshold が `7000 * 3 / 4 = 5250` に下がる。

しきい値 release には `35ms` の debounce がある。release 後は `40ms` の再アーム禁止期間が入る。

## Haptic

Force click 用 haptic は DRV2605L の RTP mode で短い強めの click を再生する。

- kick: `0x7F` を `2300us`
- brake: `0x88` を `1600us`
- zero: `0x00` を `600us`

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

Caret は `CONFIG_INPUT_IQS9151_CARET_ENABLE=n` のため無効。

コード上は、静止文脈の 1本指 Force が一定時間続いた場合に caret mode へ入り、通常 pointer 出力を止めて `INPUT_KEY_LEFT / RIGHT / UP / DOWN` を生成する設計が残っている。現在は Caret 無効のため、`CONFIG_INPUT_IQS9151_CARET_HOLD_MS=300` などの設定値は実効しない。

Caret が無効なので、静止 1本指 Force は caret candidate にはならず、Force-owned `INPUT_BTN_0` hold として扱われる。

## 実機確認が必要な点

- 1本指 Force の press / drag / release が、意図通り primary button hold として動くか。
- 2本指 Force が現在 primary button に丸められていることが意図通りか。
- 3本指 Force が現状 button down を出さないことが意図通りか。
- moving context からの Force で haptic が抑制されることが、Precision 無効中でも自然に感じられるか。
- TapDrag 中に Force threshold を超えた場合の ownership / release が実機操作で破綻しないか。

