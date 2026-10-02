# M1 の報告：版 `a0d8f9e`（`orbit-m1-a0d8f9e-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、クラウドのセッションから貼られた手順（`main` ＋ 壊れた UUID の補正、PR #16）を行った。`.bin` の SHA-256 は一致（`d36f2b51…b85d`）。`orbit-m1-25bb881-ble.bin` は使っていない（SHA-256 の記録が手元になく、確かめられなかったため）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）。`M1` の行は全部ファイルに残した（`m1-a0d8f9e-M1-lines.log` 6,786 行。利用者の PC に保存、リポジトリには入れていない）。

**機器は、手順書の MD600・meteorite40 ではなく、今までと違う 2 台で試した**（利用者の判断）：

- **IST Trackball**（マウス、static アドレス）
- **4 台目の機器**（public アドレス `71:2c`。利用者が手順書として示したのは 機器 B）

書き込みのあと、最初のペアリングの間はログ取りが COM11 を開けず、その部分は残っていない（07:00:33 の起動から残っている）。途中で Claude のアプリが落ち、07:31〜07:34 のログが欠けている。

## 結論

**UUID の補正による回帰は見つからなかった。** `broken 128-bit UUID` 0 回、`WARNING` 0 回、止まる不具合 0 回。IST Trackball はふつうに使えた。

**ただし、4 台目の機器は最後まで使えなかった。** 原因は UUID の補正ではなく、以前からある暗号化のやり直しの仕組みの不具合（下記 1）。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| 1. 書き込み後 RST、`app=` | **合格** | `START app=a0d8f9e` |
| 2. 自動でつながる | **IST は合格、4 台目は不合格** | IST：`subscribed 1`。暗号化の行は `(stored key)` ではなく **`(no key stored)`**（下記 2）。4 台目：37 回つないで、1 回も `subscribed` にならなかった |
| 3. PC で動く | **IST は合格** | カーソルが動いた（利用者が確認）。キーは 4 台目が使えなかったので確かめていない |
| **4. `broken 128-bit UUID` が出ない** | **合格** | 0 回 |
| 5. M5Dial の RST 2 回 | **合格（遅れあり）** | IST が使えるまで 1 回目 7.29 秒、2 回目 2.41 秒。1 回目の遅れは 4 台目が先につかまったため（下記 3） |
| 6. 補正が働く機器 | **該当機器なし（確認できず）** | IST は補正が働かなかった（正しい 16 ビットの UUID と見られる）。4 台目は探索の途中で切れたので、補正が要る機器かは分からない |

`WARNING`：0 回。止まる不具合（`connect attempt stuck`）・BLE ホストのリセット：0 回。`discovery stalled`・`nothing subscribed`：0 回。A9：`heap_min` の最小 95,572。`lost=0`。A8：`LAT` 23,890 件、平均 0.84 ms、最大 3.64 ms、`unrelated=0`。

## 直してほしいこと

### 1. 暗号化が済んだあとに、予約したやり直しで暗号化をもう一度始めてしまう（最重要）

- 4 台目の機器は、つながるとすぐ自分からペアリングを始める。そのため `ble_gap_security_initiate()` が `rc=0x2` で失敗し、`security start failed rc=0x2, retrying` が出て `sec_pending` が立つ。
- 機器が始めたペアリングは成功し、`encryption on (new pairing) pairing mode` が出る。**ところが `sec_pending` が消されないので**、0.4〜0.6 秒後に `periodic_check()` が `security started` をもう一度出す。これが `encryption failed status=0x23d` になり、M5Dial が切る。
- Pair new device で 2 回試し、2 回とも同じ流れだった（07:14:38、07:17:13）。
- そのあと保存した鍵でつなぐと、毎回 `no encryption after 5 s` → `encryption failed status=0x23d` で切れる。2 回目のペアリングで鍵が合わなくなったと見ている（推測）。この版の試験の間に 37 回つないで、1 回も使えなかった。
- 案：`BLE_GAP_EVENT_ENC_CHANGE` を受けたら（成功でも失敗でも）`sec_pending` を消す。または `periodic_check()` で、暗号化済みの接続はやり直さない。
- UUID の補正とは関係なく、`sec_pending` を入れた版からある不具合と見ている。今までの 2 台は自分からペアリングを始めないので、表に出なかった。

### 2. IST Trackball の暗号化の行が `(no key stored)` になる

