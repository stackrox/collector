package collector

import (
	"fmt"
	"io"
	"net/http"

	"github.com/stackrox/collector/integration-tests/pkg/log"
)

func (k *K8sCollectorManager) IntrospectionQuery(endpoint string) ([]byte, error) {
	uri := fmt.Sprintf("http://%s:8080%s", k.IP(), endpoint)
	body := []byte{}
	resp, err := http.Get(uri)
	if err != nil {
		return body, err
	}

	if resp.StatusCode != http.StatusOK {
		return body, log.Error("IntrospectionQuery failed with %s", resp.Status)
	}

	defer resp.Body.Close()
	body, err = io.ReadAll(resp.Body)
	return body, err
}
