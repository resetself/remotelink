package macd

import "embed"

//go:embed cmd/server/macctl.c
var ServerFiles embed.FS
