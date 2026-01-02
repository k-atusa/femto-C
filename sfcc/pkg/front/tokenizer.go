// test787b : femto-C front.tokenizer R0

package front

import (
	"fmt"
	"strconv"
)

// token is single unit of source code
type TokenType int

const (
	// Literals
	LIT_INT TokenType = iota
	LIT_FLOAT
	LIT_STR
	LIT_INT2  // for tokenizer only
	LIT_INT8  // for tokenizer only
	LIT_INT16 // for tokenizer only
	// identifier
	ID
	// + - * / %
	OP_ADD
	OP_SUB
	OP_MUL
	OP_DIV
	OP_MOD
	// < <= > >= == !=
	OP_LT
	OP_LT_EQ
	OP_GT
	OP_GT_EQ
	OP_EQ
	OP_NEQ
	// && || ! & | ~ ^ << >>
	OP_LOGIC_AND
	OP_LOGIC_OR
	OP_LOGIC_NOT
	OP_BIT_AND
	OP_BIT_OR
	OP_BIT_NOT
	OP_BIT_XOR
	OP_BIT_LSHIFT
	OP_BIT_RSHIFT
	// ? . , : ; ( ) { } [ ]
	OP_COND
	OP_DOT
	OP_COMMA
	OP_COLON
	OP_SEMICOLON
	OP_LPAREN
	OP_RPAREN
	OP_LBRACE
	OP_RBRACE
	OP_LBRACKET
	OP_RBRACKET
	// = += -= *= /= %= ++ --
	OP_ASSIGN
	OP_ASSIGN_ADD
	OP_ASSIGN_SUB
	OP_ASSIGN_MUL
	OP_ASSIGN_DIV
	OP_ASSIGN_MOD
	OP_INC
	OP_DEC
	// Keywords
	KEY_AUTO
	KEY_INT
	KEY_I8
	KEY_I16
	KEY_I32
	KEY_I64
	KEY_UINT
	KEY_U8
	KEY_U16
	KEY_U32
	KEY_U64
	KEY_F32
	KEY_F64
	KEY_BOOL
	KEY_VOID
	KEY_NULL
	KEY_TRUE
	KEY_FALSE
	KEY_IF
	KEY_ELSE
	KEY_WHILE
	KEY_FOR
	KEY_SWITCH
	KEY_CASE
	KEY_DEFAULT
	KEY_BREAK
	KEY_CONTINUE
	KEY_FALL
	KEY_RETURN
	KEY_STRUCT
	KEY_ENUM
	// integrated functions
	IFUNC_SIZEOF
	IFUNC_CAST
	IFUNC_MAKE
	IFUNC_LEN
	IFUNC_MOVE
	// compiler order
	KEY_INCLUDE
	KEY_TYPEDEF
	KEY_TEMPLATE
	KEY_DEFER
	KEY_DEFINE
	KEY_VA_ARG
	KEY_RAW_C
	KEY_RAW_IR
	KEY_CONST
	KEY_VOLATILE
	KEY_EXTERN
	KEY_EXPORT
	// for token match
	PRECOMPILE
)

type Token struct {
	ObjType  TokenType
	Location Loc
	Value    Literal
	Text     string
}

// helper functions for type checking
func IsSInt(t TokenType) bool {
	return t == KEY_INT || t == KEY_I8 || t == KEY_I16 || t == KEY_I32 || t == KEY_I64
}

func IsUInt(t TokenType) bool {
	return t == KEY_UINT || t == KEY_U8 || t == KEY_U16 || t == KEY_U32 || t == KEY_U64
}

func IsInt(t TokenType) bool {
	return IsSInt(t) || IsUInt(t)
}

func IsFloat(t TokenType) bool {
	return t == KEY_F32 || t == KEY_F64
}

func IsPrimitive(t TokenType) bool {
	return IsInt(t) || IsFloat(t) || t == KEY_VOID || t == KEY_BOOL
}

// tokenizer status
type TknStatus int

const (
	Default TknStatus = iota
	ShortComment
	LongComment
	Identifier
	DoubleOp
	Number
	Char
	CharEscape
	String
	StringEscape
	RawString
)

// helper functions for tokenizing
func isDoubleOpStart(c byte) bool {
	return c == '<' || c == '>' || c == '=' || c == '!' || c == '&' || c == '|' ||
		c == '+' || c == '-' || c == '*' || c == '/' || c == '%'
}

