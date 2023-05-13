package framework

import (
	"fmt"
	"math/rand"
	"signaling/src/glog"
	"time"
)

func init() {
	rand.Seed(time.Now().Unix())
}

func GetLogId32() uint32 {
	return rand.Uint32()
}

type logItem struct {
	field string
	value string
}

type timeItem struct {
	field     string
	beginTime int64
	endTime   int64
}

type ComLog struct {
	mainLog []logItem
	timeLog []timeItem
}

// 这里运行field重复
func (l *ComLog) TimeBegin(field string) {
	item := timeItem{
		field:     field,
		beginTime: time.Now().UnixNano() / 1000,
	}
	l.timeLog = append(l.timeLog, item)
}

func (l *ComLog) TimeEnd(field string) {
	for k, v := range l.timeLog {
		if v.field == field {
			l.timeLog[k].endTime = time.Now().UnixNano() / 1000
			break
		}
	}
}

func (l *ComLog) AddNotice(field, value string) {
	item := logItem{
		field: field,
		value: value,
	}
	l.mainLog = append(l.mainLog, item)
}

func (l *ComLog) getPrefixLog() string {
	prefixLog := ""

	// mainLog
	for _, item := range l.mainLog {
		prefixLog += fmt.Sprintf("%s [%s] ", item.field, item.value)
	}

	// timeLog
	for _, item := range l.timeLog {
		diff := item.endTime - item.beginTime
		if diff < 0 {
			continue
		}
		fdiff := float64(diff) / 1000.0
		prefixLog += fmt.Sprintf("%s [%.3fms] ", item.field, fdiff)
	}

	return prefixLog
}

func (l *ComLog) Debugf(format string, args ...interface{}) {
	totalLog := l.getPrefixLog() + format
	glog.Debugf(totalLog, args...)
}

func (l *ComLog) Infof(format string, args ...interface{}) {
	totalLog := l.getPrefixLog() + format
	glog.Infof(totalLog, args...)
}

func (l *ComLog) Warningf(format string, args ...interface{}) {
	totalLog := l.getPrefixLog() + format
	glog.Warningf(totalLog, args...)
}
