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

const (
	DefaultLogLevel = InfoLevel
)

var logLevelString = [...]string{
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
}

var logLevelMap = map[string]LogLevel{
	"TRACE": TraceLevel,
	"DEBUG": DebugLevel,
	"INFO":  InfoLevel,
	"WARN":  WarnLevel,
	"ERROR": ErrorLevel,
}

type Logger struct {
	level  LogLevel
	logger *log.Logger
}

var DefaultLogger *Logger

func init() {
	DefaultLogger = NewLogger()
}

func NewLogger() *Logger {
	levelStr := config.ReadEnvVar("LOG_LEVEL")
	level, exists := logLevelMap[strings.ToUpper(levelStr)]
	if !exists {
		level = DefaultLogLevel
	}

	return &Logger{
		level:  level,
		logger: log.New(os.Stdout, "", log.Ldate|log.Ltime),
	}
}

func (l *Logger) Log(level LogLevel, format string, v ...interface{}) {
	if level < l.level && level >= TraceLevel && level <= ErrorLevel {
		return
	}
	l.logger.Printf(logLevelString[level]+": "+format, v...)
}

func Log(logLevel LogLevel, format string, v ...interface{}) {
	DefaultLogger.Log(logLevel, format, v...)
}

func Debug(format string, v ...interface{}) {
	DefaultLogger.Log(DebugLevel, format, v...)
}

func Info(format string, v ...interface{}) {
	DefaultLogger.Log(InfoLevel, format, v...)
}

func Warn(format string, v ...interface{}) {
	DefaultLogger.Log(WarnLevel, format, v...)
}

func Trace(format string, v ...interface{}) {
	DefaultLogger.Log(TraceLevel, format, v...)
}

func Error(format string, v ...interface{}) error {
	DefaultLogger.Log(ErrorLevel, format, v...)
	return fmt.Errorf(format, v...)
}

func ErrorWrap(err error, format string, v ...interface{}) error {
	return Error("%s: %w", fmt.Sprintf(format, v...), err)
}
