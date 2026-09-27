# M1 の報告：版 `6254d16`（`orbit-m1-6254d16-ble.bin`、2026-09-27〜28）

利用者の PC（Windows 11、PC 本体のポート）と初代 M5Dial で、m1-handoff.md §6.1（`0d00f22` 版）の 1〜8 を進めた。`.bin` の SHA-256 は一致。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）。`M1` の行は全部ファイルに残した（`m1-6254d16-M1-lines.log` 7,339 行、`m1-86a2d16-M1-lines.log` 4,343 行。利用者の PC に保存、リポジトリには入れていない）。機器：MD600（キーボード）、meteorite40（トラックボール付きキーボード）。

## 結論

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| §6.1-1 書き込み後の自動接続 | **合格** | `ble ready bonds=1` → MD600 がキー 1 つで自動接続。**書き込みでペアリング情報が消えなくなった**（NVS をアプリの後ろに移した効果）。`SUM` の `heap_min` も正しい値 |
| A2 2 台・7.5 ms | **合格** | 設定ツールに `HID Remapper Bluetooth …` と Pair new device が出た。2 台とも `itvl=6(7.50ms) lat=0`、キーとトラックボールが PC で動いた |
| A3 保存・再起動後も残る | **合格** | A→B を追加して Save（`config saved err=0x0`、8.2 ms）→ A で b → RST → `config loaded from NVS err=0x0` → A で b、Load from device で残っている |
| A4 設定ツールの命令 | **合格** | Pair new device（`pair_new_device` → `scan start (pairing)`）、Flash firmware（`entering download mode (config tool)` → VID 303A）、Forget all devices（`clear_bonds rc=0x0`、2 台とも `hci=0x16` で切断 → `scan start (pairing)`）。書き込みモードへの戻り方は 3 つとも確認済みになった |
| A7 10 分 | **接続は合格、操作量は不足** | 23:41:32〜23:51:32：切断 0、2 台とも 580 秒すべて 7.50 ms、`conn=2/2`、`lost=0`。ただし 2 台同時に動かしていたのは 78 秒（meteorite40 208 秒、MD600 90 秒） |
| A8 遅れ 3 ms 以内 | **判定保留**（測り方の問題） | 18,054 件の加重平均 0.95 ms、98.9% が 8 ms 以下。1 秒ごとの最大は 326 秒中 244 秒で 3 ms 以内だが、meteorite40 が動いているときに 7.8／15.5／23.4／30.8／38.4／45.8／53.4／83.4／94.2 ms（≒7.5 ms の倍数）が出る。下記 |
| A9 空きメモリ 50 KB 以上 | **合格** | `heap_min` の最小 **74,500**（設定を保存した瞬間に約 18 KB 下がる）。ふだんは 92〜98 KB |
| A10 自動再接続 | **合格** | M5Dial の RST（2 台とも約 4 秒／1 秒以内）、書き込み直後のスリープ中の MD600、MD600 と meteorite40 の電源の入れ直し。どれも保存した鍵で暗号化、アドレス変わらず、7.50 ms |
| A5 BIOS、A6 PC のスリープ解除 | 未確認 | 今回は行っていない |

## A8 の大きな値について（推測）

- MD600 だけのときは最大 1.86 ms。大きな値は meteorite40 が動いているときだけ（meteorite40 だけを動かしていた時間にも出る）。
- 値が接続間隔 7.5 ms のほぼ倍数にそろい、100 ms 未満で頭打ち。
- README §6 の測り方は「まだ送信に反映されていない最も古い受信」を基準にし、出力が変わらなければ 100 ms で捨てる。**出力を変えない報告（本家コアの中で次の報告と合算される小さな動きなど）を基準にしたまま、出力が変わる次の報告まで待ってしまい、「次に出力が変わるまでの時間」を測っている**と見ている。実際に動きのある報告の遅れは 1〜2 ms と見ている。
- 直してほしい：出力が変わった報告だけを数える、または受信した報告ごとに印を付けて、その報告を反映した送信で測る。直した版で A8 を測り直したい。

## 直してほしいこと・確かめてほしいこと

