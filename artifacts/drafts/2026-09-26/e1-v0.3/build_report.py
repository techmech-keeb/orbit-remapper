from pathlib import Path
import json,math
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor, Color
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import Paragraph,Table,TableStyle
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from PIL import Image
R=Path(__file__).resolve().parent
pdfmetrics.registerFont(TTFont('IPAexGothic',str(R/'fonts/ipaexg.ttf')))
FONT='IPAexGothic'; W,H=595.28,841.89
C=canvas.Canvas(str(R/'remapper_design_review.pdf'),pagesize=(W,H))
C.setTitle('ORBITAL POD / E1 | Remapper PCB & enclosure design v0.3')
C.setAuthor('Remapper project')
ink=HexColor('#172d3c');muted=HexColor('#566b79');accent=HexColor('#227b89');line=HexColor('#d8e1e6')
style=ParagraphStyle('jp',fontName=FONT,fontSize=10,leading=16,textColor=ink,wordWrap='CJK',spaceAfter=5)
small=ParagraphStyle('small',parent=style,fontSize=8.5,leading=13)
pg=0
def page(title,kicker):
 global pg
 if pg:C.showPage()
 pg+=1;C.setFillColor(HexColor('#ffffff'));C.rect(0,0,W,H,fill=1,stroke=0)
 C.setFillColor(accent);C.setFont('Helvetica',9);C.drawString(36,H-34,'ORBITAL POD / E1     '+kicker)
 C.setFillColor(ink);C.setFont(FONT,21);C.drawString(36,H-68,title)
 C.setStrokeColor(line);C.line(36,31,W-36,31)
 C.setFillColor(muted);C.setFont('Helvetica',8);C.drawString(36,18,'MECHANICAL DESIGN v0.3  |  2026-09-24  |  mm');C.drawRightString(W-36,18,str(pg))
def para(text,x,y,width=523,sty=style):
 p=Paragraph(text,sty);_,h=p.wrap(width,1000);p.drawOn(C,x,y-h);return y-h-7
def pic(name,x,y,w,h):
 im=Image.open(R/'drawings'/name);iw,ih=im.size;s=min(w/iw,h/ih)
 C.drawImage(str(R/'drawings'/name),x+(w-iw*s)/2,y-h+(h-ih*s)/2,width=iw*s,height=ih*s)
def table(rows,y,widths,fs=9):
 st=ParagraphStyle('cell',parent=small,fontSize=fs,leading=fs*1.5)
 data=[[Paragraph(str(x),st) for x in row] for row in rows]
 t=Table(data,colWidths=widths,hAlign='LEFT')
 t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),HexColor('#edf3f5')),('VALIGN',(0,0),(-1,-1),'TOP'),('LINEBELOW',(0,0),(-1,0),.7,line),('LINEBELOW',(0,1),(-1,-1),.3,line),('LEFTPADDING',(0,0),(-1,-1),7),('RIGHTPADDING',(0,0),(-1,-1),7),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6)]))
 _,h=t.wrap(523,1000);t.drawOn(C,36,y-h);return y-h-12
def note(text,y):return para(text,36,y,523,small)

checks=json.loads((R/'fit_checks.json').read_text())
page('E1を、組み立てられる形へ。','SELECTED DESIGN')
pic('perspective.png',23,745,549,394)
y=para('採用したE1の丸いPodと細い画面枠を、Glass2ユニットを保持する機構に落とし込みました。下部の回路・Grove端子は本体内に収め、透明な表示窓の背後は開放。背面に接続面、底面に着脱用ねじを集約しています。',36,343)
y=table([['主要仕様','v0.3の設計値'],['外形 / 全高','幅104 × 奥行約87.4 mm / ゴム足込み 約61.3 mm'],['画面 / 材料','Glass2 Unit U158-B、机面から65° / 黒色MJF PA12'],['PCB / 操作','74 × 58 × 1.6 mm / φ22ノブ、戻る、独立RUNリセット'],['造形構成','上ケース、底蓋、画面ホルダー、ノブ、押し子2種の6点']],y,[126,397])
note('本書の外観図は同梱CADから描画しています。採用時の生成コンセプト画像はreferences/selected_E1_concept.pngに収載。v0.3は機構試作案で、実物組立と電気動作は未検証です。',y)

