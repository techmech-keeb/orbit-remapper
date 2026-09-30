# Orbit ファーム（M1：本家 HID Remapper を M5Dial で動かす）

初代 M5Dial（ESP32-S3）1 台で、BLE のキーボード・マウス 2 台を受け、本家 [HID Remapper](https://github.com/jfedor2/hid-remapper) のコアでリマップして、USB-C から PC へ USB HID として出すファーム。依頼書は [m1-brief.md](../../docs/drafts/2026-09-27/m1-brief.md)、設計は [implementation-design.md](../../docs/drafts/2026-09-27/implementation-design.md)。

M1 の範囲は「本家の BLE 版（XIAO nRF52840）と同じことをする」まで。機器ごとのレイヤー、設定セット、画面の UI は M2・M3。**本家コアは無改造**（`// ORBIT:` の印のある変更はゼロ。§7）。

## 1. 構成

```
BLE 機器 ──(NimBLE, CPU0)──▶ キュー ──▶ 主ループ（CPU1、本家コアを呼ぶ唯一のタスク）──▶ TinyUSB ──▶ PC
                                              │                                        HID 0：割り当て後の出力
                                              ├─ 1 ms の刻み（esp_timer）               HID 1：設定ツール
                                              └─ ログ（CDC）                             CDC  ：ログ
```

| ファイル | 内容 |
| --- | --- |
| `main/main.cc` | 起動、主ループ（本家 BLE 版 `main()` の写し）、`SUM`・`DEV`・`LAT` 行、画面 |
| `main/ble.c` | BLE の受信。Q31 の `experiments/q31-s3-two-ble/main/ble_central.c` が種。接続を始める時点で 7.5 ms、GATT サーバー有効、自分から暗号化、`esp_hidh` は使わない |
| `main/usb.cc` | USB の記述子と TinyUSB の受け口。本家 USB 版 `tinyusb_stuff.cc` が手本（boot protocol、リモートウェイクアップ） |
| `main/platform.cc` | 本家コアが求める関数（`platform.h`）、書き込みモードへの再起動 |
| `main/storage.cc` | 本家の設定の塊を NVS に保存・読み込み |
| `main/log.c` | ログ。待たずに書けるリング状のバッファ（16 KB）。主ループが CDC へ送る |
| `main/display.c`, `font5x7.c` | 文字だけの画面（Q31 から流用。M3 で LVGL に置き換える） |
| `components/hid_remapper_core/` | 本家コアを `firmware/hid-remapper/`（subtree）からそのままビルド |
| `partitions.csv` | NVS（ペアリング情報と設定）をアプリの後ろ `0x400000` に置く。`0x0` への書き込みで消えない |

## 2. ビルドと書き込み

ESP-IDF **v5.5.5**（ほかの版は未確認）。環境を読み込んでから、このディレクトリで：

```sh
idf.py build merge-bin
```

`build/merged-binary.bin` が、アドレス `0x0` に書き込む 1 ファイル（起動プログラム・パーティション表・アプリ入り）。書き込みは https://espressif.github.io/esptool-js/ （Chrome）か `esptool.py --chip esp32s3 -p <ポート> write_flash 0x0 build/merged-binary.bin`。

**本体が動いている間は、書き込みツールから自動では書き込みモードに入れない**（USB を TinyUSB が使っているため）。先に §3 の方法で書き込みモードに入れる。

### 書き込みモードに入る方法（A1）

| 方法 | やり方 | 実機 |
| --- | --- | --- |
| 1 | 設定ツール（§4）の「Reset to bootloader」 | 未確認 |
| 2 | 画面（ダイヤルの中央）を押し込んだまま RST を押す。または押したまま電源を入れる | 合格（PC 本体のポート） |
| 3 | ログの COM ポートを **1200 bps** で開いて閉じる | 合格（PC 本体のポート） |
| 4 | 背面を開け、M5StampS3 の G0 を押しながら電源を入れる（M5Stack の公式手順。ファームがどう壊れていても効く） | 合格 |

どれでも、画面に `DOWNLOAD MODE` と出て、PC に ESP32-S3 の USB シリアル（VID 303A / PID 1001）が現れる。書き込んだら RST でアプリに戻る。**USB ハブ経由では 2 で失敗した例がある**（未解決。PC 本体のポートを使う）。

## 3. 使い方

1. 書き込むと、ペアリング情報が無ければ**ペアリング待ち**で起動する（画面 3 行目 `PAIRING 0/2`、黄色）。近くでペアリング待ちにした HID 機器に自分からつなぐ。**ほかの HID 機器がペアリング待ちだと、そちらにつないでしまう。**
2. 1 台つながって暗号化できると、**以後はペアリング済みの機器だけ**を待つ（`WAIT`。ペアリング済みのアドレスを無線チップの許可リストに渡し、どれかが現れたら接続する）。2 台目を足すには、設定ツールの「Pair new device」を押す（本家と同じ）。
3. 切断されると自動でつなぎ直す。電源の入れ直し・スリープからの復帰も同じ。
4. ペアリング情報を全部消すには、設定ツールの「Clear bonds」。機器側でも Orbit のペアリングを消してから、ペアリングし直す（機器側で消し直すと、機器のアドレスが変わることがある。Q31 で MD600 が該当）。

同時接続は 2 台。番号（画面の D0・D1、ログの `D0`・`D1`）はつながった順で、起動のたびに入れ替わることがある。設定ツールの「ポート番号」は、ペアリング情報の並び順（本家 BLE 版と同じ。安定した番号は M2）。

## 4. 設定ツール

本家の Web ツール https://www.remapper.org/config/ （Chrome か Chrome 系。WebHID）で、本家の XIAO 版と同じように使える。USB の VID/PID は本家と同じ `0xCAFE` / `0xBAF2`（M1 の間。Orbit 独自の値は open-questions.md）。使い方は https://www.remapper.org/manual/ 。

設定は「Save to device」で NVS に保存され、再起動後も残る。

## 5. ログの読み方

USB の CDC（Windows では COM ポート、Linux では `/dev/ttyACM*`）に出る。速度は何でもよいが、**1200 bps で開くと書き込みモードに入る**（§2）。**DTR を立てる**（端末ソフトは通常立てる。PowerShell の `SerialPort` は `DtrEnable = $true`）。ポートを開くまでの分は 16 KB までためて後から送る。行はすべて `M1` で始まる。

```text
M1 START idf=v5.5.5 app=<git describe> upstream=51ab8b3 config_size=2048 descriptor=0 vid=cafe pid=baf2 max_devs=2 conn_itvl=6(7.50ms) lvgl_reserve=ok
M1 SUM t=12 conn=2/2 scan=0 wait=0 pairing=0 usb=mounted boot_protocol=0 heap_free=150000 heap_min=140000
M1 DEV t=12 D0 addr=..:3a:5f h=1 itvl=6(7.50ms) lat=0 to=400 enc=1 subs=3 rpt=120 maxgap=9.1ms gaps<=8/16/32/>32=100/18/1/0 total=1440 disc=0
M1 LAT t=12 n=118 avg=1.32ms max=2.10ms <=8/<=16/>16=118/0/0
M1 EVT t=3.512 D0 addr=..:3a:5f connected itvl=6(7.50ms) lat=0 to=400(4000ms)
```

（数値は書式の例で、測った値ではない。）

| 行 | 出るとき | 内容 |
| --- | --- | --- |
| `START` | 起動時と、端末が DTR を立てるたび | ESP-IDF の版、このファームの版（`git describe`。未コミットの変更があると `-dirty`）、本家のコミット、設定の大きさ、出力の記述子の番号、VID/PID |
| `SUM` | 1 秒ごと | 接続台数、探しているか、ペアリング待ちか、USB の状態（`none`／`mounted`／`suspended`）、boot protocol か、**空きメモリ（今の値と、起動してからの最小値。バイト）**。報告のキューがあふれたら `lost=` |
| `DEV` | 1 秒ごと、機器ごと | 下の表 |
| `LAT` | 1 秒ごと（送った報告があるとき） | **受信から USB 送信までの遅れ**。件数、平均、最大、8 ms 以下／16 ms 以下／それ以上の件数。測り方は §6 |
| `EVT` | そのつど | 接続、切断、暗号化、記述子の受信、通知の登録、設定の保存、設定ツールからの命令、USB のマウント・サスペンド・復帰、書き込みモードへの移行、失敗。通知の登録の後に機器の情報を読んで `info name=`／`appearance=`／`manufacturer=`／`model=`／`pnp_id:` を出す（無い機器は `none`）。記述子の行には `hash=`（内容の指紋） |
| `NVS` | 起動時 | 保存領域の使用量。区画全体の項目数（1 項目 32 バイト）と、上限（`max_bonds`＝ペアリング情報、`accept_list_max`＝許可リスト、`max_conns`＝同時接続）。続けて名前空間ごと（`orbit`＝設定、`nimble_bond`＝ペアリング情報）に、記録の種類ごとの件数と 1 件のバイト数（例 `peer_sec=2x100B`） |

`DEV` 行の項目：

| 項目 | 内容 |
| --- | --- |
| `addr` | 相手のアドレスの下位 2 バイトだけ |
| `h` | 接続の番号 |
| `itvl` | **実際の接続間隔**（1.25 ms 単位、括弧内はミリ秒） |
| `lat` / `to` | latency（間引いてよい回数）／切断とみなすまでの時間（10 ms 単位） |
| `enc` / `subs` | 暗号化しているか／通知を登録できた入力 Report の数。`subs=0` なら報告は届かない |
| `rpt` | 直近 1 秒に届いた報告の数。7.5 ms なら最大で約 133 |
| `maxgap` | 直近 1 秒の、報告と報告の間隔の最大値 |
| `gaps<=8/16/32/>32` | 報告の間隔を 8／16／32 ms 以下、32 ms 超に分けた回数。1 秒以上は数えない |
| `total` / `disc` | 起動してからの報告の合計／切断の回数 |

`EVT` 行の `hci=` は Bluetooth の規格の番号（`0x08` 通信が途絶えた、`0x13` 相手が切った、`0x16` こちらが切った、`0x12` 条件が不正）。

**画面**：1 行目に版、2 行目に USB（`OK`／`SUSP`／`--`、boot protocol なら `BOOT`）、3 行目に `PAIRING`／`SCAN`／`WAIT`／`IDLE` と台数、機器ごとに 2 行（アドレスの下位と実際の間隔：緑＝7.5 ms、黄＝違う／latency、暗号化、登録数、直近 1 秒の報告数）、`LAT`（緑＝最大 3 ms 以内）、ログの状態（`LOG DTR 3K C12`：DTR あり、3 KB 渡し、PC が 12 回受け取った）、空きメモリ、最後のイベント。

## 6. 遅れ（`LAT`）の測り方

通知が届いた時刻（`ble.c` がキューに入れるときの `esp_timer_get_time()`）から、`tud_hid_n_report()`（HID 0）を呼んだ時刻までの差。1 ms の刻み待ちを含む。本家コアは「どの入力がこの出力を生んだか」を教えないので、**コアに渡した最も新しい受信**を基準にする。送信が最後の受信から 20 ms 以上たっていれば、その受信が原因ではない（自動連打、マクロ、タップ／ホールドの時間切れなど）とみなし、`unrelated=` に数えて遅れには入れない。同じ 1 ms の中に 2 件届いたときは新しい方から測るので、その分だけ小さく出る（最大 1 ms 未満）。

以前の版（`6254d16` まで）は「まだ送信に反映されていない最も古い受信」を基準にしていたため、出力を変えない報告が続くと 7.5 ms の倍数の値が出た。

## 7. 本家コアとの関係

- 本家 `jfedor2/hid-remapper` のコミット `51ab8b3` を `firmware/hid-remapper/` に git subtree で取り込んでいる（[UPSTREAM.md](../hid-remapper/UPSTREAM.md)）。使うのは本家 BLE 版と同じ 9 ファイル。
- **本家のソースへの変更：なし。** ESP-IDF の警告設定だけ、コアのファイルに対して `-Wno-narrowing -Wno-missing-field-initializers` で緩めている。
- 本家 BLE 版との違い（本家コアの外側）：
  - 1 ms の刻みは `esp_timer`（本家 BLE 版は USB の SOF。設計の決定 2 に従った）。
  - 報告は、報告 ID を先頭に付けず `external_report_id` で渡す。本家 BLE 版は常に先頭に付けるが、報告 ID を使わない機器では 1 バイトずれる。
  - 機器からの接続条件の変更要求は受けたうえで、機器の範囲に 7.5 ms が入っていれば 1 回だけ 7.5 ms を求め直す（Q31 の `60540d8`）。2 台目では断られる見込み。
  - TinyUSB は `esp_tinyusb` を通さず直接使い、主ループで回す（`esp_tinyusb` 2.x は必ず自分のタスクで回すため）。

## 8. 未確認のこと・制約

- 合格条件の結果は [m1-results.md](../../docs/drafts/2026-09-27/m1-results.md)。A5・A6 が未確認、A8 は測り直し中（このファイルの更新時点）。
- 機器側に古いペアリングが残っていると暗号化に失敗する（`encryption failed status=0x503/0x505`）。本体はその接続を切る。機器側で Orbit のペアリングを消してやり直す。
- リモートウェイクアップ（A6）と boot protocol（A5）は本家 USB 版のコードを写したが、ESP32-S3 の TinyUSB で動くかは未確認。USB の機器の区分は複合機器（IAD）にしてあり、BIOS で使えるかも未確認。
- 数字入力が必要なペアリングには対応していない（「入出力なし」として名乗る）。
- 機器へ送る出力報告（キーボードの LED など）は M1 では送らない（本家 BLE 版と同じ）。
- USB ハブ経由で書き込みモードに入れないことがある（§2）。
- ESP32-S3 の USB は送信用の FIFO が 5 本（IN endpoint 0〜4）。HID 2 つと CDC でちょうど使い切っている。IN endpoint はこれ以上足せない。
- LVGL 用に 48 KB を起動時に確保して使っていない（決定 I3）。`SUM` の `heap_min` はこれを引いた後の値。

## 9. ライセンスと出典

- このディレクトリのコードは MIT（リポジトリの [LICENSE](../../LICENSE)）。`main/usb.cc` の記述子と HID の受け口は、本家 USB 版 `firmware/src/tinyusb_stuff.cc`（MIT、Jacek Fedorynski / Ha Thach）を手本にした。
- 本家 HID Remapper：MIT（`firmware/hid-remapper/LICENSE`）。
- ビルド時に取り込む部品（リポジトリには入れない）：ESP-IDF と NimBLE（Apache-2.0）、`espressif/tinyusb` 0.21.0（MIT）、`espressif/esp_lcd_gc9a01`（Apache-2.0）。版と検査値は `dependencies.lock`。
- 画面・ボタン・電源保持の端子番号は、M5Stack の公式ライブラリ（M5GFX、M5Unified。MIT）の M5Dial の設定から読み取った。
- ログに出すアドレスは下位 2 バイトだけ。USB のシリアル番号は MAC アドレスから作った値の置き換えで、MAC そのものではない。ペアリングの鍵は NVS にだけ残る。
