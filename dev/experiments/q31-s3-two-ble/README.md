# Q31 試験プログラム：M5Dial で BLE 機器 2 台を 7.5 ms で受けられるか

初代 M5Dial（ESP32-S3）が、BLE のキーボードとマウスの 2 台を、どちらも接続間隔 7.5 ms で受けられるかを測るための試験プログラム。仕様は [q31-experiment-brief.md](../../drafts/2026-09-26/q31-experiment-brief.md) による。

**測定は途中。** ビルドは下記の設定すべてで警告なしに通る（ESP-IDF v5.5.5）。実機（初代 M5Dial）では 2 台と 7.5 ms でつながり、報告が届くところまで確かめた。ただし Mistel MD600 Alpha が約 30 秒ごとに切れる問題を調べている（下の「30 秒ごとの切断を調べる版」）。

やること：HID 機器（サービス 0x1812）を探して最大 2 台につなぎ、暗号化して、入力の報告（0x2A4D）の通知を受け、届いた回数と間隔を 1 秒ごとに出す。報告の中身は読まない。リマップや PC への USB 出力はしない。Wi-Fi は初期化しない。

## 必要なもの

- ESP-IDF v5.5.5（ビルドを確かめた版。v5.3 以降なら動く見込みだが未確認）
- 初代 M5Dial と USB-C ケーブル
- BLE キーボード 1 台、BLE マウス 1 台

## ビルドと書き込み

ESP-IDF の環境を読み込んでから（`. $IDF_PATH/export.sh`、Windows は「ESP-IDF PowerShell」など）、このディレクトリで実行する。

```sh
idf.py build
idf.py -p <ポート> flash monitor
```

- `<ポート>` は Linux なら `/dev/ttyACM0`、Windows なら `COM3` など。
- 画面の部品（`espressif/esp_lcd_gc9a01`）は、初回のビルドのときに自動で `managed_components/` へ取り込まれる。
- ログをファイルに残すには、`monitor` の画面で `Ctrl+T` → `Ctrl+L` を押す（もう一度押すと止まる）。`idf.py -p <ポート> monitor --timestamps` にすると、行ごとに PC の時刻が付く。

### T7 用（接続後に 7.5 ms を要求する方式）

設定を分けたいので、ビルド先のディレクトリを別にする。

```sh
idf.py -B build-after -DSDKCONFIG=build-after/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.after_connect" build
idf.py -B build-after -p <ポート> flash monitor
```

### 30 秒ごとの切断を調べる版（GATT サーバー入り）

見立て：このプログラムは機器を受ける側（central）の役だけでビルドしていて、NimBLE の GATT サーバーが入っていない。この状態の NimBLE は、機器から届いた ATT の要求（MTU の交換、機器名やサービス一覧の読み出しなど）を、エラー応答も返さずに捨てる（ESP-IDF v5.5.5 の `ble_att.c`、`ble_att_rx_extended()` と `ble_att_rx_handle_unknown_request()` で確認）。規格では、要求を出した側は応答を 30 秒待ち、来なければ接続を切る。MD600 が実際に要求を出しているかは**未確認**で、下の `PKT` 行で確かめる。

GATT サーバーを入れ、標準の GAP・GATT サービス（機器名など）を登録した版は、次のようにビルドする。広告（advertising）はしないので、ほかの機器からつながれることはない。

```sh
idf.py -B build-gatts -DSDKCONFIG=build-gatts/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.gatt_server" build
```

どちらの版かは、`START` 行の `gatts=0`（GATT サーバーなし）／`gatts=1`（あり）で見分ける。

### そのほかの設定

`idf.py menuconfig` → 「Q31 experiment」で変えられる。

