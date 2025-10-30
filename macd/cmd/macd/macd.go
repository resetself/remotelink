package main

import (
	"bufio"
	"bytes"
	"context"
	"fmt"
	"io/fs"
	"macd"
	"net"
	"os"
	"os/exec"
	"strings"
	"syscall"
)

const port = 52698
const mountPath = "~/.remotelink/files/"
const macctlSource = "cmd/server/macctl.c"

func main() {
	cfg := &net.ListenConfig{
		Control: func(_, _ string, c syscall.RawConn) error {
			return c.Control(func(fd uintptr) {
				syscall.SetsockoptInt(int(fd), syscall.SOL_SOCKET, syscall.SO_REUSEADDR, 1)
				_ = syscall.SetsockoptInt(int(fd), syscall.SOL_SOCKET, 0x0F, 1)
			})
		},
	}

	ln, err := cfg.Listen(context.Background(), "tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		os.Exit(1)
	}
	defer ln.Close()

	fmt.Printf("🚀 监听端口 %d\n", port)

	for {
		conn, _ := ln.Accept()
		go handle(conn)
	}
}

func handle(conn net.Conn) {
	defer conn.Close()
	data, _ := bufio.NewReader(conn).ReadBytes('\n')
	parts := strings.Split(strings.TrimSpace(string(data)), " ")
	if len(parts) < 1 {
		return
	}

	command := parts[0]
	if command == "install_macctl" {
		macctlInstall(conn)
		return
	}
	params := ""

	for _, part := range parts[1:] {
		value := strings.Split(strings.TrimSpace(part), "|")

		switch value[0] {
		case "FILE":
			// 如果是文件类型的，给文件添加本地文件映射路径
			params += mountPath + value[1]
		default:
			params += value[0]
		}
	}
	execCmd(conn, fmt.Sprintf("%s %s", command, params))
}

func macctlInstall(conn net.Conn) {
	content, err := fs.ReadFile(macd.ServerFiles, macctlSource)
	if err != nil {
		fmt.Println("error: ", err)
		return
	}
	conn.Write(content)
}

func execCmd(conn net.Conn, cmd string) {
	var out, stderr bytes.Buffer
	c := exec.Command("sh", "-c", cmd)
	c.Stdout = &out
	c.Stderr = &stderr

	if c.Run() != nil {
		conn.Write([]byte("ERROR|" + stderr.String() + "\n"))
	} else {
		conn.Write([]byte("OK|" + out.String() + "\n"))
	}
}
