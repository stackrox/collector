package log

import (
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/stackrox/collector/integration-tests/pkg/config"
)

type LogLevel int

const (
	TraceLevel LogLevel = iota
	DebugLevel
	InfoLevel
	WarnLevel
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
	case "trace":
		return TraceLevel
	case "debug":
		return DebugLevel
	case "warn":
		return WarnLevel
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

func (l *Logger) Warn(format string, v ...interface{}) {
	if l.level <= WarnLevel {
		l.logger.Printf("WARN: "+format, v...)
	}
}

func (l *Logger) Log(format string, v ...interface{}) {
	l.logger.Printf(format, v...)
}

func (l *Logger) Error(format string, v ...interface{}) error {
	l.logger.Printf("ERROR: "+format, v...)
	return fmt.Errorf(format, v...)
}

func (l *Logger) ErrorWrap(err error, format string, v ...interface{}) error {
	errMsg := fmt.Sprintf(format, v...)
	errMsgWithErr := fmt.Errorf("%s: %w", errMsg, err)
	l.logger.Printf("ERROR: %s", errMsgWithErr)
	return err
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

func Warn(format string, v ...interface{}) {
	DefaultLogger.Warn(format, v...)
}

func Error(format string, v ...interface{}) error {
	return DefaultLogger.Error(format, v...)
}

func ErrorWrap(err error, format string, v ...interface{}) error {
	return DefaultLogger.ErrorWrap(err, format, v...)
}
