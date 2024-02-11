package log

import (
	"log"
	"os"
	"strings"

	"github.com/stackrox/collector/integration-tests/suites/config"
)

type LogLevel int

const (
	DebugLevel LogLevel = iota
	InfoLevel
	ErrorLevel
)

type Logger struct {
	level  LogLevel
	logger *log.Logger
}

var DefaultLogger *Logger

func init() {
	DefaultLogger = NewLogger()
}

func parseLogLevel(levelStr string) LogLevel {
	switch strings.ToLower(levelStr) {
	case "debug":
		return DebugLevel
	case "info":
		return InfoLevel
	case "error":
		return ErrorLevel
	default:
		return InfoLevel
	}
}

func NewLogger() *Logger {
	levelStr := config.ReadEnvVar("LOG_LEVEL")
	level := parseLogLevel(levelStr)

	return &Logger{
		level:  level,
		logger: log.New(os.Stdout, "", log.Ldate|log.Ltime),
	}
}

func (l *Logger) Debug(format string, v ...interface{}) {
	if l.level <= DebugLevel {
		l.logger.Printf("DEBUG: "+format, v...)
	}
}

func (l *Logger) Info(format string, v ...interface{}) {
	if l.level <= InfoLevel {
		l.logger.Printf("INFO: "+format, v...)
	}
}

func (l *Logger) Log(format string, v ...interface{}) {
	l.logger.Printf(format, v...)
}

func (l *Logger) Error(format string, v ...interface{}) {
	l.logger.Printf("ERROR: "+format, v...)
}

func Log(format string, v ...interface{}) {
	DefaultLogger.Log(format, v...)
}

func Debug(format string, v ...interface{}) {
	DefaultLogger.Debug(format, v...)
}

func Info(format string, v ...interface{}) {
	DefaultLogger.Info(format, v...)
}

func Error(format string, v ...interface{}) {
	DefaultLogger.Error(format, v...)
}
