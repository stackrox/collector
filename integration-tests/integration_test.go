package integrationtests

import (
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
	"testing"
	"time"
	"sort"

	"encoding/json"

	"github.com/boltdb/bolt"
	"github.com/gonum/stat"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"
)

const (
	processBucket            = "Process"
	processLineageInfoBucket = "LineageInfo"
	networkBucket            = "Network"
	endpointBucket           = "Endpoint"
	parentUIDStr             = "ParentUid"
	parentExecFilePathStr    = "ParentExecFilePath"

	defaultWaitTickSeconds = 30 * time.Second

	// defaultStopTimeoutSeconds is the amount of time to wait for a container
	// to stop before forcibly killing it. It needs to be a string because it
	// is passed directly to the docker command via the executor.
	//
	// 10 seconds is the default for docker stop when not providing a timeout
	// argument. It is kept the same here to avoid changing behavior by default.
	defaultStopTimeoutSeconds = "10"

	nilTimestamp = "(timestamp: nil Timestamp)"
)

func TestProcessNetwork(t *testing.T) {
	suite.Run(t, new(ProcessNetworkTestSuite))
}

func TestImageLabelJSON(t *testing.T) {
	suite.Run(t, new(ImageLabelJSONTestSuite))
}

// TestMissingProcScrape only works with local fake proc directory
func TestMissingProcScrape(t *testing.T) {
	if ReadEnvVarWithDefault("REMOTE_HOST_TYPE", "local") == "local" {
		suite.Run(t, new(MissingProcScrapeTestSuite))
	}
}

func TestRepeatedNetworkFlow(t *testing.T) {
	// Perform 11 curl commands with a 2 second sleep between each curl command.
	// The scrapeInterval is increased to 4 seconds to reduce the chance that jiter will effect the results.
	// The first server to client connection is recorded as being active.
	// The second through ninth curl commands are ignored, because of afterglow.
	// The last server to client connection is recorded as being inacitve when the afterglow period has expired
	// Thus the reported connections are active, inactive
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod:        10,
		scrapeInterval:         4,
		enableAfterglow:        true,
		numMetaIter:            1,
		numIter:                11,
		sleepBetweenCurlTime:   2,
		sleepBetweenIterations: 1,
		expectedReports:        []bool{true, false},
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

func TestRepeatedNetworkFlowWithZeroAfterglowPeriod(t *testing.T) {
	// Afterglow is disables as the afterglowPeriod is 0
	// All server to client connections are reported.
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod:        0,
		scrapeInterval:         2,
		enableAfterglow:        true,
		numMetaIter:            1,
		numIter:                3,
		sleepBetweenCurlTime:   3,
		sleepBetweenIterations: 1,
		expectedReports:        []bool{false, false, false},
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

func TestRepeatedNetworkFlowThreeCurlsNoAfterglow(t *testing.T) {
	// The afterglow period is set to 0 so afterglow is disabled
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod:        0,
		scrapeInterval:         4,
		enableAfterglow:        false,
		numMetaIter:            1,
		numIter:                3,
		sleepBetweenCurlTime:   6,
		sleepBetweenIterations: 1,
		expectedReports:        []bool{false, false, false},
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

// There is one test in which scraping is turned on and we expect to see
// endpoints opened before collector is turned on. There is another test
// in which scraping is turned off and we expect that we will not see
// endpoint opened before collector is turned on.
func TestProcfsScraper(t *testing.T) {
	connScraperTestSuite := &ProcfsScraperTestSuite{
		turnOffScrape:	false,
		roxProcessesListeningOnPort: true,
	}
	suite.Run(t, connScraperTestSuite)
}

func TestProcfsScraperNoScrape(t *testing.T) {
	connScraperTestSuite := &ProcfsScraperTestSuite{
		turnOffScrape:	true,
		roxProcessesListeningOnPort: true,
	}
	suite.Run(t, connScraperTestSuite)
}

func TestProcfsScraperDisableFeatureFlag(t *testing.T) {
	connScraperTestSuite := &ProcfsScraperTestSuite{
		turnOffScrape:	false,
		roxProcessesListeningOnPort: false,
	}
	suite.Run(t, connScraperTestSuite)
}

func TestProcessListeningOnPort(t *testing.T) {
	suite.Run(t, new(ProcessListeningOnPortTestSuite))
}

func TestSocat(t *testing.T) {
	suite.Run(t, new(SocatTestSuite))
}

type IntegrationTestSuiteBase struct {
	suite.Suite
	db        *bolt.DB
	executor  Executor
	collector *collectorManager
	metrics   map[string]float64
}

type ProcessNetworkTestSuite struct {
	IntegrationTestSuiteBase
	clientContainer string
	clientIP        string
	serverContainer string
	serverIP        string
	serverPort      string
}

type ContainerStat struct {
	Timestamp string
	Id        string
	Name      string
	Mem       string
	Cpu       float64
}

type PerformanceResult struct {
	TestName         string
	Timestamp        string
	InstanceType     string
	VmConfig         string
	CollectionMethod string
	Metrics          map[string]float64
	ContainerStats   []ContainerStat
}

type MissingProcScrapeTestSuite struct {
	IntegrationTestSuiteBase
}

type RepeatedNetworkFlowTestSuite struct {
	//The goal with these integration tests is to make sure we report the correct number of
	//networking events. Sometimes if a connection is made multiple times within a short time
	//called an "afterglow" period, we only want to report the connection once.
	IntegrationTestSuiteBase
	clientContainer        string
	clientIP               string
	serverContainer        string
	serverIP               string
	serverPort             string
	enableAfterglow        bool
	afterglowPeriod        int
	scrapeInterval         int
	numMetaIter            int
	numIter                int
	sleepBetweenCurlTime   int
	sleepBetweenIterations int
	expectedReports        []bool // An array of booleans representing the connection. true is active. fasle is inactive.
	observedReports        []bool
}

type ImageLabelJSONTestSuite struct {
	IntegrationTestSuiteBase
}

type ProcfsScraperTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer			string
	turnOffScrape			bool
	roxProcessesListeningOnPort	bool
}

type ProcessListeningOnPortTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer		string
}

type SocatTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer		string
}

