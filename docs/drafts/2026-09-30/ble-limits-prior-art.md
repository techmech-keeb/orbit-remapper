---
status: draft
snapshot_date: 2026-09-30
finalized: false
---

# 接続数とペアリング情報が上限に達したとき、本家と各 OS はどうするか

Orbit の「いっぱいのときの扱い」（要件 C、M2 の機器台帳）を決めるための調査。本家 HID Remapper の Bluetooth 版はソースを読み、各 OS は公式資料と開発元の回答を 2026-09-30 に確認した。**Orbit の実機では何も試していない。** 出典は [sources.md](../2026-09-26/sources.md#接続数とペアリング情報の上限2026-09-30-確認)。

## 1. 結論

- **本家も各 OS も、「いっぱいです」とは知らせない。** 接続は黙って失敗し、ペアリング情報は上限が事実上無いか、黙って古いものを消す。
- Orbit の今の振る舞い（ペアリング 5 台目で古い記録が黙って消える、3 台目の登録済み機器を待ち受けに入れない）は、この流儀と同じ。
- **それでも Orbit では「断って利用者に選ばせる」を勧める。** OS はペアリング情報の上限が事実上無制限（Windows、Linux）か 100 件（Android）で上書きが実害になりにくいのに対し、Orbit は 4〜8 件で上書きが日常的に起きる。Orbit は画面を持つので、OS の設定画面と同じ「一覧から消す」操作を本体で提供できる。

## 2. 本家 HID Remapper の Bluetooth 版（XIAO nRF52840、Zephyr、`51ab8b3`）

`firmware-bluetooth/prj.conf` と `firmware-bluetooth/src/main.cc` を読んだ事実。

| 項目 | 値・振る舞い |
| --- | --- |
| 同時接続 | 8 台（`CONFIG_BT_MAX_CONN=8`） |
| ペアリング情報 | 32 台（`CONFIG_BT_MAX_PAIRED=32`） |
| ペアリング情報がいっぱい | Zephyr の `CONFIG_BT_KEYS_OVERWRITE_OLDEST=y`：「今つながっていない鍵のうち、いちばん古いもの」を黙って上書きする。本家側に知らせる処理も、消す相手を選ぶ処理もない |
| 登録済みが全部つながった | スキャンをやめる（`all bonded peers connected, not scanning`）。LED は点滅のまま |
| 接続がいっぱいで 9 台目が現れた | Zephyr の接続要求が失敗し、本家は警告ログ（`scan_connecting_error`）を出して 1 秒後に再スキャン。利用者には何も出ない |
| ペアリングモードで 8 台埋まっている | 同上。新しい機器は黙って接続できない |

## 3. 汎用 OS

数値を一次資料で公表しているのは Android（AOSP のソース）と Linux（カーネルのソース）だけ。Windows と macOS の数値は開発元の担当者の回答か二次情報で、公式文書ではない（要確認）。

### 3.1 同時接続数（セントラル側）

| OS | 上限 | 超えたとき |
| --- | --- | --- |
| Windows 10/11 | 公式の数値なし。Microsoft の回答（Q&A、2021-06-28）は「ホストは理論上 7、実用 3〜4、仕様で解除不可」。5 台で頭打ちの報告と 8 台の報告があり、コントローラ依存の可能性が高い | 新しい機器が「接続できない」だけ。既存の接続は切らない。特定のエラーコードの一次資料は見つからず |
| macOS / iOS | Apple は非公開。担当者の回答（Forums、2026-01）は visionOS と watchOS が 2 台、「iOS で 4 台動くという文書も無い」。macOS は「理論 7、実用 3〜4」（Macworld 2016 年、サポート由来の二次情報） | visionOS の実例では 3 台目が「接続中」のまま成功も失敗も返らない。iOS と macOS も同様と推測 |
| Android | AOSP `bt_target.h`：静的確保 `GATT_MAX_PHY_CHANNEL 16`、互換性定義の最低 `GATT_MAX_PHY_CHANNEL_FLOOR 8`。実効値はシステムプロパティ `bluetooth.core.le.max_number_of_concurrent_connections` でメーカーが設定。旧版は 7 | `gatt_main.cc` の `gatt_act_connect()` が "Max TCB … reached" をログして失敗 → アプリには `onConnectionStateChange` でエラー 133（`GATT_ERROR 0x85`）。既存の接続は切らない |
| Linux（BlueZ） | カーネル（`net/bluetooth/hci_conn.c`）に LE 接続数の上限検査はなく、コントローラ任せ。「同時に 1 本しか接続を試みられない」制約だけ `-EBUSY` で返す | コントローラが「接続数超過（0x09）」を返すと bluetoothd が `org.bluez.Error.Failed` を返し、`bluetoothctl` には "Failed to connect" とだけ出る |

### 3.2 ペアリング情報の数

| OS | 上限 | いっぱいのとき |
| --- | --- | --- |
| Windows | Microsoft の回答は「事実上無制限」。ペアリング済みの LE 機器は GATT サービスごとに機器オブジェクトを作り、上限の設定は無い（Q&A、2026-04-06） | 該当なし。設定の一覧から利用者が消す |
| macOS / iOS | Apple の回答（Forums、2024-06）は「文書化した上限は無い。妥当な数は保持できるが無制限ではない」。数値は非公開 | 上限に達するとペアリングが失敗する、という示唆のみ。設定の Bluetooth 一覧で個別に削除 |
| Android | スタック内のセキュリティ記録は `BTM_SEC_MAX_DEVICE_RECORDS 100`（`bt_target.h`）。永続保存（`bt_config.conf`）の件数上限は一次資料で未確認。Zebra の資料（本文未取得）は「Oreo は 100、Android 10 以降は静的な検査を廃止」と述べる | `btm_dev.cc` の `btm_sec_allocate_dev_rec()`：いっぱいなら最古の記録を消して割り当てる。`btm_find_oldest_dev_rec()` は未ペアリングの記録を先に追い出し、全部ペアリング済みなら最古のものを消す。**利用者への通知なし** |
| Linux（BlueZ） | 上限なし（`/var/lib/bluetooth/<adapter>/<device>/info` に機器ごとのファイル） | 該当なし |

### 3.3 利用者への見え方（共通）

- Windows：設定の一覧から削除するだけ。「いっぱい」の表示は無い。
- macOS / iOS：設定の Bluetooth 一覧で「このデバイスの登録を解除」。エラー表示は無い。
- Android：設定の接続済みデバイスから「削除」。接続失敗は無言か「接続できませんでした」。
- Linux：`bluetoothctl remove` か GUI で削除。接続失敗は "Failed to connect" のみ。

## 4. Orbit の今の振る舞い（事実）

- 同時接続 2 台（`ORBIT_MAX_DEVS`）、ペアリング情報 4 台（`CONFIG_BT_NIMBLE_MAX_BONDS=4`）。
- 5 台目をペアリングすると、NimBLE の既定の処理（`ble_store_util_status_rr`）が古い記録を黙って消す。2026-09-29 の試験で MD600 の古い記録が消えた（本家と同じ振る舞い）。
- 2 台つながっていると、許可リストに入れる相手が無いので待ち受けを始めない。3 台目の登録済み機器が現れてもつなぎに行かず、利用者には何も出ない（画面の `2/2` だけ）。

## 5. Orbit への含意（案）

- ペアリング情報がいっぱいのとき：**ペアリングを始めずに `bonds full` を表示し、利用者に 1 台消してもらう**（M2 の機器台帳、要件 C）。「最後に使った時刻」で自動で消す案は、久しぶりの機器が消えていて驚く形になるので採らない。ただし「同じ機器らしい古い記録の置き換え」（要件 D の後半）は「更新」なので自動で行う。
- 接続数がいっぱいのとき：OS と同じく黙って待ち受けに入れないままでよい。画面の `2/2` で状況は分かる。
- 上限は 8 台に増やす（本家の設定形式でのポート番号の上限は 15、許可リストは最大 15、NVS は 128 KB）。

## 6. 要確認

- Windows と macOS の数値（担当者の回答と二次情報のみ）。
- Windows で接続数を超えたときのエラーコード。
- iOS と macOS で接続数を超えたときの振る舞い（visionOS の実例からの推測）。
- Android の永続保存（`bt_config.conf`）の件数上限。Zebra の資料 000027764 の本文。
- Orbit の NVS で 1 台あたりの記録の大きさ（要件 C の前半で起動時にログに出して測る）。
