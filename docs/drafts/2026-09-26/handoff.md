---
status: draft
snapshot_date: 2026-09-26
finalized: false
---

# セッション引き継ぎ（2026-09-26）

別のセッション（Claude など）がこのプロジェクトを引き継ぐための要約。詳細は各文書を正とし、ここは入口と現在地だけを書く。**実装・実機検証はまだ一切していない。**

## 1. まず読む順番

1. この文書
2. [requirements.md](requirements.md) の「構成の決定」（H1〜H3）と「機能要件」（F1〜F6、U1〜U4）
3. [open-questions.md](open-questions.md) の Q24〜Q27 と、末尾の「回路図」「技適の調査」「Q21」「Q23」の節
4. [architecture.md](architecture.md) の D 案・E 案・ESP32-S3 と nRF52840 の比較
5. [prior-art.md](prior-art.md)（類似プロジェクトと、ESP-IDF の BLE の不具合）
6. 必要に応じて [history.md](history.md)（経緯 1〜32）

## 2. リポジトリとブランチの状態

| リポジトリ | 状態 |
| --- | --- |
| `techmech-keeb/orbit-remapper`（private、将来 OSS 公開前提） | `main` に PR #2・#4 がマージ済み。**PR #5（ブランチ `claude/gifted-pascal-husnvk`）が未マージ**で、構成の見直し・調査・この引き継ぎ文書を含む。Issue #3（E1 生成物の Release 作成）が未着手 |
| `techmech-keeb/keyboard-hardware`（private） | PR #23 マージ済み。`products/orbit-remapper/archive/2026-09-26/` に公開しない原本（元 BOM、Claude 引き継ぎ文書、設計レビュー PDF）を保管 |
| `techmech-keeb/hid-remapper`（public。本家 jfedor2/hid-remapper のフォーク） | 変更なし。本家 master = `51ab8b3`（2026-09-26 確認） |
| orbit-remapper のブランチ `docs/draft-design-record-20260926` | **削除しない**。E1 の生成物（STL・描画・ビューア）が Release 作成（Issue #3）まで ここにしかない |

## 3. 決まっていること（ユーザー決定）

- **将来 OSS として公開する前提**で private 開発。公開できない物（第三者素材、価格入りの BOM、AI の会話記録）は orbit-remapper の履歴に入れない。公開時は `main` の履歴だけを新しい public リポジトリへ push する方針（PR #1 経由で原本が見えるため）。
- ソフトウェアは MIT。ハードウェア・文書のライセンスは未定（Q17）。商標確認は未実施（Q18）。
- **表示と操作は M5Dial**（H1）。透過表示の要件 R02 は取り下げ。E1（Glass2 の筐体）は経緯として保存。
- **入力は BLE 機器のみ、PC への出力は USB**（H3）。有線 USB 機器は対象外（ユーザーの OLSK60v2 など有線機は使えなくなることを了承）。
- 機能要件：別機器のキーの組み合わせ、**機器ごとに独立したレイヤー**（機器のキーと本体ダイヤルの両方で切替、別機器のレイヤーも切替可）、切断時はその機器の入力だけ解除、本体で状態表示・ペアリング管理（1 台ずつ削除）・設定セット切替・簡単な割り当て編集、出力はキーボード＋マウス／ゲームパッド／絶対座標、BIOS 操作とスリープ解除、LED の送り返し。F6（パススルー）とハブ機能は見送り。
- Vial やファーム更新は、機器を PC に挿し替えて行う（U2）。
- **ユーザーの M5Dial は初代**（M5StampS3 搭載、技適 219-229318）。

## 4. 決まっていないこと（最優先）

**Q27：D 案（M5Dial 単体。S3 が BLE 受信・リマップ・画面・USB 出力）か、E 案（XIAO nRF52840 Plus が本家 BLE 版で BLE 受信・リマップ・USB 出力、M5Dial は画面とダイヤルだけ）か。**

requirements の H3 には D 案で記録してあるが、最終選択は Q27 で決める、と注記済み。

決め方は 2 通り：

1. **ユーザーに 2 点を聞く（会話の最後で質問したまま、回答待ち）**
   - 同時に何台の BLE 機器をつなぐか（F1-3）
   - 15 ms（1 秒に約 66 回）で足りるか（マウスでゲームをするか）
   - → 1〜2 台・15 ms で可なら D 案（S3 に実績あり）。3 台以上・7.5 ms が必要なら E 案。
2. **実験する**：初代 M5Dial と BLE マウスで、①接続時に最初から 7.5 ms を指定したときの実際の間隔、②スリープ復帰後の再接続、③2 台同時接続（両方 7.5 ms にそろえる）。結果を M5Dial の画面に表示するファームを作る提案をしている。

### 判断材料の要点

- 無線回路の性能差は小さい（仕様は S3 が上）。差は BLE のコントローラ以降のソフトと、機器を受ける側としての実績。
- S3 の実績：**finger563/esp-usb-ble-hid**（BLE で受けて USB で出す、1 台、15 ms、完成度が高い、MIT）が D 案の土台候補。WikDra/esp32-hid-gamepad-bridge は ESP-IDF の `esp_hidh` の不具合と修正の記録が豊富。
- nRF52840 の実績：本家 HID Remapper の BLE 版（8 台設定、7.5 ms 要求）、ZMK の分割キーボード・ドングル（7.5 ms が標準）。
- 「S3 は 15 ms より短くできない」と一度説明したが、先行例の条件（PC とも BLE 接続、接続後の変更要求）での結果で、Orbit の条件では未確定と訂正済み。

## 5. そのほかの未解決事項

- Q24：M5Dial V1.1（Stamp-S3A）の技適は未確認。公開時は動作確認した型番を明記する。手元の初代 M5Dial の技適表示も未確認。
- Q25：S3 の BLE HID ホスト（D 案の場合）。Q26：RAM（S3 は PSRAM なし 512 KB）。
- Q17 ライセンス、Q18 商標（Kensington の「Orbit®」など）。
- naming.md：名前に「M5Stack」を入れない方針は提案で、ユーザーの確定は未記録。
- M5Dial の給電：回路図上、Grove の 5 V は出力専用で給電不可。電源は USB-C（D 案では PC への口、E 案では筐体内で 5 V を入れる）。
- 筐体：M5Dial 用の筐体は未設計。USB-C はパネル取付の延長ケーブルで背面に出す案。

## 6. 作業上の注意

- 会話は日本語、平易な表現で。専門用語は言い換える。**結論を先に、短く**。前提が何度も変わって混乱させた経緯があるので、判断が変わるときは理由と訂正をはっきり書く。
- 事実と推測を分け、未確認は「要確認」と書く。出典は URL と確認日を `sources.md` に残す。
- コミットの author は `techmech <88352328+techmech-keeb@users.noreply.github.com>`（これまでのコミットと同じ）。1 テーマ 1 コミット。
- 開発ブランチはハーネス指定の `claude/gifted-pascal-husnvk`。**PR がマージされた後は、最新の `main` から作り直してから**次の作業を始める（マージ済み PR に積まない）。
- AI-agent-playbook と knowledge-base の CLAUDE.md の規則にも従う（機密を入れない、一次情報優先など）。
- 総務省の技適データベースは、この種のクラウド環境からは 403 で見られなかった。秋月電子の商品ページの HTML に技適番号が載る（`curl` で取得して確認できた）。
