package main

import (
	"flag"
	"net"
	"fmt"
	"google.golang.org/grpc"
	"google.golang.org/grpc/testdata"
	"google.golang.org/grpc/credentials"

	pb "bitbucket.org/stack-rox/stackrox/pkg/generated/proto/api/private/signal-service"
	"log"
)

var (
	tls        = flag.Bool("tls", false, "Connection uses TLS if true, else plain TCP")
	certFile   = flag.String("cert_file", "", "The TLS cert file")
	keyFile    = flag.String("key_file", "", "The TLS key file")
	port       = flag.Int("port", 9092, "The server port")
)

type signalServer struct {

}

func newServer() *signalServer {
	return &signalServer{}
}

func (s *signalServer) PushSignals(stream pb.SignalService_PushSignalsServer) error {
	count := 0
	for {
		signal, err := stream.Recv()
		if err != nil {
			return err
		}
		fmt.Println("%v", signal)
		count++
		if count % 10 == 0 {
			fmt.Println(count)
		}
	}
}

func main() {
	flag.Parse()
	lis, err := net.Listen("tcp", fmt.Sprintf("127.0.0.1:%d", *port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	var opts []grpc.ServerOption
	if *tls {
		if *certFile == "" {
			*certFile = testdata.Path("server1.pem")
		}
		if *keyFile == "" {
			*keyFile = testdata.Path("server1.key")
		}
		creds, err := credentials.NewServerTLSFromFile(*certFile, *keyFile)
		if err != nil {
			log.Fatalf("Failed to generate credentials %v", err)
		}
		opts = []grpc.ServerOption{grpc.Creds(creds)}
	}
	grpcServer := grpc.NewServer(opts...)
	pb.RegisterSignalServiceServer(grpcServer, newServer())
	grpcServer.Serve(lis)
}
