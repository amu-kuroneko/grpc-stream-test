C++ で簡単な Chat System を書いてみた。

# Environment
Mac OS X (High Sierra) 上で開発を行い、下記の Version で Build を行った
* `g++`: 4.2.1  
* `go`: 1.11.5

# Packages
上記環境に加えて更に必要な Package の Install を行った

## gRPC
gRPC で通信を行う必要があるため gRPC の Install を行った。gRPC を Install することで、\*.proto File の Build に必要な protoc や grpc\_cpp\_plugin も同時に Install することができる

    $ brew tap grpc/grpc
    $ brew install grpc
## boost の Install
gRPC 通信を扱う上では必須ではないが、Chat System を構築する上で非同期処理が必要だったので Install した。

    $ brew install boost

# How to build
本 Program の Build は go, cpp のそれぞれの Directory で make を実行する

    $ (cd go && make)
    $ (cd cpp && make)
 
# Usage
はじめに Server を起動する必要があるため、golang で作成した stream-server を実行する

    $ go/bins/stream-server

次に Client を起動する

    $ cpp/bins/stream-client

＊ この System は Test のため Unix Domain Socket を用いた通信を行っているため、別 Server に対して通信を行うことはできない

# Develop
## proto
まず通信を行う形式を決めるために protos/stream\_test.proto を作成した。
内容は name と text を送って、同じ内容を返すだけのもの。  
ただし今回は双方向の stream 通信で行うため、Request, Response それぞれの型に対して stream 修飾子を指定している。

    service StreamTest {
      rpc Test(stream TestRequest) returns (stream TestResponse) {}
    }

    message TestRequest {
      string name = 1;
      string text = 2;
    }

    message TestResponse {
      string name = 1;
      string text = 2;
    }

## golang
golang の開発は全て `go` Directory で行うものとする。

golang で gRPC を用いた開発を行うためには下記の2つの Package を用いる必要がある。

    $ go get -u github.com/golang/protobuf/protoc-gen-go
    $ go get -u google.golang.org/grpc

`protoc-gen-go` は \*.proto の File を \*.go の File を作るために必要な protoc の Plugin となっている。  
また、`grpc` は golang で gRPC を利用するために必要な Package となっている。

準備ができたら、まずは作成した proto から go の Source Code を作成する必要がある

    # protoc -I <proto dir> --plugin=protoc-gen-go=<protoc-gen-go> --go_out=grpc:<output directory> <proto file>
    #   <proto dir>       : 作成した *.proto の File がある Directory を指定する
    #   <protoc-gen-go>   : Install した protoc-gen-go の Path を指定する
    #                       ただし、protoc-gen-go に既に Path が通っている場合は不要
    #   <output directory>: 生成された *.pb.go の File を出力する Directory を指定する
    #   <proto file>      : *.pb.go の File を作成する元となる proto File
    $ protoc -I protos --plugin=protoc-gen-go=$GOPATH/bin/protoc-gen-go --go_out=grpc:stream/pb stream_test.proto

すると stream/pb/stream\_test.pb.go という File が生成されていることが確認できる

これで gRPC を用いた golang の Program を書き始めることができる。  
golang で gRPC の Server を起動するためには主に2つの作業が必要となる。
1. 生成した stream/pb/stream\_test.pb.go で定義されている \<ServiceName\>Server と名前がついてる interface に沿った struct の実装
1. gRPC Server の起動

まず struct の実装だが、今回は StreamTestServer という interface が定義されており、Test という関数を持っていることが確認できる

    type StreamTestServer interface {
      Test(StreamTest_TestServer) error
    }

そのため、それに沿った struct を用意する

    import pb "./pb"
    type streamTestService struct {}
    func (service *streamTestService) Test(stream pb.StreamTest_TestServer) error {
      // do something
      return nil
    }

そして引数には Request も Response も stream にしている場合には入出力を stream でのみ行える interface が渡される

    type StreamTest_TestServer interface {
      Send(*TestResponse) error
      Recv() (*TestRequest, error)
      grpc.ServerStream
    }

使い方は Recv() で Client から送られてきた情報を受取、Send() で Client に情報を渡す。
また、処理を終了したい場合はそのまま return すれば stream は閉じられることになる。 

    func (service *streamTestService) Test(stream pb.StreamTest_TestServer) error {
      for { // Loop している限り Connection を繋いだ状態になる
        req, err := stream.Recv() // Client からの情報を受取る
        if err != nil { // Client が接続を切った場合にも err が入る
          break
        }
        if req.Text == "exit" { // 例えば Client から exit が送られて来たら処理を終了させる場合
          break
        }
        res := &pb.TestResponse{} // Client に返す情報を作成する
        res.Name = req.Name
        res.Text = req.Text
        stream.Send(res) // 入力をそのまま返すだけの Echo Server の場合の処理
      }
      return nil
    }