page('球面に近いPodと、細い独立した枠。','EXTERIOR & ACCESS')
pic('front.png',28,744,258,214);pic('rear.png',300,744,270,214)
y=table([['箇所','形状・アクセス'],['本体','扁平な回転楕円体を基本とし、背面だけY=35の平面にする。頂部Z=38。底蓋はわずかに内側へ収める。'],['画面枠','外形47.2 × 32 mm、正面窓37.4 × 19.4 mm。根元は本体に埋め込む。表示範囲は35.05 × 18 mm想定。'],['ノブ / 戻る','中心 (24,-20) / (0,-25)。ノブ上端Z=33.2、戻る上端Z=34.2。周囲をくぼませて指の空間を確保。'],['外部リセット','中心 (-35.5,17)。上面から凹ませたφ4押し子。細い先端で操作する独立RUNリセット。'],['背面端子','USB-C（PC）とUSB-A（キーボード）。中心X=0 / 24。端子前端Y=34、筐体背面Y=35。'],['ケーブルの逃げ','USB-C樹脂14×9、USB-A樹脂20×11 mmを仮定して開口を拡張。プラグ同士の横間隔7 mm。']],514,[104,419],8.7)
note('Xは左右、Yは奥が＋、Zは底面が0。足の下端はZ=-1。実際に使うUSBケーブルの樹脂外形とGroveの曲げ半径は試作時に照合してください。',y)

page('Glass2の枠・基板・端子をそのまま包む。','GLASS2 INTEGRATION')
pic('display_mount.png',22,745,282,263)
C.drawImage(str(R/'references/dimensions.png'),309,498,width=250,height=220,preserveAspectRatio=True,anchor='c')
y=table([['構成','保持と収容'],['元のユニット','ガラスを外さず、既存のPCB枠と下部ケースを一体で収容。下部ケースと右側のGroveプラグに逃げを設ける。'],['正面 / 背面','正面の化粧枠は上ケースと一体。別体の背面ホルダーで元の枠を隠し、表示窓の前後は開放する。'],['ユニット固定','既存のφ3取付穴（中心間32 mm）を使い、M2.5ねじ＋座金で背面ホルダーへ固定する案。ガラスとFPCを締め付けない。'],['ホルダー固定','左右の足を、内側からM2ねじ2本で上ケースの支柱へ固定。底蓋・PCBを外すとアクセスできる。'],['公称逃げ','PCB枠幅42に対して幅43.4のポケット。ガラス全体42.04×27.22×1.25も別途干渉チェック。薄い弾性パッドでガタを調整。']],479,[103,420],8.6)
y=para('寸法の採用根拠と未確定点',36,y-3)
note('機械図は幅42、上端から取付穴中心まで44、耳部R1.5穴を含む外周の下方延長を約3、厚み6.35 mmと読んでモデル化。一方、公式仕様表は53×42×6 mmです。表と図の差は未解決であり、今回の適合確認は機械図モデルに対してのみ成立します。製作前に実機の外形・穴位置を照合し、保持部を確定してください。',y)

page('底蓋側から固定し、配線を収める。','ASSEMBLY & PCB')
pic('exploded.png',26,741,267,310);pic('pcb_layout.png',302,741,266,310)
y=para('組立順序',36,421)
for t in ['1. 除粉・黒染色後、上ケース6か所、底蓋4か所のM2インサートと、画面ホルダー2か所のM2.5インサートを施工。',
'2. Glass2を背面ホルダーへM2.5×4ねじ・座金2組で固定。Groveケーブルを接続し、底面開口から差し入れて正面枠へ合わせる。',
'3. ホルダーの左右の足をM2×6ねじ2本で固定。戻る・リセット押し子を内側から挿入。',
'4. 実装したPCBを底蓋の4支柱へM2×4低頭ねじで固定。Pico裏面TP2/TP3用開口から配線し、Groveケーブルを接続。',
'5. ケーブルの噛込みを避けて底蓋を組み、M2×6ねじ4本で固定。ノブをM2止めねじで固定し、ゴム足を貼る。']:
 y=para(t,36,y,523,small)
y=table([['PCBの機械配置','確保しているもの'],['既存の主要配置を継承','左Pico、奥中央XIAO、奥右USB-A。戻るスイッチは前側へ移動。基板上面Z=7.6、裏面空間は基本4 mm。'],['機構用の開口','固定穴、RF用開口、Pico裏面作業開口を仮配置。USB信号の通路予約を青点線で示す。実配線ではない。']],y,[128,395],8.4)
note('ねじ長さ、工具の進入、ユニットの挿入・回転経路、Grove余長は初回試作で確認。ホルダー固定ねじは下向きのアクセスで、画面を外す際はPCBを取り出します。',y)

