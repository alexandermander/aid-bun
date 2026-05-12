package protocol

const (
	CmdSendText          byte = 1
	CmdGetApps           byte = 2
	CmdConnectSession    byte = 3
	CmdOutputText        byte = 4
	CmdDisconnectSession byte = 5
	CmdPushFile          byte = 6
	CmdExecApp           byte = 7
	CmdEchoSend          byte = 8
)

const HeaderSize = 3
