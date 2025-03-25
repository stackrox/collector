package mock_sensor

import (
	"fmt"
	"log"
	"net"
	"os"
	"slices"
	"strings"
	"sync"
	"time"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"

	"google.golang.org/grpc"
	"google.golang.org/grpc/keepalive"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

const (
	gMockSensorPort = 9999
	gMaxMsgSize     = 12 * 1024 * 1024

	gDefaultRingSize = 32
)

// Using maps in this way allows us a very quick
// way of identifying when specific events arrive. (Go allows
// us to use any comparable type as the key)
type ProcessMap map[types.ProcessInfo]interface{}
type LineageMap map[types.ProcessLineage]interface{}
type EndpointMap map[types.EndpointInfo]interface{}

type MockSensor struct {
	testName string
	logger   *log.Logger
	logFile  *os.File

	listener   net.Listener
	grpcServer *grpc.Server

	processes       map[string]ProcessMap
	processLineages map[string]LineageMap
	processMutex    sync.Mutex

	connections  map[string][]types.NetworkInfoBatch
	endpoints    map[string]EndpointMap
	networkMutex sync.Mutex

	// every event will be forwarded to these channels, to allow
	// tests to look directly at the incoming data without
	// losing anything underneath
	processChannel    RingChan[*sensorAPI.ProcessSignal]
	lineageChannel    RingChan[*sensorAPI.ProcessSignal_LineageInfo]
	connectionChannel RingChan[*sensorAPI.NetworkConnection]
	endpointChannel   RingChan[*sensorAPI.NetworkEndpoint]
}

func NewMockSensor(test string) *MockSensor {
	return &MockSensor{
		testName:        test,
		processes:       make(map[string]ProcessMap),
		processLineages: make(map[string]LineageMap),
		connections:     make(map[string][]types.NetworkInfoBatch),
		endpoints:       make(map[string]EndpointMap),
	}
}

// LiveProcesses returns a channel that can be used to read live
// process events
func (m *MockSensor) LiveProcesses() <-chan *sensorAPI.ProcessSignal {
	return m.processChannel.Stream()
}

// Processes returns a list of all processes that have been receieved for
// a given container ID
func (m *MockSensor) Processes(containerID string) []types.ProcessInfo {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	if processes, ok := m.processes[containerID]; ok {
		keys := make([]types.ProcessInfo, 0, len(processes))
		for k := range processes {
			keys = append(keys, k)
		}
		return keys
	}
	return make([]types.ProcessInfo, 0)
}

// HasProcess returns whether a given process has been seen for a given
// container ID.
func (m *MockSensor) HasProcess(containerID string, process types.ProcessInfo) bool {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	if processes, ok := m.processes[containerID]; ok {
		_, exists := processes[process]
		return exists
	}

	return false
}

// LiveLineages returns a channel that can be used to read live
// process lineage events
func (m *MockSensor) LiveLineages() <-chan *sensorAPI.ProcessSignal_LineageInfo {
	return m.lineageChannel.Stream()
}

// ProcessLineages returns a list of all processes that have been received for
// a given container ID
func (m *MockSensor) ProcessLineages(containerID string) []types.ProcessLineage {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	if lineages, ok := m.processLineages[containerID]; ok {
		keys := make([]types.ProcessLineage, 0, len(lineages))
		for k := range lineages {
			keys = append(keys, k)
		}
		return keys
	}
	return make([]types.ProcessLineage, 0)
}

// HasLineage returns whether a given process lineage has been seen for a given
// container ID
func (m *MockSensor) HasLineage(containerID string, lineage types.ProcessLineage) bool {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	if lineages, ok := m.processLineages[containerID]; ok {
		_, exists := lineages[lineage]
		return exists
	}

	return false
}

// LiveConnections returns a channel that can be used to read live
// connection events
func (m *MockSensor) LiveConnections() <-chan *sensorAPI.NetworkConnection {
	return m.connectionChannel.Stream()
}

// Connections returns a list of all connections that have been received for
// a given container ID
func (m *MockSensor) GetConnectionsInBatches(containerID string) []types.NetworkInfoBatch {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	if connections, ok := m.connections[containerID]; ok {
		conns := make([]types.NetworkInfoBatch, len(connections))
		copy(conns, connections)
		for _, conn := range conns {
			types.SortConnections(conn)
		}

		return conns
	}
	return make([]types.NetworkInfoBatch, 0)
}

// Connections returns a list of all connections that have been received for
// a given container ID
func (m *MockSensor) Connections(containerID string) []types.NetworkInfo {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	allConns := make([]types.NetworkInfo, 0)
	if connections, ok := m.connections[containerID]; ok {
		conns := make([]types.NetworkInfoBatch, len(connections))
		copy(conns, connections)
		for _, conn := range conns {
			allConns = append(allConns, conn...)
		}

		types.SortConnections(allConns)

		return allConns
	}
	return make([]types.NetworkInfo, 0)
}

// HasConnection returns whether a given connection has been seen for a given
// container ID
func (m *MockSensor) HasConnection(containerID string, conn types.NetworkInfo) bool {
	conns := m.Connections(containerID)
	if len(conns) > 0 {
		return slices.ContainsFunc(conns, func(c types.NetworkInfo) bool {
			return c.Equal(conn)
		})
	}

	return false
}

// Liveendpoints returns a channel that can be used to read live
// endpoint events
func (m *MockSensor) LiveEndpoints() <-chan *sensorAPI.NetworkEndpoint {
	return m.endpointChannel.Stream()
}

// Endpoints returns a list of all endpoints that have been received for
// a given container ID
func (m *MockSensor) Endpoints(containerID string) []types.EndpointInfo {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	if endpoints, ok := m.endpoints[containerID]; ok {
		keys := make([]types.EndpointInfo, 0, len(endpoints))
		for k := range endpoints {
			keys = append(keys, k)
		}
		return keys
	}
	return make([]types.EndpointInfo, 0)
}

// HasEndpoint returns whether a given endpoint has been seen for a given
// container ID
func (m *MockSensor) HasEndpoint(containerID string, endpoint types.EndpointInfo) bool {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	if endpoints, ok := m.endpoints[containerID]; ok {
		for ep := range endpoints {
			if ep.Equal(endpoint) {
				return true
			}
		}
	}

	return false
}

// Start will initialize the gRPC server and begin serving
// The server itself runs in a separate thread.
func (m *MockSensor) Start() {
	var err error

	m.logFile, err = common.PrepareLog(m.testName, "events.log")

	if err != nil {
		log.Fatalf("failed to open log file: %v", err)
	}

	m.logger = log.New(m.logFile, "", log.LstdFlags)

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

	sensorAPI.RegisterCollectorServiceServer(m.grpcServer, m)
	sensorAPI.RegisterNetworkConnectionInfoServiceServer(m.grpcServer, m)

	m.processChannel = NewRingChan[*sensorAPI.ProcessSignal](gDefaultRingSize)
	m.lineageChannel = NewRingChan[*sensorAPI.ProcessSignal_LineageInfo](gDefaultRingSize)
	m.connectionChannel = NewRingChan[*sensorAPI.NetworkConnection](gDefaultRingSize)
	m.endpointChannel = NewRingChan[*sensorAPI.NetworkEndpoint](gDefaultRingSize)

	go func() {
		if err := m.grpcServer.Serve(m.listener); err != nil {
			log.Fatalf("failed to serve: %v", err)
		}
	}()
}

// Stop will shut down the gRPC server and clear the internal store of
// all events
func (m *MockSensor) Stop() {
	m.grpcServer.Stop()
	m.listener.Close()
	m.logFile.Close()
	m.logger = nil

	m.processes = make(map[string]ProcessMap)
	m.processLineages = make(map[string]LineageMap)
	m.connections = make(map[string][]types.NetworkInfoBatch)
	m.endpoints = make(map[string]EndpointMap)

	m.processChannel.Stop()
	m.lineageChannel.Stop()
	m.connectionChannel.Stop()
	m.endpointChannel.Stop()
}

func (m *MockSensor) convertConnection(connection *sensorAPI.NetworkConnection) types.NetworkInfo {
	conn := types.NetworkInfo{
		LocalAddress:   types.TranslateAddress(connection.LocalAddress),
		RemoteAddress:  types.TranslateAddress(connection.RemoteAddress),
		Role:           connection.GetRole().String(),
		SocketFamily:   connection.GetSocketFamily().String(),
		CloseTimestamp: connection.GetCloseTimestamp().String(),
	}

	m.logger.Printf("NetworkInfo: %s, %s\n", connection.GetContainerId(), conn)

	return conn
}

func (m *MockSensor) convertToContainerConnsMap(connections []*sensorAPI.NetworkConnection) map[string][]types.NetworkInfo {
	containerConnsMap := make(map[string][]types.NetworkInfo)
	for _, connection := range connections {
		conn := m.convertConnection(connection)
		containerID := connection.GetContainerId()

		if c, ok := containerConnsMap[containerID]; ok {
			containerConnsMap[containerID] = append(c, conn)
		} else {
			containerConnsMap[containerID] = []types.NetworkInfo{conn}
		}
	}

	return containerConnsMap
}

// Communicate conforms to the Sensor API. It is here that process signals and
// process lineage information is handled and stored/sent to the relevant channel
func (m *MockSensor) Communicate(stream sensorAPI.CollectorService_CommunicateServer) error {
	for {
		signal, err := stream.Recv()
		if err != nil {
			return err
		}

		switch signal.GetMsg().(type) {
		case *sensorAPI.MsgFromCollector_ProcessSignal:
			m.pushSignal(signal.GetProcessSignal())
		case *sensorAPI.MsgFromCollector_Register:
			return nil
		case *sensorAPI.MsgFromCollector_Info:
			return nil
		}
	}
}

func (m *MockSensor) pushSignal(signal *sensorAPI.ProcessSignal) error {
	if signal == nil {
		return nil
	}

	if strings.HasPrefix(signal.GetExecFilePath(), "/proc/self") {
		//
		// There exists a potential race condition for the driver
		// to capture very early container process events.
		//
		// This is known in falco, and somewhat documented here:
		//     https://github.com/falcosecurity/falco/blob/555bf9971cdb79318917949a5e5f9bab5293b5e2/rules/falco_rules.yaml#L1961
		//
		// It is also filtered in sensor here:
		//    https://github.com/stackrox/stackrox/blob/4d3fb539547d1935a35040e4a4e8c258a53a92e4/sensor/common/signal/signal_service.go#L90
		//
		// Further details can be found here https://issues.redhat.com/browse/ROX-11544
		//
		m.logger.Printf("runtime-process: %s %s:%s:%d:%d:%d:%s\n",
			signal.GetContainerId(),
			signal.GetName(),
			signal.GetExecFilePath(),
			signal.GetUid(),
			signal.GetGid(),
			signal.GetPid(),
			signal.GetArgs())
		return nil
	}

	m.pushProcess(signal.GetContainerId(), signal)
	m.processChannel.Write(signal)

	for _, lineage := range signal.GetLineageInfo() {
		m.pushLineage(signal.GetContainerId(), signal, lineage)
		m.lineageChannel.Write(lineage)
	}
	return nil
}

// PushNetworkConnectionInfo conforms to the Sensor API. It is here that networking
// events (connections and endpoints) are handled and stored/sent to the relevant channel
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
			m.endpointChannel.Write(endpoint)
		}

		containerConnsMap := m.convertToContainerConnsMap(connections)
		m.pushConnections(containerConnsMap)
		for _, connection := range connections {
			m.connectionChannel.Write(connection)
		}
	}
}

