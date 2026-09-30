---
status: draft
snapshot_date: 2026-09-27
finalized: false
---

# セッション引き継ぎ：M1（本家 HID Remapper を M5Dial で動かす）の途中経過

M1 を作っているクラウドのセッションから、利用者の PC で動く Claude Desktop のセッションへ引き継ぐための文書。会話は日本語、平易な表現で、結論を先に書く。事実（実機やログで見た値）と推測を分け、確かめていないことは「未確認」と書く。

## 0. ローカルのセッションへの伝言（最新。ここだけ読めば次の作業ができる）

- `747c13d` の短い確認（[報告](../2026-09-27/reports/m2-747c13d-report.md)）：合格。`heap_min` は 88.0 KB（+3.5 KB、見込みどおり）。「`ledger version 1 is not 2`」が出なかったのは報告の読みのとおり（`102144d` は版の番号を保存していないので 0 と読める。作りどおりで、動きは正しい）。
- **保存のときの入力の遅れ**：報告の案 1〜4 をそのまま採った（→ `2b27551`）。台帳の変更は印を付けるだけにし、主ループが「最後の入力から 1 秒空いた」か「印から 10 秒たった」ときに 1 回だけ書く。`forget`・`move` と書き込みモードに入る前は即書く。設定の保存（`persist_config`）は変えない。別タスクへの移動とフラッシュのサスペンドはやらない（案 4 のとおり。フラッシュ書き込み中はキャッシュが止まり全タスクが止まるので、別タスクにしても遅れは残る）。書くたびに `ledger saved (N B, X ms, why=...)` を出す。
- **新しい版 `2b27551`**（`orbit-m2-2b27551-ble.bin`。名前と SHA-256 は利用者が貼る）。`747c13d` との違いは台帳を書く時機だけ。台帳の形式は同じ（別名は残る）。
- **試験（短い。報告の「確かめ方の提案」のとおり）**：
  1. 書き込み → 起動後、`ledger saved (... why=touch)` などの行が **接続の直後ではなく、入力が止まってから**（または 10 秒後）に出ること。1 起動あたりの保存の回数と `ms` を写す（2 台つないで 1〜2 回の見込み）。
  2. RST の直後から IST を動かし続ける（30 秒ほど）→ つながった直後の秒の `LAT` 最大が 3 ms 以内で、`ledger saved` が入力の止まった後に出ること。
  3. `orbit alias 2 pedal` → すぐ `ledger saved (... why=set_alias)` が 1 秒ほどで出ること（入力していなければ）。`orbit alias 2` で戻す。
  4. `orbit forget 2` → 直後に `ledger saved (... why=forget, forced)`。`orbit pair` → Cube Turner をペアリングし直し → ポート 2 で動くこと。
  5. 3 分の使用で切断 0、`LAT` 最大 3 ms 以内。
- 合格なら **PR #27 はマージできる状態**（マージは利用者が判断する）。次は 2b。
- **行う場合（MD600 があるとき）**：B5・B6 は下の手順のまま。
- **注意**：ペアリング情報の上限が 4 → 15 になったので `sdkconfig` が変わる。書き込みは今までどおり 1 ファイル（NVS は消えない）。M1 のペアリング情報 2 件は、最初の起動で台帳に足される（`ledger: bond ... had no row, added as port N`。IST が 1、Cube Turner が 2 のはず＝M1 の `hub_port` と同じ）。
- **命令の打ち方**：ログを取っている COM ポートに 1 行ずつ送る（改行で確定）。返事は `M1 CMD` と `M1 LDG` の行。
- **行う場合（MD600 があるとき。B5・B6 の本番）**：
  - B5：Pair new device で MD600 をペアリング（ポート 3）→ `orbit alias 3 md600` → MD600 をリセット（アドレスが変わる）→ Pair new device で MD600 をもう一度ペアリング → `ledger: looks like port 3 (...), taking over its port` が出て、ポート 3・別名 `md600` のまま動くこと。`orbit list` で行が 3 つ（増えていない）。
  - B6：MD600 側で Orbit のペアリングを消す（アドレスが変わらない手順があれば）→ MD600 の電源を入れ直す → `approval wanted: device has no key for us; press the button within 60 s` → 画面を短く押す → `approved: port 3 ... may pair again` → MD600 が新しい鍵でつながり `new key accepted (approved by the user)`。もう一度同じことをして、今度は押さずに 60 秒待つ → `approval for port 3 not given within 60 s (1 of 3)`。