1. **`LAT` の測り方**（上記）。
2. **暗号化に失敗した接続を、本体が持ったままにする。** 機器側に古い鍵が残っていると、`encryption failed`（MD600 は `0x505`、meteorite40 は `0x503`）→ `subscribed 0` のまま接続が残り、2 つしかない枠を埋める。Forget all devices の直後に 2 台とも古い鍵でつながりに来て、両方の枠が埋まった。**暗号化に失敗したら本体から切って、ペアリング待ちに戻す**（または、ペアリング待ちでは新しいペアリングを始め直す）のがよい。
3. **ペアリングし直すと本家のポート番号が入れ替わる。** 1 回目：MD600＝1、meteorite40＝2。Forget all devices 後：meteorite40＝1、MD600＝2（ペアリングした順）。本家どおりの動きで、M2 の決定 I4（住所に固定）で直す予定の問題。
4. **meteorite40 が latency 30 になることがある。** Forget all devices の後のペアリングでは、暗号化のあとに meteorite40 から変更要求（`LL update req ... itvl=6-12 lat=30`）が来て、`device allows 7.50ms, asking for it again` で 7.50 ms・**latency 30** になった。それまでの接続（`lat=0`）では要求は来なかった。暗号化に失敗した接続でも同じ要求が来た。latency 30 の影響（入力の遅れ、途切れ）は未確認。latency も 0 を求め直すかは検討してほしい。
5. `EVT config loaded from NVS err=0x0 (non-zero: defaults)`：`err=0x0` でも `(non-zero: defaults)` が付く。説明文を固定で付けているだけで実害はないが、読み違えやすい。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 23:27:13 | 書き込み（PC から 1200 bps で書き込みモード）→ RST | `START app=6254d16`、`config ... err=0x1102`（未保存）、`ble ready bonds=1`、`scan start (bonded devices only)` |
| 23:27:44 | MD600 のキーを押す（スリープ中） | MD600（`26:27`）が自動接続。`encryption on`、Report Map 192 バイト、4 つ登録 |
| 23:29:30 | 設定ツールで Pair new device | `pair_new_device` → `scan start (pairing)` |
| 23:29:50 | meteorite40（古い鍵のまま） | `encryption failed status=0x503` → `subscribed 0` |
| 23:30:08 | meteorite40 のペアリングを消す | つなぎ直し → `encryption on`、Report Map 198 バイト、3 つ登録。`conn=2/2` |
| 23:31〜23:32 | 2 台同時に 30 秒 | 2 台とも 7.50 ms、`lost=0`。`LAT` に最大 83 ms（上記） |
| 23:37:12 | A→B を追加して Save | `config saved err=0x0`、8.2 ms。A で b |
| 23:39:18 | RST | `config loaded from NVS err=0x0`、`bonds=2`。2 台とも約 4 秒で自動接続。A で b、Load from device で残っている |
| 23:41:32〜23:51:32 | A7（10 分） | 切断 0、7.50 ms のまま、`lost=0`、`heap_min` 92,396 |
| 23:55:25〜23:56:28 | MD600 の電源の入れ直し | 自動接続、保存した鍵、7.50 ms |
| 23:56:55〜23:57:26 | meteorite40 の電源の入れ直し | 自動接続、保存した鍵、7.50 ms |
| 23:58:26 | 設定ツールで Flash firmware | `entering download mode (config tool)` → VID 303A（COM9）。設定ツールは本家 Pico 向けの「UF2 をコピー」の案内を出すが、無視してよい |
| 23:59:38 | RST | 2 台とも 1 秒以内に自動接続 |
| 00:00:22 | 設定ツールで Forget all devices | `clear_bonds rc=0x0`、2 台とも `hci=0x16` で切断 → `scan start (pairing)`。直後に 2 台とも古い鍵で接続し、暗号化に失敗して枠を埋めた |
| 00:01〜00:03 | Pair new device を 2 回、MD600 は見つからず | 原因は MD600 がまだ古い鍵を持っていたこと（後で判明） |
| 00:03:52 | RST | `bonds=0`、`scan start (pairing)` |
| 00:04:28 | meteorite40 のペアリングを消す | 新しい鍵で接続、3 つ登録（ポート 1）。変更要求で 7.50 ms・latency 30 |
| 00:05:11〜00:05:35 | MD600（古い鍵）→ 利用者が MD600 のペアリングを消す | 古い鍵で `encryption failed 0x505` → 相手が切断 → `scan start (bonded devices only)` |
| 00:06:00 | Pair new device → MD600 | 新しいアドレス `5a:1d` で接続、4 つ登録（ポート 2）。`conn=2/2` |
| 00:07:15 | A→B を消して Save | `config saved err=0x0`、10.3 ms。A で a に戻った（利用者が確認） |

## 変わらないこと

- ハブ経由の問題（`3662016` で見つかったもの）は未確認のまま。
- A5（BIOS）、A6（PC のスリープ解除）は未確認。
- A7 を「2 台を動かし続けて 10 分」で厳密にやり直すかは利用者が判断する。