// pushProcess converts a process signal into the test's own structure
// and stores it
func (m *MockSensor) pushProcess(containerID string, processSignal *sensorAPI.ProcessSignal) {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	m.logger.Printf("ProcessInfo: %s %s:%s:%d:%d:%d:%s\n",
		processSignal.GetContainerId(),
		processSignal.GetName(),
		processSignal.GetExecFilePath(),
		processSignal.GetUid(),
		processSignal.GetGid(),
		processSignal.GetPid(),
		processSignal.GetArgs())

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

// pushLineage converts a process lineage into the test's own structure
// and stores it
func (m *MockSensor) pushLineage(containerID string, process *sensorAPI.ProcessSignal, lineage *sensorAPI.ProcessSignal_LineageInfo) {
	m.processMutex.Lock()
	defer m.processMutex.Unlock()

	m.logger.Printf("ProcessLineageInfo: %s %s:%s:%s:%d:%s:%s\n",
		process.GetContainerId(),
		process.GetName(),
		process.GetExecFilePath(),
		"ParentUid", lineage.GetParentUid(),
		"ParentExecFilePath", lineage.GetParentExecFilePath())

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

func (m *MockSensor) pushConnections(containerConnsMap map[string][]types.NetworkInfo) {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	for containerID, connections := range containerConnsMap {
		if c, ok := m.connections[containerID]; ok {
			m.connections[containerID] = append(c, connections)
		} else {
			m.connections[containerID] = []types.NetworkInfoBatch{connections}
		}
	}
}

// pushEndpoint converts an endpoint event into the test's own structure
// and stores it
func (m *MockSensor) pushEndpoint(containerID string, endpoint *sensorAPI.NetworkEndpoint) {
	m.networkMutex.Lock()
	defer m.networkMutex.Unlock()

	m.logger.Printf("EndpointInfo: %s|%s|%s|%s|%s\n",
		endpoint.GetSocketFamily().String(),
		endpoint.GetProtocol().String(),
		endpoint.GetListenAddress().String(),
		endpoint.GetCloseTimestamp().String(),
		endpoint.GetOriginator().String())

	var originator types.ProcessOriginator
	if endpoint.GetOriginator() != nil {
		originator = types.ProcessOriginator{
			ProcessName:         endpoint.GetOriginator().ProcessName,
			ProcessArgs:         endpoint.GetOriginator().ProcessArgs,
			ProcessExecFilePath: endpoint.GetOriginator().ProcessExecFilePath,
		}
	}

	var listen types.ListenAddress
	if endpoint.GetListenAddress() != nil {
		listen = types.ListenAddress{
			AddressData: string(endpoint.GetListenAddress().GetAddressData()),
			Port:        int(endpoint.GetListenAddress().GetPort()),
			IpNetwork:   string(endpoint.GetListenAddress().GetIpNetwork()),
		}
	}

	ep := types.EndpointInfo{
		Protocol:       endpoint.GetProtocol().String(),
		Originator:     originator,
		CloseTimestamp: endpoint.GetCloseTimestamp().String(),
		Address:        listen,
	}

	if endpoints, ok := m.endpoints[containerID]; ok {
		endpoints[ep] = true
	} else {
		endpoints := EndpointMap{ep: true}
		m.endpoints[containerID] = endpoints
	}
}

func (m *MockSensor) SetTestName(testName string) {
	m.testName = testName
}
