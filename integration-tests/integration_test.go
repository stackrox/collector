package integrationtests

import (
	"fmt"
	"strings"
	"testing"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
	"github.com/stackrox/rox/generated/storage"

	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/types"
	"github.com/stackrox/collector/integration-tests/suites"
)

const (
	// This is used when an IP address for a test is not known
	// before the test is run. This is the case for the remote
	// address field in ConnectionsAndEndpointsTestSuite. The
	// RemoteAddress is still specified when the
	// ConnectionsAndEndpointsTestSuite objects are created.
	// The port field of the RemoteAddress is set, but the
	// IP address is unknown so it is set to the placeholder.
	placeholderIP = "0.0.0.0"
)

func TestProcessNetwork(t *testing.T) {
	suite.Run(t, new(suites.ProcessNetworkTestSuite))
}

func TestImageLabelJSON(t *testing.T) {
	suite.Run(t, new(suites.ImageLabelJSONTestSuite))
}

// TestMissingProcScrape only works with local fake proc directory
func TestMissingProcScrape(t *testing.T) {
	if !config.HostInfo().IsK8s() {
		suite.Run(t, new(suites.MissingProcScrapeTestSuite))
	}
}

func TestRepeatedNetworkFlow(t *testing.T) {
	// Perform 11 curl commands with a 2 second sleep between each curl command.
	// The scrapeInterval is increased to 4 seconds to reduce the chance that jiter will effect the results.
	// The first server to client connection is recorded as being active.
	// The second through ninth curl commands are ignored, because of afterglow.
	// The last server to client connection is recorded as being inacitve when the afterglow period has expired
	// Thus the reported connections are active, inactive
	repeatedNetworkFlowTestSuite := &suites.RepeatedNetworkFlowTestSuite{
		AfterglowPeriod:        10,
		ScrapeInterval:         4,
		EnableAfterglow:        true,
		NumMetaIter:            1,
		NumIter:                11,
		SleepBetweenCurlTime:   2,
		SleepBetweenIterations: 1,
		ExpectedMinActive:      1,
		ExpectedMaxActive:      1,
		ExpectedInactive:       1,
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

func TestRepeatedNetworkFlowWithZeroAfterglowPeriod(t *testing.T) {
	// Afterglow is disables as the afterglowPeriod is 0
	// All server to client connections are reported.
	repeatedNetworkFlowTestSuite := &suites.RepeatedNetworkFlowTestSuite{
		AfterglowPeriod:        0,
		ScrapeInterval:         2,
		EnableAfterglow:        true,
		NumMetaIter:            1,
		NumIter:                3,
		SleepBetweenCurlTime:   3,
		SleepBetweenIterations: 1,
		// It is possible that at the scrap interval the connection will be active.
		// If the connections is active at t=0 and that coinsides with a scrape interval.
		// The next connection will be at t=3 and the next scrape is at t=2.
		// The next connection after that will be at t=6. That also coisides with a
		// scrape interval. That is a second opportunity for the connection to be active
		// during the scrape inteval. Therefore it is possible that between 0 and 2 active
		// connections will be seen by the test.
		ExpectedMinActive: 0,
		ExpectedMaxActive: 2,
		ExpectedInactive:  3,
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

func TestRepeatedNetworkFlowThreeCurlsNoAfterglow(t *testing.T) {
	// The afterglow period is set to 0 so afterglow is disabled
	repeatedNetworkFlowTestSuite := &suites.RepeatedNetworkFlowTestSuite{
		AfterglowPeriod:        0,
		ScrapeInterval:         4,
		EnableAfterglow:        false,
		NumMetaIter:            1,
		NumIter:                3,
		SleepBetweenCurlTime:   6,
		SleepBetweenIterations: 1,
		// Similar analysis as for the above test.
		ExpectedMinActive: 0,
		ExpectedMaxActive: 2,
		ExpectedInactive:  3,
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

// There is one test in which scraping is turned on and we expect to see
// endpoints opened before collector is turned on. There is another test
// in which scraping is turned off and we expect that we will not see
// endpoint opened before collector is turned on.
func TestProcfsScraper(t *testing.T) {
	connScraperTestSuite := &suites.ProcfsScraperTestSuite{
		TurnOffScrape:               false,
		RoxProcessesListeningOnPort: true,
		Expected: []types.EndpointInfo{
			{
				Protocol:       "L4_PROTOCOL_TCP",
				CloseTimestamp: types.NilTimestamp,
				Address: types.ListenAddress{
					AddressData: "\x00\x00\x00\x00",
					Port:        80,
					IpNetwork:   "\x00\x00\x00\x00 ",
				},
				Originator: types.ProcessOriginator{
					ProcessName:         "nginx",
					ProcessExecFilePath: "/usr/sbin/nginx",
					ProcessArgs:         "",
				},
			},
		},
	}
	suite.Run(t, connScraperTestSuite)
}

func TestProcfsScraperNoScrape(t *testing.T) {
	connScraperTestSuite := &suites.ProcfsScraperTestSuite{
		TurnOffScrape:               true,
		RoxProcessesListeningOnPort: true,
		Expected:                    []types.EndpointInfo{},
	}
	suite.Run(t, connScraperTestSuite)
}

func TestProcfsScraperDisableFeatureFlag(t *testing.T) {
	connScraperTestSuite := &suites.ProcfsScraperTestSuite{
		TurnOffScrape:               false,
		RoxProcessesListeningOnPort: false,
		Expected: []types.EndpointInfo{
			{
				Protocol:       "L4_PROTOCOL_TCP",
				CloseTimestamp: types.NilTimestamp,
				Address: types.ListenAddress{
					AddressData: "\x00\x00\x00\x00",
					Port:        80,
					IpNetwork:   "\x00\x00\x00\x00 ",
				},
				// expect endpoint but no originator
				Originator: types.ProcessOriginator{
					ProcessName:         "",
					ProcessExecFilePath: "",
					ProcessArgs:         "",
				},
			},
		},
	}
	suite.Run(t, connScraperTestSuite)
}

func TestProcessListeningOnPort(t *testing.T) {
	suite.Run(t, new(suites.ProcessListeningOnPortTestSuite))
}

func TestSymbolicLinkProcess(t *testing.T) {
	suite.Run(t, new(suites.SymbolicLinkProcessTestSuite))
}

func TestSocat(t *testing.T) {
	suite.Run(t, new(suites.SocatTestSuite))
}

func TestDuplicateEndpoints(t *testing.T) {
	suite.Run(t, new(suites.DuplicateEndpointsTestSuite))
}

func TestConnectionsAndEndpointsNormal(t *testing.T) {
	// Server uses a normal port. Client is assigned a port in the ephemeral range in the normal way
	port := 40
	normalPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-0",
			Cmd:  "socat TCP4-LISTEN:40,reuseaddr,fork - &",
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   types.CreateNetworkAddress("", "", uint32(port)),
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, 0),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
					Role:           sensorAPI.ClientServerRole_ROLE_SERVER,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: []types.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: types.ListenAddress{
						AddressData: "\x00\x00\x00\x00",
						Port:        port,
						IpNetwork:   "\x00\x00\x00\x00 ",
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-0",
			Cmd:  fmt.Sprintf("echo hello | socat - TCP4:SERVER_IP:%d", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   nil,
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, uint32(port)),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
					Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, normalPorts)
}

func TestConnectionsAndEndpointsHighLowPorts(t *testing.T) {
	// The server is assigned a port in the ephemeral ports range.
	// The client is assigned a source port in a non-ephemeral ports range
	port := 40000
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-1",
			Cmd:  fmt.Sprintf("socat TCP4-LISTEN:%d,reuseaddr,fork - &", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   types.CreateNetworkAddress("", "", uint32(port)),
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, 0),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
					Role:           sensorAPI.ClientServerRole_ROLE_SERVER,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: []types.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: types.ListenAddress{
						AddressData: "\x00\x00\x00\x00",
						Port:        port,
						IpNetwork:   "\x00\x00\x00\x00 ",
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-1",
			Cmd:  fmt.Sprintf("echo hello | socat - TCP4:SERVER_IP:%d,sourceport=10000", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   nil,
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, uint32(port)),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
					Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestConnectionsAndEndpointsServerHigh(t *testing.T) {
	// The server is assigned a port in the ephemeral ports range.
	// The client is assigned a port in the ephemeral ports range in the normal way.
	port := 60999
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-2",
			Cmd:  fmt.Sprintf("socat TCP4-LISTEN:%d,reuseaddr,fork - &", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   types.CreateNetworkAddress("", "", uint32(port)),
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, 0),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
					Role:           sensorAPI.ClientServerRole_ROLE_SERVER,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: []types.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: types.ListenAddress{
						AddressData: "\x00\x00\x00\x00",
						Port:        port,
						IpNetwork:   "\x00\x00\x00\x00 ",
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-2",
			Cmd:  "echo hello | socat - TCP4:SERVER_IP:60999",
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   nil,
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, uint32(port)),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
					Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestConnectionsAndEndpointsSourcePort(t *testing.T) {
	// The server is assigned a port in the ephemeral ports range.
	// The client is assigned a source port in a non-ephemeral ports range
	port := 10000
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-1",
			Cmd:  fmt.Sprintf("socat TCP4-LISTEN:%d,reuseaddr,fork - &", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   types.CreateNetworkAddress("", "", uint32(port)),
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, 0),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
					Role:           sensorAPI.ClientServerRole_ROLE_SERVER,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: []types.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: types.ListenAddress{
						AddressData: "\x00\x00\x00\x00",
						Port:        port,
						IpNetwork:   "\x00\x00\x00\x00 ",
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-1",
			Cmd:  fmt.Sprintf("echo hello | socat - TCP4:SERVER_IP:%d,sourceport=40000", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   nil,
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, uint32(port)),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
					Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestConnectionsAndEndpointsUDPNormal(t *testing.T) {
	// A test for UDP
	port := 53
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-udp",
			Cmd:  fmt.Sprintf("socat UDP-LISTEN:%d,reuseaddr,fork - &", port),
			// TODO UDP connections are not always reported on the server side
			ExpectedNetwork: nil,
			// ExpectedNetwork: []types.NetworkInfo{
			// 	{
			// 		LocalAddress:   ":53",
			// 		RemoteAddress:  "CLIENT_IP",
			// 		Role:           "ROLE_SERVER",
			// 		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			// 		CloseTimestamp: types.NilTimestamp,
			// 	},
			// },
			// TODO UDP listening endpoints should be reported
			ExpectedEndpoints: nil,
		},
		Client: suites.Container{
			Name: "socat-client-udp",
			Cmd:  fmt.Sprintf("echo hello | socat - UDP:SERVER_IP:%d", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   nil,
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, uint32(port)),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_UDP,
					Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestConnectionsAndEndpointsUDPNoReuseaddr(t *testing.T) {
	// A test for UDP without reuseaddr
	port := 53
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-udp",
			Cmd:  fmt.Sprintf("socat UDP-LISTEN:%d,fork - &", port),
			// TODO UDP connections are not always reported on the server side
			ExpectedNetwork: nil,
			// ExpectedNetwork: []types.NetworkInfo{
			// 	{
			// 		LocalAddress:   ":53",
			// 		RemoteAddress:  "CLIENT_IP",
			// 		Role:           "ROLE_SERVER",
			// 		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			// 		CloseTimestamp: types.NilTimestamp,
			// 	},
			// },
			// TODO UDP listening endpoints should be reported
			ExpectedEndpoints: nil,
		},
		Client: suites.Container{
			Name: "socat-client-udp",
			Cmd:  fmt.Sprintf("echo hello | socat - UDP:SERVER_IP:%d", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   nil,
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, uint32(port)),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_UDP,
					Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestConnectionsAndEndpointsUDPNoFork(t *testing.T) {
	// A test for UDP without fork or reuseaddr
	port := 53
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-udp",
			Cmd:  fmt.Sprintf("socat UDP-LISTEN:%d - &", port),
			// TODO UDP connections are not always reported on the server side
			ExpectedNetwork: nil,
			// ExpectedNetwork: []types.NetworkInfo{
			// 	{
			// 		LocalAddress:   ":53",
			// 		RemoteAddress:  "CLIENT_IP",
			// 		Role:           "ROLE_SERVER",
			// 		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			// 		CloseTimestamp: types.NilTimestamp,
			// 	},
			// },
			// TODO UDP listening endpoints should be reported
			ExpectedEndpoints: nil,
		},
		Client: suites.Container{
			Name: "socat-client-udp",
			Cmd:  fmt.Sprintf("echo hello | socat - UDP:SERVER_IP:%d", port),
			ExpectedNetwork: []*sensorAPI.NetworkConnection{
				{
					LocalAddress:   nil,
					RemoteAddress:  types.CreateNetworkAddress("", placeholderIP, uint32(port)),
					Protocol:       storage.L4Protocol_L4_PROTOCOL_UDP,
					Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
					SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
					CloseTimestamp: types.NotNilTimestamp,
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestIntrospectionAPI(t *testing.T) {
	endpointTestSuite := &suites.HttpEndpointAvailabilityTestSuite{
		Port: 8080,
		CollectorOptions: collector.StartupOptions{
			Env: map[string]string{
				"ROX_COLLECTOR_INTROSPECTION_ENABLE": "true",
			},
		},
		Endpoints: []string{
			"/state/network/connection",
			"/state/network/endpoint",
		}}
	suite.Run(t, endpointTestSuite)
}

// By default, a failed connection is not reported.
func TestAsyncConnectionBlocked(t *testing.T) {
	blockedAsyncConnection := &suites.AsyncConnectionTestSuite{
		DisableConnectionStatusTracking: false,
		BlockConnection:                 true,
		ExpectToSeeTheConnection:        false,
	}
	suite.Run(t, blockedAsyncConnection)
}

// A successfull connection is always reported
func TestAsyncConnectionSuccess(t *testing.T) {
	asyncConnection := &suites.AsyncConnectionTestSuite{
		DisableConnectionStatusTracking: false,
		BlockConnection:                 false,
		ExpectToSeeTheConnection:        true,
	}
	suite.Run(t, asyncConnection)
}

// With connection status tracking disabled, failed async connections are reported.
func TestAsyncConnectionBlockedWithDisableTracking(t *testing.T) {
	blockedAsyncConnection := &suites.AsyncConnectionTestSuite{
		DisableConnectionStatusTracking: true,
		BlockConnection:                 true,
		ExpectToSeeTheConnection:        true,
	}
	suite.Run(t, blockedAsyncConnection)
}

// With connection status tracking disabled, a successfull connection is always reported
func TestAsyncConnectionSuccessWithDisableTracking(t *testing.T) {
	asyncConnection := &suites.AsyncConnectionTestSuite{
		DisableConnectionStatusTracking: true,
		BlockConnection:                 false,
		ExpectToSeeTheConnection:        true,
	}
	suite.Run(t, asyncConnection)
}

func TestCollectorStartup(t *testing.T) {
	suite.Run(t, new(suites.CollectorStartupTestSuite))
}

func TestPerfEvent(t *testing.T) {
	suite.Run(t, new(suites.PerfEventOpenTestSuite))
}

func TestGperftools(t *testing.T) {
	if ok, arch := common.ArchSupported("x86_64"); !ok {
		t.Skip("[WARNING]: skip GperftoolsTestSuite on ", arch)
	}
	suite.Run(t, new(suites.GperftoolsTestSuite))
}

func TestRingBuffer(t *testing.T) {
	suite.Run(t, new(suites.RingBufferTestSuite))
}

func TestUdpNetworkFlow(t *testing.T) {
	if strings.Contains(config.VMInfo().Config, "rhel-8-4-sap") {
		t.Skip("Skipping test on RHEL 8.4 SAP due to a verifier issue")
	}
	if strings.Contains(config.VMInfo().Config, "fedora-coreos-stable") {
		t.Skip("Skipping due to ROX-27673")
	}
	suite.Run(t, new(suites.UdpNetworkFlow))
}

func TestRuntimeConfigFile(t *testing.T) {
	suite.Run(t, new(suites.RuntimeConfigFileTestSuite))
}

func TestThreads(t *testing.T) {
	suite.Run(t, new(suites.ThreadsTestSuite))
}

func TestPrometheus(t *testing.T) {
	suite.Run(t, new(suites.PrometheusTestSuite))
}

func TestGetStatus(t *testing.T) {
	suite.Run(t, &suites.HttpEndpointAvailabilityTestSuite{
		Port:      8080,
		Endpoints: []string{"ready"},
	})
}

func TestLogLevelEndpoint(t *testing.T) {
	suite.Run(t, new(suites.LogLevelTestSuite))
}
