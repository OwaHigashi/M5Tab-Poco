# M5Tab-Poco

M5Stack Tab5 を、ブラウザや本体タッチ操作から使うリモート演出端末にする Arduino スケッチです。WiFi に STA として接続し、Tab5 の画面とスピーカーで BOMB / CHEER 演出、マイク音声の中継、ニコニコ風の横スクロール文字を表示します。

## 主な機能

- Tab5 画面でのフルカラー演出
  - `BOMB`: 爆発アニメーションと効果音
  - `CHEER`: 拍手/歓声アニメーションと効果音
- ブラウザからのリモート操作 UI
- ブラウザのマイク音声を Tab5 スピーカーへ送る `TALK`
- 複数レーン対応の横スクロール文字
- 本体タッチ操作
  - `VOL-` / `VOL+`: 音量変更
  - `ACCEPT` / `REJECT`: リモート操作の受付切替
- SD カード上の `config.ini` による設定
- 起動ごとに SD カードへ `/mac.txt` を出力

## 対象環境

- Board: M5Stack Tab5
- Arduino core: `esp32:esp32` 3.3.8
- Libraries:
  - M5Unified 0.2.13
  - M5GFX 0.2.20
- Build tool: Arduino CLI

`sketch.yaml` にプロファイルを含めています。

## SD カードの準備

`config.example.ini` を SD カード直下へコピーし、ファイル名を `config.ini` に変更してください。

最低限、以下を自分の WiFi に合わせます。

```ini
wifi_ssid = YOUR_SSID
wifi_pass = YOUR_PASS
```

このスケッチは WiFi AP フォールバックを持ちません。必ず既存 WiFi に STA として接続します。接続できない場合でも WebServer は起動しますが、ブラウザから操作するには WiFi 接続が必要です。

任意で、横スクロール用 VLW フォントを SD カードへ配置できます。

```ini
marquee_font  = /fonts/ipaexg40.vlw
marquee_scale = 2
```

VLW フォントが読めない場合は内蔵フォントにフォールバックします。TTF ではなく M5GFX が読める VLW 形式を使ってください。

## ビルド

Windows では同梱の `build.cmd` を使うのが簡単です。

```bat
.\build.cmd
```

`esp32:esp32` 3.3.8 の `m5stack_tab5` variant では `BOARD_SDMMC_POWER_CHANNEL` が未定義になるため、`build.cmd` は次の追加フラグを compiler extra flags として渡します。

```text
-DBOARD_SDMMC_POWER_CHANNEL=4
```

Arduino CLI を直接使う場合も、同等の追加フラグを指定してください。

## 起動と使い方

1. SD カードに `/config.ini` を置きます。
2. Tab5 に書き込み、起動します。
3. WiFi 接続に成功すると、画面中央に `http://<Tab5のIP>/` が表示されます。
4. 同じネットワーク上のブラウザでその URL を開きます。

ブラウザ UI では以下を操作できます。

- `BOMB`: 爆発演出を再生
- `CHEER`: 拍手/歓声演出を再生
- `TALK`: ブラウザのマイク音声を Tab5 へ送信
- 横スクロール欄: テキストを送信して画面上部の演出エリアへ流す
- `停止`: 横スクロールを停止

`TALK` はブラウザの `getUserMedia` を使うため、ブラウザによっては HTTPS または安全なコンテキストが必要です。LAN 内の `http://<IP>/` でマイク許可が出ない場合は、HTTPS のリバースプロキシ経由でアクセスしてください。

## 本体操作

画面下部の操作バーをタッチします。

- `VOL-`: 音量を 20 下げる
- `VOL+`: 音量を 20 上げる
- `ACCEPT` / `REJECT`: リモートリクエストの受付を切り替える

`REJECT` 中は `/bomb`、`/clap`、`/talk`、`/marquee` が拒否されます。

## 横スクロール文字

通常は最大 5 レーンを使います。レーンが埋まっているときに追加したい場合は、テキストに `<sp>...</sp>` を含めると重なり許可レーンを含む最大 9 レーンまで使います。

対応タグ:

- 色: `<r>` `<g>` `<b>` `<y>` `<c>` `<m>` `<w>` `<o>`
- 色リセット: `</>` または `</color>`
- サイズ: `<small>` / `<s1>`, `<normal>` / `<s2>`, `<big>` / `<s3>`
- 下線: `<u>...</u>`
- ハイライト: `<hl>...</hl>` または `<mark>...</mark>`
- 重なり許可: `<sp>...</sp>`

例:

```text
<r>重要</> <big><y>19時開始</></big> <u>集合してください</u>
```

## HTTP API

ブラウザ UI 以外から直接呼び出すこともできます。

```sh
curl -X POST http://<Tab5のIP>/bomb
curl -X POST http://<Tab5のIP>/clap
curl -X POST --data 'こんにちは <r>赤文字</>' 'http://<Tab5のIP>/marquee?speed=2'
curl -X POST http://<Tab5のIP>/marquee/stop
curl http://<Tab5のIP>/status
```

主なエンドポイント:

- `GET /`: リモート操作 UI
- `GET /status`: 受付状態、音量、WiFi 状態、RSSI を JSON で返す
- `POST /bomb`: BOMB 演出
- `POST /clap`: CHEER 演出
- `POST /talk`: 16 kHz / Int16 LE / mono PCM を再生
- `POST /marquee?speed=1..5`: UTF-8 テキストを横スクロール表示
- `POST /marquee/stop`: 横スクロール停止

## 設定項目

`config.ini` の主な項目です。

- `wifi_ssid`, `wifi_pass`: 接続先 WiFi
- `hostname`: WiFi 接続時に設定するホスト名
- `http_port`: WebServer のポート。標準は `80`
- `wifi_timeout_ms`: 起動時に WiFi 接続を待つ時間
- `ip_address`, `netmask`, `gateway`, `dns1`, `dns2`: 固定 IP 設定。`ip_address` が空なら DHCP
- `startup_volume`: 起動時音量。`0..255`
- `accept_on_boot`: 起動時にリモート操作を受け付けるか
- `debounce_ms`: FX 連打抑制時間
- `rotation`: 画面向き。標準は `1`
- `brightness`: 輝度。`0..255`
- `marquee_font`: SD 上の VLW フォントパス
- `marquee_scale`: 横スクロール文字サイズ

## 生成ファイル

起動時に SD カード直下へ `/mac.txt` を生成します。STA MAC、AP MAC、IP アドレス、ゲートウェイ、RSSI などを確認できます。DHCP 予約や固定 IP 設定時の確認に使えます。

## メモ

- FX 実行中に入った追加リクエストは `debounce_ms` により拒否され、演出が連続で積み上がらないようにしています。
- 横スクロール中に BOMB / CHEER を実行すると、横スクロールは停止します。
- ブラウザ UI の一部効果音は外部 CDN の音声を再生します。Tab5 側の効果音はスケッチ内で生成しています。