- **見てほしいこと**：台帳の保存（`ledger save failed` が出ないこと）、`heap_min`（台帳で約 4 KB 増える見込み）、命令を打っている間に `LAT` が増えないか。
- 未確認のまま：Cube Turner の LED の意味、スリープからの復帰（Cube Turner は眠らない）、機器側で鍵を捨てたとき、A5（BIOS）、MD600・meteorite40 での `a0d8f9e` 以降の回帰（下の手順）。
- 報告の形は §7。`M1` の行は全部ファイルに残す。報告のファイルは `docs/drafts/2026-09-27/reports/` に置いてよい。クラウドとローカルのセッションは直接はやり取りできない。

### MD600・meteorite40 の回帰確認（行う場合）

1. Pair new device で MD600 と meteorite40 をペアリング（IST と Cube Turner はそのまま。ボンドは 4 件までなので、超えるなら Forget all devices から）。
2. M5Dial の RST を 2 回。2 台が `(stored key)` でつながり、5 秒以内にそろうこと。
3. 機器の電源の入れ直しを各 1 回。0.5〜1 秒でつながり直すこと。
4. 5 分の使用で切断 0、`LAT` 最大 3 ms 以内、`WARNING` 0 回。
5. `encryption on` の行の末尾がどれかを報告する（何も無し／`before the connect event`／`found by polling`）。

## 1. 結論（2026-09-27 時点）

- M1 は**途中**。依頼書 [m1-brief.md](m1-brief.md) の 4.1（PC 側の USB と書き込みモードへの戻り方）まで作った。**Bluetooth の受信（4.2）、`LAT` などのログ（4.6）、画面（4.5）の本番はまだ。**
- 合格条件 A1（書き込みモードに戻れる）は、版 `3662016` で**合格**した。このときは PC 本体のポートでだけ成功し、USB ハブ経由では失敗した（§4）。**`1fd6c6a`（2026-09-29）ではハブ経由でも成功し、再現しなかった。** 9 月 28 日以降の試験はすべてハブ経由（Genesys GL850）。
- 版 `1e37eea` で、**ログが届く・ボタン＋RST・1200 bps の 3 つとも合格**した（§4.3）。
- 最新の **`86a2d16`** に Bluetooth の受信（依頼書 4.2）、`DEV`・`LAT` 行（4.6）、画面の機器表示（4.5）が入り、依頼書の「作るもの」は一通りそろった。**実機では未確認。** 次は A2〜A10 の試験（§6）。使い方とログの読み方は `firmware/orbit/README.md`。

## 2. 役割の分け方

| 誰が | 何をする |
| --- | --- |
| クラウドのセッション | コードを書く、ESP-IDF v5.5.5 でビルドする、書き込み用の `.bin` を作って利用者に渡す、コミットと push |
| ローカルの Claude Desktop | `.bin` の書き込みの手順を案内する（書き込みは利用者がブラウザで行う）、COM ポートからログを取る、PC 側の USB の見え方を調べる（PowerShell の `Win32_PnPEntity` など）、結果を報告にまとめる |
| 利用者 | 書き込み、機器の操作（ボタン、RST、ペアリング、キー入力）、判断 |

**ローカルの PC には ESP-IDF を入れない方針**（利用者の決定）。コードを直したいときは、何をなぜ直すかを報告にまとめ、クラウドのセッションに頼む。

## 3. リポジトリの状態

- リポジトリ：`techmech-keeb/orbit-remapper`、ブランチ `claude/laughing-volta-r6lpn4`（最新の `main` の `b946b8f` から切り直した）。**PR はまだ作っていない**（依頼では PR は作るがマージしない。M1 の区切りで作る予定）。
- コミット（古い順）：

| コミット | 内容 |
| --- | --- |
| `e5670a1` | 本家 `jfedor2/hid-remapper` の `51ab8b3` を `firmware/hid-remapper/` に subtree で取り込み（履歴ごと。決定 I1） |
| `cc64180` | `firmware/hid-remapper/UPSTREAM.md`：取り込んだ本家の版と更新の手順 |
| `13d5248` | `firmware/orbit/`：M1 の骨組み。USB（HID 2 つ＋ログ用 CDC）、本家コア、書き込みモードへの戻り方 2 つ、保存（NVS） |
| `3662016` | 3 つ目の戻り方：ログの COM ポートを 1200 bps で開く |
| `db94be3` | USB の切り替えの前後で、USB シリアルを切り離して待つ（ハブ対策）。**`b2e7ad7` で取り消し** |
| `728105c` | ログを主ループから CDC へ送る。画面にログの状態を出す |
| `59559e3` | この引き継ぎ文書 |
| `b2e7ad7` | `db94be3` の取り消し。ボタン＋RST の経路は `3662016` と同じに戻った |
| `1e37eea` | USB の送信口（IN endpoint）の番号を 0x84 以下に収める。画面に CDC の転送完了数 `C` を出す |
| `86a2d16` | Bluetooth の受信（`main/ble.c`）、主ループのキュー、`DEV`・`LAT` 行、画面の機器表示 |

