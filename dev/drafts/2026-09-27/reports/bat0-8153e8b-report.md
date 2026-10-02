# 電池の段階 0 の報告：版 `fda5c4e`（`orbit_hid-remapper_v0.1.0_20261002-fda5c4e.bin`、PR #5、2026-10-02）

利用者の PC（Windows 11）と初代 M5Dial で、`dev/drafts/2026-10-02/battery.md` §3 の試験を行った（techmech-keeb/orbit-remapper#5）。書き込んだのは PR #5（`8153e8b`）を `main` に合わせてビルドした `fda5c4e` で、起動の行は `M1 START idf=v5.5.5 app=fda5c4e version=0.1.0 upstream=51ab8b3`。`.bin` の SHA-256 は、ローカルのセッションでファイルを照合して `fa43fa88…c1eefa`（依頼の値と一致）。書き込みモードへは PC 側から 1200 bps で入れ、利用者が書き込みツールで `0x0` に書き込んだ。ログは COM8（115200 bps、DTR あり）で、起動の最初から取れた（ログの全文は利用者の PC に保存、リポジトリには入れていない）。

**今回は IST Trackball の分だけ**。Mistel MD600 と機器 B は手元に無かったので、戻ってから同じ版で続け、この報告に書き足す。

**ペアリングの情報は消えた状態から始まった。** 利用者が初めに書き込むアドレスを間違え、「Erase Flash」をしてから `0x0` に書き直したため（ファームのせいではない）。起動の行は `config loaded from NVS err=0x1102 (using defaults)`、`ledger loaded boot=1 rows=0`、`ble ready bonds=0`。書き込みの前は `bonds=3` だった。設定も既定に戻ったので、画面の向きは利用者がメニューから 180° にし直した。

## 結論

**IST Trackball は Battery Service が 1 つ、Battery Level が 1 つで、残量は読めた（70%）が、通知には対応していない**（`notify=0`。35 分つないだままで通知 0）。調べる処理はつないでから 0.5〜0.7 秒で終わり、その前後で `LAT` は悪くならなかった（3 ms を超えた秒 0）。調べる処理のあとも、入力・切断・つなぎ直しは今までどおり。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| **2. battery の行** | **読めた** | Battery Service 1、Battery Level 1、`read=1 notify=0 cccd=1`、説明なし、`level=70%`。初回のペアリングとつなぎ直しの 2 回とも同じ |
| **3. 機器の側の値と見比べる** | **未確認** | 機器の側の値は見ていない（PC に直接つなぐと、Orbit とペアリングし直しになるため） |
| **4. 30 分以上の使用** | **通知なし（IST は非対応）** | 12:52:45〜13:27:44 の 35 分。`notify` の行 0、切断 0、`lost=0`、2,093 回の状態の行すべてで接続。`EVT` の行そのものが 0 |
| **5. 調べる処理の前後の `LAT`** | **合格** | 調べる処理を含む秒（t=159）の `max` 1.78 ms。前後とも 3 ms 超 0 |
| **6. 入力・切断・つなぎ直し** | **合格** | 電源の入れ直し → 2.3 秒で保存した鍵でつなぎ直し、入力の登録まで 0.32 秒、そのあと probe |

## 機器ごとの結果

### IST Trackball（static `22:96`、ポート 1）

| 項目 | 値 |
| --- | --- |
| Battery Service | 1 |
| Battery Level | 1（handle 48） |
| read／notify | read=1、notify=0 |
| CCCD | あり（`cccd=1`） |
| User Description／Presentation Format | なし／なし |
| 読めた値 | 70%（12:50:01 と 12:52:25 の 2 回） |
| 機器の側の値 | 見ていない |
| 通知 | 対応していない（`notifications on` の行なし） |

初回のペアリング（12:50:01）：

```
M1 EVT t=14.538 D0 addr=..:22:96 battery: 1 Battery Service(s)
M1 EVT t=14.575 D0 addr=..:22:96 battery: 1 Battery Level characteristic(s)
M1 EVT t=14.598 D0 addr=..:22:96 battery 0: handle=48 read=1 notify=0 cccd=1 user_desc=0 presentation=0
M1 EVT t=14.620 D0 addr=..:22:96 battery 0: level=70% (1 byte(s))
M1 EVT t=14.621 D0 addr=..:22:96 battery: probe done
```

電源の入れ直しのあと（12:52:25）：

