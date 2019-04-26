/**
 * @Copyright (C) 2019 kuroneko
 */
#include <grpcpp/create_channel.h>

#include <iostream>
#include <memory>
#include <string>
#include <boost/thread.hpp>

#include "stream/pb/stream_test.grpc.pb.h"

using grpc::ClientContext;
using grpc::ClientReaderWriterInterface;
using grpc::CreateChannel;
using grpc::InsecureChannelCredentials;
using std::cin;
using std::cerr;
using std::cout;
using std::endl;
using std::flush;
using std::ios_base;
using std::shared_ptr;
using std::string;
using stream::pb::StreamTest;
using stream::pb::TestRequest;
using stream::pb::TestResponse;

using ReaderWriter = ClientReaderWriterInterface<TestRequest, TestResponse>;

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

}  // namespace

int main(void) {
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

  shared_ptr<StreamTest::StubInterface> test_stream
    = StreamTest::NewStub(CreateChannel(kAddr, InsecureChannelCredentials()));
  ClientContext context;
  shared_ptr<ReaderWriter> stream = test_stream->Test(&context);

  boost::thread thread([stream] {
    TestResponse response;
    while (stream->Read(&response)) {
      cout << endl
        << "[" << response.name() << "]: " << response.text() << endl
        << "input >> "
        << flush;
    }
    return;
  });

  TestRequest request;
  request.set_name(name);
  while (true) {
    cout << "input >> ";
    const string input = GetInput();
    if (!cin.good()) {
      stream->WritesDone();
      cout << endl << "bye" << endl;
      break;
    } else if (input.empty()) {
      continue;
    }
    request.set_text(input);
    if (!stream->Write(request)) {
      thread.join();
      cerr << "Failed to send data." << endl;
      return EXIT_FAILURE;
    }
  }

  thread.join();
  return EXIT_SUCCESS;
}