- 本家コアは**無改造**（`// ORBIT:` の印のある変更はゼロ）。ESP-IDF の警告の設定だけ、コアのファイルに対して緩めた（`-Wno-narrowing -Wno-missing-field-initializers`）。

### ファイルの地図（`firmware/orbit/`）

| ファイル | 内容 |
| --- | --- |
| `components/hid_remapper_core/CMakeLists.txt` | 本家コア 9 ファイルを subtree からそのままビルド。`PERSISTED_CONFIG_SIZE=2048`（本家 BLE 版と同じ） |
| `main/main.cc` | 起動、主ループ（コアを呼ぶのはここだけ、CPU1）、1 ms の刻み（`esp_timer`）、`SUM` 行と画面（1 秒ごと、CPU0） |
| `main/usb.cc` | USB の記述子と TinyUSB の受け口。本家 USB 版 `tinyusb_stuff.cc` が手本。1200 bps で書き込みモード |
| `main/platform.cc` | 本家コアが求める関数（`platform.h` など）、書き込みモードへの再起動、USB シリアルの切り離し |
| `main/storage.cc` | 本家の設定の塊を NVS に保存・読み込み |
| `main/log.c` | ログ。どのタスクからも待たずに書けるリング状のバッファ（16 KB）。主ループが CDC へ送る |
| `main/display.c`, `font5x7.c` | Q31 から流用した文字だけの画面 |
| `main/tusb/tusb_config.h` | TinyUSB の設定（HID 2、CDC 1） |
| `partitions.csv` | NVS をアプリの後ろ（`0x400000`）に置く |

## 4. 実機で分かったこと（事実。2026-09-27、Windows 11、初代 M5Dial）

版 `3662016`（Bluetooth なし）での利用者の報告より。

- PC からは複合機器として正しく見えた：HID 0（キーボード・マウス・コンシューマー）、HID 1（設定用、Vendor 0xFF00）、CDC。VID 0xCAFE / PID 0xBAF2。
- **ボタン（画面の押し込み）＋RST で書き込みモードに入れた**：画面に `DOWNLOAD MODE`、PC に VID 303A / PID 1001（COM9）が出た。**PC 本体のポートでだけ成功。** USB ハブ経由では、PC が「Device Descriptor Request Failed」で認識できなかった（2 回）。
- RST だけの再起動は、約 1 秒で USB がつながり直した（本体のポート、ハブ経由とも 1 回ずつ）。ただし一度だけ、ハブ経由の RST の後に「Unknown USB Device」になり、抜き差しでも直らず、PC の再起動で戻った（再現していない）。
- **ログ用の CDC には 1 行も出なかった**：ポートを開いて DTR/RTS を立てても、8 秒間で 0 バイト。
- 画面の表示：`ORBIT M1 3662016` / `USB --` または `USB OK` / `HEAP 210K MIN 205K`。
- 設定ツールの「Reset to bootloader」（戻り方 1）と、1200 bps（戻り方 3）は未確認。

利用者は一度、最初の版 `13d5248` を書き込んだ。その状態でブラウザの書き込みページがアプリの COM ポート（`0xcafe`）に `Connecting...` のまま止まったが、ボタン＋RST で書き込みモードに入り直して解決した。**アプリが動いている間は、書き込みページから自動では書き込みモードに入れない。**

### 4.2 版 `728105c` の結果（事実。利用者の報告、PC 本体のポート）