| 項目 | 既定値 | 内容 |
| --- | --- | --- |
| How to request the connection interval | In the connection request | 接続を始める時点で指定する（T1〜T6）か、接続後に要求する（T7）か |
| Requested connection interval | 6（7.5 ms） | 1.25 ms 単位 |
| Supervision timeout | 400（4 秒） | 10 ms 単位。この時間通信がなければ切断とみなす |
| Connection event length | 0（コントローラーの既定） | 1 回の通信に使う時間の長さ（0.625 ms 単位）。ESP32-S3 がこの値に従うかどうかも未確認 |
| Log ATT, SMP and L2CAP signalling packets | 有効 | 機器とやり取りした ATT・SMP・L2CAP 制御の命令を `PKT` 行に出す（下の「ログの読み方」） |
| Parameter updates requested by the device | Accept, then ask again for our interval if the device allows it | 機器から接続条件の変更を求められたときの扱い。既定では受けたうえで、機器が示した範囲に要求の間隔（7.5 ms）が入っていれば、latency と timeout は機器の希望のまま、間隔だけ 7.5 ms にするよう 1 回だけ求め直す（`EVT` 行に `asking for it again`）。meteorite40 は、そのまま受ける動作の版で 15 ms・latency 30 になった（2026-09-27、画面で確認）。ZMK の既定の希望「7.5〜15 ms、latency 30」を受けて NimBLE が 15 ms を選んだと見ているが、要求の中身はログで要確認。ほかに「そのまま受ける」「断る（機器が切断することがある）」を選べる。どれでも要求の内容はログに出す |

## 使い方

1. 書き込むと、すぐに探し始める（画面に `SCAN`）。
2. キーボードかマウスをペアリング待ちの状態にする。HID 機器を見つけると、自分から接続・ペアリングする。**近くにペアリング待ちの HID 機器がほかにあると、そちらにつないでしまう**ので、試す機器だけをペアリング待ちにする。
3. 2 台つながると探すのを止める（画面に `IDLE 2/2`）。
4. 切断されると探すのを再開し、保存したペアリング情報を使って自動でつなぎ直す。

**ペアリング情報を消す**：画面（ダイヤルの中央）を押し込んだまま、RST ボタンを押して起動する。ログに `bonds cleared` が出る。

### T1〜T7 との対応

| 番号 | このプログラムでの手順 |
| --- | --- |
| T1 / T2 | ペアリング情報を消してから、1 台だけをペアリング待ちにする |
| T3 / T4 | 1 台目がつながって `DEV` 行の `subs` が 1 以上になってから、2 台目をペアリング待ちにする。2 台目以降は、つないだ順に D0、D1 の番号が付く |
| T5 | T3 の状態のまま、10 分以上ログを残す。`total` と `disc` を見る |
| T6 | 機器の電源を入れ直す／スリープさせて戻す。`EVT` 行の `disconnected` と、つなぎ直した後の `DEV` 行の `itvl` を見る |
| T7 | 上の「T7 用」でビルドし直して T3 を行う |

## ログの読み方

ログはすべて `Q31` で始まる。`grep '^Q31'` で抜き出せる。時刻 `t` は起動からの秒数。

```text
Q31 START idf=v5.5.5 app=<コミット> mode=at-connect itvl_req=6(7.50ms) to=400 ce_len=0 peer_update=keep-itvl gatts=0 pkt_log=1 clear_bonds=0
Q31 SUM t=12 conn=2/2 scan=0 connecting=0
Q31 DEV t=12 D0 addr=..:3a:5f h=1 itvl=6(7.50ms) lat=0 to=400 enc=1 subs=3 rpt=120 maxgap=9.1ms gaps<=8/16/32/>32=100/18/1/0 total=1440 disc=0
Q31 EVT t=3.512 D0 addr=..:3a:5f connected itvl=6(7.50ms) lat=0 to=400(4000ms)
Q31 PKT t=3.590 h=1 rx ATT 0x02 MTU_REQ len=3 [05 02]
Q31 PKT t=4.600 h=1 NO ANSWER from us to the device's MTU_REQ for 1.0s
```

（上の数値は書式の説明のための例で、測った値ではない。）

