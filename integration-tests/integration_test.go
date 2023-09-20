package integrationtests

import (
	"testing"

	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/suites"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/types"
)

func TestProcessNetwork(t *testing.T) {
	suite.Run(t, new(suites.ProcessNetworkTestSuite))
}

func TestImageLabelJSON(t *testing.T) {
	suite.Run(t, new(suites.ImageLabelJSONTestSuite))
}

// TestMissingProcScrape only works with local fake proc directory
func TestMissingProcScrape(t *testing.T) {
	if config.HostInfo().IsLocal() {
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
		ExpectedReports:        []bool{true, false},
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
		ExpectedReports:        []bool{false, false, false},
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
		ExpectedReports:        []bool{false, false, false},
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
	}
	suite.Run(t, connScraperTestSuite)
}

func TestProcfsScraperNoScrape(t *testing.T) {
	connScraperTestSuite := &suites.ProcfsScraperTestSuite{
		TurnOffScrape:               true,
		RoxProcessesListeningOnPort: true,
	}
	suite.Run(t, connScraperTestSuite)
}

func TestProcfsScraperDisableFeatureFlag(t *testing.T) {
	connScraperTestSuite := &suites.ProcfsScraperTestSuite{
		TurnOffScrape:               true,
		RoxProcessesListeningOnPort: false,
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
	normalPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-0",
			Cmd:  "socat TCP4-LISTEN:40,reuseaddr,fork - &",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   ":40",
					RemoteAddress:  "CLIENT_IP",
					Role:           "ROLE_SERVER",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
				},
			},
			ExpectedEndpoints: []types.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &types.ListenAddress{
						AddressData: "\x00\x00\x00\x00",
						Port:        40,
						IpNetwork:   "\x00\x00\x00\x00 ",
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-0",
			Cmd:  "echo hello | socat - TCP4:SERVER_IP:40",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   "",
					RemoteAddress:  "SERVER_IP:40",
					Role:           "ROLE_CLIENT",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
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
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-1",
			Cmd:  "socat TCP4-LISTEN:40000,reuseaddr,fork - &",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   ":40000",
					RemoteAddress:  "CLIENT_IP",
					Role:           "ROLE_SERVER",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
				},
			},
			ExpectedEndpoints: []types.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &types.ListenAddress{
						AddressData: "\x00\x00\x00\x00",
						Port:        40000,
						IpNetwork:   "\x00\x00\x00\x00 ",
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-1",
			Cmd:  "echo hello | socat - TCP4:SERVER_IP:40000,sourceport=10000",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   "",
					RemoteAddress:  "SERVER_IP:40000",
					Role:           "ROLE_CLIENT",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
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
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-2",
			Cmd:  "socat TCP4-LISTEN:60999,reuseaddr,fork - &",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   ":60999",
					RemoteAddress:  "CLIENT_IP",
					Role:           "ROLE_SERVER",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
				},
			},
			ExpectedEndpoints: []types.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &types.ListenAddress{
						AddressData: "\x00\x00\x00\x00",
						Port:        60999,
						IpNetwork:   "\x00\x00\x00\x00 ",
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-2",
			Cmd:  "echo hello | socat - TCP4:SERVER_IP:60999",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   "",
					RemoteAddress:  "SERVER_IP:60999",
					Role:           "ROLE_CLIENT",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
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
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-1",
			Cmd:  "socat TCP4-LISTEN:10000,reuseaddr,fork - &",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   ":10000",
					RemoteAddress:  "CLIENT_IP",
					Role:           "ROLE_SERVER",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
				},
			},
			ExpectedEndpoints: []types.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &types.ListenAddress{
						AddressData: "\x00\x00\x00\x00",
						Port:        10000,
						IpNetwork:   "\x00\x00\x00\x00 ",
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-1",
			Cmd:  "echo hello | socat - TCP4:SERVER_IP:10000,sourceport=40000",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   "",
					RemoteAddress:  "SERVER_IP:10000",
					Role:           "ROLE_CLIENT",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestConnectionsAndEndpointsUDPNormal(t *testing.T) {
	// A test for UDP
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-udp",
			Cmd:  "socat UDP-LISTEN:53,reuseaddr,fork - &",
			// TODO UDP connections are not always reported on the server side
			ExpectedNetwork: nil,
			// ExpectedNetwork: []types.NetworkInfo{
			// 	{
			// 		LocalAddress:   ":53",
			// 		RemoteAddress:  "CLIENT_IP",
			// 		Role:           "ROLE_SERVER",
			// 		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			// 		CloseTimestamp: "(timestamp: nil Timestamp)",
			// 	},
			// },
			// TODO UDP listening endpoints should be reported
			ExpectedEndpoints: nil,
		},
		Client: suites.Container{
			Name: "socat-client-udp",
			Cmd:  "echo hello | socat - UDP:SERVER_IP:53",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   "",
					RemoteAddress:  "SERVER_IP:53",
					Role:           "ROLE_CLIENT",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestConnectionsAndEndpointsUDPNoReuseaddr(t *testing.T) {
	// A test for UDP without reuseaddr
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-udp",
			Cmd:  "socat UDP-LISTEN:53,fork - &",
			// TODO UDP connections are not always reported on the server side
			ExpectedNetwork: nil,
			// ExpectedNetwork: []types.NetworkInfo{
			// 	{
			// 		LocalAddress:   ":53",
			// 		RemoteAddress:  "CLIENT_IP",
			// 		Role:           "ROLE_SERVER",
			// 		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			// 		CloseTimestamp: "(timestamp: nil Timestamp)",
			// 	},
			// },
			// TODO UDP listening endpoints should be reported
			ExpectedEndpoints: nil,
		},
		Client: suites.Container{
			Name: "socat-client-udp",
			Cmd:  "echo hello | socat - UDP:SERVER_IP:53",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   "",
					RemoteAddress:  "SERVER_IP:53",
					Role:           "ROLE_CLIENT",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestConnectionsAndEndpointsUDPNoFork(t *testing.T) {
	// A test for UDP without fork or reuseaddr
	mixedHighLowPorts := &suites.ConnectionsAndEndpointsTestSuite{
		Server: suites.Container{
			Name: "socat-server-udp",
			Cmd:  "socat UDP-LISTEN:53 - &",
			// TODO UDP connections are not always reported on the server side
			ExpectedNetwork: nil,
			// ExpectedNetwork: []types.NetworkInfo{
			// 	{
			// 		LocalAddress:   ":53",
			// 		RemoteAddress:  "CLIENT_IP",
			// 		Role:           "ROLE_SERVER",
			// 		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			// 		CloseTimestamp: "(timestamp: nil Timestamp)",
			// 	},
			// },
			// TODO UDP listening endpoints should be reported
			ExpectedEndpoints: nil,
		},
		Client: suites.Container{
			Name: "socat-client-udp",
			Cmd:  "echo hello | socat - UDP:SERVER_IP:53",
			ExpectedNetwork: []types.NetworkInfo{
				{
					LocalAddress:   "",
					RemoteAddress:  "SERVER_IP:53",
					Role:           "ROLE_CLIENT",
					SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
					CloseTimestamp: "(timestamp: nil Timestamp)",
				},
			},
			ExpectedEndpoints: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
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