func isDoubleOp(c1 byte, c2 byte) TokenType {
	s := string([]byte{c1, c2})
	switch s {
	case "<=":
		return OP_LT_EQ
	case ">=":
		return OP_GT_EQ
	case "==":
		return OP_EQ
	case "!=":
		return OP_NEQ
	case "&&":
		return OP_LOGIC_AND
	case "||":
		return OP_LOGIC_OR
	case "<<":
		return OP_BIT_LSHIFT
	case ">>":
		return OP_BIT_RSHIFT
	case "+=":
		return OP_ASSIGN_ADD
	case "-=":
		return OP_ASSIGN_SUB
	case "*=":
		return OP_ASSIGN_MUL
	case "/=":
		return OP_ASSIGN_DIV
	case "%=":
		return OP_ASSIGN_MOD
	case "++":
		return OP_INC
	case "--":
		return OP_DEC
	}
	return PRECOMPILE
}

func isSingleOp(c byte) TokenType {
	switch c {
	case '+':
		return OP_ADD
	case '-':
		return OP_SUB
	case '*':
		return OP_MUL
	case '/':
		return OP_DIV
	case '%':
		return OP_MOD
	case '<':
		return OP_LT
	case '>':
		return OP_GT
	case '!':
		return OP_LOGIC_NOT
	case '&':
		return OP_BIT_AND
	case '|':
		return OP_BIT_OR
	case '~':
		return OP_BIT_NOT
	case '^':
		return OP_BIT_XOR
	case '=':
		return OP_ASSIGN
	case '?':
		return OP_COND
	case '.':
		return OP_DOT
	case ',':
		return OP_COMMA
	case ':':
		return OP_COLON
	case ';':
		return OP_SEMICOLON
	case '(':
		return OP_LPAREN
	case ')':
		return OP_RPAREN
	case '{':
		return OP_LBRACE
	case '}':
		return OP_RBRACE
	case '[':
		return OP_LBRACKET
	case ']':
		return OP_RBRACKET
	}
	return PRECOMPILE
}

func isKeyword(word string) TokenType {
	switch word {
	case "auto":
		return KEY_AUTO
	case "int":
		return KEY_INT
	case "i8":
		return KEY_I8
	case "i16":
		return KEY_I16
	case "i32":
		return KEY_I32
	case "i64":
		return KEY_I64
	case "uint":
		return KEY_UINT
	case "u8":
		return KEY_U8
	case "u16":
		return KEY_U16
	case "u32":
		return KEY_U32
	case "u64":
		return KEY_U64
	case "f32":
		return KEY_F32
	case "f64":
		return KEY_F64
	case "bool":
		return KEY_BOOL
	case "void":
		return KEY_VOID
	case "null":
		return KEY_NULL
	case "true":
		return KEY_TRUE
	case "false":
		return KEY_FALSE
	case "if":
		return KEY_IF
	case "else":
		return KEY_ELSE
	case "while":
		return KEY_WHILE
	case "for":
		return KEY_FOR
	case "switch":
		return KEY_SWITCH
	case "case":
		return KEY_CASE
	case "default":
		return KEY_DEFAULT
	case "break":
		return KEY_BREAK
	case "continue":
		return KEY_CONTINUE
	case "fall":
		return KEY_FALL
	case "return":
		return KEY_RETURN
	case "struct":
		return KEY_STRUCT
	case "enum":
		return KEY_ENUM
	case "sizeof":
		return IFUNC_SIZEOF
	case "cast":
		return IFUNC_CAST
	case "make":
		return IFUNC_MAKE
	case "len":
		return IFUNC_LEN
	case "move":
		return IFUNC_MOVE
	}
	return isCplrOrd(word)
}

func isCplrOrd(word string) TokenType {
	switch word {
	case "include":
		return KEY_INCLUDE
	case "typedef":
		return KEY_TYPEDEF
	case "template":
		return KEY_TEMPLATE
	case "defer":
		return KEY_DEFER
	case "define":
		return KEY_DEFINE
	case "va_arg":
		return KEY_VA_ARG
	case "raw_c":
		return KEY_RAW_C
	case "raw_ir":
		return KEY_RAW_IR
	case "const":
		return KEY_CONST
	case "volatile":
		return KEY_VOLATILE
	case "extern":
		return KEY_EXTERN
	case "export":
		return KEY_EXPORT
	}
	return PRECOMPILE
}

