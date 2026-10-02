# M2 の報告：版 `302af13`（`orbit-m2-302af13-ble.bin`、2026-10-01）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の確認（`230e758` の 3 点の直し、PR #27）を行った。`.bin` の SHA-256 は一致（`f430736f…c7c5`）。書き込みモードへは PC 側から 1200 bps で入れた（前の試験の書き込みツールのタブが COM11 をつかんでいて、一度開けなかった。タブを閉じて解決。書き込みの前に M5Dial が PC から見えず、USB を挿し直した）。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。命令は `102144d` と同じ方法で送り、IST を動かし続ける場面では、入力が 1 秒に 80 件以上届いているのを確かめてから送った。`M1` の行は全部ファイルに残した（`m2-302af13-M1-lines.log` 5,265 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、機器 B（public `71:2c`）。

## 結論

**3 点のうち 2 点（保存の期限、探索中の入力の減り）は直った。誤った `WARNING` は直っていない。** 機器 B をペアリングし直すたびに（2 回中 2 回）、`WARNING: key replaced outside pairing mode` が出て接続が切られた（下記 1）。合格の基準は満たさない。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| **1. IST を動かしながら `orbit alias 2 pedal`** | **合格（60 秒超は未確認）** | 06:44:45 に命令。保存は 18 秒後の 06:45:03.580（`9.2 ms, why=set_alias`）。1 秒ごとの集計では途切れは最大 218 ms だが、集計の区切りをまたいで 1 秒ほど入力が途切れた直後に書いた。命令から保存の後までの `LAT` は最大 2.74 ms、`unrelated=0`。利用者の手が 17 秒ほどで一度止まったので、「60 秒を超えて動かし続けても書かない」ことは確かめていない。`orbit alias 2` の保存も、入力が 1 秒途切れた後（06:45:57） |
| **2. 動かしながら `orbit forget 2` → `orbit pair`（2 回）** | **不合格（`WARNING` 2 回）** | 2 回とも：`forget` の保存は直後に 1 回（`forced`）。`scan start (pairing, listening 10/100 ms)`。機器 B の 1 回目の接続で **`encryption on (no key stored) pairing mode, at pairing complete (no ENC_CHANGE event)` → 0.03 秒後に `WARNING: key replaced outside pairing mode` → 切断** → 0.7〜0.9 秒後のつなぎ直しでポート 2（下記 1） |
| 2. 探索中の IST の入力 | **合格** | 探索とペアリングの間も、IST の `rpt=` は 1 回目 74〜132、2 回目 107〜134。`230e758` の 6〜9 は起きなかった |
| 2. 保存のまとまり | **合格** | 2 回とも、ペアリングの保存は手を止めた後の 1 回（`why=add`）だけ |
| 2. `pair_new_device` から `hub_port=2` まで | 7.84 秒、8.18 秒 | 探索の割合を下げたので、機器 B が見つかるまで 5.2〜5.3 秒。`WARNING` の後のつなぎ直しの約 1 秒も含む |
| 2. その間の `LAT` | **ほぼ合格** | `forced` の秒（6.43 ms）を除いて、3 ms を超えたのは `hub_port=2` になった秒の 4.13 ms（1 回目）と、その直後の 3.58 ms（2 回目） |
| **3. 3 分の使用** | **合格** | 06:56:51〜06:59:51。切断 0、2 台とも 174 秒すべて接続、`lost=0`。`LAT` 13,271 件、平均 0.88 ms、**最大 2.71 ms**（3 ms 超の秒 0）、`unrelated=0`。入力は IST 143 秒、機器 B 20 秒。この間の保存は 0 回 |

書き込み後の起動では、保存 2 回（`touch` 9.1 ms、`touch` 34.7 ms）とも入力の前。IST は起動時に `encryption on (stored key), at pairing complete (no ENC_CHANGE event)` になった（これまでの版はふつうの `(stored key)`）。

`WARNING`：**2 回**。`ledger save failed`・`no answer`・止まる不具合・BLE ホストのリセット・`broken 128-bit UUID`：0 回。`dropping the link`：6 回（`forgotten` 2、`key replaced outside pairing mode` 2、`nothing subscribed` 2）。`heap_min` の最小 87,712。A8：この版全体で `LAT` 33,260 件、平均 0.88 ms、最大 6.43 ms（`forced` の秒）、`unrelated=0`。

### 3 ms を超えた秒（版全体）

