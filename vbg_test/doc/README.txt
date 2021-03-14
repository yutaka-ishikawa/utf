
         スーパーコンピュータ「富岳」向け通信機能の大規模検証
     大規模環境におけるMPICH-TOFU集団通信機能の動作検証プログラム

                                                   2021.03 株式会社メトロ

【 構成 】

  vbg_test
   ├ doc
   │  ├ PerformanceReports
   │  │  └ PerformanceReports.xlsx   : 性能検証結果報告書
   │  │
   │  ├ README.txt                    : 本文書
   │  └ run-script.pdf                : 実行スクリプト実装仕様書
   │
   ├ run
   │  ├ TestLogs
   │  │  ├ 16rack-1ppn
   │  │  │  ├ 16rack-1ppn-program-errfile.tar.gzip2   : テストプログラムの標準エラー出力ファイル
   │  │  │  └ 16rack-1ppn-program-outfile.tar.gzip2   : テストプログラムの標準出力ファイル
   │  │  ├ 16rack-4ppn
   │  │  │  ├ 16rack-4ppn-program-errfile.tar.gzip2   : テストプログラムの標準エラー出力ファイル
   │  │  │  └ 16rack-4ppn-program-outfile.tar.gzip2   : テストプログラムの標準出力ファイル
   │  │  └ 32rack-1ppn
   │  │      ├ 32rack-1ppn-program-errfile.tar.gzip2   : テストプログラムの標準エラー出力ファイル
   │  │      └ 32rack-1ppn-program-outfile.tar.gzip2   : テストプログラムの標準出力ファイル
   │  │
   │  ├ TPlist.txt                    : TP引数リスト
   │  ├ all_run.sh                    : テスト実行用スクリプト
   │  ├ ct.env                        : MPICH-TOFUの環境設定
   │  ├ pjsub-sample.sh               : ジョブスクリプトのフォーマットファイル
   │  └ pjsub.env                     : ジョブ実行時オプション
   │
   └ src
       ├ Makefile                      : makeファイル
       ├ mkdef.fugaku                  : 翻訳時の環境定義ファイル
       ├ utf_large_scale_test.c        : テストプログラム
       └ utf_large_scale_test.h        : テストプログラムのヘッダー


【 テストプログラムの翻訳 】 

 (0) MPICH-TOFU環境を用意する。

 (1) MPICH-TOFUの翻訳コマンド(mpicc)のパスを通す。
      例)
        $ export PATH=/home/g9300001/u93160/work/MPICH-TOFU_fast/env/mpich-tofu/bin:${PATH}

 (2) vbg_test/src 配下に移動する。
     必要に応じて mkdef.fugaku を編集する。

 (3) make を実行する。
     テストプログラムの翻訳が正常に終了すると、
     実行ファイル utf_large_scale_test が vbg_test/run 配下にインストールされる。


