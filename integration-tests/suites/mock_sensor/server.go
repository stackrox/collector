package mock_sensor

import (
	"fmt"
	"log"
	"net"
	"sync"
	"time"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
	utils "github.com/stackrox/rox/pkg/net"

	"github.com/stackrox/rox/generated/storage"
	"google.golang.org/grpc"
	"google.golang.org/grpc/keepalive"

	"github.com/stackrox/collector/integration-tests/suites/types"
)

const (
	gMockSensorPort = 9999
	gMaxMsgSize     = 12 * 1024 * 1024
)

type ProcessMap map[types.ProcessInfo]interface{}
type LineageMap map[types.ProcessLineage]interface{}
type ConnMap map[types.NetworkInfo]interface{}
type EndpointMap map[types.EndpointInfo]interface{}

type MockSensor struct {
	sensorAPI.UnimplementedSignalServiceServer
	sensorAPI.UnimplementedNetworkConnectionInfoServiceServer

	listener   net.Listener
	grpcServer *grpc.Server

	processes       map[string]ProcessMap
	processLineages map[string]LineageMap
	processMutex    sync.Mutex

	connections  map[string]ConnMap
	endpoints    map[string]EndpointMap
	networkMutex sync.Mutex

	processChannel    chan *storage.ProcessSignal
	lineageChannel    chan *storage.ProcessSignal_LineageInfo
	connectionChannel chan *sensorAPI.NetworkConnection
	endpointChannel   chan *sensorAPI.NetworkEndpoint
}

func NewMockSensor() *MockSensor {
	return &MockSensor{
		processes:       make(map[string]ProcessMap),
		processLineages: make(map[string]LineageMap),
		connections:     make(map[string]ConnMap),
		endpoints:       make(map[string]EndpointMap),

		processChannel:    make(chan *storage.ProcessSignal, 32),
		lineageChannel:    make(chan *storage.ProcessSignal_LineageInfo, 32),
		connectionChannel: make(chan *sensorAPI.NetworkConnection, 32),
		endpointChannel:   make(chan *sensorAPI.NetworkEndpoint, 32),
	}
}

func (m *MockSensor) LiveProcesses() <-chan *storage.ProcessSignal {
	return m.processChannel
}

func (m *MockSensor) Processes(containerID string) []types.ProcessInfo {
	if processes, ok := m.processes[containerID]; ok {
		keys := make([]types.ProcessInfo, 0, len(processes))
		for k := range processes {
			keys = append(keys, k)
		}
		return keys
	}
	return make([]types.ProcessInfo, 0)
}

func (m *MockSensor) HasProcess(containerID string, process types.ProcessInfo) bool {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	if processes, ok := m.processes[containerID]; ok {
		_, exists := processes[process]
		return exists
	}

	return false
}

func (m *MockSensor) LiveLineages() <-chan *storage.ProcessSignal_LineageInfo {
	return m.lineageChannel
}

func (m *MockSensor) ProcessLineages(containerID string) []types.ProcessLineage {
	if lineages, ok := m.processLineages[containerID]; ok {
		keys := make([]types.ProcessLineage, 0, len(lineages))
		for k := range lineages {
			keys = append(keys, k)
		}
		return keys
	}
	return make([]types.ProcessLineage, 0)
}

func (m *MockSensor) HasLineage(containerID string, lineage types.ProcessLineage) bool {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	if lineages, ok := m.processLineages[containerID]; ok {
		_, exists := lineages[lineage]
		return exists
	}

	return false
}

func (m *MockSensor) LiveConnections() <-chan *sensorAPI.NetworkConnection {
	return m.connectionChannel
}

func (m *MockSensor) Connections(containerID string) []types.NetworkInfo {
	if connections, ok := m.connections[containerID]; ok {
		keys := make([]types.NetworkInfo, 0, len(connections))
		for k := range connections {
			keys = append(keys, k)
		}
		return keys
	}
	return make([]types.NetworkInfo, 0)
}

func (m *MockSensor) HasConnection(containerID string, conn types.NetworkInfo) bool {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	if conns, ok := m.connections[containerID]; ok {
		_, exists := conns[conn]
		return exists
	}

	return false
}

