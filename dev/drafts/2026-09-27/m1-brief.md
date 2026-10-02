---
status: draft
snapshot_date: 2026-09-27
finalized: false
---

# M1 の依頼書：本家 HID Remapper を M5Dial（ESP32-S3）でそのまま動かす

別のセッション（クラウドの Claude など）に M1 を作ってもらうための依頼書。設計は [implementation-design.md](implementation-design.md)、決定は [requirements.md](../2026-09-26/requirements.md) の H5 と I1〜I7。会話は日本語、平易な表現で、結論を先に。事実と推測を分け、確かめていないことは「未確認」と書く。

## 1. 目的と範囲

**目的**：本家 HID Remapper の BLE 版（XIAO nRF52840）と同じことを、初代 M5Dial 1 台でできるようにする。BLE のキーボード・マウス 2 台を受け、本家コアでリマップし、M5Dial の USB-C から PC へ USB HID として出す。PC の設定ツール（本家の Web ツール）で設定できる。

**この段階でやらないこと**（M2・M3 の範囲）：機器ごとのレイヤー、切断時の解除、機器の安定した番号、設定セット、ペアリング管理の UI、LED の送り返し、画面の UI。**本家コアは改造しない**（ビルドを通すための最小限の修正を除く。修正には `// ORBIT:` の印を付ける）。

「本家と差分ゼロの基準点」を作るのが M1 の狙い。あとで問題が出たとき、移植のせいか改造のせいかを切り分けられるようにする。

## 2. 先に読むもの

1. [implementation-design.md](implementation-design.md) §2（本家の構造）、§3（層）、§5（決定 1・2・3・6）
2. [q31-results.md](q31-results.md) §3・§5・§7（GATT サーバーが必要、接続時に 7.5 ms を指定、書き込みでペアリング情報が消える）
3. `dev/experiments/q31-s3-two-ble/`（README と `main/ble_central.c`。BLE 受信の種）
4. [prior-art.md](../2026-09-26/prior-art.md) の `esp_hidh` の不具合の表（使わない理由）
5. 本家のソース `jfedor2/hid-remapper` コミット `51ab8b3`：`firmware/src/platform.h`、`firmware-bluetooth/src/main.cc`（主ループの手本）、`firmware/src/tinyusb_stuff.cc` と `firmware/src/main.cc`（USB の手本）

## 3. 置き場所と本家コードの取り込み（決定 I1）

- 置き場所：`firmware/orbit/`（ESP-IDF のプロジェクト）。本家コアは `firmware/hid-remapper/` に **git subtree** で取り込む（本家 `51ab8b3`、prefix は本家リポジトリ全体でよい。使うのは `firmware/src` の下のコアだけ）。
- `firmware/orbit/` から本家コアをコンポーネントとして参照する。使うファイルは本家 BLE 版の `CMakeLists.txt` と同じ：`config.cc`、`crc.cc`、`descriptor_parser.cc`、`globals.cc`、`interval_override.cc`、`our_descriptor.cc`、`quirks.cc`、`remapper.cc`、`ps_auth.cc`。
- 本家のライセンス表記（MIT）を残す。取り込んだコミットを `firmware/hid-remapper/UPSTREAM.md` などに書く。
- ESP-IDF は **v5.5.5**（Q31 で確かめた版）。

## 4. 作るもの

本家 BLE 版の `main.cc`（約 1,000 行）の ESP-IDF 版を書く。`platform.h` が要求する関数と、コアが呼ぶ数個の関数を用意する。

### 4.1 USB の機器側（TinyUSB）

- 本家 USB 版 `tinyusb_stuff.cc` を手本に、**HID 2 つ＋CDC 1 つの複合機器**にする。
  - HID 0：本家の出力用の記述子（`our_descriptors[]`。設定で 6 種類から選ぶ。F4-1）
  - HID 1：設定ツール用の窓口（Usage Page 0xFF00 の記述子。本家の Web ツールはこれを探す）
  - **CDC：ログ用。** ESP32-S3 の USB は 1 つしかなく、TinyUSB が使うと USB Serial/JTAG のコンソールは使えなくなる。ログはこの CDC に出す。