【 テストの実行 】 

 (1) vbg_test/run 配下に移動する。
     検証時の環境に合わせて環境設定(ct.env)を編集する。 
       PATH        : MPICH-TOFUの実行コマンド(mpich_exec)のパス
       EXEC_OPTION : mpich_exec実行時のオプション
                     デフォルトでは、utfライブラリ(libmpi_vbg.so)のパスが指定。
                     なお、最新版では、LD_PRELOADの指定は不要であり、以下の環境変数を指定する必要がある。
                      - export UTF_BG_LOAD=1

 (2) 検証規模に応じて、ジョブ実行時オプション(pjsub.env)を編集する。 
     例）-L rscunit=rscunit_ft01 -L rscgrp=eap-large -L node=16x12x16:strict --mpi max-proc-per-node=1 -L elapse=180
     
       -L rscunit=<a> -L rscgrp=<b> -L node=<c> --mpi max-proc-per-node=<d> -L elapse=<e>
       --------------------------------------
       <a> : 使用するリソースユニット
       <b> : 使用するリソースグループ
       <c> : 使用ノード数・形状
       <d> : ノード内のプロセス数
       <e> : ジョブの経過時間制限値

 (3) 検証規模に応じて、all_run.sh を編集する。
     PJSTAT_MAX_NUM ：一度に投下するジョブ数を指定。
                      デフォルトは2（一度に2つジョブを投下。）
     CSV_FILE_OUT   ：実行結果をCSVファイルで出力するかをON/OFFで指定。
                      デフォルトはON（CSVファイルで出力）

 (4) all_run.sh を実行する。引数を指定した場合は、指定したファイルを
     TP引数リストとしてテストを行う。
       例1)
        $ ./all_run.sh         : TPlist.txtをTP引数リストとしてテスト実行
       例2)
        $ ./all_run.sh aaa.txt : aaa.txtをTP引数リストとしてテスト実行

     all_run.sh は、ct.env および pjsub.env を読み込み、TP引数リストに
     指定されている行数分ジョブ実行を繰り返す。
     なお、TP引数リストにおいて、行の先頭が # の項目はテストしない。
     
     注）動作検証時、上限越えの試験については試験対象外であったため、
         上限越えの試験については # としている。

 (5) all_run.sh の実行完了後、以下のディレクトリとファイルが生成される。
     ・results-<TP引数リスト名>-<YYYY-MM-DD>
         TP引数リストに指定されたテスト毎の出力ファイルを格納。

         出力ファイルは以下の通り：
           - UTF-VBG.<ジョブID>.{out|err|stats}
               ジョブの標準出力／標準エラー出力／統計情報への出力内容
           - UTF-VBG.<テスト実行番号>.<ジョブID>.{out|err}
               TPの標準出力／標準エラー出力への出力内容

         例)
           $ ls -1 results-TPlist-2021-02-24/*5214585*
           results-TPlist-2021-02-24/UTF-VBG.5214585.err
           results-TPlist-2021-02-24/UTF-VBG.5214585.out
           results-TPlist-2021-02-24/UTF-VBG.5214585.stats
           results-TPlist-2021-02-24/UTF-VBG.A1-0001.5214585.err
           results-TPlist-2021-02-24/UTF-VBG.A1-0001.5214585.out

     ・results-<TP引数リスト名>-<YYYY-MM-DD>.csv
         実行したテストの結果（CSV形式）。

         出力の形式は以下の通り：
           テスト実行番号,ジョブID,結果,VBGの使用,測定時間(μ) 

         テスト結果の出力例と意味)
           A1-0007,5214592,  correct,passed,  7.410 : VBG使用、正常終了
           A1-0008,5214593,incorrect,passed,  7.950 : VBG使用、結果NG
           A1-0009,5214594,  correct,     -, 14.030 : ソフト実行、正常終了
           A1-0010,5214598,  error,     -           : 異常終了
           A1-0011,5214599,  timeout,     -         : タイムアウト発生

     ・<TP引数リスト名>_out.txt
         TP引数リストを更新したファイル。
         結果OKのテスト項目は、行の先頭に # を付加。
         次回 all_run.sh を実行する時にこのファイルを指定することで、
         結果NGのテスト項目のみ再検証できる。


---------------------------------------------------------------------------
【 参考：テストプログラムの引数 】

＜要因分析表の因子に対応する引数＞
--func <func>     テストするMPI関数として <func> に以下のいずれかを指定。
                    Barrier/Bcast/Reduce/Allreduce
                  デフォルトはBarrier。

--comm <comm>     テストするコミュニケータとして <comm> に以下のいずれかを
                  指定。
                    WORLD/SPLIT/DUP/CREATE/
                    SPLIT_NO_FREE/DUP_NO_FREE/CREATE_NO_FREE/
                    SPLIT_FREE/DUP_FREE/CREATE_FREE
                  デフォルトはWORLD。

--no_measure      性能を計測しない場合に指定。デフォルトは性能計測あり。

--op <op>         演算タイプを <op> に指定。デフォルトはMPI_SUM。
                  テストするMPI関数がMPI_Reduce/MPI_Allreduceの場合に有効。

--datatype <type> データ型を <type> に指定。デフォルトはMPI_LONG。
                  テストするMPI関数がMPI_Barrier以外の場合に有効。

--count <count>   データの要素数を <count> に指定。デフォルトは 1。
                  テストするMPI関数がMPI_Barrier以外の場合に有効。

--use_in_place    MPI_IN_PLACE を使用する場合に指定。
                  デフォルトはMPI_IN_PLACEを使用しない。
                  テストするMPI関数がMPI_Reduce/MPI_Allreduceの場合に有効。

--diff_type       ランクによって異なるデータ型を使用する場合に指定。
                  デフォルトは同じデータ型を使用する。
                  テストするMPI関数がMPI_Bcastの場合に有効。

＜その他の引数＞
--print_info      引数などの情報を出力する場合に指定。デフォルトは出力
                  しない（結果の成否(Correct/Incorrect)のみ出力）。

--iter <N>        MPI関数呼出しの繰り返し回数(1～signed int MAX)を <N> に
                  指定。--no_measure指定時は無効。デフォルトは 1。

--comm_count <N>  コミュニケータ生成の繰り返し回数(1～signed int MAX)を
                  <N> に指定。コミュニケータに繰り返しパターン(*_FREE)を
                  指定した場合のみ有効。デフォルトは 3。

--no_check_vbg    VBGの呼び出しチェックを行わない場合に指定。
                  デフォルトは呼び出しチェックを行う。


---------------------------------------------------------------------------
 【 参考：テストプログラム(utf_large_scale_test.c)の構成 】

main()
 ├ init_val() :各種変数の初期化処理
 │ 
 ├ MPI_Init()
 ├ MPI_Comm_size() :MPI_COMM_WORLDのサイズ取得
 ├ MPI_Comm_rank() :MPI_COMM_WORLDのランク取得
 │
 ├ check_args() :引数チェックとテストパターンの設定
 │ 
 ├ print_infos() :テストパターンの詳細情報出力(--print_info指定時。WORLDのルートランクのみ)
 │ 
 ├ 対象のデータ型のサイズと要素数から必要なバッファサイズを算出
 │ 
 ├ MPI_Comm_group() :MPI_COMM_WORLDのグループ取得(MPI_Comm_createのテスト用)
 │ 
 ├ コミュニケータテスト(*_NO_FREE)用のコミュニケータテーブル、グループテーブル獲得
 │ 
 ├ comm_count数分ループ
 │  │ 
 │  ├ create_comm() :コミュニケータの作成(要因CがMPI_COMM_WORLD以外の場合。コミュニケータテスト用)
 │  │ 
 │  ├ テスト対象のMPI関数(要因A)による処理切り分け
 │  │  ├ MPI_Barrierの場合
 │  │  │ ├ test_barrier()   :MPI_Barrierのテスト
 │  │  │    ├ 開始時間取得(--no_measure未指定時)
 │  │  │    ├ 繰り返し回数分ループ(--iter指定値分)
 │  │  │    │  ├ MPI_Barrier()
 │  │  │    ├ 終了時間取得(--no_measure未指定時)
 │  │  │    ├ 結果出力(ルートランクのみ) 
 │  │  ├ MPI_Bcastの場合
 │  │  │ ├ allocate_buffer():テスト用データバッファの獲得
 │  │  │ ├ test_bcast()     :MPI_Bcastのテスト
 │  │  │    ├ テストデータ作成＆結果確認用の値設定
 │  │  │    ├ 開始時間取得(--no_measure未指定時)
 │  │  │    ├ 繰り返し回数分ループ(--iter指定値分)
 │  │  │    │  ├ MPI_Bcast()
 │  │  │    ├ 終了時間取得(--no_measure未指定時)
 │  │  │    ├ 結果確認(ルート以外のランク)＆出力(ルートランクのみ)
 │  │  ├ MPI_Reduceの場合
 │  │  │ ├ allocate_buffer():テスト用データバッファの獲得
 │  │  │ ├ test_reduce()    :MPI_Reduceのテスト
 │  │  │    ├ テストデータ作成＆結果確認用の値設定
 │  │  │    ├ 開始時間取得(--no_measure未指定時)
 │  │  │    ├ 繰り返し回数分ループ(--iter指定値分)
 │  │  │    │  ├ MPI_Reduce()
 │  │  │    ├ 終了時間取得(--no_measure未指定時)
 │  │  │    ├ 結果確認(ルートランク)＆出力(ルートランクのみ)
 │  │  ├ MPI_Allreduceの場合
 │  │     ├ allocate_buffer():テスト用データバッファの獲得
 │  │     ├ test_allreduce() :MPI_Allreduceのテスト
 │  │        ├ テストデータ作成＆結果確認用の値設定
 │  │        ├ 開始時間取得(--no_measure未指定時)
 │  │        ├ 繰り返し回数分ループ(--iter指定値分)
 │  │        │  ├ MPI_Allreduce()
 │  │        ├ 終了時間取得(--no_measure未指定時)
 │  │        ├ 結果確認(全ランク)＆出力(ルートランクのみ)
 │  │
 │  ├  MPI_Reduce()  :全ランクのテスト結果をCOMM_WORLDのルートランクに集める
 │  │
 │  ├  MPI_Reduce()  :全ランクのVBGチェック結果をCOMM_WORLDのルートランクに集める
 │  │
 │  ├  MPI_Free_comm() (MPI_COMM_WORLD,*_NO_FREE以外の場合。コミュニケータテスト用)
 │ 
 ├ 各種解放処理
 │ 
 ├ MPI_Finalize()
 │ 
 ├ テスト結果の出力


