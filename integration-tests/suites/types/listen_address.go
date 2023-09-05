package types

import (
	"fmt"
	"regexp"
	"strconv"
)

type ListenAddress struct {
	AddressData string
	Port        int
	IpNetwork   string
}

func NewListenAddress(line string) (*ListenAddress, error) {
	if line == "<nil>" {
		return nil, fmt.Errorf("ListenAddress is nil")
	}

	if line == "" {
		return nil, fmt.Errorf("ListenAddress is empty")
	}

	r := regexp.MustCompile("address_data:(.*) port:(.*) ip_network:(.*)")
	listenAddressArr := r.FindStringSubmatch(line)
	addressData := listenAddressArr[1]
	port, _ := strconv.Atoi(listenAddressArr[2])
	ipNetwork := listenAddressArr[3]

	return &ListenAddress{
		AddressData: addressData,
		Port:        port,
		IpNetwork:   ipNetwork,
	}, nil
}