page('造形条件と、確認できた範囲。','MANUFACTURING & VALIDATION')
y=table([['項目','設計値 / 検証状況'],['殻の厚み','底板2 mm。外側/内側楕円体の半径差・頂部差は2.4 mm。等肉厚オフセットではなく、法線肉厚は位置で変化。操作部・ねじ支柱は局所的に厚くする。'],['分割と公差','底蓋の外観合わせ目0.4 mm。画面背面は周囲約0.3 mmの公称逃げ。押し子は片側0.4〜0.5 mm程度を基本にする。'],['インサート','M2下穴φ3.1、M2.5下穴φ3.5は仮値。外径と長さは採用製品に合わせる。M2.5は画面ユニット取付用。'],['部品数 / 体積',f"6造形部品。体積合計約{checks['nominal_pa12_volume_cm3']} cm³。重量・価格の保証値ではない。"],['公称形状検査',f"各造形部品は有効な単一ソリッド。検査した部品・プラグ・画面全面・配線通路と筐体、および造形部品同士の体積干渉：{len(checks['collisions'])}件。"],['STL検査','6部品の三角形エッジ閉鎖性をmesh_checks.jsonに記録。STLはmm、組立座標を保持。造形姿勢は業者と相談。'],['未検証','実機採寸、公差積上げ、実際の組立動作、強度、押し子ストローク、電気動作、BLE到達距離。']],744,[112,411],8.7)
y=para('試作前に合わせる点',36,y-4)
for t in ['Glass2：機械図と仕様表の差を実機で解決。元の基板・ガラス・FPCには無理な拘束を加えず、薄いパッドとスペーサで調整する。',
'USBと操作部：USB-Aの採用型番、XIAOのUSB-C位置、EC12Dの軸形状・軸端高さ、戻る/リセットの常時押下と過大ストロークを確認する。',
'電気設計：Glass2のGrove給電は公式ピン表の5V、ロジック3.3Vを前提とする。元BOMの3.3V給電記載は修正対象。全体電流、差動配線、アンテナ位置は別途検証する。']:
 y=para(t,36,y,523,small)
note('本書と同梱CADは機構試作案です。回路図、ネットリスト、フットプリント、配線パターン、Gerber、配線表は含みません。',y)

page('更新データと出典','DELIVERABLES & SOURCES')
y=table([['ファイル','内容'],['cad/assembly_review.step','筐体・簡略部品の組立モデル。単位mm。'],['cad/01〜06_*.step / print_candidate/*.stl','6造形部品の編集用STEP / 試作用STL。'],['cad/PCB_outline_NPTH.dxf','基板外形、機構穴、作業用開口。実装フットプリントではない。'],['component_positions.csv','各部品包絡の座標。'],['fit_checks.json / mesh_checks.json','公称干渉・ソリッド妥当性・STLエッジ閉鎖性。'],['remapper_3d_viewer.html','外観 / 内部 / 分解 / 画面保持の3D表示。'],['references/','選択したE1画像、メーカー写真と機械寸法図。'],['cad/build_model.py / README.md','再生成ソース、座標系、仮定、組立と確認事項。']],741,[197,326],8.6)
y=para('出典',36,y-3)
sources=[('M5Stack Glass2：公式仕様・接続・写真','https://docs.m5stack.com/en/unit/Glass2%20Unit'),('M5Stack Glass2：公式機械寸法図','https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/757/U158BUnitGlass2-model-size.pdf'),('Seeed XIAO nRF52840 Plus','https://files.seeedstudio.com/Bazaar/product_pdf/102010672.pdf'),('Raspberry Pi Pico','https://www.raspberrypi.com/products/raspberry-pi-pico/'),('ALPS Alpine EC12D1524403','https://tech.alpsalpine.com/e/products/detail/EC12D1524403/'),('OMRON D2F','https://omronfs.omron.com/en_US/ecb/products/pdf/en-d2f.pdf'),('GCT USB1130：USB-A外形の参考','https://gct.co/connector/usb1130'),('HP：MJF Handbook','https://reinvent.hp.com/us-en-3dprint-mjfhandbook')]
for title,url in sources:y=para(f'<link href="{url}" color="#227b89">{title}</link>',36,y,523,small)
note('USB-Aは寸法参考であり採用型番は未確定。GCT USB1130は新規設計非推奨のため、調達品で外形を再照合します。BOM元ファイルは変更していません。',y-3)
C.save();print('PDF generated:',pg,'pages')