func (s *ImageLabelJSONTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.collector = NewCollectorManager(s.executor, s.T().Name())
	err := s.collector.Setup()
	s.Require().NoError(err)
	err = s.collector.Launch()
	s.Require().NoError(err)
}

func (s *ImageLabelJSONTestSuite) TestRunImageWithJSONLabel() {
	s.RunImageWithJSONLabels()
}

func (s *ImageLabelJSONTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	s.Require().NoError(err)
	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
	s.cleanupContainer([]string{"collector", "grpc-server", "jsonlabel"})
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *ProcessNetworkTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	images := []string{
		"nginx:1.14-alpine",
		"pstauffer/curl:latest",
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	s.Require().NoError(err)
	s.serverContainer = containerShortID(containerID)

	// invokes "sleep" and "sh" and "ls"
	_, err = s.execContainer("nginx", []string{"sleep", "5"})
	s.Require().NoError(err)
	_, err = s.execContainer("nginx", []string{"sh", "-c", "ls"})
	s.Require().NoError(err)

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "pstauffer/curl:latest", "sleep", "300")
	s.Require().NoError(err)
	s.clientContainer = containerShortID(containerID)

	s.serverIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.serverPort, err = s.getPort("nginx")
	s.Require().NoError(err)

	_, err = s.execContainer("nginx-curl", []string{"curl", fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)})
	s.Require().NoError(err)

	s.clientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)

	time.Sleep(10 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ProcessNetworkTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "nginx-curl"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("process_network", stats, s.metrics)
}

func (s *ProcessNetworkTestSuite) TestProcessViz() {
	expectedProcesses := []ProcessInfo {
		ProcessInfo {
			Name: "ls",
			ExePath: "/bin/ls",
			Uid: 0,
			Gid: 0,
			Args: "",
		},
		ProcessInfo {
			Name: "nginx",
			ExePath: "/usr/sbin/nginx",
			Uid: 0,
			Gid: 0,
			Args: "-g daemon off;",
		},
		ProcessInfo {
			Name: "sh",
			ExePath: "/bin/sh",
			Uid: 0,
			Gid: 0,
			Args: "-c ls",
		},
		ProcessInfo {
			Name: "sleep",
			ExePath: "/bin/sleep",
			Uid: 0,
			Gid: 0,
			Args: "5",
		},
	}

	actualProcesses, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)

	sort.Slice(actualProcesses, func(i, j int) bool {
		return actualProcesses[i].Name < actualProcesses[j].Name
	})

	assert.Equal(s.T(), len(expectedProcesses), len(actualProcesses))

	for i, expected := range expectedProcesses {
		actual := actualProcesses[i]
		s.Require().NoError(err)

		s.AssertProcessInfoEqual(expected, actual)
	}
}

func (s *ProcessNetworkTestSuite) TestProcessLineageInfo() {
	expectedLineages := []ProcessLineage {
		ProcessLineage {
			Name: "awk",
			ExePath: "/usr/bin/awk",
			ParentUid: 0,
			ParentExePath: "/bin/busybox",
		},
		ProcessLineage {
			Name: "grep",
			ExePath: "/bin/grep",
			ParentUid: 0,
			ParentExePath: "/bin/busybox",
		},
		ProcessLineage {
			Name: "sleep",
			ExePath: "/bin/sleep",
			ParentUid: 0,
			ParentExePath: "/bin/busybox",
		},
	}

	for _, expected := range expectedLineages {
		val, err := s.GetLineageInfo(expected.Name, "0", processLineageInfoBucket)
		s.Require().NoError(err)
		lineage, err := NewProcessLineage(val)
		s.Require().NoError(err)

		assert.Equal(s.T(), expected, *lineage)
	}
}