| 秒 | `LAT` 最大 | そのとき |
| --- | --- | --- |
| 06:45:56 | 3.10 ms | IST を大きく動かし続けていた。保存・接続なし |
| 06:46:10 | 3.14 ms | 同上 |
| 06:46:34 | 4.13 ms | 1 回目で `hub_port=2` になった秒 |
| 06:55:55 | 6.43 ms | `forget, forced` の保存（9.3 ms） |
| 06:56:07 | 3.58 ms | 2 回目で `hub_port=2` になった直後 |
| 06:56:11 | 3.06 ms | 大きく動かし続けていた。保存・接続なし |
| 06:56:22 | 3.29 ms | 同上 |

- 1 秒に 130 件前後の入力が続くと、3 ms をわずかに超える秒がたまに出る。
- `hub_port=2` になるとき（記述子を渡して入力を始めるとき）に 3.5〜4 ms が出た。

## 直してほしいこと

### 1. ペアリングの完了の時点で受け入れると、誤った `WARNING` で切られる（2 回中 2 回）

2 回目（06:55:58〜）の流れ：

```
pair_new_device → scan start (pairing, listening 10/100 ms)
connecting ... adv_name="" adv_appearance=0x0000
device started security itself (enc=0 bonded=0 key_size=0 at this moment)
encryption on (no key stored) pairing mode, at pairing complete (no ENC_CHANGE event)
WARNING: key replaced outside pairing mode      （0.03 秒後）
key replaced outside pairing mode, dropping the link
nothing subscribed, dropping the link to retry
（0.9 秒後）connecting via accept list → encryption on (stored key), before the connect event → ledger: new device, port 2 → ... hub_port=2
```

- 読み取りでの受け入れは止まったが、新しい経路「ペアリングの完了（`PAIRING_COMPLETE`）の時点で受け入れる」で同じことが起きた。
- `66521d2` 以降の試験で、`pairing complete` の時点ではまだ鍵が保存されていない（`after security: no stored peer keys`）ことが分かっている。この版でも受け入れた時点は `(no key stored)`。
- 受け入れた時点でペアリング待ちが終わり、そのあと NimBLE が鍵を保存した（または `ENC_CHANGE` が来た）のを、鍵の指紋が 0 から変わった＝「ペアリングモード外での鍵の置き換え」とみなしていると見ている（推測）。
- 案（どれか）：
  - ペアリングモードで受け入れた接続は、その接続が続く間、鍵の指紋が 0 から新しい値に変わっても `WARNING` にしない（「変わった」のではなく「初めて保存された」）。`WARNING` は、保存されていた 0 以外の指紋が別の値に変わったときだけにする。
  - または、受け入れを鍵が保存されるまで（`key_tag` が 0 でなくなるまで）待ち、ペアリング待ちを終えるのもその後にする。
- 実害は無い（1 秒以内にポート 2 でつながる）。ただし `WARNING` が毎回出るので、利用者には本物の警告と見分けがつかない。

### 2. `hub_port=2` になるときの 3.5〜4 ms（小さい）

- 2 回とも、機器 B の記述子を渡して入力を始めた秒に、IST の `LAT` が 4.13 ms・3.58 ms になった。
- 記述子の解釈や設定の組み立てが主ループを少し止めていると見ている（推測）。ペアリングのときだけなので、急がない。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 06:18〜06:22 | USB の挿し直し、書き込みツールのタブを閉じる → 1200 bps で書き込みモード | 書き込み |
| 06:39:52 | RST | `START app=302af13`、`ledger loaded boot=12 rows=2`、保存 2 回（入力の前） |
| 06:44:45 | IST を動かしながら `orbit alias 2 pedal` | 1 秒の途切れの後（06:45:03）に保存 |
| 06:45:49 | `orbit alias 2` | 1 秒の途切れの後（06:45:57）に保存 |
| 06:46:23〜06:47:04 | 動かしながら `orbit forget 2` → `orbit pair`（1 回目） | `WARNING` で 1 回切断、ポート 2。入力は保たれた。手を止めた後に保存 1 回 |
| 06:55:55〜06:56:27 | 同じ（2 回目） | 同上 |
| 06:56:51〜06:59:51 | **3 分の使用** | 切断 0、`lost=0`、`LAT` 最大 2.71 ms、保存 0 回 |

## 変わらないこと・未確認

- 「60 秒を超えて動かし続けても書かない」こと（手が途中で止まり、確かめられなかった）。
- MD600 の B5（自動引き継ぎ）・B6（組み直しの許可）は行っていない。
- 機器 B の LED の点滅の意味、スリープからの復帰、機器側で鍵を捨てたときの扱い。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。
