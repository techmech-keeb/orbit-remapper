---
status: draft
snapshot_date: 2026-09-26
finalized: false
---

# 成果物の索引

以下は設計過程の原本・試作候補を保存したものです。製造承認済みのデータではありません。

## 置き場所の分け方（2026-09-26）

将来オープンソースとして公開する前提で、成果物を次の4つに分けました。公開すると Git の履歴もすべて見えるため、公開できない物はこのリポジトリの履歴に入れていません。

| 区分 | 置き場所 | 対象 |
| --- | --- | --- |
| 公開できる物 | このリポジトリ | 設計経緯の文書、CAD の生成コード、STEP、PCB 外形 DXF、部品配置 CSV、検査記録、自作のコンセプト画像 |
| 非公開の原本 | 非公開の保管先（別の private リポジトリ） | 元 BOM（価格・入手先を含む）と CSV 書き出し、Claude 引き継ぎ文書（md / docx）、設計レビュー PDF（2種） |
| 生成物 | Release に添付する予定（未作成） | 試作候補 STL、描画 PNG、3D ビューア HTML、プレビュー PNG |
| 保存しない物 | 出典のリンクだけを残す | M5Stack の製品写真・寸法図、IPAex フォント、分割 ZIP |

Release はまだ作っていません。それまで生成物と分割 ZIP は、Draft PR #1 のブランチ `docs/draft-design-record-20260926` にだけ残っています。**このブランチは Release に添付し終えるまで削除しないでください。**

ファイルごとの置き場所と SHA-256 は [artifact-manifest.json](artifact-manifest.json) の `location` 欄に記録しています。

| `location` | 意味 |
| --- | --- |
| `orbit-remapper` | このリポジトリの同じパスにある |
| `private-archive` | 非公開の保管先（別の private リポジトリ）へ移した |
| `release-pending` | 生成物。Release への添付待ち |
| `not-stored` | 保存しない（第三者の素材、または重複） |

## このリポジトリにあるE1データ

- [当時のREADME](../../artifacts/drafts/2026-09-26/e1-v0.3/README.md)（原本のまま。README に載っている STL・PNG・HTML・PDF・メーカー画像はこのリポジトリにはありません）
- [CAD生成ソース](../../artifacts/drafts/2026-09-26/e1-v0.3/cad/build_model.py)
- [組立STEP](../../artifacts/drafts/2026-09-26/e1-v0.3/cad/assembly_review.step) と各部品の STEP
- [PCB機械外形DXF](../../artifacts/drafts/2026-09-26/e1-v0.3/cad/PCB_outline_NPTH.dxf)
- [部品配置CSV](../../artifacts/drafts/2026-09-26/e1-v0.3/component_positions.csv)
- [干渉等の検査記録](../../artifacts/drafts/2026-09-26/e1-v0.3/fit_checks.json)、[STL検査記録](../../artifacts/drafts/2026-09-26/e1-v0.3/mesh_checks.json)
- 描画・PDF・ビューアの生成コード（`drawings/render_views.py`、`build_report.py`、`build_viewer.py`）
- [E1採用コンセプト画像](../../artifacts/drafts/2026-09-26/e1-v0.3/references/selected_E1_concept.png)（生成画像）
- [第三者素材の出典](../../artifacts/drafts/2026-09-26/e1-v0.3/references/README.md)

## 保存時の確認（当初の格納時）

原本7点とZIP内の全ファイルを保存し、SHA-256一覧に対応付けました。単独配布PDFとZIP内PDFはバイナリが異なりますが、抽出した本文は一致しました。どちらも原本として非公開の保管先（別の private リポジトリ）に保存しています。

元BOMの各シートはCSVに書き出しました。表の値・数式の参照用で、元XLSXが正本です。どちらも非公開の保管先（別の private リポジトリ）にあります。

## 再生成時の注意

E1はCadQuery 2.7を使った生成コードです。`cad/parameters.json`は出力記録であり編集入力ではありません。生成・STL検査・描画・PDF・HTMLの手順は当時のREADMEを参照してください。PDF の再生成には、メーカーの寸法図と IPAex フォントを手元で用意する必要があります（[references/README.md](../../artifacts/drafts/2026-09-26/e1-v0.3/references/README.md)）。仕分けの作業では再生成していません。

第三者のメーカー資料やフォントは出典・ライセンスを維持します。資料整理によって、第三者資産まで新しい一括ライセンスへ変更した扱いにはしません。
