# Orbit Remapper — DRAFT

HID Remapperを基盤に、画面と操作部を備えた卓上リマッパーを検討するプロジェクトです。

**現在は設計・開発検討のドラフトです。完成品、動作確認済みファーム、製造承認済みデータの公開ではありません。**

Built on [HID Remapper](https://github.com/jfedor2/hid-remapper) by jfedor2.
本家から派生した独立プロジェクトとして検討しています。本家の公式モデル・承認済み製品ではありません。

## まず読む

- [ドラフト資料の入口](docs/drafts/2026-09-26/README.md)
- [要求と決定状況](docs/drafts/2026-09-26/requirements.md)
- [デザイン・構成の検討経緯](docs/drafts/2026-09-26/history.md)
- [E1 / M5Dial / S3の構成比較](docs/drafts/2026-09-26/architecture.md)
- [未解決事項と確定条件](docs/drafts/2026-09-26/open-questions.md)
- [成果物一覧と原本の位置づけ](docs/drafts/2026-09-26/artifacts.md)

## 現在の二つの方向

| 方向 | 状態 |
| --- | --- |
| E1 Orbital Pod + Glass2 + XIAO + Pico | E1コンセプトはユーザー採用。機構試作案v0.3のCAD等を保存。最終外観・実機適合・製造は未承認。 |
| M5Dial + XIAO（S3がUIとUSBホストを兼任） | 分岐検討。採用、対象製品リビジョン、通信、ファーム、筐体は未確定・未検証。 |

## リポジトリ

- [orbit-remapper](https://github.com/techmech-keeb/orbit-remapper): 製品全体の資料・独自開発を置く場所。
- [hid-remapper](https://github.com/techmech-keeb/hid-remapper): 本家由来コードの開発先。

この資料追加では、HID Remapper側のコード変更は行っていません。
名称はリポジトリ名として使用していますが、正式なブランド確定・商標確認を済ませた意味ではありません。

## ドラフトの扱い

2026-09-26に、一定の完成度に達するまで非公開で開発する方針へ変更しました。この資料の格納先 `orbit-remapper` はprivateです。`hid-remapper` は現在publicなので、独自の未公開実装は追加せず、そちらの開発開始前に非公開の作業先を用意します。公開時期・公開範囲は未確定です。

将来はオープンソースとして公開する前提です。そのため、このリポジトリの履歴には公開できる物だけを入れます。非公開の原本（元BOM、引き継ぎ文書など）は別の private リポジトリに置き、第三者の素材は出典のリンクだけを残します（[成果物一覧](docs/drafts/2026-09-26/artifacts.md)）。

検討資料は `docs/drafts/`、当時の成果物は `artifacts/drafts/` に保存します。
ユーザーが内容を確定した後、対象文書ごとに確定版へ整理します。Draft PRのマージと、製品仕様・製造承認は別の判断です。