func (s *ProcessNetworkTestSuite) TestNetworkFlows() {

	// Server side checks

	// NetworkSignalHandler does not currently report endpoints.
	// ProcfsScraper, which scrapes networking information from /proc reports endpoints and connections
	// However NetworkSignalHandler, which gets networking information from Falco only reports connections.
	// At some point in the future NetworkSignalHandler will report endpoints and connections.
	// At that time this test and the similar test for the client container will need to be changed.
	// The requirement should be NoError, instead of Error and there should be multiple asserts to
	// check that the endpoints are what we expect them to be.
	_, err := s.GetEndpoints(s.serverContainer)
	s.Require().Error(err)

	val, err := s.Get(s.serverContainer, networkBucket)
	s.Require().NoError(err)
	actualValues := strings.Split(string(val), "|")

	if len(actualValues) < 2 {
		assert.FailNow(s.T(), "serverContainer networkBucket was missing data. ", "val=\"%s\"", val)
	}
	actualServerEndpoint := actualValues[0]
	actualClientEndpoint := actualValues[1]

	// From server perspective, network connection info only has local port and remote IP
	assert.Equal(s.T(), fmt.Sprintf(":%s", s.serverPort), actualServerEndpoint)
	assert.Equal(s.T(), s.clientIP, actualClientEndpoint)

	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.serverContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)

	// client side checks

	// NetworkSignalHandler does not currently report endpoints.
	// See the comment above for the server container endpoint test for more info.
	_, err = s.GetEndpoints(s.clientContainer)
	s.Require().Error(err)

	val, err = s.Get(s.clientContainer, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")

	actualClientEndpoint = actualValues[0]
	actualServerEndpoint = actualValues[1]

	// From client perspective, network connection info has no local endpoint and full remote endpoint
	assert.Empty(s.T(), actualClientEndpoint)
	assert.Equal(s.T(), fmt.Sprintf("%s:%s", s.serverIP, s.serverPort), actualServerEndpoint)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s\n", s.clientContainer, s.clientIP)
}

func (s *MissingProcScrapeTestSuite) SetupSuite() {
	_, err := os.Stat("/tmp/fake-proc")
	assert.False(s.T(), os.IsNotExist(err), "Missing fake proc directory")

	s.executor = NewExecutor()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	// Mount the fake proc directory created by 'create-fake-proc.sh'
	s.collector.Mounts["/host/proc:ro"] = "/tmp/fake-proc"

	err = s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	time.Sleep(10 * time.Second)
}

func (s *MissingProcScrapeTestSuite) TestCollectorRunning() {
	collectorRunning, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	assert.True(s.T(), collectorRunning, "Collector isn't running")
}

func (s *MissingProcScrapeTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	s.Require().NoError(err)
	s.cleanupContainer([]string{"collector"})
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
func (s *RepeatedNetworkFlowTestSuite) SetupSuite() {
	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":` + strconv.Itoa(s.scrapeInterval) + `}`
	s.collector.Env["ROX_AFTERGLOW_PERIOD"] = strconv.Itoa(s.afterglowPeriod)
	s.collector.Env["ROX_ENABLE_AFTERGLOW"] = strconv.FormatBool(s.enableAfterglow)

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	scheduled_curls_image := qaImage("quay.io/rhacs-eng/qa", "collector-schedule-curls");

	images := []string{
		"nginx:1.14-alpine",
		scheduled_curls_image,
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	s.Require().NoError(err)
	s.serverContainer = containerID[0:12]

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", scheduled_curls_image, "sleep", "300")
	s.Require().NoError(err)
	s.clientContainer = containerID[0:12]

	s.serverIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.serverPort, err = s.getPort("nginx")
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	serverAddress := fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)

	numMetaIter := strconv.Itoa(s.numMetaIter)
	numIter := strconv.Itoa(s.numIter)
	sleepBetweenCurlTime := strconv.Itoa(s.sleepBetweenCurlTime)
	sleepBetweenIterations := strconv.Itoa(s.sleepBetweenIterations)
	_, err = s.execContainer("nginx-curl", []string{"/usr/bin/schedule-curls.sh", numMetaIter, numIter, sleepBetweenCurlTime, sleepBetweenIterations, serverAddress})

	s.clientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)

	totalTime := (s.sleepBetweenCurlTime*s.numIter+s.sleepBetweenIterations)*s.numMetaIter + s.afterglowPeriod + 10
	time.Sleep(time.Duration(totalTime) * time.Second)
	logLines := s.GetLogLines("grpc-server")
	s.observedReports = GetNetworkActivity(logLines, serverAddress)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *RepeatedNetworkFlowTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "nginx-curl", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("repeated_network_flow", stats, s.metrics)
}

