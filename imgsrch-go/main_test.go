package main

import (
	"strings"
	"testing"
)

func TestUsageListsPublicCommandsAndFlags(t *testing.T) {
	for _, want := range []string{
		"Usage:",
		"imgsrch <command> [arguments]",
		"setup",
		"search",
		"--help",
		"--version",
	} {
		if !strings.Contains(usageText, want) {
			t.Errorf("usageText missing %q", want)
		}
	}
}

func TestParseTopN(t *testing.T) {
	for _, tc := range []struct {
		input string
		want  int
		ok    bool
	}{
		{"1", 1, true},
		{"10", 10, true},
		{"0", 0, false},
		{"-1", 0, false},
		{"many", 0, false},
	} {
		got, err := parseTopN(tc.input)
		if tc.ok && (err != nil || got != tc.want) {
			t.Fatalf("parseTopN(%q) = %d, %v; want %d, nil", tc.input, got, err, tc.want)
		}
		if !tc.ok && err == nil {
			t.Fatalf("parseTopN(%q) unexpectedly succeeded with %d", tc.input, got)
		}
	}
}