- **ボタン＋RST：不合格。** 画面は `DOWNLOAD MODE`（記憶、確実ではない）だが、PC からは何も見えなくなった（VID 303A も CAFE も出ない）。15 分待っても同じ。PC の再起動でアプリとして復旧。同じ操作で `3662016` は合格していたので、`db94be3` の直しで壊れたと見て、取り消した（`b2e7ad7`）。なぜつなぎ直しが効かなかったかは**未解明**。
- **ログ：不合格だが手がかりあり。** 画面は `LOG DTR 2K` と `LOG LOST 21K`。PC 側は 0 バイト。2 KB は TinyUSB の CDC 送信バッファの大きさ（`CFG_TUD_CDC_TX_BUFSIZE 2048`）と一致する。**バッファは満たされたが、USB へ 1 バイトも出ていない**。
- 複合機器としての認識と、RST だけの再起動後の復帰は正常。画面 `HEAP 213K MIN 208K`。

### 4.3 版 `1e37eea` の結果（事実。利用者の報告、PC 本体のポート）

- **ログ：合格。** `M1 START`（`app=1e37eea upstream=51ab8b3 config_size=2048 descriptor=0 vid=cafe pid=baf2 lvgl_reserve=ok`）と 1 秒ごとの `M1 SUM` が COM11 に届く。DTR を立てると `START` が出直す。取りこぼしなし。**送信口を 0x84 以下に収めた直しで解決した。**
- **ボタン＋RST：合格。** VID 303A / PID 1001（COM9）。
- **1200 bps：合格。** COM11 を 1200 bps・DTR ありで開いて閉じると、約 8 秒後に VID 303A。
- どの場合も RST でアプリに戻り、約 1 秒で複合機器と COM11 が復帰、ログ再開（3 回）。
- `SUM` の例：`usb=mounted boot_protocol=0 heap_free=218576 heap_min=213788`（Bluetooth も本家コアの処理もまだ動いていない版。A9 の判定には使えない）。
- 設定の読み込みは `config loaded from NVS err=0x1102`（NVS に無いので既定値。正常）。

**`728105c` から脱出できた手順**：ボタン＋RST、1200 bps、電源を切ってボタンを押しながら入れる、のどれでも PC から見えなかった。**M5StampS3 の G0 ボタンを押しながら電源を入れる**（M5Stack の公式手順。ROM が直接見るので、アプリの処理を通らない）で書き込めた。背面を開ける必要がある。推測：`db94be3` の pull override が RST では消えず、電源を切ると消える（ソースでの裏付けなし）。**今後、USB シリアルの状態をいじる直しを入れるときは、G0 に手が届く状態で試すこと。**

## 5. 版 `1e37eea` で直したこと（§4.3 で実機確認済み）

| 問題 | 見立て | 直し |
| --- | --- | --- |
| CDC が USB へ出ない | **ESP32-S3 の USB コントローラーは、送信用の FIFO が IN endpoint 0〜4 の 5 本しかない**（TinyUSB の `dwc2_esp32.h`、`ep_in_count = 5`。TinyUSB は IN endpoint N に FIFO N を割り当てる）。CDC のデータ送信口を 0x85 にしていたので、転送が始められなかった。ソースで確認した事実だが、これが原因かは実機で要確認 | 送信口を詰めた：HID 0 = 0x81、HID 1 = 0x82、CDC 通知 = 0x83、CDC データ = 0x84（受信は 0x01・0x04）。画面 6 行目に、PC が受け取った転送の回数 `C` を追加 |
| ボタン＋RST で PC から見えない | `db94be3` の「切り離してつなぎ直す」が効かなかった（理由は未解明） | 取り消し。`3662016` と同じ動きに戻した。ハブ経由の問題は `1fd6c6a` では再現しなかった（同じハブ） |

### 以前の版 `728105c` の直し（残っているもの）

- CDC への送信を主ループに移した（TinyUSB を呼ぶのは主ループだけ）。DTR が立ったら `START` 行を出し直す。`728105c` で `LOG DTR 2K` まで進んだので、この部分は動いていると見ている。
- **ESP-IDF の再起動（`esp_restart()`）は USB シリアルをリセットしない**（`esp_system_reset_modules_on_exit()` の対象外。ソースで確認）。USB シリアルの状態をいじる直しは、これを踏まえても壊れた。次にハブ対策をするときは、まず PC 本体のポートで壊れないことを確かめる。

### 画面 6 行目（ログの状態）の読み方

| 表示 | 意味 |
| --- | --- |
| `LOG --` | 本体は DTR を受け取れていない（端末がポートを開いていない、または DTR を立てていない） |
| `LOG DTR 3K C12` | DTR が見えていて、3 KB を CDC のバッファへ渡し、PC が 12 回の転送を受け取った。**`C` が増えなければ USB へ出ていない** |
| `LOG LOST 1K`（黄、9 行目） | ためきれずに 1 KB 捨てた（ポートを開くまでの 16 KB を超えた分） |