| 行 | 出るとき | 内容 |
| --- | --- | --- |
| `START` | 起動時に 1 回 | ESP-IDF の版、プログラムの版（`git describe` の結果。コミットしていない変更があると `-dirty` が付く）、設定 |
| `SUM` | 1 秒ごと | つながっている台数、探しているか、接続を試みている最中か。空きがあるのにどちらでもなければ、探すのをやり直す（失敗したら `EVT` 行に `scan start failed` と理由が出る） |
| `DEV` | 1 秒ごと、機器ごと | 下の表 |
| `EVT` | そのつど | 接続、切断、接続条件の変更、機器からの変更要求、暗号化、通知の登録、MTU の決定（`mtu=`）、失敗 |
| `PKT` | そのつど（0.05 秒ごとにまとめて出す） | 機器とやり取りした ATT・SMP・L2CAP 制御の命令。下の表。`pkt_log=1` の版だけ |

`DEV` 行の項目：

| 項目 | 内容 |
| --- | --- |
| `addr` | 相手のアドレスの下位 2 バイトだけ |
| `h` | 接続の番号 |
| `itvl` | **実際の接続間隔**（1.25 ms 単位。括弧内はミリ秒）。`ble_gap_conn_find()` で得た値 |
| `lat` / `to` | latency（間引いてよい回数）／切断とみなすまでの時間（10 ms 単位） |
| `enc` / `subs` | 暗号化しているか／通知を登録できた Report の数。`subs=0` なら報告は届かない |
| `rpt` | 直近 1 秒に届いた報告の数。7.5 ms なら最大で約 133 |
| `maxgap` | 直近 1 秒の、報告と報告の間隔の最大値 |
| `gaps<=8/16/32/>32` | 報告の間隔を 8 ms 以下／16 ms 以下／32 ms 以下／32 ms 超 に分けた回数。7.5 ms で毎回届いていれば、ほぼすべて「8 ms 以下」に入る。1 秒以上の間隔は、操作していなかっただけとみなして数えない |
| `total` / `disc` | 起動してからの報告の合計／切断の回数（D0・D1 の枠ごと） |

`PKT` 行の項目：

| 項目 | 内容 |
| --- | --- |
| `h` | 接続の番号。`DEV` 行の `h` と同じなので、どの機器かはそこで対応させる |
| `rx` / `tx` | 機器から届いた／こちらから送った |
| `ATT` / `SMP` / `SIG` | GATT のやり取り／ペアリング／L2CAP の制御（接続条件の変更要求など） |
| `0x02 MTU_REQ` など | 命令の番号と名前 |
| `len` | 中身の長さ（バイト） |
| `[...]` | 中身の先頭。機器からの ATT の要求（何を問い合わせたか。例：`READ_BY_GROUP_TYPE_REQ [01 00 ff ff 00 28]` はサービス一覧の問い合わせ）、こちらのエラー応答（`ERROR_RSP [要求の番号 対象 対象 理由]`）、ペアリングの条件（`PAIRING_REQ`／`PAIRING_RSP` の 6 バイト：入出力の能力、OOB、認証の条件、鍵の長さ、配る鍵の種類 2 つ）、`PAIRING_FAILED` の理由だけ。鍵やアドレスが入るパケットは中身を出さない |
| `answered ... after` | 機器からの ATT の要求に、こちらが何 ms で答えたか |
| `NO ANSWER from us to the device's ...` | 機器からの ATT の要求に、1 秒たってもこちらが答えていない。30 秒続くと、機器が切る見込み |

入力の報告（通知）は数が多いので `PKT` 行には出さない（`DEV` 行の `rpt` で数える）。記録が追いつかなかったときは `PKT lost N packet record(s)` が出る。

`EVT` 行の失敗の理由は、`status` や `reason` に NimBLE の値を、`hci=` に Bluetooth の規格で決まった番号を出す（`hci=-1` は Bluetooth の番号ではない失敗）。よく出そうなもの：`0x08` 通信が途絶えた、`0x13` 相手が切った、`0x16` こちらが切った、`0x3e` 接続を始められなかった、`0x12` 条件が不正（先例で 15 ms より短い要求を断られたときの値）。

