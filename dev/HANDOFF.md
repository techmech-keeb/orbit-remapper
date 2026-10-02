# 引き継ぎ（最新）

新しいセッションは、まずこの文書を読む。2026-10-02 時点。状態が変わったら、この文書を書き換える（古い引き継ぎの文書は残すが、正本はここ）。

## 1. 今の状態

| 項目 | 状態 |
| --- | --- |
| リポジトリ | `techmech-keeb/orbit-remapper`（public、2026-10-02 公開）。`main` は `9e4f7c5` |
| リリース | `v0.1.0`（2026-10-02、コミット `a47ccc0`）。添付 5 つ、照合済み |
| 開いている PR | techmech-keeb/orbit-remapper#5：電池の残量の段階 0（ブランチ `claude/laughing-volta-r6lpn4`、版 `8153e8b`）。実機の試験報告待ち |
| 待っている報告 | ① `bat0-8153e8b-report.md`（電池の段階 0。手順は [battery.md](drafts/2026-10-02/battery.md) §3）② `bd6895d` の試験（古い鍵での繰り返しの直しと、画面なしでの `LAT` の切り分け。[m3-screen.md](drafts/2026-10-01/m3-screen.md) §6） |
| 利用者の答え待ち | ①「設定のページを開く」機能：既定の OS は Windows か、ページの置き場（GitHub Pages を使えるか）、第一段の中身 ② 本家を取り込み直すとき `git subtree pull --squash` にするか（Contributors を増やさないため） ③ ボードの層の整理（[porting.md](drafts/2026-10-02/porting.md) §1 の 1）を 2b の前にやるか |

## 2. 次にやること（利用者と合意した順番）

1. 電池の段階 0 の報告を読み、段階 1（画面の表示など）の形を決めて作る（[battery.md](drafts/2026-10-02/battery.md) §2・§4）。
2. 2b：機器ごとのレイヤーと、切断時の押下の解除。本家コアの唯一の改造になる予定（`// ORBIT:` の印を付ける）。依頼書は [m2-brief.md](drafts/2026-09-30/m2-brief.md)。
3. 画面にレイヤーを表示する。
4. 2c：設定セットと LED の送り返し。
5. 画面に設定セットを表示する。
6. 設定ツール（別のリポジトリの予定）。

あとで：探索の割合（何もつながっていないときは 30/60 ms）、E1 の生成物の Release（旧リポジトリの Issue #3）、LVGL の設定を絞ってイメージを小さくする（急がない）、ほかの機器への展開（[porting.md](drafts/2026-10-02/porting.md)）。

## 3. 進め方

- **ビルドはクラウドのセッションでする。** 利用者の PC には ESP-IDF を入れない。ESP-IDF は v5.5.5。このセッションでは `/root/esp/esp-idf-v5.5.5` にあった（新しいセッションの環境にあるかは確かめること）。Docker の本体は動かなかった。
- **試験の流れ**：クラウドで作る → `dev/tools/build/build-firmware.sh` で `.bin` と `BUILD-INFO.json` を作る → 利用者に渡す（**`.bin` はコミットしない**）→ 手順書を書く → 利用者がローカルのセッションに試験を頼む → 報告を `dev/drafts/2026-09-27/reports/` に置く → 報告を読んで次へ。クラウドとローカルのセッションは直接やり取りできない。利用者が貼り付けて仲立ちする。
- **版**：試験は「M2 ＋画面」の 1 本の版で、画面は 180° の向き。
- **遅れの基準（LAT）**：3 ms を超える秒が 1 分に 1 回以下、かつ 5 ms 以内。新しい機器をペアリングするときの探索の 1〜2 秒は除く。
- **PR**：1 テーマ 1 PR。各コミットを単独でビルドしてから出す。マージは利用者が決める（明示の指示があったときだけマージする）。作業用ブランチは `claude/laughing-volta-r6lpn4`。開いている PR とテーマが違うときは、利用者に断ってから別のブランチを切る（例：`-docs`）。
- **リリース**：`version.txt` と `CHANGELOG.md` を同じ PR で上げる → マージ → Actions の「Release firmware」を試走（`dry_run: true`）→ ノートと添付を確かめる → 本番。タグの push はこの環境から通らないので、ワークフローを手で実行する（[dev/README.md](README.md)「版とリリース」）。
- **コミットの作者**：`techmech <88352328+techmech-keeb@users.noreply.github.com>`。

## 4. 守ること

- 本家のコア（`firmware/hid-remapper/`）を改造しない。やむを得ないときは `// ORBIT:` の印。BTstack 由来のコードを持ち込まない。他人のコードはライセンスを確かめ、出典を書く。
- ログに、機器の完全なアドレス、ペアリングの鍵、M5Dial の MAC アドレスを出さない（アドレスは下位 2 バイトだけ）。
- `.bin`、ビルドの生成物（`build/`、`sdkconfig`、`managed_components/`）、ログの全文をリポジトリに入れない。
- 報告と文書に、PC の名前、ユーザー名、`C:\Users\...` のパス、個人名・社名を書かない。
- 試験機の 1 台は「機器 B」と書く。製品名・機器の名前・製造者・型番は書かない。
- `main` に直接 push しない。履歴を書き換えない。
- PR・Issue の番号は、リポジトリ名を付けて書く（このリポジトリの PR と、公開前の旧リポジトリの PR は番号が重なる）。
- 同じアドレスの 2 台は規格外として扱わない。
- 写真を公開するときは、EXIF・GPS と写り込みを確かめる。

## 5. 記録の場所

| 何 | どこ |
| --- | --- |
| 開発者向けの入口 | [dev/README.md](README.md) |
| 要件と決定、未解決事項、経緯 | [requirements.md](drafts/2026-09-26/requirements.md)、[open-questions.md](drafts/2026-09-26/open-questions.md)、[history.md](drafts/2026-09-26/history.md)（経緯の表は行 58 まで） |
| 段階ごとの依頼書と結果 | dev/README.md の「記録の読み方」の表 |
| 画面（M3） | [m3-screen.md](drafts/2026-10-01/m3-screen.md) |
| 電池の残量 | [battery.md](drafts/2026-10-02/battery.md) |
| ほかの機器への展開 | [porting.md](drafts/2026-10-02/porting.md) |
| 試験の報告 | `dev/drafts/2026-09-27/reports/` |
| 使う人向けの文書 | [docs/](../docs/README.md) |

## 6. リポジトリの外にあるもの

- **旧リポジトリ** `techmech-keeb/orbit-remapper-dev`（private）：公開前の PR #1〜#40、試験用のブランチ、Issue #3 が残っている。消さない。`dev/drafts/` に出てくる「PR #16」などは、この旧リポジトリの番号。
- **E1 の生成物と非公開の原本**：旧リポジトリの PR #1 の参照にある。
- **ローカルのセッション**：利用者の PC で、書き込みと試験とログの採取をする。
