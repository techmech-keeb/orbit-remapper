# M1 の報告：版 `9309d56`（`orbit-m1-9309d56-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の手順（`ff8e166` の問題 1〜3 の直し）を行った。`.bin` の SHA-256 は一致（`b36b3d1a…cb6a`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。`M1` の行は全部ファイルに残した（`m1-9309d56-M1-lines.log` 9,782 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、Cube Turner PRO（public `71:2c`）。

## 結論

**合格の基準のうち、試せたものはすべて満たした。** Cube Turner PRO は、Pair new device なしで保存した鍵のまま使えるようになった。IST は一度も巻き込まれなかった。

**ただし、Cube Turner の接続は半分の場面でまだ 5 秒詰まる**（下記 1）。機器が暗号化を求めるのが、M5Dial が接続を受け取った後になると詰まる。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| **1. 書き込み後 RST** | **合格** | Cube Turner が `encryption on (stored key), before the connect event` → `subscribed 6`。ペダルが PC で効いた（利用者が確認）。IST は `(stored key)`。2 台そろうまで起動から 2.00 秒 |
| **2. M5Dial の RST 3 回** | **合格（ぎりぎり）** | 2.54 秒、8.25 秒、8.24 秒。IST は 3 回とも切られなかった。2 回目と 3 回目は Cube Turner の 1 回目の接続が 5 秒詰まった |
| 3. Cube Turner の電源の入れ直し | 合格（詰まりあり） | 現れてから入力できるまで 7.66 秒（14:18）、7.76 秒（14:49）。どちらも 1 回目が 5 秒詰まり、2 回目で `before the connect event` |
| 3. スリープからの復帰 | **未確認** | Cube Turner は最後の操作から 30 分以上、つながったまま眠らなかった。手順書にも自動で眠る記述は見当たらない |
| 4. 機器側で鍵を捨てたとき | **未確認** | 電源を切って入れ直しても、Cube Turner は鍵を捨てなかった（2 回目で `(stored key)`）。機器側でペアリングを消す方法が分からなかった |
| **5. 10 分の連続使用** | **合格** | 14:18:40〜14:28:40。切断 0、2 台とも 581 秒すべて接続、`lost=0`。IST の入力 288 秒。`LAT` 14,718 件、平均 0.68 ms、最大 2.63 ms、`unrelated=0` |
| 6. `WARNING`、止まる不具合 | **合格** | どちらも 0 回。`could not start encryption for 15 s` も 0 回 |

`broken 128-bit UUID`：0 回。BLE ホストのリセット：0 回。`device asked to pair again`：0 回。`security start failed rc=0x6`：0 回。14:18〜14:49 の 31 分間、2 台とも切断 0。A9：`heap_min` の最小 93,400。`lost=0`。A8：この版全体で `LAT` 46,382 件、平均 0.70 ms、最大 6.57 ms、`unrelated=0`。

### Cube Turner の接続（この版の 6 場面）

| 場面 | 1 回目 | 入力できるまで |
| --- | --- | --- |
| 書き込み後 RST（14:08） | `before the connect event` | 起動から 2.00 秒 |
| RST 1（14:11） | `before the connect event` | 起動から 1.65 秒 |
| RST 2（14:15） | `device started pairing itself` → 5 秒で `no answer` | 起動から 8.25 秒 |
| RST 3（14:17） | 同上 | 起動から 8.24 秒 |
| 電源の入れ直し（14:18） | 同上 | 現れてから 7.66 秒 |
| 電源の入れ直し（14:49） | 同上 | 現れてから 7.76 秒 |

詰まった 4 回は、どれも 2 回目の接続で `before the connect event` になって通った。

## 直してほしいこと

### 1. 機器の暗号化の要求が接続の知らせの後に来ると、まだ 5 秒詰まる

- `device started pairing itself` が出た 4 回は、4 回とも 5 秒後に `no answer` で切られた。`before the connect event` の 6 回は、すべてすぐ通った。
- 詰まったときの値（4 回とも同じ）：

```
no answer: link enc=1 auth=0 bonded=0 key_size=0
no answer: stored peer keys ltk=1 irk=1 csrk=0 sc=0 auth=0 key_size=16 ediv_rand=set
no answer: stored our keys ltk=1 irk=1 csrk=0 sc=0 auth=0 key_size=16
```

- `ff8e166` で詰まったときは `bonded=1 key_size=16` だった。今回は `bonded=0 key_size=0` で、暗号化はされているのに保存した鍵ではない状態に見える。
- 見立て（推測）：接続を受け取ったあとに Cube Turner の Security Request が来ると、NimBLE が保存した鍵での暗号化ではなく、新しいペアリングの手順に入っている。途中（鍵を配る前）で止まり、5 秒で切られる。
- **安全面で確かめてほしい**：ペアリングモードではないのに、NimBLE が機器からのペアリングの要求に途中まで応じているかもしれない。今回は鍵が保存されず、入力も PC に通っていないので、実害は出ていない。`WARNING` も 0 回。ただ、`b8d689d` で塞いだ経路とは別の入り口があるなら、塞いでおきたい。
- 案：`device started pairing itself` のとき、通常時で保存した鍵があるなら、機器の要求を受けて保存した鍵での暗号化を始める（新しいペアリングに入らせない）。Security Request の中身（bonding、MITM、SC の要求）もログに出してほしい。

### 2. 細かいこと

- `LAT` の最大 6.57 ms は、10 分の連続使用の外で出た。いつ出たかは調べていない。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 14:07:31 | 1200 bps で書き込みモード → 書き込み → RST | 14:08:51 `START app=9309d56`。IST 0.65 秒、Cube Turner 2.00 秒（`before the connect event`）。ペダルが効く |
| 14:11:01 | **RST 1** | 2.54 秒で 2 台そろう |
| 14:15:46 | **RST 2** | Cube Turner の 1 回目が 5 秒詰まる。8.25 秒で 2 台そろう。IST は 4.36 秒 |
| 14:17:12 | **RST 3** | 同上。8.24 秒。IST は 4.27 秒 |
| 14:17:58〜14:18:16 | Cube Turner の電源の入れ直し | 1 回目が 5 秒詰まり、現れてから 7.66 秒 |
| 14:18:40〜14:28:40 | **10 分の連続使用** | 切断 0、`lost=0`、`LAT` 最大 2.63 ms |
| 14:18:33〜14:48:33 | Cube Turner を触らずに置く | 眠らなかった |
| 14:49:29〜14:49:42 | Cube Turner の電源を切り、Pair new device を押さずに入れる | 鍵を捨てず、1 回目が 5 秒詰まり、現れてから 7.76 秒で `(stored key)` |

## 変わらないこと・未確認

- スリープからの復帰（Cube Turner が自動で眠らない）。
- 機器側で鍵を捨てたときの扱い（Cube Turner でペアリングを消す方法が分からない）。
- MD600・meteorite40 では、`a0d8f9e` 以降の版を試していない。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。
