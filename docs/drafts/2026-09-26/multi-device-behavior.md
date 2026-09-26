---
status: draft
verified_as_of: 2026-09-26
upstream_commit: 51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e
finalized: false
---

# HID Remapper の複数機器接続: 現状とOrbitの設計課題

本家の[マニュアル](https://www.remapper.org/manual/)・[Bluetooth説明](https://github.com/jfedor2/hid-remapper/blob/master/BLUETOOTH.md)と、上記コミットのソースを確認した。**実機試験は未実施**。以下の「ソースからの判断」は仕様保証ではなく、テスト項目に落とす。

## 入出力の関係

| 構成 | 複数入力 | 識別 | PCから見た出力 |
| --- | --- | --- | --- |
| Pico USB版 | USBハブで複数のHID機器。同一USB無線レシーバーの機器はレシーバーが提示するHIDに依存 | ハブの**物理ポート番号**を指定可能。番号0は全ポート | 選択した種類のHIDを**一台として**エミュレート。複数PCへの振り分けではない |
| XIAO nRF52840 BLE版 | 複数のBLE HID機器 | ソースでは接続スロットで報告を分け、ボンド一覧の列挙順をポート番号としてコアへ渡す | USB HID一台としてエミュレート |
| OrbitでのUSB＋BLE混在 | 本家の既製ファームの組み合わせでは未対応 | 二系統間で衝突しないIDと、安定した機器識別を設計する必要 | XIAO側で出力を統合する案は新規実装 |

USBポートの挙動は[マニュアル「Per-device mappings」](https://www.remapper.org/manual/#per-device-mappings)を参照。BLEソース上の対応は[Bluetooth版main.cc](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware-bluetooth/src/main.cc)の`find_bond_cb`と`hogp_ready_work_fn`、[remapper.cc](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware/src/remapper.cc)の`device_connected_callback`で確認。旧資料の「BLEは機器別設定不可」は厳密ではなかったため訂正した。ただしBLEの番号はMACアドレスの固定登録ではなく、ペアリング削除・再登録後の同一番号を保証できない。[上流Issue #313](https://github.com/jfedor2/hid-remapper/issues/313)はMACアドレスによる設定を求める未解決の要望。

## 同時入力をどう扱うか

| 例 | 本家コアから読み取れる挙動 | 留意点 |
| --- | --- | --- |
| キーボードAでCtrlを押し、BでCを押す | 全ポート対象のマッピングなら入力が合流し、PCへCtrl+Cとして送れる | レイヤーや組み合わせを機器別に独立させる設定ではない |
| AとBが同じXを押し、先にAだけ離す | 異なるHIDインターフェースのデジタル入力はビットで集約。Bが押している間Xは押下状態を維持する設計 | 実際の入力レポート形式・出力モードで要確認 |
| Aのキーでレイヤー1を押し、Bで別キーを押す | レイヤー状態はリマッパー全体で共有。Bのキーにもレイヤー1が適用される | 機器別レイヤーが必要なら別設計 |
| USBのポート1でX→Yと指定し、ポート2ではXを無指定 | Xがどこか一つのポートで設定されると、自動パススルーの対象から外れる。ポート2のX→Xを明示しないとXが消える | [作者の説明](https://forum.remapper.org/t/per-device-mapping-issue/177)でも確認。UIで設定漏れを防ぐ余地あり |
| 2台のマウスが同時に動く | 出力は一つの仮想マウス。相対移動量はコアで合算する設計 | PCに独立した二つのカーソルは現れない。出力範囲で丸められる可能性 |

ここでの同時押し・レイヤー・相対移動は[remapper.cc](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware/src/remapper.cc)の入力ビット管理、`process_mapping`、`accumulated`からの推論。パススルーの境界は[公式マニュアル](https://www.remapper.org/manual/#unmapped-inputs-passthrough)にも明記されている。

## 接続台数、ペアリング、復帰

- **BLE版のビルド設定**は同時接続枠`CONFIG_BT_MAX_CONN=8`、保存できるペアリング情報`CONFIG_BT_MAX_PAIRED=32`。これは設定上限であり、「8台のあらゆる機種が安定同時動作」や「32台を同時接続」を意味しない。マッピング側のポート指定欄は4ビット（0〜15）なので、保存できるペアリング数と個別指定できる番号の範囲も一致しない。[prj.conf](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware-bluetooth/prj.conf)、[remapper.cc](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware/src/remapper.cc)
- BLEスキャンのアドレスフィルターも`8`。既存のペアリング機器を優先して再接続し、ペアリング操作で新規機器を探す。**9台以上を保存した場合の再接続挙動は未検証**。通信品質・消費電力・入力遅延も台数別試験が要る。[main.cc](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware-bluetooth/src/main.cc)
- XIAOではWeb設定画面かpin 0の短押しでペアリング、長押しで**全**ペアリング情報を削除。個別削除や名前を付けた接続機器管理は標準UIの要求として未確認。BLEの再接続は記述子等をキャッシュしないため時間がかかり得る。[公式Bluetooth説明](https://github.com/jfedor2/hid-remapper/blob/master/BLUETOOTH.md)
- 入力報告はBLE側で長さ16のキューに積み、満杯時はその報告を処理できない経路がある。ソースに複数機器の記述子読み取りの同時進行に関する未検証コメントもある。多数同時接続や高速入力は実測が必要。[main.cc](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware-bluetooth/src/main.cc)
- **押下中に機器が切れた場合のキー解除は要重点試験**。ソースでは切断時に機器の記述子とポート状態を消すが、共通の入力ビットを明示的に解放する処理は確認できなかった。全ポート共通設定でキーや修飾キーが押されたまま残る可能性があり、実機で再現・解消方法を確認する。[descriptor_parser.cc](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware/src/descriptor_parser.cc)、[remapper.cc](https://github.com/jfedor2/hid-remapper/blob/51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e/firmware/src/remapper.cc)
- USB側はハブと入力機器の組み合わせにより相性がある。ポート数・配線だけでなく、5 V給電容量、起動順、再挿抜を対象構成で試験する。[Pico版の説明](https://github.com/jfedor2/hid-remapper/blob/master/HARDWARE.md)

## Orbitの要件として決める項目（未決定）

1. 同時接続上限を**USB何台＋BLE何台**とするか。ハブを製品側で提供するか、外付けを許容するか。
2. USBとBLEの**同時押し・修飾キー・レイヤー**を共有するか。機器別設定が必要なら、物理USBポートと安定したBLE機器IDをどう表示・保存するか。
3. 一台の切断、再接続、電池切れ時に**その機器だけの押下状態を解除**し、ほかの機器の入力とレイヤーを維持するか。
4. 画面には接続台数だけ表示するか、個別機器の識別、切断状態、ペアリング、設定先まで扱うか。
5. 動作確認の対象機種、キーボード＋マウス／複数キーボードの組み合わせ、再接続の許容時間をどう定めるか。

これらは採用済み要件ではない。USB＋BLE混在の入力統合を実装する場合、同じキーを複数経路が押すときの参照カウントまたは送信元別ビット、切断時の解放、安定した送信元ID、キューあふれの回復を仕様に含める必要がある。
