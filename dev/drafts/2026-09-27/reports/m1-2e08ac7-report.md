# M1 の報告：版 `2e08ac7`（`orbit-m1-2e08ac7-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、クラウドのセッションから貼られた短い確認の手順（`da3c37f` ＋ 受け入れ条件の引き締め `7eb602c`、データ長の実験 `f2093b6` は取り消し）を行った。`.bin` の SHA-256 は一致（`2e7d9d20…fdac`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。`M1` の行は全部ファイルに残した（`m1-2e08ac7-M1-lines.log` 1,514 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、Cube Turner PRO（public `71:2c`）。

## 結論

**`da3c37f` と同等。合格の基準を満たした。** ただし手順 3 の「数分使う」は、つながったままだったが操作はほとんど無かった。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| **1. 書き込み後 RST** | **合格** | `default data length set` は出なかった（0 回）。IST は `(stored key)` で起動から 0.59 秒。Cube Turner は `(stored key), before the connect event` で起動から 1.91 秒。**2 台そろうまで 1.91 秒** |
| **2. Cube Turner の電源の入れ直し 3 回** | **合格** | 1.91 秒、1.55 秒、2.17 秒（現れてから `subscribed` まで）。3 回とも `device started security itself (enc=0 bonded=0 key_size=0 at this moment)` → `found by polling (NimBLE posted no event)`。`da3c37f`（1.68〜2.33 秒）と同等 |
| 3. `WARNING`・`dropping the link` | **合格** | どちらも 0 回。`encryption changed again`・`could not start encryption`・`no answer`・止まる不具合・BLE ホストのリセットも 0 回 |
| 3. 数分使って切断 0 | **合格（操作は少ない）** | 19:45:00〜19:48:00。切断 0、2 台とも 175 秒すべて接続、`lost=0`。**入力は IST 3 秒、Cube Turner 0 秒**。`LAT` 151 件、平均 0.81 ms、最大 1.15 ms、`unrelated=0` |

`broken 128-bit UUID`：0 回。A9：`heap_min` の最小 93,408。`lost=0`。A8：この版全体で `LAT` 3,715 件、平均 0.83 ms、最大 2.60 ms、`unrelated=0`。

`f2093b6` の試験とあわせても、読み取りで受け入れた Cube Turner の接続が `7eb602c` の条件で切られた例は無い。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 19:38:54 | 1200 bps で書き込みモード → 書き込み → RST | 19:40:38 `START app=2e08ac7`、`bonds=2`。1.91 秒で 2 台そろう |
| 19:42:53〜19:43:01 | Cube Turner の電源の入れ直し 1 | 1.91 秒（`found by polling`） |
| 19:43:28〜19:43:35 | 電源の入れ直し 2 | 1.55 秒（同上） |
| 19:43:49〜19:43:57 | 電源の入れ直し 3 | 2.17 秒（同上） |
| 19:45:00〜19:48:00 | 2 台をつないだまま置く（操作は少ない） | 切断 0、`lost=0` |

## 変わらないこと・未確認

- Cube Turner の LED の点滅の意味（`f2093b6` の報告のとおり、PC でも点滅する。電池の残りの表示かどうか）。
- スリープからの復帰（Cube Turner が自動で眠らない）。
- 機器側で鍵を捨てたときの扱い（Cube Turner でペアリングを消す方法が分からない）。
- MD600・meteorite40 では、`a0d8f9e` 以降の版を試していない。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。