- VID/PID は **M1 では本家と同じ 0xCAFE / 0xBAF2** にする（設定ツールをそのまま使うため）。Orbit 独自の値は後で決める（open-questions に追加すること）。
- boot protocol（BIOS 対応、F4-2）：本家 USB 版と同じく `boot_protocol_keyboard` に応じて記述子を切り替える。
- リモートウェイクアップ（F4-3）：本家 USB 版 `do_send_report()` と同じく、`tud_suspended()` のときは `tud_remote_wakeup()`。TinyUSB の ESP32-S3 版で動くかは**未確認**なので、動かなければその旨を書く。
- **書き込みモードに戻る手段を必ず用意する。** TinyUSB が USB を使っていると、ブラウザの書き込みページや esptool が自動で書き込みモードに入れられない。次の 2 つを用意する：
  1. `reset_to_bootloader()`（設定ツールの「Reset to bootloader」から呼ばれる）で、ROM の書き込みモードに再起動する（`RTC_CNTL_OPTION1_REG` の `FORCE_DOWNLOAD_BOOT` を立てて再起動）。
  2. 起動時に画面のボタン（GPIO42、押すと L）を押したままなら、同じく書き込みモードに入る。Q31 では「ボタン＋RST でペアリング情報を消す」に使っていたので、**ペアリング情報の消去は設定ツールの機能（本家の `clear_bonds`）に任せ、ボタンは書き込みモードに割り当てる。**
  これがないと、書き込み直せなくなる。**最初に実機で確かめてから先へ進む。**

### 4.2 BLE の受信（NimBLE）

- Q31 の `ble_central.c` を種にする。次を必ず引き継ぐ：接続を始める時点で `itvl_min = itvl_max = 6`（7.5 ms）、latency 0、timeout 4 秒／**GATT サーバー有効**（`gatts=1` の設定）／自分から暗号化／`esp_hidh` は使わない／ペアリング情報は NVS に保存／切断されたら自動で再接続。
- 本家 BLE 版 `main.cc` と同じ役割を足す：
  - 機器の Report Map（レポート記述子）を読んで `descriptor_received_callback()` に渡す。`interface` は本家 BLE 版と同じく「接続スロット番号 << 8 | レポートの番号」、`hub_port` は本家 BLE 版と同じくボンド一覧の順（M1 では本家のまま。安定した番号は M2）。
  - 入力の報告（通知）は**キューに入れるだけ**。主ループが取り出して `handle_received_report()` に渡す（決定 I2）。キューに入れるとき `esp_timer_get_time()` の時刻を添える（4.6 の遅れの計測に使う）。
  - 切断は `device_disconnected_callback()`。
  - `pair_new_device()`（設定ツールの「Pair new device」）：ペアリング待ちの新しい機器を探して接続する。`clear_bonds()`：ペアリング情報を全部消す。
- 機器からの接続条件の変更要求は、Q31 の `60540d8` の動作（受けたうえで、機器の範囲に 7.5 ms が入っていれば 1 回だけ求め直す）を引き継ぐ。ただし q31-results.md §5 のとおり、2 台目では断られる見込み。断られたらログに出すだけでよい。
- 同時接続は 2 台（`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=2`、F1-3）。

### 4.3 保存

- `do_persist_config()`：本家の設定の塊を NVS に保存する（キーは 1 つ。設定セットは M2）。起動時に読んで `load_config()` → `set_mapping_from_config()`。
- NimBLE のペアリング情報も NVS（Q31 と同じ `CONFIG_BT_NIMBLE_NVS_PERSIST=y`）。

### 4.4 主ループとタスクの配置（決定 I2）

- 本家 BLE 版の `main()` の while ループを写す：キューから報告を取り出して `handle_received_report()` → 1 ms の刻みが来ていれば `process_mapping(true)` → `send_report()` → `send_monitor_report()`。切断・記述子・PC からの設定（`handle_set_report1` など）もキュー経由で主ループが処理する。
- **コアを呼ぶのはこのタスクだけ。** NimBLE のコールバック、TinyUSB のコールバック、ログのタスクからはコアを呼ばない。
- 主ループのタスクは **CPU1** に固定。NimBLE のホストは **CPU0**（Q31 と同じ）。TinyUSB のタスクは ESP-IDF の `esp_tinyusb` の既定でよい。
- 1 ms の刻みは `esp_timer`（周期 1 ms）。コールバックの中では旗を立てるだけにして、主ループで `process_mapping()` を呼ぶ。
- `my_mutexes_init()` などは FreeRTOS のミューテックスで。`get_time()` は `esp_timer_get_time()`（本家はマイクロ秒）。`get_unique_id()` は eFuse の MAC などから。GPIO 関係（`get_gpio_valid_pins_mask()`、`set_gpio_inout_masks()`）は M1 では「使えるピンなし」でよい。`flash_b_side()` は空でよい。

### 4.5 画面

M1 では **Q31 の `display.c`（文字だけ）を流用して、接続台数と接続間隔だけ出す**。LVGL はまだ入れない。ただし決定 I3 のため、**LVGL 用に 48 KB を起動時に確保して使わないまま持っておく**（`heap_caps_malloc` で DMA 可能な内部メモリから。確保できなければログに出す）。

### 4.6 ログ（CDC に出す）

Q31 と同じ形式にそろえる。すべて `M1` で始める。

