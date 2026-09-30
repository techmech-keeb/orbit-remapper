# M2 の報告：版 `102144d`（`orbit-m2-102144d-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の手順（M2 の 2a：機器台帳・ペアリング管理・組み直しの許可・命令、PR #27）を行った。`.bin` の SHA-256 は一致（`6a7c5d39…465b`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。命令は、ログ取りのスクリプトに「命令のファイルに書いた行を COM11 に送る」機能を足して送った（送った行はログに `# sent` として残る）。`M1` の行は全部ファイルに残した（`m2-102144d-M1-lines.log` 11,524 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、Cube Turner PRO（public `71:2c`）。MD600 の「行う場合」（B5・B6）は行っていない。

## 結論

**手順 1〜9 はすべて合格。** `ledger save failed` は 0 回。

**気になった点が 2 つある**（下記 1・2）：設定の保存で入力が 1 件 9.40 ms 遅れた。`heap_min` が M1 より約 9 KB 下がり、設定の保存では一時 66,648 まで下がった。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| **1. 書き込み → 台帳への移行** | **合格** | `ledger loaded boot=1 rows=0` → M1 のペアリング情報 2 件が `had no row, added as port 1`（IST）、`port 2`（Cube Turner）。2 台とも `(stored key)`、`DEV` の `port=` は IST 1、Cube Turner 2。情報を読んだあと `ledger: port 1 "IST TrackBall" kind=mouse`、`ledger: port 2 "TurnerPro" kind=keyboard+mouse` |
| 1. 設定ツールでポート 1 に割り当て | **合格** | 利用者が設定ツールでポート 1 に割り当てを付けて保存 → IST で効いた（利用者が確認）→ 外して保存し直した。`config saved err=0x0` 2 回 |
| **2. `orbit list`** | **合格** | `LDG` に名前・製造者・型番・`shown=` が入った。IST の製造者名は先頭の空白が削られて `PixArt Inc.` |
| **3. `orbit alias`** | **合格** | `orbit alias 1 left` → `shown="left"`。`orbit alias 1` → `shown="IST TrackBall"` |
| **4. RST 2 回** | **合格** | 2 回とも `ledger loaded`（`boot=2 rows=2`、`boot=3 rows=2`）、`port=` は IST 1・Cube Turner 2 のまま。2 台そろうまで 4.26 秒、4.34 秒 |
| **5. `orbit forget 2`** | **合格** | `forget port 2` → `forgotten, dropping the link` → 切断。`ORB` は `devices=1/15`、`orbit list` は `1 of 15 ports used`。その後 39 秒、Cube Turner はつながらなかった |
| **6. `orbit pair` → 新しいペアリング** | **合格** | Cube Turner は切られた後も広告を出していたので、電源を入れ直す前にペアリングまで済んだ（下記の流れ）。**`held` から `hub_port=2` まで 0.77 秒**。ペダルが効いた（利用者が確認） |
| **7. `orbit pair` → 待つ** | **合格** | 23:08:32 `pair_new_device` → **23:10:33 `pairing mode ended after 120 s without a new device`** → `stop_pairing bonds=2`。2 台つながっていたので `scan start` は出なかった。`ORB` は `pairing=119s` → `pairing=off` |
| **8. `orbit approve`／短い押し込み** | **合格** | どちらも `approve: nothing is waiting for approval`。押し込みは `button released after 165 ms` |
| **9. 5 分の使用** | **合格** | 23:28:30〜23:33:30。切断 0、2 台とも 291 秒すべて接続、`lost=0`。`ORB` 行は 291 回（毎秒）。`LAT` 3,160 件、平均 0.86 ms、**最大 2.32 ms**、`unrelated=0`。入力は IST 44 秒、Cube Turner 2 秒（操作は少なめ） |
| 9. 命令を打っている間の `LAT` | **影響なし** | 20 秒ごとに `orbit list` を 14 回送った。送った直後の 1 秒間（入力があったのは 4 秒）は最大 2.04 ms、それ以外は最大 2.32 ms |

`WARNING`・`ledger save failed`・`no answer`・`read failed`・止まる不具合・BLE ホストのリセット・`broken 128-bit UUID`：すべて 0 回。`dropping the link` は手順 5 の 1 回だけ。A8：この版全体で `LAT` 20,463 件、平均 0.81 ms、最大 9.40 ms（下記 1）、`unrelated=0`。

### 手順 1 の起動ログ（そのまま）

```
M1 NVS t=0.203 err=0x0 entries used=168 free=3864 available=3738 total=4032 (32 B each) namespaces=3 max_bonds=15 accept_list_max=15 max_conns=2
M1 NVS t=0.205 namespace orbit: entries=67 config=1x2048B
M1 NVS t=0.209 namespace nimble_bond: entries=32 our_sec=2x88B peer_sec=2x88B rpa_rec=1x14B csfc_sec=2x8B local_irk=1x23B
M1 EVT t=0.213 ledger loaded boot=1 rows=0
M1 EVT t=0.326 ledger: bond addr=..:21:96 had no row, added as port 1
M1 EVT t=0.335 ledger: bond addr=..:71:2c had no row, added as port 2
M1 LDG t=0.336 port=1 addr=..:21:96 static key=yes kind=device vid=0000 pid=0000 hash=00000000 last=1 "device ..:21:96"
M1 LDG t=0.336 port=2 addr=..:71:2c public key=yes kind=device vid=0000 pid=0000 hash=00000000 last=1 "device ..:71:2c"
```