- 保存した鍵でつないだときも、Pair new device で新しくペアリングしたときも、`encryption on (no key stored)` だった。暗号化そのものは通り、ふつうに使えた。
- `key_tag()` は `ble_store_read_our_sec()` の `ltk_present` を見ている。IST ではここに鍵が見つからないらしい。IST が別の形で鍵を配っているのかもしれない（推測）。
- この状態だと、`key_tag` が 0 同士になるので、**ペアリングモード外での組み直しの見張り（`WARNING: paired outside pairing mode`）が IST には働かない**。

### 3. 暗号化で失敗する機器が 1 台あると、ほかの機器も 5 秒待たされる

- RST 1 回目：起動直後に 4 台目が先につかまり、`no encryption after 5 s` で切れるまでの 5 秒、IST への接続も止まっていた。IST は 4 台目が切れてから 0.29 秒でつながった。
- つないで暗号化されるまで次の接続を待つ作り（`schedule()`）のためと見ている。

### 4. ペアリングモードでも、答えない古い鍵から抜け出せない

- 4 台目は、最初（ログが残っていない間）のペアリングのあと、保存した鍵での暗号化に答えなくなった。
- Pair new device を押しても、M5Dial は同じアドレスの 4 台目に古い鍵で暗号化を試み、答えがないまま 5 秒で切るのをくり返した。`device lost the bond, pairing again (pairing mode)` の経路は、機器が「鍵が無い」と答えるときしか働かない。
- 抜けるには Forget all devices しかなく、ほかの機器のペアリング情報もすべて消えた。案：ペアリングモードで、保存した鍵での暗号化が答えなしで失敗したら、その機器の鍵を消してペアリングし直す。または設定ツールから 1 台だけ消せるようにする。

## 気づいたこと

- **ペアリング情報の上限は 4 件**：`CONFIG_BT_NIMBLE_MAX_BONDS=4`。07:00 の起動では 4 件（MD600 `8f:d3`、meteorite40 `e4:f9`、IST `20:96`、4 台目 `71:2c`）で、MD600 の古い `d2:1b` は消えていた。どの時点で消えたかはログが残っていない。
- **IST はペアリングし直すとアドレスが変わる**：`20:96` → `21:96`（どちらも static）。
- **接続の条件**：IST は 7.50 ms のまま latency 44・supervision timeout 2,160 ms を求めた。4 台目は 11.25〜15 ms を求め、M5Dial は受け入れた（一度は `device allows 7.50ms, asking for it again` で 7.50 ms に戻した）。
- **Report Map**：IST 92 バイト（入力 1 つ）。4 台目 297 バイト（読み切れたのは 1 回だけで、その直後に切れた）。
- **1200 bps で書き込みモードに入ると、M5Dial の画面は通常の表示のまま**だった（書き込みモードでは画面が更新されない）。PC には VID 303A（COM9）が出ていたので、書き込みモードには入っていた。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 06:57:50 | 1200 bps で書き込みモード → 書き込み → RST | （ログなし）利用者が Pair new device で IST と 4 台目をペアリング |
| 07:00:33 | M5Dial が起動 | `START app=a0d8f9e`、`bonds=4`。IST が 2.85 秒で `(no key stored)` → `subscribed 1` |
| 07:03:39〜07:06:31 | （自動） | 4 台目が許可リストでつながるが、5 回とも `no encryption after 5 s` → 30 秒外す |
| 07:12:41 | Pair new device | 4 台目が同じアドレスでつながり、古い鍵で同じ失敗を 2 回 |
| 07:14:21 | **Forget all devices** | `clear_bonds`。IST が切れる |
| 07:14:38 | （ペアリングモード） | 4 台目が `(new pairing) pairing mode` → やり直しの暗号化で失敗して切れる（上記 1） |
| 07:14:40〜46 | （自動） | 4 台目が保存した鍵でつながるが `no encryption after 5 s` |
| 07:15:54 | **Forget all devices**（2 回目） | IST が新アドレス `21:96` で `(no key stored) pairing mode` → 0.6 秒で `subscribed 1` |
| 07:17:08 | Pair new device | 4 台目が `(new pairing) pairing mode` → やり直しで失敗（2 回目も同じ） |
| 07:19:46 | **RST 1 回目** | 4 台目で 5 秒待たされ、IST は 7.29 秒で `subscribed 1` |
| 07:19〜07:31 | （自動） | 4 台目が 30 秒ごとに失敗をくり返す。IST は使えたまま |
| 07:31〜07:34 | （アプリが落ちてログなし） | |
| 07:36:04 | **RST 2 回目** | IST が 2.41 秒で `subscribed 1` |

## 変わらないこと・未確認

- MD600・meteorite40 では試していない（手順書の 2 の「MD600 は 4、meteorite40 は 3」は確かめていない）。
- 補正が働く機器での確認（手順 6）はできていない。
- A5（BIOS）は保留のまま。
