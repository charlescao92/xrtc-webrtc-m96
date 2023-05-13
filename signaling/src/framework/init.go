package framework

import (
	"fmt"
	"signaling/src/glog"
)

var gconf *FrameworkConf

func Init(confFile string) error {
	var err error
	gconf, err = loadConf(confFile)
	if err != nil {
		return err
	}

	// &{logDir:./log logFile:signaling logLevel:DEBUG logToStderr:false}
	fmt.Printf("%+v\n", gconf)

	glog.SetLogDir(gconf.logDir)
	glog.SetLogFileName(gconf.logFile)
	glog.SetLogLevel(gconf.logLevel)
	glog.SetLogToStderr(gconf.logToStderr)

	err = loadXrpc()
	if err != nil {
		return err
	}

	return nil
}