## 6. 次にやること

### 6.0 版 `86a2d16` の結果（事実。利用者の報告、途中まで）

- 起動・ログ・A2 前半（MD600 1 台）合格：7.50 ms・latency 0、暗号化、Report Map 192 バイト、入力 Report 4 つ登録、キー入力が PC に届いた。
- A8（1 台、キー入力）：`LAT` 最大 1.86 ms、通常は平均 0.7〜0.9 ms。キューあふれなし。
- A3：設定ツールで本体が見え、Load from device できた。保存は未確認。
- **止まった点**：本家の設定ツールは USB の製品名に `Bluetooth` が入っていないと Pair new device / Forget all devices を出さない（`config-tool-web/code.js` 261 行目）。→ 製品名を `HID Remapper Bluetooth XXXX` に変えた。
- **`heap_min` の異常値（970560 など）は表示の不具合**：`lost=` の値が区切りなしで後ろに付いていた（`97056` + `0`）。実際の最小値は約 97 KB。→ 直した。
- 機器側に Q31 の古いペアリングが残っていると `encryption failed status=0x505` → 相手が切断。機器側でペアリングをやり直すと、新しいアドレスでつながった（MD600 はやり直すとアドレスが変わる）。

### 6.1 すぐにやる：製品名を直した版で A2 後半〜A10（利用者と Desktop）

書き込み用のファイルは `orbit-m1-6254d16-ble.bin`（SHA-256 `36b6702fea5e0191e4e5992b7614c8c44a8cbe8fa770a5f34bd8f857de742ab6`）。アドレス `0x0`。書き込みモードへは 1200 bps かボタン＋RST で入る。NVS はアプリの後ろなので、`86a2d16` で作った MD600 のペアリング情報と設定は**残る見込み**（初めての確認）。

順番：
1. 書き込み後 RST。ログに `START`（`app=6254d16`）と `SUM`（`heap_min` が 10 万前後の正しい値、`lost=0`）。**MD600 が自動でつながる**か（`ble ready bonds=1` → `scan start (bonded devices only)` → `connected`）。つながらなければ NVS が消えている：`bonds=0` と `PAIRING` を報告し、MD600 をペアリングし直す。
2. **A2 後半**：設定ツール（https://www.remapper.org/config/ 、Chrome）で本体を開く。製品名が `HID Remapper Bluetooth ...` で、Actions に **Pair new device** が出ることを確かめる。押してから meteorite40 をペアリング待ちにする（1 台つながった後は、ペアリング済みの機器しか探さない）。2 台とも `itvl=6(7.50ms) lat=0`、トラックボールとキーが PC で動く。
3. **A3**：設定ツールで本体が見え、割り当てを変えて Save、再起動後も残る（`EVT` に `config saved err=0x0`）。
4. **A4**：Pair new device（2 で確認済み）、Forget all devices（`EVT` に `clear_bonds rc=0x0`、両方切れて `PAIRING` に戻る。機器側でもペアリングを消してやり直す）、Flash firmware（= Reset to bootloader。`EVT` に `entering download mode (config tool)`、VID 303A。戻るときは RST）。
5. **A8**：`LAT` の `max` が 3 ms 以内か。超えるなら `avg` と分布も記録。
6. **A9**：`SUM` の `heap_min` が 50 KB（51200）以上か。
7. **A7**：2 台を動かし続けて 10 分。`DEV` の `disc` と `itvl`。
8. **A10**：電源の入れ直し、スリープからの復帰で、自動でつなぎ直す。
9. **A5**（BIOS）、**A6**（PC のスリープ解除）は利用者の PC の都合で。A6 は `EVT` の `usb suspended`、`remote_wakeup sent/refused` を見る。

**動かないときに見るところ**：`subs=0`（通知を登録できていない）、`descriptor parsed` が出ない（Report Map を読めていない）、`SUM` の `lost=`（キューあふれ）、`LOG LOST`。

### 6.2 参考：版 `1e37eea` の確認（済み）

書き込み用のファイルは `orbit-m1-1e37eea-A1-usb-only.bin`（SHA-256 `f92b1235c1e4b76a7f93d431c7fac012b94bcdfefa0c7118041a82a0602b5aa2`）。アドレス `0x0` に書き込む。**PC 本体のポートにつないで書き込む。** いま入っている `728105c` はボタン＋RST で PC から見えなくなるので、書き込みモードに入るには **PC を再起動せずに、ケーブルを抜き差ししてから**ボタン＋RST を試す（抜き差しで直るかは未確認。直らなければ PC の再起動）。