func (m *MockSensor) LiveEndpoints() <-chan *sensorAPI.NetworkEndpoint {
	return m.endpointChannel
}

func (m *MockSensor) Endpoints(containerID string) []types.EndpointInfo {
	if endpoints, ok := m.endpoints[containerID]; ok {
		keys := make([]types.EndpointInfo, 0, len(endpoints))
		for k := range endpoints {
			keys = append(keys, k)
		}
		return keys
	}
	return make([]types.EndpointInfo, 0)
}

func (m *MockSensor) HasEndpoint(containerID string, endpoint types.EndpointInfo) bool {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	if endpoints, ok := m.endpoints[containerID]; ok {
		_, exists := endpoints[endpoint]
		return exists
	}

	return false
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

	m.processes = make(map[string]ProcessMap)
	m.connections = make(map[string]ConnMap)
	m.endpoints = make(map[string]EndpointMap)
}

func (m *MockSensor) PushSignals(stream sensorAPI.SignalService_PushSignalsServer) error {
	for {
		signal, err := stream.Recv()
		if err != nil {
			return err
		}

		if signal != nil && signal.GetSignal() != nil && signal.GetSignal().GetProcessSignal() != nil {
			processSignal := signal.GetSignal().GetProcessSignal()
			m.pushProcess(processSignal.GetContainerId(), processSignal)
			m.processChannel <- processSignal

			for _, lineage := range processSignal.GetLineageInfo() {
				m.pushLineage(processSignal.GetContainerId(), processSignal, lineage)
				m.lineageChannel <- lineage
			}
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
			m.pushEndpoint(endpoint.GetContainerId(), endpoint)
			m.endpointChannel <- endpoint
		}

		for _, connection := range connections {
			m.pushConnection(connection.GetContainerId(), connection)
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

	process := types.ProcessInfo{
		Name:    processSignal.GetName(),
		ExePath: processSignal.GetExecFilePath(),
		Uid:     int(processSignal.GetUid()),
		Gid:     int(processSignal.GetGid()),
		Args:    processSignal.GetArgs(),
	}

	if processes, ok := m.processes[containerID]; ok {
		processes[process] = true
	} else {
		processes := ProcessMap{process: true}
		m.processes[containerID] = processes
	}
}

func (m *MockSensor) pushLineage(containerID string, process *storage.ProcessSignal, lineage *storage.ProcessSignal_LineageInfo) {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	lin := types.ProcessLineage{
		Name:          process.GetName(),
		ParentExePath: lineage.GetParentExecFilePath(),
		ParentUid:     int(lineage.GetParentUid()),
	}

	if lineages, ok := m.processLineages[containerID]; ok {
		lineages[lin] = true
	} else {
		lineages := LineageMap{lin: true}
		m.processLineages[containerID] = lineages
	}
}

func (m *MockSensor) pushConnection(containerID string, connection *sensorAPI.NetworkConnection) {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	conn := types.NetworkInfo{
		LocalAddress:  m.translateAddress(connection.LocalAddress),
		RemoteAddress: m.translateAddress(connection.RemoteAddress),
	}

	if connections, ok := m.connections[containerID]; ok {
		connections[conn] = true
	} else {
		connections := ConnMap{conn: true}
		m.connections[containerID] = connections
	}
}

func (m *MockSensor) pushEndpoint(containerID string, endpoint *sensorAPI.NetworkEndpoint) {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	ep := types.EndpointInfo{
		Protocol: endpoint.GetProtocol().String(),
	}

	if endpoints, ok := m.endpoints[containerID]; ok {
		endpoints[ep] = true
	} else {
		endpoints := EndpointMap{ep: true}
		m.endpoints[containerID] = endpoints
	}
}

func (m *MockSensor) translateAddress(addr *sensorAPI.NetworkAddress) string {
	ipPortPair := utils.NetworkPeerID{
		Address: utils.IPFromBytes(addr.GetAddressData()),
		Port:    uint16(addr.GetPort()),
	}
	return ipPortPair.String()
}
