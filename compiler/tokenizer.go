// test000a : TSLL-C compiler tokenizer

package main

import "fmt"

type Token struct {
	Data    string
	TknType int
	SubType int
	Line    int
}

/* Token types:
1: literal (1:int10, 2:int16, 3:float, 4:char, 5:string)
2: punctuation (1+ 2- 3* 4/ 5% 6< 7<= 8> 9>= 10== 11!= 12&& 13|| 14! 15~ 16& 17| 18^ 19<< 20>> 21: 22. 23= 24( 25) 26{ 27} 28[ 29] 30#)
3: keyword
4: identifier
*/

func Tokenize(text []byte, filename string) ([]Token, error) {
	res := make([]Token, 0, 100)
	var buffer []byte
	status := 0 // 0: normal, 1: word, 2: short comment, 3: long comment, 4: operator, 5: char, 6: string, 7: str escape
	readPos := 0
	line := 1
	text = append(text, 0) // add EOF

	for true {
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
							buffer = []byte{c}
							status = 4
						}
					} else { // operator
						res = append(res, Token{Data: string(c), TknType: 2, SubType: 4, Line: line})
					}

				case '\'': // char
					buffer = []byte{c}
					status = 5

				case '"': // string
					buffer = []byte{c}
					status = 6

				default: // punctuation
					subType := isSingleOp(c)
					if subType > 0 {
						res = append(res, Token{Data: string(c), TknType: 2, SubType: subType, Line: line})
					} else {
						return nil, fmt.Errorf("E0101 invalid char %c at %s:%d", filename, line, c) // E0101
					}
				}
			}

		case 1: // word
		case 2: // short comment
		case 3: // long comment
		case 4: // operator
		case 5: // string
		case 6: // str escape
		}
	}
}

func isDoubleOpStart(c byte) bool {
	return (c == '<') || (c == '>') || (c == '=') || (c == '!') || (c == '&') || (c == '|')
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
	case '.':
		return 22
	case '=':
		return 23
	case '(':
		return 24
	case ')':
		return 25
	case '{':
		return 26
	case '}':
		return 27
	case '[':
		return 28
	case ']':
		return 29
	case '#':
		return 30
	}
	return 0
}