画面には同じ要点を出す：探している（`SCAN`）／接続を試みている（`CONN`）／どちらでもない（`IDLE`）、台数、機器ごとのアドレスの下位・実際の間隔（緑＝要求どおり、黄＝違う）・latency・timeout（ミリ秒）・通知を登録できた数（`S`）・直近 1 秒の報告数（`R`）と最大間隔（`MAX`）、最後のイベント。

## 作りについて

| ファイル | 内容 |
| --- | --- |
| `main/ble_central.c` | 探す・つなぐ・暗号化・GATT の探索・通知の登録・報告の計数。NimBLE の GAP と GATT を直接使い、`esp_hidh` は使わない（prior-art.md の不具合の表を避けるため） |
| `main/pkt_log.c` | `PKT` 行。NimBLE とコントローラーの間の送受信口（受信 `ble_transport_to_hs_acl_impl()`、送信 `esp_vhci_host_send_packet()`）をリンク時に差し替えて（`--wrap`、`main/CMakeLists.txt`）、命令の番号だけを記録する。NimBLE のログ水準を上げると機器の完全なアドレスや鍵まで出るため、その方法は使わない |
| `main/main.c` | 起動、1 秒ごとのログと画面。ログと画面は core 1 で動かし、BLE（core 0）の邪魔をしないようにしている |
| `main/display.c`, `main/font5x7.c` | 丸い画面（GC9A01）への文字表示。字の形はこのプロジェクトで作った |
| `main/Kconfig.projbuild` | 上の「そのほかの設定」 |

先例（prior-art.md）への対応：

- §4.33：接続を始める時点で 7.5 ms を指定する。比較用に、接続後の要求にも切り替えられる（T7）。
- §4.29：接続したら自分から暗号化を始める。5 秒たっても暗号化されなければ、そのまま通知の登録を試みる（`EVT` 行に `no encryption after 5 s`）。後から暗号化され、登録できた数がまだ 0 なら、登録をやり直す。
- §4.20：スリープから戻った機器が HID の識別子を出さずに接続を求めても、保存したアドレスと一致するか、こちら宛ての接続要求であればつなぐ。

## 未確認のこと・制約

- 実機で確かめたこと（2026-09-27、初代 M5Dial）：Mistel MD600 Alpha と meteorite40 の 2 台と 7.5 ms でつながり、報告が届く。MD600 は約 30 秒ごとに相手から切られる（`hci=0x13`）。原因は調査中。
- `PKT` 行の仕組みと GATT サーバー入りの版は、ビルドと PC 上での読み取りの試験だけで、実機では未確認。
- 数字入力が必要なペアリング（キーボードで 6 桁を打つ方式）には対応していない。こちらは「入出力なし」として名乗るので、通常は確認なしのペアリングになる見込み。
- 機器がアドレスを定期的に変える方式（プライバシー機能）を使っている場合、HID の識別子を出さずに接続を求められると、見つけられないことがある。
- ペアリング情報は最大 3 台分保存する（ESP-IDF の既定）。

## ライセンスと出典

- このディレクトリのコードは MIT（リポジトリの [LICENSE](../../../LICENSE)）。
- ビルドに使う物（リポジトリには入れない）：ESP-IDF と NimBLE（Apache-2.0）、ビルド時に取り込む `espressif/esp_lcd_gc9a01` と `espressif/cmake_utilities`（Apache-2.0。版と検査値は `dependencies.lock`）。
- 画面・ボタン・電源保持の端子番号は、M5Stack の公式ライブラリ（M5GFX の `src/M5GFX.cpp`、M5Unified の `src/M5Unified.inl`、どちらも MIT。2026-09-26 確認）の M5Dial の設定から読み取った。コードは写していない。
- ESP-IDF の見本（blecent など）のコードは写していない。
- ログに出すアドレスは下位 2 バイトだけ。ペアリングの鍵は M5Dial の内部（NVS）にだけ残る。`sdkconfig` とビルドの生成物は `.gitignore` で除いている。