func (s *RepeatedNetworkFlowTestSuite) TestRepeatedNetworkFlow() {
	// Server side checks
	assert.Equal(s.T(), s.expectedReports, s.observedReports)

	val, err := s.Get(s.serverContainer, networkBucket)
	s.Require().NoError(err)
	actualValues := strings.Split(string(val), "|")

	if len(actualValues) < 2 {
		assert.FailNow(s.T(), "serverContainer networkBucket was missing data. ", "val=\"%s\"", val)
	}
	actualServerEndpoint := actualValues[0]
	actualClientEndpoint := actualValues[1]

	// From server perspective, network connection info only has local port and remote IP
	assert.Equal(s.T(), fmt.Sprintf(":%s", s.serverPort), actualServerEndpoint)
	assert.Equal(s.T(), s.clientIP, actualClientEndpoint)

	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.serverContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)

	// client side checks
	val, err = s.Get(s.clientContainer, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")

	actualClientEndpoint = actualValues[0]
	actualServerEndpoint = actualValues[1]

	// From client perspective, network connection info has no local endpoint and full remote endpoint
	assert.Empty(s.T(), actualClientEndpoint)
	assert.Equal(s.T(), fmt.Sprintf("%s:%s", s.serverIP, s.serverPort), actualServerEndpoint)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s\n", s.clientContainer, s.clientIP)
}

// Launches nginx container
// Launches gRPC server in insecure mode
// Launches collector
// Note it is important to launch the nginx container before collector, which is the opposite of
// other tests. The purpose is that we want ProcfsScraper to see the nginx endpoint and we do not want
// NetworkSignalHandler to see the nginx endpoint.
func (s *ProcfsScraperTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":` + strconv.FormatBool(s.turnOffScrape) + `,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = strconv.FormatBool(s.roxProcessesListeningOnPort)

	s.launchNginx()

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(10 * time.Second)

	s.cleanupContainer([]string{"nginx"})
	time.Sleep(10 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ProcfsScraperTestSuite) launchNginx() {
	image := "nginx:1.14-alpine"

	err := s.executor.PullImage(image)
	s.Require().NoError(err)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", image)
	s.Require().NoError(err)
	s.serverContainer = containerShortID(containerID)

	time.Sleep(10 * time.Second)
}

func (s *ProcfsScraperTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ProcfsScraper", stats, s.metrics)
}

func (s *ProcfsScraperTestSuite) TestProcfsScraper() {
	var imageFamily = os.Getenv("IMAGE_FAMILY")
	// TODO: These tests fail for rhel-7, ubuntu-pro-1804-lts, sles-12, and sometimes for ubuntu-2204-lts.
	// Make the tests pass for them and remove this if statement
	if imageFamily != "rhel-7" && imageFamily != "ubuntu-pro-1804-lts" && imageFamily != "sles-12" && imageFamily!= "ubuntu-2204-lts" {
		endpoints, err := s.GetEndpoints(s.serverContainer)
		if (!s.turnOffScrape && s.roxProcessesListeningOnPort) {
			// If scraping is on and the feature flag is enables we expect to find the nginx endpoint
			s.Require().NoError(err)
			assert.Equal(s.T(), 2, len(endpoints))
			processes, err := s.GetProcesses(s.serverContainer)
			s.Require().NoError(err)

			assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoints[0].Protocol)
			assert.Equal(s.T(), "(timestamp: nil Timestamp)", endpoints[0].CloseTimestamp)
			assert.Equal(s.T(), endpoints[0].Originator.ProcessName, processes[0].Name)
			assert.Equal(s.T(), endpoints[0].Originator.ProcessExecFilePath, processes[0].ExePath)
			assert.Equal(s.T(), endpoints[0].Originator.ProcessArgs, processes[0].Args)

			assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoints[1].Protocol)
			assert.NotEqual(s.T(), "(timestamp: nil Timestamp)", endpoints[1].CloseTimestamp)
			assert.Equal(s.T(), endpoints[1].Originator.ProcessName, processes[0].Name)
			assert.Equal(s.T(), endpoints[1].Originator.ProcessExecFilePath, processes[0].ExePath)
			assert.Equal(s.T(), endpoints[1].Originator.ProcessArgs, processes[0].Args)
		} else {
			// If scraping is off or the feature flag is disabled
			// we expect not to find the nginx endpoint and we should get an error
			s.Require().Error(err)
		}
	}
}

func (s *ProcessListeningOnPortTestSuite) waitForFileToBeDeleted(file string) error {
	count := 0
	maxCount := 10

	output, _ := s.executor.Exec("stat", file, "2>&1")
	fmt.Println(output)
	for !strings.Contains(output, "No such file or directory") {
		time.Sleep(1 * time.Second)
		count += 1
		if count == maxCount {
			return fmt.Errorf("Timed out waiting for %s to be deleted", file)
		}
		output, _ = s.executor.Exec("stat", file, "2>&1")
	}

	return nil
}

