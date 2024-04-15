package types

type ProcessInfo struct {
	Name    string
	ExePath string
	Uid     int
	Gid     int
	Pid     int
	Args    string
}

type ProcessLineage struct {
	Name          string
	ExePath       string
	ParentUid     int
	ParentExePath string
}
