# M1 の報告：版 `d2c4295`（`orbit-m1-d2c4295-ble.bin`、2026-09-28〜29）

利用者の PC（Windows 11、PC 本体のポート）と初代 M5Dial で、クラウドのセッションから貼られた手順（`f102c45` ＋ G'' の直し）を行った。`.bin` の SHA-256 は一致（`d7321c0f…28f3`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）。`M1` の行は全部ファイルに残した（`m1-d2c4295-M1-lines.log` 16,237 行。利用者の PC に保存、リポジトリには入れていない）。機器：MD600（キーボード）、meteorite40（トラックボール付きキーボード）。

## 結論

**合格の基準をすべて満たした。**

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| **合格の基準 2**（G''：5 秒ごとのくり返しにならず、1 分以内に入力できる） | **合格** | 探索の途中で MD600 が切る場面が出た。`discovery stalled for 5 s` → つなぎ直し → **`discovery stalled again (2 in a row), keeping the link until the ATT timeout`** → 30 秒の時間切れ → `nothing subscribed` → つなぎ直し → **0.7 秒で `subscribed 4`**。**つながり始めてから入力できるまで 36.7 秒**。くり返しは 2 回で止まった |
| **合格の基準 4**（10 分で切断 0） | **合格** | 23:07:08〜23:17:08 の 10 分間、切断 0、2 台とも 581 秒すべて 7.50 ms・latency 0、`conn=2/2`、`lost=0`、`LAT` 3,555 件で平均 0.72 ms・最大 1.93 ms・`unrelated=0`。**ただし操作は断続的**（meteorite40 57 秒、MD600 28 秒、2 台同時 18 秒） |
| 1. 書き込み後の接続 | 合格 | 2 台ともスリープ中だったため `ble ready` から 24〜36 秒（起こすまで広告なし）。`connecting` から `subscribed` まで meteorite40 1.0 秒、MD600 0.92 秒 |
| 3. 電源の入れ直し | **合格** | MD600 0.61 秒、meteorite40 0.69 秒（`connecting` → `subscribed`） |
| 3. スリープからの復帰 | **合格（条件つき）** | 2 台とも自然にスリープ（meteorite40 は最後の操作から約 15 分、MD600 は約 23 分、`hci=0x08`）。起こしたとき、meteorite40 0.69 秒、MD600 0.62 秒でつながり直した。**ただし起こす前に PC がスリープし、PC の復帰時に M5Dial も再起動していた**ので、「M5Dial が動いたまま機器だけが起きる」場面は測れていない |
| Forget all devices → 2 台ペアリング | 合格 | meteorite40 は古い鍵で 1 回 `0x503` → 30 秒避ける → 利用者が BT クリア → 30 秒明けに新しい鍵でペアリング |
| 止まる不具合 | **0 回** | `connect attempt stuck`・BLE ホストのリセット・`connect failed before the connect event` は、この版では一度も出なかった |
| A5 BIOS | 先送り | |

A9：`heap_min` の最小 91,444（合格のまま）。A8：この版全体で 4,168 件、平均 0.72 ms、最大 2.31 ms、`unrelated=0`。A6（PC のスリープ）：01:27:58 に `usb suspended remote_wakeup_enabled=1`（PC が眠った）。今回はキーで起こす試験はしていない。

### G'' の詳細（MD600、新アドレス `1c:b0`）

| 時刻 | できごと |
| --- | --- |
| 23:02:32.5 | ペアリング（`encryption on`）→ 探索の途中で MD600 が切る（`characteristic discovery failed status=0x7` → `hci=0x13`） |
| 23:02:32.9 | 保存した鍵でつなぎ直し、`encryption on` → 探索が進まない |
| 23:02:38.2 | `discovery stalled for 5 s, dropping the link to retry` → 切断 → つなぎ直し → `encryption on` |
| 23:02:44.5 | `discovery stalled again (2 in a row), keeping the link until the ATT timeout` |
| 23:03:08.5 | `service discovery failed status=0xd`（30 秒の時間切れ）→ `nothing subscribed, dropping the link to retry` → 切断 → 0.005 秒後につなぎ直し |
| 23:03:09.2 | `found 1 HID service(s)` → Report Map 192 バイト → **`subscribed 4`** |

`f102c45` の「5 秒ごとに 10 回くり返して直らない」は起きなかった。`a195bc8` の「30 秒待ってからつなぎ直すと直る」と同じ形になった。

## 気づいたこと

- **PC のスリープ中に M5Dial が再起動した**：01:27:58 に USB が休止し、PC の復帰（翌 06:00 ごろ）の時点で M5Dial は起動し直していた（`START`、`t` がゼロに戻る、`usb mounted` が起動 8 秒後と 46 秒後に 2 回）。PC がスリープ中に USB の電源を切ったか、復帰時にリセットがかかったと見ている（推測）。ペアリング情報と設定は残っていた。
- **ログ取りのスクリプトが PC のスリープを越えられない**：スリープの間にポートが消えても、読み込みが止まったまま例外にならず、つなぎ直さなかった（ログ取り側の問題。M5Dial は 16 KB までためていたので、起動後の分は読めた）。
- ペアリング情報は 2 件（MD600 `1c:b0`＝ポート 1、meteorite40＝ポート 2）。Forget all devices で古い MD600 の情報は消えた。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 22:59:50 | 書き込み（1200 bps で書き込みモード）→ RST | `START app=d2c4295`、`bonds=3` |
| 23:00:13〜23:00:26 | 2 台を起こす | meteorite40 → MD600 の順に自動接続（それぞれ約 1 秒） |
| 23:02:19 | **Forget all devices** | 2 台とも切断 → 古い鍵で 1 回ずつ失敗 → 30 秒避ける |
| 23:02:32〜23:03:09 | MD600 をリセット | 新アドレス `1c:b0` → G''（上記）→ 36.7 秒で入力できるように |
| 23:03:58〜23:04:31 | Pair new device → meteorite40（古い鍵 → BT クリア） | 30 秒明けに新しい鍵でペアリング |
| 23:07:08〜23:17:08 | **10 分の連続使用** | 切断 0、`lost=0`、`LAT` 最大 1.93 ms |
| 23:19:21〜23:19:37 | MD600 の電源の入れ直し | 0.61 秒でつながり直し |
| 23:22:33〜23:23:10 | meteorite40 の電源の入れ直し | 0.69 秒でつながり直し |
| 23:38:24 | （触らず） | meteorite40 がスリープ（`hci=0x08`） |
| 23:42:28 | （触らず） | MD600 がスリープ（`hci=0x08`） |
| 01:27:58 | （PC がスリープ） | `usb suspended remote_wakeup_enabled=1` |
| 翌 06:00 ごろ | PC 復帰、M5Dial 再起動、2 台を起こす | meteorite40 0.69 秒、MD600 0.62 秒でつながり直し |

## 変わらないこと・未確認

- A5（BIOS）：先送り。
- M5Dial が動いたままの「機器だけのスリープからの復帰」：この版では測れていない（前の版では問題なし）。
- 10 分の連続使用を「動かし続けたまま」で行う（今回は断続的）。
- ハブ経由の問題（`3662016` で見つかったもの）は未確認のまま。