### 手順 2 の `LDG` と `ORB`（そのまま）

```
M1 LDG port=1 addr=..:21:96 key=yes kind=mouse vid=056e pid=0184 hash=5c7471f4 last=1 name="IST TrackBall" manufacturer="PixArt Inc." model="MS 2822" alias="" shown="IST TrackBall"
M1 LDG port=2 addr=..:71:2c key=yes kind=keyboard+mouse vid=0000 pid=0000 hash=b5fa2fcc last=1 name="TurnerPro" manufacturer="sincoaudio" model="ble device" alias="" shown="TurnerPro"
M1 LDG 2 of 15 ports used
M1 ORB t=514 devices=2/15 pairing=off full=0 ask=0:-:0s granted=0:0s duplicates=0
```

### 手順 6 の流れ（Cube Turner）

| 時刻 | 行 |
| --- | --- |
| 23:07:34.683 | `pair_new_device` → `scan start (pairing)` |
| 23:07:34.841 | `connecting rssi=-45 adv_type=0 addr_kind=public adv_name="" adv_appearance=0x0000` |
| 23:07:35.113 | `encryption on (new pairing) pairing mode`（`security done status=0x0`、NimBLE の知らせが普通に届いた） |
| 23:07:35.120 | `ledger: new device, port 2` |
| 23:07:35.755 | `report map 297 byte(s) hash=b5fa2fcc, held until the ledger row is settled` |
| 23:07:36.343 | `subscribed 6 input report(s)` |
| 23:07:36.372〜36.494 | `info` 5 行 |
| 23:07:36.512 | `ledger: port 2 "TurnerPro" kind=keyboard+mouse` |
| 23:07:36.513 | `report map 297 byte(s) hash=b5fa2fcc, hub_port=2` |

`held` から `hub_port=2` まで 0.77 秒。`connecting` から入力できるまで 1.68 秒。

## 確かめてほしいこと

### 1. 設定の保存で入力が 1 件 9.40 ms 遅れた

- 22:56:03 に、設定ツールで割り当てを外して保存した。そのとき `config saved err=0x0`、`persist_config took 11.4 ms` が出て、同じ秒の `LAT` が `max=9.40ms`（`<=8/<=16/>16=62/1/0`）だった。
- 1 回目の保存（22:55:44、9.6 ms）の秒は、入力が無かったので比べられない。
- 保存の間は主ループが止まり、その間に来た入力が遅れると見ている（推測）。設定の保存はたまにしか起きないので実害は小さいが、M1 から同じかどうかは調べていない。

### 2. `heap_min` が M1 より下がった

| 時点 | `heap_min` |
| --- | --- |
| M1（`2cb6aad`〜`66521d2`）の起動後、2 台つながった後 | 約 92,996〜93,400 |
| この版の起動後、2 台つながった後 | 約 84,400〜84,500（3 回とも） |
| 設定ツールで保存したとき（22:55:44） | **66,648** |
| 手順 6 の新しいペアリングの後 | 83,664 |

- 起動後の最小は M1 より約 9 KB 少ない。依頼書の見込み（台帳で約 4 KB）より大きい。
- 設定の保存のときに一時 66,648 まで下がった（この版の最小）。RST すると戻る。保存のときに大きな一時領域を取っているのかもしれない（推測）。

## そのほか

- Cube Turner は、`orbit forget 2` で切られた後も広告を出し続けていたので、手順 6 では電源の入れ直しが要らなかった。
- 手順 6 の新しいペアリングでは、Cube Turner の暗号化の知らせが普通に届いた（`found by polling` ではない）。
- 起動直後、IST のつながりが起動から 9.4 秒後だった（1 回目）。IST が眠っていたか、広告が遅れたと見ている。RST の 2 回は 4.26 秒、4.34 秒。
- 短い押し込みは `button released after N ms` としてログに出るようになった。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 22:45:32 | 1200 bps で書き込みモード → 書き込み → RST | 22:48:04 `START app=102144d`、台帳へ 2 件移行 |
| 22:55:44、22:56:03 | 設定ツールでポート 1 に割り当てを保存 → 外して保存 | IST で効いた。2 回目の保存の秒に `LAT` 9.40 ms |
| 22:56:37〜22:57:03 | `orbit list`、`orbit alias 1 left`、`orbit alias 1` | 合格 |
| 22:57:49、23:00:18 | **RST 2 回** | `port=` 変わらず、4.26 秒・4.34 秒 |
| 23:06:44 | `orbit forget 2` | Cube Turner が切れ、`devices=1/15` |
| 23:07:34 | `orbit pair` | Cube Turner が新しいペアリング、ポート 2、0.77 秒 |
| 23:08:32〜23:10:33 | `orbit pair` → 待つ | 120 秒で終わった |
| 23:10:56、23:27:08 | `orbit approve`、画面の短い押し込み | どちらも `nothing is waiting for approval` |
| 23:28:30〜23:33:30 | **5 分の使用**（20 秒ごとに `orbit list`） | 切断 0、`LAT` 最大 2.32 ms |

## 変わらないこと・未確認

- MD600 の「行う場合」（B5：同じ機器らしい古い行の自動引き継ぎ、B6：組み直しの許可）は行っていない。`approval wanted`・`looks like port` の行は一度も出ていない。
- Cube Turner の LED の点滅の意味（PC でも点滅する）。
- スリープからの復帰（Cube Turner は自動で眠らない）。
- 機器側で鍵を捨てたときの扱い（Cube Turner でペアリングを消す方法が分からない）。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。
