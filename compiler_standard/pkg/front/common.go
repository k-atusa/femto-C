package front

import (
	"fmt"
	"os"
	"path/filepath"
	"unicode/utf8"
)

// read text data
func ReadFile(path string) (string, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return "", fmt.Errorf("E0001 file open_r fail: %s, %w", path, err)
	}
	return string(data), nil
}

// write text data
func WriteFile(path string, data string) error {
	err := os.WriteFile(path, []byte(data), 0644)
	if err != nil {
		return fmt.Errorf("E0002 file open_w fail: %s, %w", path, err)
	}
	return nil
}

// extract file name from path
func GetFileName(path string) string {
	return filepath.Base(path)
}

// extract directory from path
func GetWorkingDir(path string) string {
	return filepath.Dir(path)
}

// get absolute path by baseDir(abspath)
func AbsPath(path string, baseDir string) (string, error) {
	if filepath.IsAbs(path) {
		return filepath.Clean(path), nil
	}
	absBase, err := filepath.Abs(baseDir)
	if err != nil {
		return "", fmt.Errorf("E0003 path resolve fail: <%s, %s>, %w", path, baseDir, err)
	}
	fullPath := filepath.Join(absBase, path)
	return fullPath, nil
}

// source location
type Loc struct {
	SrcID int
	Line  int
	Col   int
}

// literal data
type LiteralType int

const (
	LitNptr LiteralType = iota
	LitBool
	LitInt
	LitFloat
	LitString
)

type Literal struct {
	ObjType LiteralType
	Value   interface{} // int64, float64, string, bool, nil
}

func (l *Literal) Init(v interface{}) error {
	switch v.(type) {
	case int64:
		l.ObjType = LitInt
		l.Value = v
	case float64:
		l.ObjType = LitFloat
		l.Value = v
	case string:
		l.ObjType = LitString
		l.Value = v
	case bool:
		l.ObjType = LitBool
		l.Value = v
	case nil:
		l.ObjType = LitNptr
		l.Value = v
	default:
		return fmt.Errorf("E0004 literal init fail: %v", v)
	}
	return nil
}

// compiler message, error counter
type CplrMsg struct {
	Level    int
	ErrCount int
}

func (m *CplrMsg) Init(level int) {
	m.Level = level
	m.ErrCount = 0
}

func (m *CplrMsg) Log(msg string, lvl int) {
	if lvl >= m.Level {
		fmt.Println(msg)
	}
}

// convert unicode(int) to bytes
func UniToByte(uni int) []byte {
	if uni < 0 {
		return []byte{}
	}
	buf := make([]byte, 4)
	n := utf8.EncodeRune(buf, rune(uni))
	return buf[:n]
}

// convert bytes to unicode(int)
func ByteToUni(bytes []byte) int {
	if len(bytes) == 0 {
		return -1
	}
	r, _ := utf8.DecodeRune(bytes)
	if r == utf8.RuneError {
		return -1
	}
	return int(r)
}

// saves source path, provides location string
type SrcLocSave struct {
	Paths []string
}

func (s *SrcLocSave) Init() {
	s.Paths = make([]string, 0)
}

func (s *SrcLocSave) GetLoc(l Loc) string {
	if l.SrcID < 0 || l.SrcID >= len(s.Paths) {
		return "unknown"
	}
	return fmt.Sprintf("%s:%d.%d", s.Paths[l.SrcID], l.Line, l.Col)
}