func isNumber(text string) TokenType {
	if len(text) == 0 {
		return PRECOMPILE
	}
	isHex := false
	isOct := false
	isBin := false
	isFloat := false
	chars := []byte(text)
	if len(chars) > 2 && chars[0] == '0' {
		switch chars[1] {
		case 'x', 'X':
			isHex = true
		case 'o', 'O':
			isOct = true
		case 'b', 'B':
			isBin = true
		}
		chars = chars[2:]
	}

	for _, c := range chars {
		if c == '.' && !isFloat {
			isFloat = true
			continue
		}
		if isHex {
			if !('0' <= c && c <= '9' || 'a' <= c && c <= 'f' || 'A' <= c && c <= 'F') {
				return PRECOMPILE
			}
		} else if isOct {
			if !('0' <= c && c <= '7') {
				return PRECOMPILE
			}
		} else if isBin {
			if !('0' <= c && c <= '1') {
				return PRECOMPILE
			}
		} else {
			if !('0' <= c && c <= '9') {
				return PRECOMPILE
			}
		}
	}

	if isHex {
		return LIT_INT16
	} else if isOct {
		return LIT_INT8
	} else if isBin {
		return LIT_INT2
	} else if isFloat {
		return LIT_FLOAT
	}
	return LIT_INT
}

func getHexEscape(c0 byte, c1 byte) byte {
	if '0' <= c0 && c0 <= '9' {
		c0 = c0 - '0'
	} else if 'a' <= c0 && c0 <= 'f' {
		c0 = c0 - 'a' + 10
	} else {
		c0 = c0 - 'A' + 10
	}
	if '0' <= c1 && c1 <= '9' {
		c1 = c1 - '0'
	} else if 'a' <= c1 && c1 <= 'f' {
		c1 = c1 - 'a' + 10
	} else {
		c1 = c1 - 'A' + 10
	}
	return (c0 << 4) | c1
}

