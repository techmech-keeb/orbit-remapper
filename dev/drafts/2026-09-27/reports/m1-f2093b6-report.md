# M1 の報告：版 `f2093b6`（`orbit-m1-f2093b6-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、クラウドのセッションから貼られた手順（`da3c37f` ＋ 読み取りで受け入れる条件の固め ＋ データ長の既定値の実験）を行った。`.bin` の SHA-256 は一致（`be79abd8…6fa3`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。`M1` の行は全部ファイルに残した（`m1-f2093b6-M1-lines.log` 8,095 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、機器 B（public `71:2c`）。

## 結論

**合格の基準を満たした。** 時間は `da3c37f` と同等で、`dropping the link` 0 回、`WARNING` 0 回。

**データ長の実験は効かなかった。** 機器 B の 12 回の接続のうち 10 回が `found by polling`（`da3c37f` は 9 回中 6 回）。RST のあとも読み取りになる場面が増えた。

**機器 B の LED が点滅したままだったが、M5Dial とは関係がなかった**（下記 1）。`da3c37f` に戻しても、PC に直接つないでも点滅した。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| 1. 書き込み後 RST | **合格** | `default data length set rc=0x0`。IST は `(stored key)` で起動から 0.67 秒。機器 B は電源が切れていたので、利用者が入れてから現れて 1.99 秒（`found by polling`） |
| **2. 機器 B の電源の入れ直し 5 回** | **合格** | 1.42 秒、2.10 秒、2.28 秒、2.14 秒、2.04 秒（現れてから `subscribed` まで）。5 回とも `found by polling`。`no answer` 0 回 |
| **3. M5Dial の RST 3 回** | **合格** | 4.38 秒、4.25 秒、4.39 秒（2 台そろうまで）。利用者が続けてもう 1 回押し、それも 4.38 秒。機器 B は `before the connect event` 2 回、`found by polling` 2 回。IST は毎回 4.0〜4.4 秒で、2 台そろう時間は IST で決まっている |
| 4. `WARNING`・`dropping the link`、止まる不具合 | **合格** | すべて 0 回。`encryption changed again` 0 回、`could not start encryption` 0 回、BLE ホストのリセット 0 回 |
| **4. 5 分間 2 台を使う** | **合格** | 17:52:00〜17:57:00。切断 0、2 台とも 291 秒すべて接続、`lost=0`。`LAT` 497 件、平均 0.84 ms、最大 2.03 ms、`unrelated=0`。**ただし操作は少なかった**（IST 14 秒、機器 B 4 秒） |

`broken 128-bit UUID`：0 回。A9：`heap_min` の最小 93,240。`lost=0`。A8：この版全体で `LAT` 7,868 件、平均 0.80 ms、最大 2.70 ms、`unrelated=0`。

### `device started security itself (...)` の括弧の値

10 回とも `enc=0 bonded=0 key_size=0 at this moment`。

### 機器 B の接続（この版の 12 回）

| 時刻 | 場面 | 暗号化の行の末尾 | 現れてから入力できるまで |
| --- | --- | --- | --- |
| 17:15:43 | 書き込み後、電源を入れた | `found by polling` | 1.99 秒 |
| 17:21:13 | 電源の入れ直し 1 | `found by polling` | 1.42 秒 |
| 17:23:23 | RST（LED の確認のため） | `found by polling` | 起動から 1.89 秒 |
| 17:29:51 | Pair new device の後、電源の入れ直し | `pairing mode, found by polling` | 2.66 秒 |
| 17:30:56 | 電源の入れ直し 2 | `found by polling` | 2.10 秒 |
| 17:32:23 | 電源の入れ直し 3 | `found by polling` | 2.28 秒 |
| 17:33:37 | 電源の入れ直し 4 | `found by polling` | 2.14 秒 |
| 17:34:14 | 電源の入れ直し 5 | `found by polling` | 2.04 秒 |
| 17:40:04 | RST 1 | `before the connect event` | 起動から 1.62 秒 |
| 17:42:12 | RST 2 | `found by polling` | 起動から 1.89 秒 |
| 17:50:56 | RST 3 | `found by polling` | 起動から 1.89 秒 |
| 17:51:01 | RST（追加） | `before the connect event` | 起動から 1.63 秒 |

