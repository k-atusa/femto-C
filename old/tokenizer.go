// test784a : TSLL-C compiler tokenizer

package tsll

import (
	"fmt"
	"strings"
)

type Token struct {
	Data    string
	TknType int
	SubType int
	Line    int
}

func PrintTokens(tokens []Token) string {
	res := make([]string, len(tokens))
	for i, t := range tokens {
		var tp string
		switch t.TknType {
		case 1:
			tp = "literal"
		case 2:
			tp = "punctuation"
		case 3:
			tp = "keyword"
		case 4:
			tp = "identifier"
		default:
			tp = "unknown"
		}
		res[i] = fmt.Sprintf("%s@%d (%d.%d) %s", tp, t.Line, t.TknType, t.SubType, t.Data)
	}
	return strings.Join(res, "\n")
}

/* Token types:
1: literal (1:int10, 2:int16, 3:float, 4:char, 5:string)
2: punctuation (1+ 2- 3* 4/ 5% 6< 7<= 8> 9>= 10== 11!= 12&& 13|| 14! 15~ 16& 17| 18^ 19<< 20>> 21: 22; 23. 24, 25= 26( 27) 28{ 29} 30[ 31] 32#)
3: keyword (1i8 2i16 3i32 4i64 5u8 6u16 7u32 8u64 9f32 10f64 11void 12null 13true 14false 15sizeof 16if 17else 18while 19for 20switch 21case 22default 23break 24continue 25return 26struct 27enum)
4: identifier
*/

