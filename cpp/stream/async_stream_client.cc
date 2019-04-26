/**
 * @Copyright (C) 2019 kuroneko
 */
#include <grpcpp/create_channel.h>

#include <chrono> // NOLINT
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <boost/thread.hpp>

#include "stream/pb/stream_test.grpc.pb.h"

using boost::posix_time::seconds;
using boost::this_thread::sleep;
using boost::thread;
using grpc::ClientContext;
using grpc::ClientAsyncReaderWriterInterface;
using grpc::CompletionQueue;
using grpc::CreateChannel;
using grpc::InsecureChannelCredentials;
using grpc::Status;
using std::cin;
using std::cerr;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using std::cout;
using std::endl;
using std::flush;
using std::make_shared;
using std::ios_base;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using stream::pb::StreamTest;
using stream::pb::TestRequest;
using stream::pb::TestResponse;

using ReaderWriter
  = ClientAsyncReaderWriterInterface<TestRequest, TestResponse>;
using NextStatus = grpc::CompletionQueue::NextStatus;

namespace {

const char *kAddr = "unix:/tmp/stream_test.sock";
const int32_t kBufferSize = 1024;

string GetInput(void) {
  static char buffer[kBufferSize];
  string input;
  while (!cin.bad() && !cin.eof()) {
    cin.getline(buffer, kBufferSize);
    input += string(buffer);
    if (cin.fail()) {
      cin.clear(cin.rdstate() & ~ios_base::failbit);
    } else {
      break;
    }
  }
  return input;
}

class StreamHandler {
  class StreamManager;
 public:
  StreamHandler(void)
      : managers_(),
        queue_(),
        next_(0) {}
  void Run(void) {
    cout << "===== " << __func__ << " =====" << endl;
    void *raw_tag;
    bool ok;
    milliseconds timeout(1000);
    system_clock::time_point deadline(system_clock::now() + timeout);
    while (this->queue_.Next(&raw_tag, &ok)) {
      if (raw_tag == nullptr) {
        continue;
      }
      unique_ptr<Tag> tag(static_cast<Tag *>(raw_tag));
      if (tag->manager_->is_finished_) {
        continue;
      }
      switch (tag->type_) {
        case kRead: {
            if (!ok) {
              break;
            }
            cout << "Read" << endl;
            ReadTag *read_tag = new ReadTag(tag->manager_);
            tag->manager_->stream_->Read(&read_tag->response_, read_tag);
            cout << static_cast<ReadTag *>(tag.get())->response_.text() << endl;
          }
          break;

        case kStartCall:
          if (!ok) {
            // this->ConnectStream(tag->manager_);
          }
          cout << "ok: " << ok << ", type: " << tag->type_ << endl;
          break;

        case kFinish:
          if (!tag->manager_->is_finished_) {
            // this->ConnectStream(tag->manager_);
          }
          cout << "ok: " << ok << ", type: " << tag->type_ << endl;
          break;

        case kWrite: {
            cout << "ok: " << ok << ", type: " << tag->type_ << endl;
            if (ok) {
              break;
            }
            this->Write(static_cast<WriteTag *>(tag.get())->request_);
          }
          break;

        case kWritesDone:
          // FALL THROUGHT
        case kReadInitialMetadata:
          cout << "ok: " << ok << ", type: " << tag->type_ << endl;
          // do nothing
          break;
      }
    }
    cout << "called Run" << endl;
    return;
  }
  void Write(const TestRequest &request) {
    shared_ptr<StreamManager> manager = this->managers_[this->next_];
    manager->stream_->Write(request, new WriteTag(manager, request));
    this->next_ = ++this->next_ % this->managers_.size();
    return;
  }
  void Finish(void) {
    for (shared_ptr<StreamManager> manager : this->managers_) {
      manager->is_finished_ = true;
      manager->stream_->WritesDone(new Tag(manager, kWritesDone));
    }
    // これを呼ぶと Queue に貯まらなくなり
    // 全て吐き出したら Next は false を返す
    // AsyncNext は SHUTDOWN を返す
    this->queue_.Shutdown();
    return;
  }
  void AppendTestStream(const string &address) {
    cout << "===== " << __func__ << " =====" << endl;
    auto manager = make_shared<StreamManager>(StreamTest::NewStub(
        CreateChannel(address, InsecureChannelCredentials())));
    this->ConnectStream(manager);
    return;
  }
  void ConnectStream(shared_ptr<StreamManager> manager) {
    // context が開放されると落ちることを確認
    manager->stream_
      = manager->stub_->PrepareAsyncTest(&manager->context_, &this->queue_);
    manager->stream_->StartCall(new Tag(manager, kStartCall));
    // manager->stream_->ReadInitialMetadata(
    //     new Tag(manager, kReadInitialMetadata));
    // stream 終了時に渡される tag を設定することができる
    // status が開放されると問題があるかどうかは不明
    manager->stream_->Finish(
        &manager->status_, new Tag(manager, kFinish, true));

    ReadTag *read_tag = new ReadTag(manager);
    manager->stream_->Read(&read_tag->response_, read_tag);
    this->managers_.emplace_back(manager);
    return;
  }

 private:
  enum TagType {
    kWrite,
    kWritesDone,
    kStartCall,
    kRead,
    kReadInitialMetadata,
    kFinish,
  };
  struct StreamManager {
    explicit StreamManager(unique_ptr<StreamTest::StubInterface> &&stub)
        : stub_(move(stub)),
          stream_(nullptr),
          context_(),
          is_finished_(false) {}
    unique_ptr<StreamTest::StubInterface> stub_;
    shared_ptr<ReaderWriter> stream_;
    ClientContext context_;
    Status status_;
    bool is_finished_;
  };
  struct Tag {
    Tag(shared_ptr<StreamManager> manager,
        TagType type, bool is_finished = false)
        : manager_(manager),
          type_(type) {}
    virtual ~Tag(void) {}
    const shared_ptr<StreamManager> manager_;
    const TagType type_;
  };
  struct ReadTag : public Tag {
    explicit ReadTag(shared_ptr<StreamManager> manager)
        : Tag(manager, kRead),
          response_() {}
    TestResponse response_;
  };
  struct WriteTag : public Tag {
    WriteTag(shared_ptr<StreamManager> manager, const TestRequest &request)
        : Tag(manager, kWrite),
          request_(request) {}
    TestRequest request_;
  };
  vector<shared_ptr<StreamManager>> managers_;
  CompletionQueue queue_;
  int32_t next_;
};

}  // namespace

int main(void) {
  /*
  string name;
  while (true) {
    cout << "input name >> ";
    name = GetInput();
    if (!cin.good()) {
      cout << endl << "bye" << endl;
      return EXIT_SUCCESS;
    } else if (!name.empty()) {
      break;
    }
  }
  */

  StreamHandler handler;
  handler.AppendTestStream("unix:/tmp/stream_test2.sock");
  handler.AppendTestStream(kAddr);

  // thread read_thread(&StreamHandler::Run, &handler);
  thread read_thread(&StreamHandler::Run, &handler);

  TestRequest request;
  request.set_name("name");
  while (true) {
    cout << "input >> ";
    const string input = GetInput();
    if (!cin.good()) {
      // stream->WritesDone();
      cout << endl << "bye" << endl;
      break;
    } else if (input.empty()) {
      continue;
    }
    request.set_text(input);
    handler.Write(request);
    // if (!handler->Write(request)) {
    //   cerr << "Failed to send data." << endl;
    //   break;
    // }
  }

  handler.Finish();
  read_thread.join();
  return EXIT_SUCCESS;
}