func (s *ProcessListeningOnPortTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	processImage := qaImage("quay.io/rhacs-eng/qa", "collector-processes-listening-on-ports")

	containerID, err := s.launchContainer("process-ports", "-v", "/tmp:/tmp", processImage)

	s.Require().NoError(err)
	s.serverContainer = containerShortID(containerID)

	actionFile := "/tmp/action_file.txt"

	_, err = s.collector.executor.Exec("sh", "-c", "rm "+actionFile+" || true")

	_, err = s.collector.executor.Exec("sh", "-c", "echo open 8081 > "+actionFile)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)
	_, err = s.collector.executor.Exec("sh", "-c", "echo open 9091 > "+actionFile)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)

	time.Sleep(6 * time.Second)

	_, err = s.collector.executor.Exec("sh", "-c", "echo close 8081 > "+actionFile)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)
	_, err = s.collector.executor.Exec("sh", "-c", "echo close 9091 > "+actionFile)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ProcessListeningOnPortTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"process-ports", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ProcessListeningOnPort", stats, s.metrics)
}

func (s *ProcessListeningOnPortTestSuite) TestProcessListeningOnPort() {
	processes, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)
	endpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	if !assert.Equal(s.T(), 4, len(endpoints)) {
		// We can't continue if this is not the case, so panic immediately.
		// It indicates an internal issue with this test and the non-deterministic
		// way in which endpoints are reported.
		assert.FailNowf(s.T(), "", "only retrieved %d endpoints (expect 4)", len(endpoints))
	}

	// Note that the first process is the shell and the second is the process-listening-on-ports program.
	// All of these asserts check against the processes information of that program.
	assert.Equal(s.T(), 2, len(processes))
	process := processes[1]

	possiblePorts := []int{8081, 9091}

	// First verify that all endpoints have the expected metadata, that
	// they are the correct protocol and come from the expected Originator
	// process.
	for _, endpoint := range endpoints {
		assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoint.Protocol)

		// TODO
		// There is a difference in the lengths of the process name
		// The process stream truncates to 16 characters while endpoints does not.
		// We need to decide which way to go and make these tests consistent with that.
		//assert.Equal(s.T(), endpoint.Originator.ProcessName, process.Name)
		assert.Equal(s.T(), endpoint.Originator.ProcessExecFilePath, process.ExePath)
		assert.Equal(s.T(), endpoint.Originator.ProcessArgs, process.Args)

		// assert that we haven't got any unexpected ports - further checking
		// of this data will occur subsequently
		assert.Contains(s.T(), possiblePorts, endpoint.Address.Port)
	}

	// We can't guarantee the order in which collector reports the endpoints.
	// Check that we have precisely two pairs of endpoints, for opening
	// and closing the port. A closed port will have a populated CloseTimestamp

	endpoints8081 := make([]EndpointInfo, 0)
	endpoints9091 := make([]EndpointInfo, 0)
	for _, endpoint := range endpoints {
		if endpoint.Address.Port == 8081 {
			endpoints8081 = append(endpoints8081, endpoint)
		} else {
			endpoints9091 = append(endpoints9091, endpoint)
		}
		// other ports cannot exist at this point due to previous assertions
	}

	// This helper simplifies the assertions a fair bit, by checking that
	// the recorded endpoints have an open event (CloseTimestamp == nil) and
	// a close event (CloseTimestamp != nil) and not two close events or two open
	// events.
	//
	// It is also agnostic to the order in which the events are reported.
	hasOpenAndClose := func(infos []EndpointInfo) bool {
		if !assert.Len(s.T(), infos, 2) {
			return false
		}
		return infos[0].CloseTimestamp != infos[1].CloseTimestamp &&
			(infos[0].CloseTimestamp == nilTimestamp || infos[1].CloseTimestamp == nilTimestamp)
	}

	assert.True(s.T(), hasOpenAndClose(endpoints8081), "Did not capture open and close events for port 8081")
	assert.True(s.T(), hasOpenAndClose(endpoints9091), "Did not capture open and close events for port 9091")
}

func (s *SocatTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	processImage := qaImage("quay.io/rhacs-eng/qa", "socat")

	containerID, err := s.launchContainer("socat", "-e", "PORT1=80", "-e", "PORT2=8080", processImage, "/bin/sh", "-c", "socat TCP-LISTEN:$PORT1,fork STDOUT & socat TCP-LISTEN:$PORT2,fork STDOUT")

	s.Require().NoError(err)
	s.serverContainer = containerShortID(containerID)

	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *SocatTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"socat", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("Socat", stats, s.metrics)
}