// tokenize source code into tokens
func Tokenize(source string, filename string, sourceID int) ([]Token, error) {
	var result []Token
	buffer := make([]byte, 0, 16)
	status := Default
	line := 1
	col := 1
	src := []byte(source)
	readPos := 0

	for {
		var c byte
		if readPos >= len(src) {
			c = '\n'
			if readPos > len(src) { // end loop
				break
			}
		} else {
			c = src[readPos]
			readPos++
			col++
		}

		switch status {
		case Default:
			if (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c > 127 { // id start
				buffer = []byte{c}
				status = Identifier
			} else if c >= '0' && c <= '9' { // number start
				buffer = []byte{c}
				status = Number
			} else if isDoubleOpStart(c) { // op start
				buffer = []byte{c}
				status = DoubleOp
			} else {
				switch c {
				case ' ', '\t', 0: // skip whitespace
				case '\r': // mac newline
					line++
					if readPos < len(src) && src[readPos] == '\n' {
						readPos++
					}
					col = 1
				case '\n': // unix newline
					line++
					col = 1
				case '/': // comment or divide
					if readPos < len(src) {
						if src[readPos] == '/' {
							readPos++
							status = ShortComment
						} else if src[readPos] == '*' {
							readPos++
							status = LongComment
						} else { // divide
							result = append(result, Token{ObjType: OP_DIV, Location: Loc{SrcID: sourceID, Line: line, Col: col}, Text: "/"})
						}
					} else { // divide
						result = append(result, Token{ObjType: OP_DIV, Location: Loc{SrcID: sourceID, Line: line, Col: col}, Text: "/"})
					}
				case '\'': // char start
					buffer = []byte{}
					status = Char
				case '"': // string start
					buffer = []byte{}
					status = String
				case '`': // raw string start
					buffer = []byte{}
					status = RawString
				default:
					tknType := isSingleOp(c)
					if tknType != PRECOMPILE {
						result = append(result, Token{ObjType: tknType, Location: Loc{SrcID: sourceID, Line: line, Col: col}, Text: string(c)})
					} else {
						return nil, fmt.Errorf("E0101 invalid char %c at %s:%d.%d", c, filename, line, col)
					}
				}
			}

		case ShortComment:
			if c == '\r' {
				line++
				if readPos < len(src) && src[readPos] == '\n' {
					readPos++
				}
				col = 1
				status = Default
			} else if c == '\n' {
				line++
				col = 1
				status = Default
			}

		case LongComment:
			if c == '\r' {
				line++
				if readPos < len(src) && src[readPos] == '\n' {
					readPos++
				}
			} else if c == '\n' {
				line++
			} else if c == '*' {
				if readPos < len(src) && src[readPos] == '/' {
					readPos++
					col = 1
					status = Default
				}
			}

		case Identifier:
			if (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c > 127 || (c >= '0' && c <= '9') {
				buffer = append(buffer, c)
			} else {
				idStr := string(buffer)
				tkn := Token{Location: Loc{SrcID: sourceID, Line: line, Col: col}, Text: idStr}
				kwType := isKeyword(idStr)
				if kwType != PRECOMPILE {
					tkn.ObjType = kwType
				} else {
					tkn.ObjType = ID
					tkn.Value.Init(idStr)
				}
				result = append(result, tkn)
				status = Default
				readPos--
			}

		case DoubleOp:
			tknType := isDoubleOp(buffer[0], c)
			if tknType != PRECOMPILE {
				result = append(result, Token{ObjType: tknType, Location: Loc{SrcID: sourceID, Line: line, Col: col}, Text: string(buffer) + string(c)})
				status = Default
			} else {
				result = append(result, Token{ObjType: isSingleOp(buffer[0]), Location: Loc{SrcID: sourceID, Line: line, Col: col}, Text: string(buffer)})
				status = Default
				readPos--
			}

		case Number:
			if (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == 'x' || c == 'X' || c == '.' {
				buffer = append(buffer, c)
			} else {
				numStr := string(buffer)
				numType := isNumber(numStr)
				if numType == PRECOMPILE {
					return nil, fmt.Errorf("E0102 invalid number %s at %s:%d", numStr, filename, line)
				}
				tkn := Token{ObjType: numType, Location: Loc{SrcID: sourceID, Line: line, Col: col}, Text: numStr}

				var val Literal
				switch numType {
				case LIT_INT, LIT_INT16, LIT_INT2, LIT_INT8:
					base := 10
					if numType == LIT_INT16 {
						base = 16
					}
					if numType == LIT_INT8 {
						base = 8
					}
					if numType == LIT_INT2 {
						base = 2
					}

					// remove prefixes if present for strconv
					parseStr := numStr
					if len(parseStr) > 2 && parseStr[0] == '0' {
						parseStr = parseStr[2:]
					}

					i, err := strconv.ParseInt(parseStr, base, 64)
					if err != nil {
						u, err2 := strconv.ParseUint(parseStr, base, 64)
						if err2 != nil {
							return nil, fmt.Errorf("E0103 number literal conversion fail at %s:%d.%d", filename, line, col)
						}
						i = int64(u)
					}
					val.Init(i)
					tkn.ObjType = LIT_INT // set type to int(same)

				case LIT_FLOAT:
					f, err := strconv.ParseFloat(numStr, 64)
					if err != nil {
						return nil, fmt.Errorf("E0103 number literal conversion fail at %s:%d.%d", filename, line, col)
					}
					val.Init(f)
					tkn.ObjType = LIT_FLOAT
				}
				tkn.Value = val
				result = append(result, tkn)
				status = Default
				readPos--
			}

		case Char:
			if c == '\\' {
				status = CharEscape
			} else if c == '\r' || c == '\n' {
				return nil, fmt.Errorf("E0104 newline in char literal at %s:%d.%d", filename, line, col)
			} else if c == '\'' {
				if len(buffer) == 0 {
					return nil, fmt.Errorf("E0105 empty char literal at %s:%d.%d", filename, line, col)
				}
				uni := ByteToUni(buffer)
				if uni == -1 {
					return nil, fmt.Errorf("E0106 invalid char literal at %s:%d.%d", filename, line, col)
				}
				var v Literal
				v.Init(int64(uni))
				result = append(result, Token{
					ObjType:  LIT_INT,
					Location: Loc{SrcID: sourceID, Line: line, Col: col},
					Text:     string(buffer),
					Value:    v,
				})
				status = Default
			} else {
				buffer = append(buffer, c)
			}

		case CharEscape:
			switch c {
			case '0':
				buffer = append(buffer, 0)
			case 'n':
				buffer = append(buffer, '\n')
			case 'r':
				buffer = append(buffer, '\r')
			case 't':
				buffer = append(buffer, '\t')
			case '\\':
				buffer = append(buffer, '\\')
			case '\'':
				buffer = append(buffer, '\'')
			case '"':
				buffer = append(buffer, '"')
			case 'x':
				var c0, c1 byte
				if readPos < len(src) {
					c0 = src[readPos]
					readPos++
				}
				if readPos < len(src) {
					c1 = src[readPos]
					readPos++
				}
				buffer = append(buffer, getHexEscape(c0, c1))
			default:
				return nil, fmt.Errorf("E0108 invalid char escape \\%c at %s:%d.%d", c, filename, line, col)
			}
			status = Char

		case String:
			if c == '\\' {
				status = StringEscape
			} else if c == '\r' || c == '\n' {
				return nil, fmt.Errorf("E0109 newline in string literal at %s:%d.%d", filename, line, col)
			} else if c == '"' {
				strVal := string(buffer)
				var v Literal
				v.Init(strVal)
				result = append(result, Token{
					ObjType:  LIT_STR,
					Location: Loc{SrcID: sourceID, Line: line, Col: col},
					Text:     strVal,
					Value:    v,
				})
				status = Default
			} else {
				buffer = append(buffer, c)
			}

		case StringEscape:
			// Same as char escape
			switch c {
			case '0':
				buffer = append(buffer, 0)
			case 'n':
				buffer = append(buffer, '\n')
			case 'r':
				buffer = append(buffer, '\r')
			case 't':
				buffer = append(buffer, '\t')
			case '\\':
				buffer = append(buffer, '\\')
			case '\'':
				buffer = append(buffer, '\'')
			case '"':
				buffer = append(buffer, '"')
			case 'x':
				var c0, c1 byte
				if readPos < len(src) {
					c0 = src[readPos]
					readPos++
				}
				if readPos < len(src) {
					c1 = src[readPos]
					readPos++
				}
				buffer = append(buffer, getHexEscape(c0, c1))
			default:
				return nil, fmt.Errorf("E0111 invalid string escape \\%c at %s:%d.%d", c, filename, line, col)
			}
			status = String

		case RawString:
			if c == '`' {
				strVal := string(buffer)
				var v Literal
				v.Init(strVal)
				result = append(result, Token{
					ObjType:  LIT_STR,
					Location: Loc{SrcID: sourceID, Line: line, Col: col},
					Text:     strVal,
					Value:    v,
				})
				status = Default
			} else {
				buffer = append(buffer, c)
				if c == '\r' {
					line++
					col = 1
					if readPos < len(src) && src[readPos] == '\n' {
						readPos++
					}
				} else if c == '\n' {
					line++
					col = 1
				}
			}
		}

		if readPos > len(src) {
			break
		}
	}
	if status != Default {
		return nil, fmt.Errorf("E0113 source not completed at %s:%d.%d", filename, line, col)
	}
	return result, nil
}

// tokenprovider helps traversing the token list
type TokenProvider struct {
	Tokens  []Token
	Pos     int
	NullTkn Token
}

func (tp *TokenProvider) Init(data []Token) {
	tp.Tokens = data
	tp.Pos = 0
	if len(data) > 0 {
		tp.NullTkn.Location.SrcID = data[0].Location.SrcID
	}
}

func (tp *TokenProvider) CanPop(num int) bool {
	return tp.Pos+num-1 < len(tp.Tokens)
}

func (tp *TokenProvider) Pop() Token {
	if tp.Pos >= len(tp.Tokens) {
		return tp.NullTkn
	}
	tkn := tp.Tokens[tp.Pos]
	tp.Pos++
	return tkn
}

func (tp *TokenProvider) Seek() Token {
	if tp.Pos >= len(tp.Tokens) {
		return tp.NullTkn
	}
	return tp.Tokens[tp.Pos]
}

func (tp *TokenProvider) Rewind(n int) {
	if tp.Pos > n-1 {
		tp.Pos -= n
	}
}

func (tp *TokenProvider) Match(types []TokenType) bool {
	if !tp.CanPop(len(types)) {
		return false
	}
	for i, t := range types {
		if t == PRECOMPILE { // PRECOMPILE is generic token type
			continue
		}
		if tp.Tokens[tp.Pos+i].ObjType != t {
			return false
		}
	}
	return true
}