```
M1 EVT t=158.141 D0 addr=..:22:96 connected itvl=6(7.50ms) lat=0 to=400(4000ms)
M1 EVT t=158.186 D0 addr=..:22:96 encryption on (stored key)
M1 EVT t=158.463 D0 addr=..:22:96 subscribed 1 input report(s)
M1 EVT t=158.554 D0 addr=..:22:96 ledger: port 1 "IST TrackBall" kind=mouse
M1 EVT t=158.575 D0 addr=..:22:96 battery: 1 Battery Service(s)
M1 EVT t=158.605 D0 addr=..:22:96 battery: 1 Battery Level characteristic(s)
M1 EVT t=158.621 D0 addr=..:22:96 battery 0: handle=48 read=1 notify=0 cccd=1 user_desc=0 presentation=0
M1 EVT t=158.650 D0 addr=..:22:96 battery 0: level=70% (1 byte(s))
M1 EVT t=158.651 D0 addr=..:22:96 battery: probe done
M1 LAT t=159 n=45 avg=0.95ms max=1.78ms <=8/<=16/>16=45/0/0 unrelated=0
```

- 調べる処理（Battery Service を探してから `probe done` まで）は約 0.08〜0.11 秒。つないでからは 0.71 秒（初回、ペアリングと台帳の保存を含む）と 0.51 秒（つなぎ直し）。
- 入力の登録（t=158.463）が調べる処理より先に済むので、調べている間もボールの入力は届いていた（t=159 の 45 件）。

## LAT（調べる処理の前後）

| 区間 | `LAT` のある秒 | 件数 | 平均 | 最大 | 3 ms 超の秒 | `unrelated` |
| --- | --- | --- | --- | --- | --- | --- |
| 初回のペアリングのあと（12:49:47〜12:52:21） | 10 | 419 | 0.93 ms | 2.09 ms | 0 | 0 |
| つなぎ直しの直後、ボールを回し続けた（12:52:25〜12:52:45） | 19 | 1,607 | 0.95 ms | 2.77 ms | 0 | 0 |
| 35 分の使用（12:52:45〜13:27:44） | 200 | 15,321 | 0.97 ms | 2.77 ms | 0 | 0 |

初回のペアリングの直後は入力が無く、調べる処理の最中の `LAT` は取れていない。つなぎ直しのほうで取れた（t=159）。

段階 0 は読み直しをしない（1 回読むだけ）ので、35 分の間に電池を読む処理は動いていない。

## 気づいたこと

- **IST は Battery Level に CCCD（0x2902）があるのに、性質では通知に対応していない（`notify=0 cccd=1`）。** ファームは性質を見て登録しなかった。段階 1 では、IST は「10 分ごとに読み直す」側になる。CCCD があるからといって登録しに行かないほうがよい（推測）。
- ペアリングの情報が消えたあと、IST は起動から 13.9 秒で新しい static アドレス `..:22:96` でペアリングし直した（前は `..:21:96`。`pnp_id` は同じ `vid=0x056e pid=0x0184`）。
- Erase Flash のあとの初回の起動で、`phy_init: failed to load RF calibration data (0x1102), falling back to full calibration` が 1 回出た。消したあとの初回は毎回出るもので、問題ではない（推測）。
- `heap_min` の最小は 94,160（ペアリングの直後から変わらない）。
- 35 分の使用は軽め（入力のあった秒は 200、多いのは 12:55 と 13:03 の各 35〜37 秒）。IST を 1 台だけで大きく使い続けた試験ではない。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 12:46:55 | 前の版でログを取り始める | `bonds=3`、IST（`..:21:96`）がつながっている |
| 12:48 | ログのポートを 1200 bps で開いて閉じる | 書き込みモード（COM7） |
| 12:48〜12:49 | 書き込み（アドレスを間違え → Erase Flash → `0x0` に書き直し）→ RST | `START app=fda5c4e`、`bonds=0` |
| 12:50:00〜12:50:01 | IST をペアリング | ポート 1、`battery: probe done`、70% |
| 12:50:26〜12:50:32 | メニューで画面を 180° に | `screen: rotation 180` |
| 12:52:22〜12:52:25 | IST の電源を入れ直す | 切断（`reason=0x208`）→ 2.3 秒でつなぎ直し、probe、70% |
| 12:52:25〜12:52:45 | ボールを回し続ける | `LAT` 最大 2.77 ms、3 ms 超 0 |
| 12:52:45〜13:27:44 | 35 分の使用（ふだんどおり。入力のあった秒は 200） | 切断 0、通知 0、`LAT` 最大 2.77 ms、3 ms 超 0 |

## 未確認

- Mistel MD600、機器 B（手元に無い。戻ってから同じ版で続ける）。
- 通知に対応している機器での、最初の通知までの時間・間隔・値の変わり方。
- IST の機器の側の残量（PC に直接つないだときの値）。