func getEndpointByPort(endpoints []EndpointInfo, port int) (*EndpointInfo, error) {
	for _, endpoint := range endpoints {
		if endpoint.Address.Port == port {
			return &endpoint, nil
		}
	}

	err := fmt.Errorf("Could not find endpoint with port %d", port)

	return nil, err
}

func getProcessByPort(processes []ProcessInfo, port int) (*ProcessInfo, error) {
	re := regexp.MustCompile(`:(` + strconv.Itoa(port) + `),`)
	for _, process := range processes {
		portArr := re.FindStringSubmatch(process.Args)
		if len(portArr) == 2 {
			return &process, nil
		}
	}

	err := fmt.Errorf("Could not find process with port %d", port)

	return nil, err
}

func (s *SocatTestSuite) TestSocat() {
	processes, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)
	endpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	if !assert.Equal(s.T(), 2, len(endpoints)) {
		// We can't continue if this is not the case, so panic immediately.
		// It indicates an internal issue with this test and the non-deterministic
		// way in which endpoints are reported.
		assert.FailNowf(s.T(), "", "only retrieved %d endpoints (expect 2)", len(endpoints))
	}

	assert.Equal(s.T(), 3, len(processes))

	endpoint80, err := getEndpointByPort(endpoints, 80)
	s.Require().NoError(err)
	endpoint8080, err := getEndpointByPort(endpoints, 8080)
	s.Require().NoError(err)

	process80, err := getProcessByPort(processes, 80)
	s.Require().NoError(err)
	process8080, err := getProcessByPort(processes, 8080)
	s.Require().NoError(err)

	assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoint80.Protocol)
	assert.Equal(s.T(), endpoint80.Originator.ProcessName, process80.Name)
	assert.Equal(s.T(), endpoint80.Originator.ProcessExecFilePath, process80.ExePath)
	// TODO Enable this assert
	// assert.Equal(s.T(), endpoint80.Originator.ProcessArgs, process80.Args)
	assert.Equal(s.T(), 80, endpoint80.Address.Port)

	assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoint8080.Protocol)
	assert.Equal(s.T(), endpoint8080.Originator.ProcessName, process8080.Name)
	assert.Equal(s.T(), endpoint8080.Originator.ProcessExecFilePath, process8080.ExePath)
	assert.Equal(s.T(), endpoint8080.Originator.ProcessArgs, process8080.Args)
	assert.Equal(s.T(), 8080, endpoint8080.Address.Port)
}

func (s *IntegrationTestSuiteBase) launchContainer(args ...string) (string, error) {
	cmd := []string{"docker", "run", "-d", "--name"}
	cmd = append(cmd, args...)

	output, err := retry(func() (string, error) {
		return s.executor.Exec(cmd...)
	})

	outLines := strings.Split(output, "\n")
	return outLines[len(outLines)-1], err
}

func (s *IntegrationTestSuiteBase) waitForContainerToExit(containerName, containerID string, tickSeconds time.Duration) (bool, error) {
	cmd := []string{
		"docker", "ps", "-qa",
		"--filter", "id=" + containerID,
		"--filter", "status=exited",
	}

	start := time.Now()
	tick := time.Tick(tickSeconds)
	tickElapsed := time.Tick(1 * time.Minute)
	timeout := time.After(30 * time.Minute)
	for {
		select {
		case <-tick:
			output, err := s.executor.Exec(cmd...)
			outLines := strings.Split(output, "\n")
			lastLine := outLines[len(outLines)-1]
			if lastLine == containerShortID(containerID) {
				return true, nil
			}
			if err != nil {
				fmt.Printf("Retrying waitForContainerToExit(%s, %s): Error: %v\n", containerName, containerID, err)
			}
		case <-timeout:
			fmt.Printf("Timed out waiting for container %s to exit, elapsed Time: %s\n", containerName, time.Since(start))
			return false, nil
		case <-tickElapsed:
			fmt.Printf("Waiting for container: %s, elapsed time: %s\n", containerName, time.Since(start))
		}
	}
}

func (s *IntegrationTestSuiteBase) execContainer(containerName string, command []string) (string, error) {
	cmd := []string{"docker", "exec", containerName}
	cmd = append(cmd, command...)
	return s.executor.Exec(cmd...)
}

func (s *IntegrationTestSuiteBase) cleanupContainer(containers []string) {
	for _, container := range containers {
		s.executor.Exec("docker", "kill", container)
		s.executor.Exec("docker", "rm", container)
	}
}

func (s *IntegrationTestSuiteBase) stopContainers(containers ...string) {
	timeout := ReadEnvVarWithDefault("STOP_TIMEOUT", defaultStopTimeoutSeconds)
	for _, container := range containers {
		s.executor.Exec("docker", "stop", "-t", timeout, container)
	}
}

func (s *IntegrationTestSuiteBase) removeContainers(containers ...string) {
	for _, container := range containers {
		s.executor.Exec("docker", "rm", container)
	}
}