- 末尾が何も無い `encryption on (stored key)`（NimBLE の知らせが普通に届く形）は、機器 B では 0 回。IST は毎回この形。
- 読み取りで受け入れた 10 回は、すべて登録済みのアドレスで鍵の指紋が変わっておらず、新しい条件で切られたことは無い。

## 分かったこと・確かめてほしいこと

### 1. 機器 B の LED の点滅は、M5Dial とは関係がなかった

- 利用者によると、17:21 の電源の入れ直しのあと、機器 B の LED は点滅したままだった。それでもペダルを踏むと PC に入力が届いた。17:23 の RST のあと、17:29 のペアリングモードでの接続のあとも点滅のままだった。
- 比べるため、18:12 に `da3c37f` を書き直した。`before the connect event` 1 回、`noticed by polling` 3 回の接続で、LED はずっと点滅だった。
- さらに利用者が 機器 B を PC に直接つないだところ、そこでも点滅した。**M5Dial の受け入れ方やデータ長の設定とは関係がない。**
- 利用者の見立て：電池の残りが少ないことを知らせている。手順書には電源の表示の段落があるが、取り出せた文字が途中で切れていて確かめられなかった。
- NimBLE が 機器 B の始めた手続きを最後まで終えていない問題（`found by polling` になる原因）は、LED とは別に残っている。今回は 5 分以上つないで切れなかった。

### 2. データ長の既定値の実験は効かなかった

- `default data length set rc=0x0` は毎回出た。
- それでも 機器 B の暗号化で NimBLE の知らせが届く形は 0 回。`found by polling` は 12 回中 10 回で、`da3c37f`（9 回中 6 回）より多い。
- `da3c37f` では RST のあと 3 回とも `before the connect event` だったが、この版では 4 回中 2 回が `found by polling`。データ長の設定で、接続の直後の流れが変わったのかもしれない（推測）。

### 3. そのほか

- Pair new device を押したあと、機器 B は保存した鍵のまま `pairing mode, found by polling` で受け入れられた。ペアリングモードでも、保存した鍵が使えれば新しいペアリングにはならない。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 17:13:12 | 1200 bps で書き込みモード → 書き込み → RST | 17:14:17 `START app=f2093b6`、`default data length set rc=0x0`、`bonds=2`。IST 0.67 秒 |
| 17:15:43 | 機器 B の電源を入れる | `found by polling`、1.99 秒 |
| 17:21:08〜17:21:14 | 電源の入れ直し 1 | 1.42 秒。LED が点滅したまま、入力は届く |
| 17:23:22 | RST（LED の確認） | `found by polling`。LED は点滅 |
| 17:25:53 | Pair new device | 機器 B はつながったまま |
| 17:29:47〜17:29:53 | 電源の入れ直し | `pairing mode, found by polling`。LED は点滅 |
| 17:30:46〜17:34:16 | 電源の入れ直し 2〜5 | 2.04〜2.28 秒 |
| 17:40:04 | **RST 1** | 4.38 秒 |
| 17:42:12 | **RST 2** | 4.25 秒 |
| 17:50:56、17:51:01 | **RST 3**（続けてもう 1 回） | 4.39 秒、4.38 秒 |
| 17:52:00〜17:57:00 | **5 分間 2 台を使う** | 切断 0、`lost=0`、`LAT` 最大 2.03 ms |
| 18:12:04 | LED を比べるため `da3c37f` を書き直す → RST → 機器 B の電源の入れ直し（2 回ずつ） | `before the connect event` 1 回、`noticed by polling` 3 回。LED はずっと点滅 |
| （その後） | 利用者が 機器 B を PC に直接つなぐ | PC でも LED は点滅 |

## 変わらないこと・未確認

- 機器 B の LED の点滅の意味（電池の残りの表示かどうか）。
- スリープからの復帰（機器 B が自動で眠らない）。
- 機器側で鍵を捨てたときの扱い（機器 B でペアリングを消す方法が分からない）。
- MD600・meteorite40 では、`a0d8f9e` 以降の版を試していない。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。
