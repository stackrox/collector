package common

// ContainerID is an identifier for a container. It can
// be a container name *OR* a hex ID
type ContainerID string

// Long returns the whole containerID as a string
func (c ContainerID) Long() string {
	return string(c)
}

// Short returns the first twelve character of a containerID
// to match the shortened IDs returned by docker.
func (c ContainerID) Short() string {
	return string(c)[:12]
}
