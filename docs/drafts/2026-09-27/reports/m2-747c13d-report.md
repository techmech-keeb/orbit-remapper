# M2 の報告：版 `747c13d`（`orbit-m2-747c13d-ble.bin`、2026-10-01）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の短い確認（`102144d` ＋ 台帳の保存の仕方の変更、PR #27）を行った。`.bin` の SHA-256 は一致（`b5ca34ec…0466`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。命令は `102144d` と同じ方法（命令のファイルに書いた行を COM11 に送る）で送った。`M1` の行は全部ファイルに残した（`m2-747c13d-M1-lines.log` 2,411 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、Cube Turner PRO（public `71:2c`）。

## 結論

**手順 1〜4 はすべて合格。** 新しい形式で台帳を保存でき、RST のあとも別名が残った。`heap_min` は見込みどおり約 3.5 KB 増えた。

**ただし、手順書にある `ledger version 1 is not 2, starting empty` の行は出なかった**（下記 1。動きは正しい）。**台帳の保存でも入力が 1 件 8.32 ms 遅れた**（下記 2）。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| **1. 書き込み → 台帳の作り直し** | **合格（行は出ない）** | `ledger loaded boot=4 rows=0` → `ledger: bond addr=..:21:96 had no row, added as port 1`、`..:71:2c ... added as port 2`。2 台とも `(stored key)`、`DEV` の `port=` は IST 1・Cube Turner 2。情報を読んだあと `ledger: port 2 "TurnerPro"`、`ledger: port 1 "IST TrackBall"`。`ledger version ... starting empty` の行は出なかった |
| **2. 別名を付けて RST** | **合格** | `orbit alias 2 pedal` → `alias="pedal" shown="pedal"` → RST → 起動ログの `LDG` で `"pedal"`、つないだあと `ledger: port 2 "pedal"`、`orbit list` で `alias="pedal"` のまま。`orbit alias 2` で `shown="TurnerPro"` に戻した |
| **3. `heap_min`** | **見込みどおり** | 2 台つながった後：**87,972**（書き込み後）、**88,088**（RST 後）。`102144d` の約 84,400 より約 3.5 KB 増えた |
| **4. 3 分の使用** | **合格** | 00:11:30〜00:14:30。切断 0、2 台とも 175 秒すべて接続、`lost=0`。`LAT` 1,386 件、平均 0.69 ms、**最大 2.30 ms**、`unrelated=0`。入力は IST 28 秒、Cube Turner 0 秒（操作は少なめ） |

RST のあと 2 台そろうまで 4.26 秒。書き込み後は、IST のつながりが起動から 9.9 秒後だった（`102144d` の 1 回目と同じ。IST が眠っていたか、広告が遅れたと見ている）。

`WARNING`・`dropping the link`・`ledger save failed`・`ledger load failed`・`no answer`・止まる不具合・BLE ホストのリセット・`broken 128-bit UUID`：すべて 0 回。A8：この版全体で `LAT` 6,246 件、平均 0.78 ms、最大 8.32 ms（下記 2）、`unrelated=0`。

### `M1 NVS` の行（書き込み後と RST 後）

```
（書き込み後）M1 NVS namespace orbit: entries=129 config=1x2048B boots=1x0B ledger=1x1864B
（RST 後）    M1 NVS namespace orbit: entries=129 config=1x2048B boots=1x0B ledger_v=1x0B ledger=1x1860B
```

- 書き込み後は `ledger_v` が無く、`ledger` は 1,864 B（`102144d` の形式）。RST 後は `ledger_v` ができ、`ledger` は 1,860 B（新しい形式）に置き換わった。
- 全体は 4,032 エントリ中 231 を使用（`102144d` の起動時は 168）。

## 確かめてほしいこと

### 1. `ledger version 1 is not 2, starting empty` の行が出ない

- `ledger.c` の `orbit_ledger_init()` は、保存された版の番号（`KEY_VERSION`）が 0 のときはこの行を出さない（`version != 0 && version != LEDGER_VERSION`）。
- `102144d` は版の番号を保存していなかった（`KEY_VERSION` が無い）ので、読み出すと 0 になり、行が出ない。
- 動きは正しい：台帳は空から始まり、ペアリング情報から `added as port 1 / port 2` で作り直された。手順書の説明だけ直せばよいと見ている。版の番号が無い古い形式を捨てたことをログで分かるようにしたいなら、`version == 0` でも古い `ledger` があるときは行を出すとよい。

### 2. 台帳の保存でも入力が 1 件遅れた

- 00:06:36 に `orbit alias 2 pedal` を送った秒の `LAT` が `max=8.32ms`（`<=8/<=16/>16=32/1/0`）だった。`102144d` で設定ツールの保存のとき（9.40 ms）と同じ形。
- 別名を消したとき（00:10:53）の秒は、入力が無く比べられない。
- 保存の間は主ループが止まり、その間の入力が遅れると見ている（推測）。台帳の保存は別名やペアリングのときだけなので、実害は小さい。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 00:03:02 | 1200 bps で書き込みモード → 書き込み → RST | 00:05:31 `START app=747c13d`、台帳を作り直し（ポート 1・2） |
| 00:06:36 | `orbit alias 2 pedal` | `shown="pedal"`。同じ秒に `LAT` 8.32 ms |
| 00:10:23 | **RST** | 台帳が読み込まれ、`"pedal"` のまま。4.26 秒で 2 台そろう |
| 00:10:53 | `orbit alias 2` | `shown="TurnerPro"` |
| 00:11:30〜00:14:30 | **3 分の使用** | 切断 0、`lost=0`、`LAT` 最大 2.30 ms |

## 変わらないこと・未確認

- MD600 の B5（自動引き継ぎ）・B6（組み直しの許可）は行っていない。
- Cube Turner の LED の点滅の意味、スリープからの復帰、機器側で鍵を捨てたときの扱い。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。
