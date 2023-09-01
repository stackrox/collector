package mock_sensor

import (
	"fmt"
	"log"
	"net"
	"sync"
	"time"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
	"github.com/stackrox/rox/generated/storage"
	"google.golang.org/grpc"
	"google.golang.org/grpc/keepalive"
)

const (
	gMockSensorPort = 9999
	gMaxMsgSize     = 12 * 1024 * 1024
)

type MockSensor struct {
	sensorAPI.UnimplementedSignalServiceServer
	sensorAPI.UnimplementedNetworkConnectionInfoServiceServer

	listener   net.Listener
	grpcServer *grpc.Server

	processes       map[string][]*storage.ProcessSignal
	processMutex    sync.Mutex
	processWatchers []<-chan interface{}

	connections     map[string][]*sensorAPI.NetworkConnection
	endpoints       map[string][]*sensorAPI.NetworkEndpoint
	networkMutex    sync.Mutex
	networkWatchers []<-chan interface{}

	processChannel    chan *storage.ProcessSignal
	connectionChannel chan *sensorAPI.NetworkConnection
	endpointChannel   chan *sensorAPI.NetworkEndpoint
}

func NewMockSensor() *MockSensor {
	return &MockSensor{
		processes:   make(map[string][]*storage.ProcessSignal),
		connections: make(map[string][]*sensorAPI.NetworkConnection),
		endpoints:   make(map[string][]*sensorAPI.NetworkEndpoint),

		processChannel:    make(chan *storage.ProcessSignal, 32),
		connectionChannel: make(chan *sensorAPI.NetworkConnection, 32),
		endpointChannel:   make(chan *sensorAPI.NetworkEndpoint, 32),
	}
}

func (m *MockSensor) Processes() <-chan *storage.ProcessSignal {
	return m.processChannel
}

func (m *MockSensor) Connections() <-chan *sensorAPI.NetworkConnection {
	return m.connectionChannel
}

func (m *MockSensor) Endpoints() <-chan *sensorAPI.NetworkEndpoint {
	return m.endpointChannel
}

func (m *MockSensor) Start() {
	var err error
	m.listener, err = net.Listen("tcp", fmt.Sprintf(":%d", gMockSensorPort))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	m.grpcServer = grpc.NewServer(
		grpc.MaxRecvMsgSize(gMaxMsgSize),
		grpc.KeepaliveParams(keepalive.ServerParameters{
			Time: 40 * time.Second,
		}),
		grpc.KeepaliveEnforcementPolicy(keepalive.EnforcementPolicy{
			MinTime:             5 * time.Second,
			PermitWithoutStream: true,
		}),
	)

	sensorAPI.RegisterSignalServiceServer(m.grpcServer, m)
	sensorAPI.RegisterNetworkConnectionInfoServiceServer(m.grpcServer, m)

	go func() {
		if err := m.serve(); err != nil {
			log.Fatalf("failed to serve: %v", err)
		}
	}()
}

func (m *MockSensor) Stop() {
	m.grpcServer.Stop()
	m.listener.Close()

	m.processes = make(map[string][]*storage.ProcessSignal)
	m.connections = make(map[string][]*sensorAPI.NetworkConnection)
	m.endpoints = make(map[string][]*sensorAPI.NetworkEndpoint)
}

func (m *MockSensor) PushSignals(stream sensorAPI.SignalService_PushSignalsServer) error {
	for {
		signal, err := stream.Recv()
		if err != nil {
			return err
		}

		if signal != nil && signal.GetSignal() != nil && signal.GetSignal().GetProcessSignal() != nil {
			processSignal := signal.GetSignal().GetProcessSignal()
			processInfo := fmt.Sprintf("%s:%s:%d:%d:%d:%s", processSignal.GetName(), processSignal.GetExecFilePath(), processSignal.GetUid(), processSignal.GetGid(), processSignal.GetPid(), processSignal.GetArgs())
			fmt.Printf("+ ProcessInfo: %s\n", processInfo)
			m.processChannel <- processSignal
			// m.pushProcess(processSignal.GetContainerId(), processSignal)
		}
	}
}

func (m *MockSensor) PushNetworkConnectionInfo(stream sensorAPI.NetworkConnectionInfoService_PushNetworkConnectionInfoServer) error {
	for {
		signal, err := stream.Recv()
		if err != nil {
			return err
		}

		networkConnInfo := signal.GetInfo()
		connections := networkConnInfo.GetUpdatedConnections()
		endpoints := networkConnInfo.GetUpdatedEndpoints()

		for _, endpoint := range endpoints {
			endpointInfo := fmt.Sprintf("EndpointInfo: %s|%s|%s|%s|%s\n", endpoint.GetSocketFamily().String(), endpoint.GetProtocol().String(), endpoint.GetListenAddress().String(), endpoint.GetCloseTimestamp().String(), endpoint.GetOriginator().String())
			fmt.Printf("EndpointInfo: %s %s\n", endpoint.GetContainerId(), endpointInfo)
			// m.pushEndpoint(endpoint.GetContainerId(), endpoint)
			m.endpointChannel <- endpoint
		}

		for _, connection := range connections {
			networkInfo := fmt.Sprintf("%v|%s|%s|%s|%s", connection.GetLocalAddress(), connection.GetRemoteAddress(), connection.GetRole().String(), connection.GetSocketFamily().String(), connection.GetCloseTimestamp().String())
			fmt.Printf("NetworkInfo: %s %s\n", connection.GetContainerId(), networkInfo)
			// m.pushConnection(connection.GetContainerId(), connection)
			m.connectionChannel <- connection
		}
	}
}

func (m *MockSensor) serve() error {
	return m.grpcServer.Serve(m.listener)
}

func (m *MockSensor) pushProcess(containerID string, processSignal *storage.ProcessSignal) {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	if processes, ok := m.processes[containerID]; ok {
		processes = append(processes, processSignal)
	} else {
		processes := []*storage.ProcessSignal{processSignal}
		m.processes[containerID] = processes
	}
}

func (m *MockSensor) pushConnection(containerID string, connection *sensorAPI.NetworkConnection) {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	if connections, ok := m.connections[containerID]; ok {
		connections = append(connections, connection)
	} else {
		connections := []*sensorAPI.NetworkConnection{connection}
		m.connections[containerID] = connections
	}
}

func (m *MockSensor) pushEndpoint(containerID string, endpoint *sensorAPI.NetworkEndpoint) {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	if endpoints, ok := m.endpoints[containerID]; ok {
		endpoints = append(endpoints, endpoint)
	} else {
		endpoints := []*sensorAPI.NetworkEndpoint{endpoint}
		m.endpoints[containerID] = endpoints
	}
}