func Tokenize(text []byte, filename string) ([]Token, error) {
	res := make([]Token, 0, 100)
	var buffer []byte
	status := 0 // 0: normal, 1: word, 2: short comment, 3: long comment, 4: operator, 5: char, 6: char escape, 7: string, 8: str escape 9: number
	readPos := 0
	line := 1
	text = append(text, 0) // add EOF

	for {
		if readPos >= len(text) { // terminate when EOF
			break
		}
		c := text[readPos] // getchar
		readPos++

		switch status {
		case 0: // normal
			if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128 { // word start
				buffer = []byte{c}
				status = 1
			} else if c >= '0' && c <= '9' { // number start
				buffer = []byte{c}
				status = 9
			} else if isDoubleOpStart(c) { // operator start
				buffer = []byte{c}
				status = 4

			} else {
				switch c {
				case '\r': // mac newline
					line++
					if readPos < len(text) && text[readPos] == '\n' { // windows newline
						readPos++
					}

				case '\n': // unix newline
					line++

				case ' ', '\t', 0: // whitespace
					// do nothing

				case '/': // comment or operator
					if readPos < len(text) {
						if text[readPos] == '/' { // short comment
							readPos++
							status = 2
						} else if text[readPos] == '*' { // long comment
							readPos++
							status = 3
						} else { // operator
							res = append(res, Token{Data: string(c), TknType: 2, SubType: 4, Line: line})
						}
					} else { // operator
						res = append(res, Token{Data: string(c), TknType: 2, SubType: 4, Line: line})
					}

				case '\'': // char
					buffer = make([]byte, 0, 4)
					status = 5

				case '"': // string
					buffer = make([]byte, 0, 10)
					status = 7

				default: // punctuation
					subType := isSingleOp(c)
					if subType == 2 && readPos < len(text) && text[readPos] >= '0' && text[readPos] <= '9' { // '-' could be number start
						buffer = []byte{c}
						status = 9
					} else if subType > 0 {
						res = append(res, Token{Data: string(c), TknType: 2, SubType: subType, Line: line})
					} else {
						return nil, fmt.Errorf("E0101 invalid char %c at %s:%d", c, filename, line) // E0101
					}
				}
			}

		case 1: // word
			if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c >= 128 { // word continue
				buffer = append(buffer, c)
			} else { // word end
				word := string(buffer)
				subType := isKeyword(word)
				if subType > 0 { // keyword
					res = append(res, Token{Data: word, TknType: 3, SubType: subType, Line: line})
				} else { // identifier
					res = append(res, Token{Data: word, TknType: 4, SubType: 0, Line: line})
				}
				status = 0
				readPos-- // rewind one char
			}

		case 2: // short comment
			if c == '\r' { // mac newline
				line++
				if readPos < len(text) && text[readPos] == '\n' { // windows newline
					readPos++
				}
				status = 0
			} else if c == '\n' { // unix newline
				line++
				status = 0
			}

		case 3: // long comment
			if c == '\r' { // mac newline
				line++
				if readPos < len(text) && text[readPos] == '\n' { // windows newline
					readPos++
				}
			} else if c == '\n' { // unix newline
				line++
			} else if c == '*' {
				if readPos < len(text) && text[readPos] == '/' { // comment end
					readPos++
					status = 0
				}
			}

		case 4: // operator
			subType := isDoubleOp(buffer[0], c)
			if subType > 0 { // double char operator
				res = append(res, Token{Data: string(buffer) + string(c), TknType: 2, SubType: subType, Line: line})
				status = 0
			} else { // single char operator + char c
				res = append(res, Token{Data: string(buffer), TknType: 2, SubType: isSingleOp(buffer[0]), Line: line})
				status = 0
				readPos-- // rewind one char
			}

		case 5: // char
			if c == '\\' { // escape
				status = 6
			} else if c == '\'' || c == '\r' || c == '\n' { // invalid char end
				return res, fmt.Errorf("E0102 invalid char literal termination at %s:%d", filename, line) // E0102
			} else if readPos < len(text) && text[readPos] == '\'' { // normal char
				res = append(res, Token{Data: string(c), TknType: 1, SubType: 4, Line: line})
				readPos++
				status = 0
			} else {
				return res, fmt.Errorf("E0103 invalid char literal at %s:%d", filename, line) // E0103
			}

		case 6: // char escape
			if readPos < len(text) && text[readPos] == '\'' {
				readPos++
				status = 0
				switch c {
				case '0':
					res = append(res, Token{Data: "\x00", TknType: 1, SubType: 4, Line: line})
				case 'r':
					res = append(res, Token{Data: "\r", TknType: 1, SubType: 4, Line: line})
				case 'n':
					res = append(res, Token{Data: "\n", TknType: 1, SubType: 4, Line: line})
				case 't':
					res = append(res, Token{Data: "\t", TknType: 1, SubType: 4, Line: line})
				case '\'':
					res = append(res, Token{Data: "'", TknType: 1, SubType: 4, Line: line})
				case '\\':
					res = append(res, Token{Data: "\\", TknType: 1, SubType: 4, Line: line})
				default:
					return res, fmt.Errorf("E0104 invalid escape letter %c at %s:%d", c, filename, line) // E0104
				}
			} else {
				return res, fmt.Errorf("E0105 invalid char literal at %s:%d", filename, line) // E0105
			}

		case 7: // string
			if c == '\\' { // escape
				status = 8
			} else if c == '"' { // string end
				res = append(res, Token{Data: string(buffer), TknType: 1, SubType: 5, Line: line})
				status = 0
			} else if c == '\r' || c == '\n' { // invalid string end
				return res, fmt.Errorf("E0106 invalid string literal termination at %s:%d", filename, line) // E0106
			} else { // normal string
				buffer = append(buffer, c)
			}

		case 8: // str escape
			switch c {
			case '0':
				buffer = append(buffer, '\x00')
			case 'r':
				buffer = append(buffer, '\r')
			case 'n':
				buffer = append(buffer, '\n')
			case 't':
				buffer = append(buffer, '\t')
			case '"':
				buffer = append(buffer, '"')
			case '\\':
				buffer = append(buffer, '\\')
			default:
				return res, fmt.Errorf("E0107 invalid escape letter %c at %s:%d", c, filename, line) // E0107
			}

		case 9: // number
			if (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == 'x' || c == 'X' || c == '.' { // number continue
				buffer = append(buffer, c)
			} else { // number end
				subType := isNumber(buffer)
				if subType > 0 { // number
					res = append(res, Token{Data: string(buffer), TknType: 1, SubType: subType, Line: line})
				} else { // invalid number
					return res, fmt.Errorf("E0108 invalid number literal %s at %s:%d", string(buffer), filename, line) // E0108
				}
				status = 0
				readPos-- // rewind one char
			}
		}
	}
	if status == 5 || status == 6 || status == 7 || status == 8 {
		return res, fmt.Errorf("E0109 literal not terminated at %s:%d", filename, line) // E0109
	}
	return res, nil
}

func isDoubleOpStart(c byte) bool {
	return (c == '<') || (c == '>') || (c == '=') || (c == '!') || (c == '&') || (c == '|')
}

func isDoubleOp(c1 byte, c2 byte) int {
	switch {
	case c1 == '<' && c2 == '=':
		return 7
	case c1 == '>' && c2 == '=':
		return 9
	case c1 == '=' && c2 == '=':
		return 10
	case c1 == '!' && c2 == '=':
		return 11
	case c1 == '&' && c2 == '&':
		return 12
	case c1 == '|' && c2 == '|':
		return 13
	case c1 == '<' && c2 == '<':
		return 19
	case c1 == '>' && c2 == '>':
		return 20
	default:
		return 0
	}
}

func isSingleOp(c byte) int {
	switch c {
	case '+':
		return 1
	case '-':
		return 2
	case '*':
		return 3
	case '/':
		return 4
	case '%':
		return 5
	case '<':
		return 6
	case '>':
		return 8
	case '!':
		return 14
	case '~':
		return 15
	case '&':
		return 16
	case '|':
		return 17
	case '^':
		return 18
	case ':':
		return 21
	case ';':
		return 22
	case '.':
		return 23
	case ',':
		return 24
	case '=':
		return 25
	case '(':
		return 26
	case ')':
		return 27
	case '{':
		return 28
	case '}':
		return 29
	case '[':
		return 30
	case ']':
		return 31
	case '#':
		return 32
	}
	return 0
}

func isKeyword(word string) int {
	switch word {
	case "i8":
		return 1
	case "i16":
		return 2
	case "i32":
		return 3
	case "i64":
		return 4
	case "u8":
		return 5
	case "u16":
		return 6
	case "u32":
		return 7
	case "u64":
		return 8
	case "f32":
		return 9
	case "f64":
		return 10
	case "void":
		return 11
	case "null":
		return 12
	case "true":
		return 13
	case "false":
		return 14
	case "sizeof":
		return 15
	case "if":
		return 16
	case "else":
		return 17
	case "while":
		return 18
	case "for":
		return 19
	case "switch":
		return 20
	case "case":
		return 21
	case "default":
		return 22
	case "break":
		return 23
	case "continue":
		return 24
	case "return":
		return 25
	case "struct":
		return 26
	case "enum":
		return 27
	default:
		return 0
	}
}

func isNumber(word []byte) int {
	isHex := false
	isFloat := false
	for i, c := range word {
		switch {
		case c == '-' && i == 0:
			continue
		case (c == 'x' || c == 'X') && !isHex && !isFloat:
			isHex = true
		case c == '.' && !isHex && !isFloat:
			isFloat = true
		case '0' <= c && c <= '9':
			continue
		case isHex && (('a' <= c && c <= 'f') || ('A' <= c && c <= 'F')):
			continue
		default:
			return 0 // invalid number
		}
	}
	if isFloat {
		return 3 // float
	}
	if isHex {
		return 2 // int16
	}
	return 1 // int10
}

// provides tokens one by one
type TokenProvider struct {
	Tokens   []Token
	Pos      int
	Filename string
}

func (tp *TokenProvider) Set(tkns []Token, fname string) {
	tp.Tokens = tkns
	tp.Pos = 0
	tp.Filename = fname
}

func (tp *TokenProvider) Pop() *Token {
	if tp.Pos < len(tp.Tokens) {
		t := &tp.Tokens[tp.Pos]
		tp.Pos++
		return t
	}
	return nil
}

func (tp *TokenProvider) Seek() *Token {
	if tp.Pos < len(tp.Tokens) {
		return &tp.Tokens[tp.Pos]
	}
	return nil
}

func (tp *TokenProvider) Rewind() {
	if tp.Pos > 0 {
		tp.Pos--
	}
}

func (tp *TokenProvider) IsEnd() bool {
	return tp.Pos >= len(tp.Tokens)
}