func (s *IntegrationTestSuiteBase) containerLogs(containerName string) (string, error) {
	return s.executor.Exec("docker", "logs", containerName)
}

func (s *IntegrationTestSuiteBase) getIPAddress(containerName string) (string, error) {
	stdoutStderr, err := s.executor.Exec("docker", "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	return strings.Replace(string(stdoutStderr), "'", "", -1), err
}

func (s *IntegrationTestSuiteBase) getPort(containerName string) (string, error) {
	stdoutStderr, err := s.executor.Exec("docker", "inspect", "--format='{{json .NetworkSettings.Ports}}'", containerName)
	if err != nil {
		return "", err
	}
	rawString := strings.Trim(string(stdoutStderr), "'\n")
	var portMap map[string]interface{}
	err = json.Unmarshal([]byte(rawString), &portMap)
	if err != nil {
		return "", err
	}

	for k := range portMap {
		return strings.Split(k, "/")[0], nil
	}

	return "", fmt.Errorf("no port mapping found: %v %v", rawString, portMap)
}

func (s *IntegrationTestSuiteBase) Get(key string, bucket string) (val string, err error) {
	if s.db == nil {
		return "", fmt.Errorf("Db %v is nil", s.db)
	}
	err = s.db.View(func(tx *bolt.Tx) error {
		b := tx.Bucket([]byte(bucket))
		if b == nil {
			return fmt.Errorf("Bucket %s was not found", bucket)
		}
		val = string(b.Get([]byte(key)))
		return nil
	})
	return
}

func (s *IntegrationTestSuiteBase) GetProcesses(containerID string) ([]ProcessInfo, error) {
	if s.db == nil {
		return nil, fmt.Errorf("Db %v is nil", s.db)
	}

	processes := make([]ProcessInfo, 0)
	err := s.db.View(func(tx *bolt.Tx) error {
		process := tx.Bucket([]byte(processBucket))
		if process == nil {
			return fmt.Errorf("Process bucket was not found!")
		}
		container := process.Bucket([]byte(containerID))
		if container == nil {
			return fmt.Errorf("Container bucket %s not found!", containerID)
		}

		return container.ForEach(func(k, v []byte) error {
			pinfo, err := NewProcessInfo(string(v))
			if err != nil {
				return err
			}

			if strings.HasPrefix(pinfo.ExePath, "/proc/self") {
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
				return nil
			}

			processes = append(processes, *pinfo)
			return nil
		})
	})

	if err != nil {
		return nil, err
	}

	return processes, nil
}

func (s *IntegrationTestSuiteBase) GetEndpoints(containerID string) ([]EndpointInfo, error) {
	if s.db == nil {
		return nil, fmt.Errorf("Db %v is nil", s.db)
	}

	endpoints := make([]EndpointInfo, 0)
	err := s.db.View(func(tx *bolt.Tx) error {
		endpoint := tx.Bucket([]byte(endpointBucket))
		if endpoint == nil {
			return fmt.Errorf("Endpoint bucket was not found!")
		}
		container := endpoint.Bucket([]byte(containerID))
		if container == nil {
			return fmt.Errorf("Container bucket %s not found!", containerID)
		}

		return container.ForEach(func(k, v []byte) error {
			einfo, err := NewEndpointInfo(string(v))
			if err != nil {
				return err
			}

			endpoints = append(endpoints, *einfo)
			return nil
		})
	})

	if err != nil {
		return nil, err
	}

	return endpoints, nil
}

func (s *IntegrationTestSuiteBase) GetLineageInfo(processName string, key string, bucket string) (val string, err error) {
	if s.db == nil {
		return "", fmt.Errorf("Db %v is nil", s.db)
	}
	err = s.db.View(func(tx *bolt.Tx) error {
		b := tx.Bucket([]byte(bucket))

		if b == nil {
			return fmt.Errorf("Bucket %s was not found", bucket)
		}

		processBucket := b.Bucket([]byte(processName))
		if processBucket == nil {
			return fmt.Errorf("Process bucket %s was not found", processName)
		}
		val = string(processBucket.Get([]byte(key)))
		return nil
	})
	return
}

func (s *IntegrationTestSuiteBase) RunCollectorBenchmark() {
	benchmarkName := "benchmark"
	benchmarkImage := qaImage("quay.io/rhacs-eng/collector-performance", "phoronix");

	err := s.executor.PullImage(benchmarkImage)
	s.Require().NoError(err)

	benchmarkArgs := []string{
		benchmarkName,
		"--env", "FORCE_TIMES_TO_RUN=1",
		benchmarkImage,
		"batch-benchmark", "collector",
	}

	containerID, err := s.launchContainer(benchmarkArgs...)
	s.Require().NoError(err)

	_, err = s.waitForContainerToExit(benchmarkName, containerID, defaultWaitTickSeconds)
	s.Require().NoError(err)

	benchmarkLogs, err := s.containerLogs("benchmark")
	re := regexp.MustCompile(`Average: ([0-9.]+) Seconds`)
	matches := re.FindSubmatch([]byte(benchmarkLogs))
	if matches != nil {
		fmt.Printf("Benchmark Time: %s\n", matches[1])
		f, err := strconv.ParseFloat(string(matches[1]), 64)
		s.Require().NoError(err)
		s.metrics["hackbench_avg_time"] = f
	} else {
		fmt.Printf("Benchmark Time: Not found! Logs: %s\n", benchmarkLogs)
		assert.FailNow(s.T(), "Benchmark Time not found")
	}
}

func (s *IntegrationTestSuiteBase) RunImageWithJSONLabels() {
	name := "jsonlabel"
	image := qaImage("quay.io/rhacs-eng/collector-performance", "json-label");
	err := s.executor.PullImage(image)
	s.Require().NoError(err)
	args := []string{
		name,
		image,
	}
	containerID, err := s.launchContainer(args...)
	s.Require().NoError(err)
	_, err = s.waitForContainerToExit(name, containerID, defaultWaitTickSeconds)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	name := "container-stats"
	image := qaImage("quay.io/rhacs-eng/collector-performance", "stats");
	args := []string{name, "-v", "/var/run/docker.sock:/var/run/docker.sock", image}

	err := s.executor.PullImage(image)
	s.Require().NoError(err)

	_, err = s.launchContainer(args...)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) GetLogLines(containerName string) []string {
	logs, err := s.containerLogs(containerName)
	s.Require().NoError(err, containerName+" failure")
	logLines := strings.Split(logs, "\n")
	return logLines
}

func GetNetworkActivity(lines []string, serverAddress string) []bool {
	var networkActivity []bool
	inactivePattern := "^Network.*" + serverAddress + ".*Z$"
	activePattern := "^Network.*" + serverAddress + ".*nil Timestamp.$"
	for _, line := range lines {
		activeMatch, _ := regexp.MatchString(activePattern, line)
		inactiveMatch, _ := regexp.MatchString(inactivePattern, line)
		if activeMatch {
			networkActivity = append(networkActivity, true)
		} else if inactiveMatch {
			networkActivity = append(networkActivity, false)
		}

	}
	return networkActivity
}

func (s *IntegrationTestSuiteBase) GetContainerStats() (stats []ContainerStat) {
	logs, err := s.containerLogs("container-stats")
	if err != nil {
		assert.FailNow(s.T(), "container-stats failure")
		return nil
	}
	logLines := strings.Split(logs, "\n")
	for _, line := range logLines {
		var stat ContainerStat
		json.Unmarshal([]byte(line), &stat)
		stats = append(stats, stat)
	}
	s.cleanupContainer([]string{"container-stats"})
	return stats
}

func (s *IntegrationTestSuiteBase) PrintContainerStats(stats []ContainerStat) {
	cpuStats := map[string][]float64{}
	for _, stat := range stats {
		cpuStats[stat.Name] = append(cpuStats[stat.Name], stat.Cpu)
	}
	for name, cpu := range cpuStats {
		s.metrics[fmt.Sprintf("%s_cpu_mean", name)] = stat.Mean(cpu, nil)
		s.metrics[fmt.Sprintf("%s_cpu_stddev", name)] = stat.StdDev(cpu, nil)

		fmt.Printf("CPU: Container %s, Mean %v, StdDev %v\n",
			name, stat.Mean(cpu, nil), stat.StdDev(cpu, nil))
	}
}

func (s *IntegrationTestSuiteBase) WritePerfResults(testName string, stats []ContainerStat, metrics map[string]float64) {
	perf := PerformanceResult{
		TestName:         testName,
		Timestamp:        time.Now().Format("2006-01-02 15:04:05"),
		InstanceType:     ReadEnvVarWithDefault("VM_INSTANCE_TYPE", "default"),
		VmConfig:         ReadEnvVarWithDefault("VM_CONFIG", "default"),
		CollectionMethod: ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module"),
		Metrics:          metrics,
		ContainerStats:   stats,
	}

	perfJson, _ := json.Marshal(perf)
	perfFilename := "perf.json"

	fmt.Printf("Writing %s\n", perfFilename)
	f, err := os.OpenFile(perfFilename, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	s.Require().NoError(err)
	defer f.Close()

	_, err = f.WriteString(string(perfJson))
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) AssertProcessInfoEqual(expected, actual ProcessInfo) {
	assert := assert.New(s.T())

	assert.Equal(expected.Name, actual.Name)
	assert.Equal(expected.ExePath, actual.ExePath)
	assert.Equal(expected.Uid, actual.Uid)
	assert.Equal(expected.Gid, actual.Gid)
	// Pid is non-deterministic, so just check that it is set
	assert.True(actual.Pid > 0)
	assert.Equal(expected.Args, actual.Args)
}
