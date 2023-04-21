package integrationtests

import (
	"testing"

	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/suites"
	"github.com/stackrox/collector/integration-tests/suites/common"
)

func TestProcessNetwork(t *testing.T) {
	suite.Run(t, new(suites.ProcessNetworkTestSuite))
}

func TestImageLabelJSON(t *testing.T) {
	suite.Run(t, new(suites.ImageLabelJSONTestSuite))
}

// TestMissingProcScrape only works with local fake proc directory
func TestMissingProcScrape(t *testing.T) {
	if common.ReadEnvVarWithDefault("REMOTE_HOST_TYPE", "local") == "local" {
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
		TurnOffScrape:               false,
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

func TestMixedUpEphemeralPortsHighLowPorts(t *testing.T) {
	mixedHighLowPorts := &suites.MixedUpEphemeralPortsTestSuite{
		Server: suites.Container{
			Name:            "socat-server-0",
			Cmd:             "socat TCP4-LISTEN:40000,reuseaddr,fork - &",
			ExpectedNetwork: "ROLE_SERVER",
			ExpectedEndpoint: []common.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &common.ListenAddress{
						AddressData: "0.0.0.0",
						Port:        40000,
					},
				},
			},
		},
		Client: suites.Container{
			Name:             "socat-client-0",
			Cmd:              "echo hello | socat - TCP4:LISTEN_IP:40000,sourceport=10000",
			ExpectedNetwork:  "ROLE_CLIENT",
			ExpectedEndpoint: nil,
		},
	}
	suite.Run(t, mixedHighLowPorts)
}

func TestMixedUpEphemeralPortsNormal(t *testing.T) {
	// Server uses a normal port. Client is assigned a port in the ephemeral range
	normalPorts := &suites.MixedUpEphemeralPortsTestSuite{
		Server: suites.Container{
			Name:            "socat-server-1",
			Cmd:             "socat TCP4-LISTEN:40,reuseaddr,fork - &",
			ExpectedNetwork: "ROLE_SERVER",
			ExpectedEndpoint: []common.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &common.ListenAddress{
						AddressData: "0.0.0.0",
						Port:        40,
					},
				},
			},
		},
		Client: suites.Container{
			Name:             "socat-client-1",
			Cmd:              "echo hello | socat - TCP4:LISTEN_IP:40",
			ExpectedNetwork:  "ROLE_CLIENT",
			ExpectedEndpoint: nil,
		},
	}
	suite.Run(t, normalPorts)
}

func TestMixedUpEphemeralPortsPersistent(t *testing.T) {
	// Client uses a port not in the ephemeral ports range as an ephemeral port and the connection is kept open.
	// or at least that is the goal
	persistentConnection := &suites.MixedUpEphemeralPortsTestSuite{
		Server: suites.Container{
			Name:            "socat-server-2",
			Cmd:             "socat TCP4-LISTEN:50000,reuseaddr,fork - &",
			ExpectedNetwork: "ROLE_SERVER",
			ExpectedEndpoint: []common.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &common.ListenAddress{
						AddressData: "0.0.0.0",
						Port:        50000,
					},
				},
			},
		},
		Client: suites.Container{
			Name: "socat-client-2",
			Cmd:  "dd if=/dev/zero bs=1MB count=100000 | socat - TCP4:LISTEN_IP:50000,sourceport=20000 &",
			//Cmd:              "tail -f /dev/null | socat - TCP4:LISTEN_IP:50000,sourceport=20000 &",
			ExpectedNetwork:  "ROLE_CLIENT",
			ExpectedEndpoint: nil,
		},
	}
	suite.Run(t, persistentConnection)
}
