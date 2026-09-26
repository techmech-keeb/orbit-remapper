---
status: draft
verified_as_of: 2026-09-26
upstream_commit: 51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e
finalized: false
---

# 本家 HID Remapper の機能境界と Orbit Remapper への影響

本書の「できる」は**本家の該当するファームウェア単体**での意味です。異なる版の機能を一台に組み合わせた動作、特定のキーボードとの相性、Orbit Remapper 実機での成立を保証しません。確認対象は上記コミットのソース、[公式マニュアル](https://www.remapper.org/manual/)、[Bluetooth版説明](https://github.com/jfedor2/hid-remapper/blob/master/BLUETOOTH.md)、[Picoハードウェア説明](https://github.com/jfedor2/hid-remapper/blob/master/HARDWARE.md)、[配布版](https://github.com/jfedor2/hid-remapper/releases)です。仕様・配布物は更新されるため、着手時に再確認します。

## まず区別する三つの構成

| 構成 | 入力 | PCへの出力 | 現状 |
| --- | --- | --- | --- |
| USB版・Pico単体 | USB HID、USB MIDI、一部GPIO | USB HID | 本家で提供。単体PicoのPIO USBホストは機器相性に制約。 |
| USB版・Pico 2台 | Pico BのUSBホストからUARTでPico Aへ | Pico AのUSB HID | 本家で両側の対になるファームを提供。USB機器の互換性が単体版より良い。 |
| BLE版・XIAO nRF52840 | Bluetooth LE HID | USB HID | 本家でXIAO用ファームを提供。実験的な版で、USBホスト入力は含まない。 |

**Orbit想定の「Pico B → UART → XIAOがUSBとBLE入力を統合 → PC」は上のいずれとも異なります。** Pico Bに本家のファームを書くだけでは動きません。USB版のPico Aが担うUART受信・接続管理をXIAO側に実装し、BLE入力と同じリマップ処理へ合流させる必要があります。これは[Bluetooth版のビルド定義](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware-bluetooth/CMakeLists.txt)と[主処理](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware-bluetooth/src/main.cc)、[Pico B側](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware/src/remapper_dual_b.cc)、[UARTコマンド定義](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware/src/dual.h)の比較からの判断です。

## 機能別の確認

| 項目 | USB版・Pico | BLE版・XIAO | Orbitでの判断 |
| --- | --- | --- | --- |
| USBキーボード・マウス・対応ゲームパッド入力 | 対応。USBハブで複数も可。全機種保証はない | 対応しない。XIAO版にUSBホストはない | E1案ではPico BからXIAOへ渡す独自実装が必要。 |
| BLEキーボード等の入力 | 対応しない | Bluetooth **LE HID** を複数接続可能 | 対象機器・再接続挙動の現物検証が必要。 |
| Bluetooth Classic入力 | Pico W / Pico 2 W向け**試験版**は別途存在 | nRF52840/XIAO版では非対応 | XIAO案の対象に含めない。将来要求するなら構成から再検討。 |
| キー、マウス、ゲームパッドの変換 | 対応 | 主要なリマップ機能は共通 | XIAO側で二系統の入力を統合した後に適用する設計が必要。 |
| USB機器をそのままPCへ中継すること | HID入力を解釈し、選んだ種類のUSB HIDとして出力。汎用USBハブではない | BLE HID入力をUSB HIDに変換 | オーディオ・ストレージ・任意のUSB機器の透過中継は対象に含めない。 |
| レイヤー、トグル、タップ・ホールド、マクロ、式 | 対応 | リマップ機能の大部分が利用可能 | キーボード系のコア機能として流用候補。ただし実機試験前。 |
| 設定Web UI、端末内保存、JSON書き出し・読み込み | 対応 | 設定Web UIを利用可能 | PCでの設定は流用候補。筐体側の画面・ダイヤル操作は新規連携。 |
| USBハブのポート別設定 | 対応 | 該当しない | USB入力側のディスクリプタ・ポート情報をXIAO側まで伝える必要。 |
| BLE機器ごとの識別・固定設定 | 該当しない | ソース上はペアリング済み機器の列挙順を入力ポート番号に割り当てる。MACアドレスに紐付いた安定した名前付き設定は未確認 | 前版の「BLEでは機器別設定不可」は訂正。[複数機器の挙動](multi-device-behavior.md)を参照し、再ペアリング時の番号維持を実機で検証。上流の[Issue #313](https://github.com/jfedor2/hid-remapper/issues/313)はMACアドレスでの明示的な機器別設定を要望。 |
| GPIOを通常のリマップ入力・LED等の出力に使う | 対応 | 対応しない | XIAOにつなぐ操作ボタン・LEDは別の制御を実装する。XIAOのペアリング用pin 0操作は別機能。 |
| 旧BIOS向けUSB boot keyboard | 対応 | 対応しない | 必須ならPC出力側の方式を再検討。 |
| PCのスリープ解除 | USB版のキーボード／マウスモードで対応する配布版あり | Bluetooth版では現状非対応 | 必要なら電源・ファーム・OSの組み合わせで確認。 |
| USB MIDI機器の入力 | 対応 | XIAO版のUSBホスト入力はない | MIDIを対象にするなら範囲・転送仕様を追加。**MIDI出力機器としての動作は本家非対応**。 |
| 特殊なHIDの癖への対処 | USB側ではカスタムusage等で一部対処可 | Bluetooth版のquirks機構は動作しない | 特定製品の対応は実機とレポート記述子で判定。 |
| 入力機器への出力・Featureレポート | Pico B経由の転送コマンドがある | 現行ソースでは転送関数が未実装 | 入力側のLED、独自機能、認証などを必要とする機器は個別に検証。 |
| 透過OLED、筐体側メニュー、ロータリーUI | 標準機能ではない | 標準機能ではない | Glass2の表示、設定読み書き、画面と物理操作の同期を新規実装。 |
| 本体リセット | 基板側のリセット・書き込み手順による | 基板側のリセット・書き込み手順による | 外部リセットボタンは機構・配線の独立した要求。ペアリングと混同しない。 |

USB/BLE別の公式説明は[マニュアルの入力・出力・設定・Bluetooth章](https://www.remapper.org/manual/)と[Bluetooth版説明](https://github.com/jfedor2/hid-remapper/blob/master/BLUETOOTH.md)に基づきます。入力機器向けレポートの制限は上記コミットの[Bluetooth版主処理](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware-bluetooth/src/main.cc)内の未実装箇所からの判断です。スリープ解除は[2026-01-20リリース](https://github.com/jfedor2/hid-remapper/releases/tag/r2026-01-20)、Bluetooth Classicの例外は[2026-08-29試験リリース](https://github.com/jfedor2/hid-remapper/releases/tag/r2026-08-29-test)で確認しました。「本家全体がClassic非対応」という表現は避けます。

## USBホストをS3に置き換える分岐

M5Dial内のESP32-S3をUSBホストにする案では、Pico B用UF2をS3へ転用できません。S3でUSB HIDを読んでXIAOへ渡すファーム、双方の通信仕様、XIAOの入力統合が必要です。S3の表示・操作処理を加えても、XIAO側のUART統合課題は残ります。現行のE1/透過OLED案とは**未採用の別構成**です。

## 要件を確定する前に区切る範囲

1. **初期リリースの入力**: USBキーボード1台とBLE HIDキーボード1台で足りるか。マウス、ハブ、複合機器、MIDIを含めるか。
2. **同時利用**: USBとBLEを同時接続・同時押し・切断後復帰まで扱うか。共有レイヤーと優先順位をどうするか。
3. **本体UI**: 画面は状態表示のみか、レイヤー切替・プロファイル選択・ペアリング・設定編集まで行うか。
4. **PC向け出力**: キーボード・マウスだけで足りるか。旧BIOS、ゲームパッド、スリープ解除を必要とするか。
5. **対応保証**: 型番を指定したUSB/BLE機器で試験するか。BLE Classic機器を対象外とするか。

ここに挙げた項目は**確認事項**であり、機能の採用決定ではありません。機能が本家に存在しても、Orbitの複合構成で利用できることを実機で確認する必要があります。
