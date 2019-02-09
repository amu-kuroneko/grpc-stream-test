package main

import (
	"fmt"
	"log"
	"net"
	"os"
	"sync"

	"google.golang.org/grpc"

	pb "./pb"
)

const protocol string = "unix"
const address string = "/tmp/stream_test.sock"

// StreamTestService For gRPC service
type streamTestService struct {
	streams sync.Map
}

// Test For gRPC rpc
func (service *streamTestService) Test(stream pb.StreamTest_TestServer) error {
	service.streams.Store(&stream, stream)
	fmt.Printf("Add: %p\n", &stream)
	for {
		req, err := stream.Recv()
		if err != nil {
			service.streams.Delete(&stream)
			fmt.Printf("Remove: %p\n", &stream)
			break
		}
		fmt.Printf("[%s]: %s\n", req.Name, req.Text)
		res := &pb.TestResponse{}
		res.Name = req.Name
		res.Text = req.Text
		service.streams.Range(func(key, value interface{}) bool {
			if key == &stream {
				return true
			}
			if s, ok := value.(pb.StreamTest_TestServer); ok {
				s.Send(res)
			}
			return true
		})
	}
	return nil
}

func connect(protocol string, address string) (net.Listener, error) {
	if protocol == "unix" {
		if _, err := os.Stat(address); err == nil {
			if err := os.Remove(address); err != nil {
				return nil, err
			}
		}
	}
	return net.Listen(protocol, address)
}

func main() {
	listen, err := connect(protocol, address)
	if err != nil {
		log.Fatalln(err)
	}

	server := grpc.NewServer()
	pb.RegisterStreamTestServer(server, &streamTestService{})
	server.Serve(listen)
}
