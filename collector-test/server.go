package main

import (
	"flag"
	"net"
	"fmt"
	"log"
	"google.golang.org/grpc"
	"strconv"

	pb "github.com/stackrox/collector/go-proto-generated/proto/api/private/signal-service"
)

var port       = flag.Int("port", 9092, "The server port")

type signalServer struct {

}

func newServer() *signalServer {
	return &signalServer{}
}

func (s *signalServer) PushSignals(stream pb.SignalService_PushSignalsServer) error {
	for {
		signal, err := stream.Recv()
		if err != nil {
			return err
		}
		fmt.Printf("%v\n", signal)
		if signal.GetSignal().GetNetworkSignal() != nil {
			if  signal.GetSignal().GetNetworkSignal().GetClientAddress().GetIpv4Address() != nil {
				fmt.Printf("client endpoint: %s:%d\n", InttoIP4(signal.GetSignal().GetNetworkSignal().GetClientAddress().GetIpv4Address().GetAddress()), signal.GetSignal().GetNetworkSignal().GetClientAddress().GetIpv4Address().Port)
				fmt.Printf("server endpoint: %s:%d\n", InttoIP4(signal.GetSignal().GetNetworkSignal().GetServerAddress().GetIpv4Address().GetAddress()), signal.GetSignal().GetNetworkSignal().GetServerAddress().GetIpv4Address().Port)
			}
		}
	}
}

func InttoIP4(ipInt uint32) string {
	ipAddr := int64(ipInt)
	// need to do two bit shifting and “0xff” masking
	b0 := strconv.FormatInt((ipAddr>>24)&0xff, 10)
	b1 := strconv.FormatInt((ipAddr>>16)&0xff, 10)
	b2 := strconv.FormatInt((ipAddr>>8)&0xff, 10)
	b3 := strconv.FormatInt((ipAddr & 0xff), 10)
	return b0 + "." + b1 + "." + b2 + "." + b3
}

func main() {
	flag.Parse()
	lis, err := net.Listen("tcp", fmt.Sprintf("127.0.0.1:%d", *port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	grpcServer := grpc.NewServer()
	pb.RegisterSignalServiceServer(grpcServer, newServer())
	grpcServer.Serve(lis)
}