| 行 | 内容 |
| --- | --- |
| `START` | ESP-IDF の版、`git describe`、本家のコミット、設定 |
| `SUM`（1 秒ごと） | 接続台数、USB の状態（マウント済み／サスペンド）、boot protocol か、**空きメモリ（今の値と、起動してからの最小値。`esp_get_free_heap_size()` と `esp_get_minimum_free_heap_size()`）** |
| `DEV`（1 秒ごと、機器ごと） | Q31 と同じ（下位アドレス 2 バイト、実際の接続間隔、latency、timeout、暗号化、登録した通知の数、直近 1 秒の報告数、最大間隔、切断回数） |
| `LAT`（1 秒ごと） | **受信から USB 送信までの遅れ**：通知を受けた時刻（4.2 でキューに添えた値）から、その報告を反映した `tud_hid_n_report()` を呼んだ時刻までの差。直近 1 秒の平均・最大と、8 ms 以下／16 ms 以下／それ以上の件数。1 ms の刻み待ちを含む値になる |
| `EVT` | 接続、切断、暗号化、記述子の受信、設定の保存、設定ツールからの命令、USB のマウント／サスペンド、失敗 |

機器の完全なアドレスは出さない（下位 2 バイトだけ）。M5Dial 自身の MAC アドレスも出さない。

## 5. 合格の条件（M1）

すべて**実機で**確かめる。利用者の PC で試すものは、依頼のときにそう書く。

| 番号 | 条件 | 誰が確かめるか |
| --- | --- | --- |
| A1 | 書き込みモードに戻れる（4.1 の 2 つの方法）。**これを最初に確かめる** | 利用者 |
| A2 | MD600 と meteorite40 の 2 台が 7.50 ms・latency 0 でつながり、キーとトラックボールが PC で動く | 利用者 |
| A3 | 本家の Web 設定ツール（WebHID）で本体が見え、割り当てを変えて保存し、再起動後も残る | 利用者 |
| A4 | 「Pair new device」「Clear bonds」「Reset to bootloader」が設定ツールから効く | 利用者 |
| A5 | BIOS（または UEFI の設定画面）でキーボードが使える（F4-2） | 利用者 |
| A6 | PC をスリープさせ、キーで復帰する（F4-3）。動かなければ理由を書く | 利用者 |
| A7 | 2 台を動かし続けて 10 分、切断なし・7.5 ms のまま（Q31 の T5 の「操作を続けたまま」版） | 利用者 |
| A8 | `LAT` の最大が **3 ms 以内**（決定 I6、Q33）。超えるなら内訳を調べて書く | 両方 |
| A9 | `SUM` の空きメモリの最小値を記録し、**LVGL 用の 48 KB を確保した状態で 50 KB 以上**残る（決定 I3、Q32）。残らないなら何が大きいかを書く | 両方 |
| A10 | 電源の入れ直しとスリープからの復帰で自動再接続（Q31 の T6 と同じ） | 利用者 |

## 6. 渡すもの

- ブランチ（最新の `main` から切る）。1 テーマ 1 コミット、author は `techmech <88352328+techmech-keeb@users.noreply.github.com>`。PR は作るがマージは利用者が判断する。
- `firmware/orbit/README.md`：ビルドと書き込みの手順、設定ツールの使い方（本家の URL）、ログの読み方（4.6 の表）、書き込みモードに戻る手順、未確認のこと。
- 書き込み用の `.bin`（アドレス 0x0 の 1 ファイル。ファイル名に `git describe` の結果を入れる）。**リポジトリには入れず**、利用者に直接渡す。
- 結果の報告：5 の表を埋める。測った値と推測を分ける。

## 7. 守ること

- 本家コアを改造しない。やむを得ない修正は最小にして `// ORBIT:` の印を付け、README に列挙する。
- `esp_hidh` を使わない。BTstack 由来のコードを持ち込まない（ライセンス）。
- 他人のコードを持ち込むときはライセンスを確かめて出典を書く。ビルドの生成物（`build/`、`sdkconfig`、`managed_components/`）はコミットしない。
- ログに完全なアドレス、鍵、個人の情報を出さない。
- `main` に直接 push しない。

## 8. 決めてよいこと・聞いてほしいこと

- **決めてよい**：ファイルの分け方、タスクの優先度の数値、キューの長さ（本家 BLE 版は 16。足りなければ増やしてログに `lost` を出す）、CDC のログの出し方。
- **聞いてほしい**：本家コアの修正が必要になったとき（何をなぜ）／リモートウェイクアップや boot protocol が TinyUSB の ESP32-S3 版で動かないとき／空きメモリが足りないとき（設定の上限を下げるか、LVGL の確保を減らすか）／VID/PID を本家のままにしておけない事情が出たとき。
