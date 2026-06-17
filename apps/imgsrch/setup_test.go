package main

import (
	"testing"
	"time"
)

func TestDownloadClientHasBoundedTimeouts(t *testing.T) {
	client := newDownloadClient(time.Minute)
	if client.Timeout != time.Minute {
		t.Fatalf("client timeout = %v, want 1m", client.Timeout)
	}
}
