# ell_lite_rtplog_app
ELL LiteのRTPジッターログ解析アプリ（PC用）

## 概要

ELL Liteで取得したRTPジッターログの解析を行い、ブラウザ上で解析結果（グラフ）を確認するためのアプリケーションです。

## 動作環境

- **OS**: Ubuntu 22.04 以降
  - カーネルバージョン: 5.15.x 以降
  - GLib バージョン: 2.72.x 以降

> [!CAUTION]
> 本アプリケーションはWindowsでは動作いたしません

## ダウンロード方法

1. リポジトリの [Releases](https://github.com/MiharuCommunications/ell_lite_rtplog_app/releases) タブから、最新のアーカイブファイル（zip または tar.gz）をダウンロードしてください。
2. ダウンロードしたファイルをローカルディレクトリに保存します。

## 実行方法

1. ダウンロードしたアーカイブファイルを展開します。
   ```bash
   # tar.gz の場合
   tar -xzf ell_lite_rtplog_app-vX.X.X.tar.gz
   cd ell_lite_rtplog_app-vX.X.X
   
   # zip の場合
   unzip ell_lite_rtplog_app-vX.X.X.zip
   cd ell_lite_rtplog_app-vX.X.X
   ```

2. ELL Liteで取得したRTPジッターログファイルを、展開したディレクトリに `rtp-log` というディレクトリ名でコピーします。
   ```bash
   cp -r /path/to/rtp-log ./rtp-log
   ```

3. 以下のコマンドを実行して解析を開始します。
   ```bash
   bin/ell-lite-rtplog_app rtp-log/
   ```

4. 解析が完了すると、`outdata/rtp_plot.html` に結果が生成されます。以下の方法でブラウザに表示します。
   - **コマンドラインから表示**:
     ```bash
     browse outdata/rtp_plot.html
     ```
    > [!NOTE]
    > `outdata/rtp_plot.html` ファイルをダブルクリックすることでもブラウザで表示できます。