1. **ログの COM ポートに `M1 START` と、1 秒ごとの `M1 SUM` が出るか**（今回はこちらが先）。出なければ画面 6 行目の `LOG ... C` の値を記録する。PowerShell の `System.IO.Ports.SerialPort` で開くなら、`DtrEnable = $true` にする。**1200 bps で開くと書き込みモードに入るので、速度は 115200 などにする。**
2. **PC 本体のポートで、ボタン＋RST で書き込みモードに入れるか**（`3662016` と同じ動きに戻したことの確認）。
3. 1 が通ったら、1200 bps で開いて書き込みモードに入るか（戻り方 3）。
4. ハブ経由は後回し（**2026-09-29 追記：`1fd6c6a` ではハブ経由でも問題なし**）。
4. 結果を §7 の形でまとめ、クラウドのセッションに渡す。

### 6.3 その後（クラウドのセッションが作る）

- A2〜A10 の結果を受けた直し。
- 結果の文書（依頼書 §5 の表を埋める）、設計の記録の直し（§8）、PR の仕上げ。

## 7. 報告の書き方（クラウドのセッションへ渡すとき）

A1 の報告（利用者が 2026-09-27 に書いたもの）の形がよい：結論 → 時系列の表（PC の時刻、操作、PC 側の見え方）→ 直してほしいこと → 変わらないこと。書き込んだ版（`START` 行の `app=`、画面 2 行目）を必ず書く。ログは `M1` で始まる行を抜き出して貼る。

## 8. 作っている途中で分かったこと（次の人向け）

- **本家 BLE 版の 1 ms の刻みは USB の SOF**（PC からの合図）だった（`firmware-bluetooth/src/main.cc` の `status_cb`）。[implementation-design.md](implementation-design.md) の決定 2 にある「`esp_timer`（本家 BLE 版と同じ）」の括弧内は事実と違う。M1 は決定どおり `esp_timer` で作った。設計書はまだ直していない。
- **`esp_tinyusb` 2.x は、TinyUSB を必ず自分のタスクで回す**（止める設定がない）。そのままでは設定ツールからの問い合わせが別のタスクからコアを呼ぶことになり、決定 I2 に反する。そのため `espressif/tinyusb` を直接使い、主ループで `tud_task_ext(0, false)` を回している。USB の PHY の初期化は自分で書いた（`usb_new_phy()`）。
- **USB 機器の区分を複合機器（IAD、`0xEF/0x02/0x01`）にした。** ログ用の CDC を Windows に正しく認識させるため。BIOS で使えるかは A5 で確かめる（未確認）。
- **NVS（ペアリング情報と設定）をアプリの後ろ（`0x400000`）に移した。** `0x0` に書く 1 ファイルは `0x6A4C0` までなので、今後は書き込んでも消えない見込み（Q31 では毎回消えた）。場所を移した最初の書き込みでは、Q31 のペアリング情報は読めなくなる。
- **ボタン＋RST で書き込みモードに入るのは、USB を使い始める前に判定する**ので、アプリの USB の不具合に左右されにくい。3 つの戻り方の中で最も確実。ただし `db94be3` のように再起動の前に USB シリアルをいじると壊れる。
- **ESP32-S3 の USB は送信用の FIFO が 5 本（IN endpoint 0〜4）**。IN endpoint は 0x84 までしか使えない。HID 2 つと CDC（通知＋データ）でちょうど 4 本を使い切っている。これ以上 IN endpoint を足せない。
- 静的に使う RAM が約 105 KB ある（`idf.py size` の DIRAM）。A9 のときに内訳を調べる。
- Q31 の試験プログラム（`experiments/q31-s3-two-ble/`）はそのまま残してある。

## 9. 守ること

- 機器の完全なアドレス、ペアリングの鍵、M5Dial 自身の MAC アドレスをログや報告に書き写さない（`M1` の行は下位 2 バイトしか出さない。USB のシリアル番号は MAC から作った値の置き換えで、MAC そのものではない）。
- `.bin`、ビルドの生成物、ログの全文はリポジトリに入れない。
- `main` に直接 push しない。コミットの author は `techmech <88352328+techmech-keeb@users.noreply.github.com>`。
