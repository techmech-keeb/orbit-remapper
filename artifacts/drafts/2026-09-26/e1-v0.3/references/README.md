# references

`selected_E1_concept.png` は、E1 を採用したときに生成したコンセプト画像です。寸法図ではありません。

## このリポジトリに置かない素材

次の素材は第三者の著作物なので、再配布しません。必要なときは出典から取得してください。

| 当時のファイル名 | 内容 | 出典 |
| --- | --- | --- |
| `front.webp` / `detail.webp` | M5Stack Glass2 Unit (U158-B) の製品写真 | https://docs.m5stack.com/en/unit/Glass2%20Unit |
| `dimensions.png` | Glass2 の機械寸法図 | https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/757/U158BUnitGlass2-model-size.pdf |

`build_report.py` は `dimensions.png` と IPAex ゴシック（`fonts/ipaexg.ttf`）を読み込みます。PDF を再生成するときは、次の2つを手元で用意してください。

- `dimensions.png`: 上の機械寸法図を画像に変換して、このフォルダに置く。
- `fonts/ipaexg.ttf`: IPAex フォント（https://moji.or.jp/ipafont/）を入手して置く。

どちらも `.gitignore` の対象にしてあるので、誤ってコミットされません。
