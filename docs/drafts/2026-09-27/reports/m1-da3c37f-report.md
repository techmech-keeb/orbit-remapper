# M1 の報告：版 `da3c37f`（`orbit-m1-da3c37f-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、クラウドのセッションから貼られた手順（`9309d56` の残り 1 場面の直し）を行った。`.bin` の SHA-256 は一致（`83cb2a04…5e45`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。`M1` の行は全部ファイルに残した（`m1-da3c37f-M1-lines.log` 4,939 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、Cube Turner PRO（public `71:2c`）。

書き込みの前に一度、M5Dial が PC から見えなくなった（COM11 も COM9 も無し）。利用者が USB を挿し直して戻った。原因は分からない。

## 結論

**合格の基準をすべて満たした。** Cube Turner の電源の入れ直し 5 回はすべて 3 秒以内、`no answer` 0 回。RST 3 回はすべて 5 秒以内。

**ただし、受け入れ方の安全面を確かめてほしい**（下記 1）。読み取りで受け入れた 6 回は、すべて `enc=1 bonded=0 key_size=0` だった。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| 1. 書き込み後 RST | **合格** | IST は `(stored key)` で起動から 0.59 秒。Cube Turner は電源が切れていたので、利用者が入れてから現れて 2.08 秒（読み取りで受け入れ） |
| **2. Cube Turner の電源の入れ直し 5 回** | **合格** | 2.33 秒、1.88 秒、2.13 秒、1.68 秒、1.80 秒。5 回とも `device started pairing itself` → `noticed by polling`。`no answer` 0 回 |
| **3. M5Dial の RST 3 回** | **合格** | 2.62 秒、4.38 秒、4.36 秒。3 回とも Cube Turner は `before the connect event`（1.6〜1.9 秒）。IST は 2.6〜4.4 秒 |
| 4. `waiting for encryption:` の前後 | 報告 | 下記。6 回とも最初の読み取りで `enc=1` |
| **5. 5 分間 2 台を使う** | **合格** | 15:40:00〜15:45:00。切断 0、2 台とも 290 秒すべて接続、`lost=0`。入力は IST 98 秒、Cube Turner 7 秒。`LAT` 4,913 件、平均 0.89 ms、最大 2.51 ms、`unrelated=0` |
| 5. `WARNING`、止まる不具合 | **合格** | `WARNING`（`key replaced outside pairing mode` を含む）0 回、`encryption changed again` 0 回、止まる不具合 0 回、BLE ホストのリセット 0 回、`could not start encryption` 0 回 |

`broken 128-bit UUID`：0 回。`lost the bond`・`ignoring the device`：0 回。A9：`heap_min` の最小 93,360。`lost=0`。A8：この版全体で `LAT` 9,921 件、平均 0.88 ms、最大 2.53 ms、`unrelated=0`。

### `waiting for encryption:` の前後（手順 4）

6 回とも同じ流れだった。例（15:28:05、電源の入れ直し 1 回目）：

```
[15:28:05.740] D1 ..:71:2c connecting via accept list after 7.1 s of waiting
[15:28:05.741] D1 ..:71:2c device started pairing itself
（L2CAP の間隔の要求 2 回、15 ms に更新）
[15:28:06.768] D1 ..:71:2c waiting for encryption: enc=1 bonded=0 key_size=0
[15:28:06.768] D1 ..:71:2c encryption on (stored key), noticed by polling
（探索 → Report Map 297 バイト）
[15:28:08.074] D1 ..:71:2c subscribed 6 input report(s)
```

| 時刻 | 場面 | つながってから受け入れまで | 現れてから入力できるまで |
| --- | --- | --- | --- |
| 15:21:45 | 書き込み後、電源を入れた | 0.85 秒 | 2.08 秒 |
| 15:28:05 | 電源の入れ直し 1 | 1.03 秒 | 2.33 秒 |
| 15:29:13 | 電源の入れ直し 2 | 0.65 秒 | 1.88 秒 |
| 15:30:34 | 電源の入れ直し 3 | 0.91 秒 | 2.13 秒 |
| 15:31:19 | 電源の入れ直し 4 | 0.46 秒 | 1.68 秒 |
| 15:32:07 | 電源の入れ直し 5 | 0.58 秒 | 1.80 秒 |

- 6 回とも、最初の読み取りですでに `enc=1` だった。受け入れた後に `ENC_CHANGE`（`encryption changed again`、`WARNING`）は一度も来なかった。
- 電源の入れ直しでは 5 回とも `device started pairing itself` の場面になり、RST では 3 回とも `before the connect event` になった。

## 確かめてほしいこと

### 1. 読み取りで受け入れるとき、`bonded=0 key_size=0` を「保存した鍵」とみなしてよいか（安全面）

- `da3c37f` は、`enc=1` なら `bonded` と `key_size` を見ずに受け入れ、`encryption on (stored key)` と出す。今回の 6 回は、すべて `bonded=0 key_size=0` だった。
- ソースの説明は「コントローラは、そのアドレスに対して持っている鍵でしか暗号化しない」。**ただ、Legacy のペアリングの途中（STK での暗号化）でも `enc=1` になるはず。** ペアリングモード外で新しいペアリングの途中まで進んだ接続を、保存した鍵での暗号化と見分けられない可能性がある。そうなら、アドレスをまねた機器が、通常時に入力を送り込める。
- 見立て（推測）：`bonded=0 key_size=0` のまま `enc=1` になるのは、NimBLE が暗号化の知らせを自分の手続きと結びつけられなかったとき（手続きが見つからないと、sec_state の暗号化だけを立てる）と見ている。新しいペアリングの途中なら、手続きがあるので key_size が入るはず。そうなら今回は保存した鍵だった可能性が高い。**NimBLE のソースで確かめてはいない。**
- 案：
  - NimBLE の `ble_sm` で、「手続きが見つからない暗号化の知らせ」のときに sec_state がどう設定されるかを確かめる。
  - 受け入れる条件を固める。例：こちらが保存した LTK で暗号化を始めた接続で、かつ NimBLE のペアリングの手続きが動いていないときだけ受け入れる。または、受け入れた後に保存した鍵の指紋が変わっていないことを確かめる。
  - 読み取りで受け入れたときは、行の文言を `(stored key)` ではなく、根拠が分かる形（例：`(encrypted, key not confirmed)`）にする。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 15:11 | 1200 bps で書き込みモードに入れようとした | COM11 が無い。M5Dial が PC から見えない |
| 15:18 | 利用者が USB を挿し直す → 1200 bps で書き込みモード → 書き込み → RST | 15:19:44 `START app=da3c37f`、`bonds=2`。IST 0.59 秒 |
| 15:21:45 | Cube Turner の電源を入れる | 読み取りで受け入れ、2.08 秒で `subscribed 6` |
| 15:27:58〜15:32:09 | **Cube Turner の電源の入れ直し 5 回** | 1.68〜2.33 秒。5 回とも読み取りで受け入れ |
| 15:38:22 | **RST 1** | 2.62 秒で 2 台そろう |
| 15:38:57 | **RST 2** | 4.38 秒 |
| 15:39:34 | **RST 3** | 4.36 秒 |
| 15:40:00〜15:45:00 | **5 分間 2 台を使う** | 切断 0、`lost=0`、`LAT` 最大 2.51 ms |

## 変わらないこと・未確認

- スリープからの復帰（Cube Turner が自動で眠らない、`9309d56` で確認）。
- 機器側で鍵を捨てたときの扱い（Cube Turner でペアリングを消す方法が分からない）。
- MD600・meteorite40 では、`a0d8f9e` 以降の版を試していない。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。