このように双方向で stream を用いた場合には任意の Timing で自由に情報を送ることができる。
そして一つの RPC に対して Connection を繋いだ状態で情報のやり取りが可能になる。
ただし、通常の関数 Call と同じようには扱えないので注意が必要。

struct の実装ができたら gRPC Server として起動させる

    listen, err := net.Listen("unix", "/tmp/stream_test.sock") // Unix Domain Socket で通信を行う
    if err != nil { // ただし Unix Domain Socket を用いる場合は既に File がある場合にも err となるので注意が必要
      log.Fatalln(err)
    }

    server := grpc.NewServer()
    pb.RegisterStreamTestServer(server, &streamTestService{}) // ここで先程作成した struct を渡してあげる
    server.Serve(listen) // Server として起動

## C++
C++ の開発は全て `cpp` Directory で行うものとする。

C++ で gRPC を用いた開発を行うためには golang と同様に、まずは作成した proto から C++ の Source Code を作成する必要がある。

    # gRPC の service 情報が記述されている C++ の Program を作成する
    # protoc -I <proto dir> --plugin=protoc-gen-grpc=<protoc-gen-grpc> --grpc_out=<output directory> <proto file>
    #   <proto dir>       : 作成した *.proto の File がある Directory を指定する
    #   <protoc-gen-grpc> : protoc-gen-grpc の Path を指定する。ただし、protoc-gen-grpc に既に Path が通っている場合は不要
    #   <output directory>: 生成された File を出力する Directory を指定する
    #   <proto file>      : *.grpc.pb.{cc,h} の File を作成する元となる proto File
    $ protoc -I protos --plugin=protoc-gen-grpc=/usr/local/bin/grpc\_cpp\_plugin --grpc_out=stream/pb stream_test.proto

    # gRPC の message 情報が記述されている C++ の Program を作成する
    # protoc -I <proto dir> --cpp_out=<output directory> <proto file>
    #   <proto dir>       : 作成した *.proto の File がある Directory を指定する
    #   <output directory>: 生成された File を出力する Directory を指定する
    #   <proto file>      : *.pb.{cc,h} の File を作成する元となる proto File
    $ protoc -I protos --cpp_out=stream/pb stream_test.proto

すると stream/pb Directory に service 情報が記述されている stream\_test.grpc.pb.cc と stream\_test.grpc.pb.h と、message 情報が記述されている stream\_test.pb.cc と stream\_test.pb.h の4つの File が生成されていることが確認できる。

これで gRPC を用いた C++ の Program を書き始めることができる。  
まず gRPC の Client を実装する場合は Server との Connection を確立する必要がある。

    // Server との Connection を確立するための関数は生成した stream/pb/stream_test.grpc.pb.h に記述されている
    // <package>::<ServiceName>::NewStab という構成になり、今回は package を stream.pb としているため
    // stream::pb::StreamTest::NewStub という形の関数を Call することで Connection を確立することができる
    auto stub = stream::pb::StreamTest::NewStub(grpc::CreateChannel(
        // Server への接続先は grpc::CreateChannel 関数の第一引数で指定することができる
        // 今回は Unix Domain Socket を利用している
        "unix:/tmp/stream_test.sock", grpc::InsecureChannelCredentials()));

Server との Connection が確立できたら実際に RPC で接続を行う。
    
    // Server に Header 情報を渡したい場合などに利用できる
    grpc::ClientContext context;
    // この返り値を用いて情報をやり取りを行う
    auto stream = stub->Test(&context);

Server に情報を送信する場合は下記のように記述することができる。

    // Server と情報をやり取りするための型は stream/pb/stream_test.pb.h に記述されている
    stream::pb::TestRequest request;
    request.set_name("name");
    request.set_text("text");

    // Server に stream 形式で、同じ情報を3回送信する場合の処理
    stream->Write(request);
    stream->Write(request);
    stream->Write(request);

    // Server に全ての情報を送り終えたら呼び出す必要がある
    stream->WritesDone();

Server から情報を取得する場合は下記のように記述することができる。

    stream::pb::TestResponse response;
    // Server からの全ての情報が取得できるまで Loop を回す
    // 取得に成功した場合は Read 関数は true を、取得できなかった場合に false を返すので、それで判定する
    while (stream->Read(&response)) {
        std::cout << "[" << response.name() << "]: " << response.text() << endl
    }

Program の記述が終わったら Compile する必要があるが、gRPC を利用する場合には下記を Link Option として指定する必要がある。
* `$(pkg-config --libs grpc++ grpc)`
  * 展開されると `-L/usr/local/Cellar/grpc/1.18.0/lib -lgrpc++ -lgrpc` となる
* `-lgrpc++_reflection `
* `-lprotobuf `

# Repository
[amu-kuroneko/grpc-stream-test](https://github.com/amu-kuroneko/grpc-stream-test)

# Author
[kuroneko](https://github.com/amu-kuroneko)

