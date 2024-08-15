package types

type ProcessOriginator struct {
	ProcessName         string
	ProcessExecFilePath string
	ProcessArgs         string
}

func (p *ProcessOriginator) Less(other ProcessOriginator) bool {
	return p.ProcessName < other.ProcessName ||
		p.ProcessExecFilePath < other.ProcessExecFilePath ||
		p.ProcessArgs < other.ProcessArgs
}

func (p *ProcessOriginator) Equal(other ProcessOriginator) bool {
	return p.ProcessName == other.ProcessName &&
		p.ProcessExecFilePath == other.ProcessExecFilePath &&
		p.ProcessArgs == other.ProcessArgs
}
