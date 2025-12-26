package middle

import (
	"fmt"

	"../front"
)

// return pratt precedence, -1 if not operator
func getPrattPrecedence(tknType front.TokenType, isUnary bool) int {
	if isUnary {
		switch tknType {
		case front.OP_ADD, front.OP_SUB,
			front.OP_LOGIC_NOT, front.OP_BIT_NOT,
			front.OP_MUL, front.OP_BIT_AND: // * (deref), & (addr)
			return 15
		case front.OP_INC, front.OP_DEC:
			return 20
		default:
			return -1
		}
	} else {
		switch tknType {
		case front.OP_DOT, front.OP_LPAREN, front.OP_LBRACKET:
			return 20
		case front.OP_MUL, front.OP_DIV, front.OP_MOD:
			return 11
		case front.OP_ADD, front.OP_SUB:
			return 10
		case front.OP_BIT_LSHIFT, front.OP_BIT_RSHIFT:
			return 9
		case front.OP_LT, front.OP_LT_EQ, front.OP_GT, front.OP_GT_EQ:
			return 8
		case front.OP_EQ, front.OP_NEQ:
			return 7
		case front.OP_BIT_AND:
			return 6
		case front.OP_BIT_XOR:
			return 5
		case front.OP_BIT_OR:
			return 4
		case front.OP_LOGIC_AND:
			return 3
		case front.OP_LOGIC_OR:
			return 2
		case front.OP_COND:
			return 1
		default:
			return -1
		}
	}
}

// convert TokenType to A1ExprOpT
func getBinaryOpType(tknType front.TokenType) A1ExprOpT {
	switch tknType {
	case front.OP_MUL:
		return B1_Mul
	case front.OP_DIV:
		return B1_Div
	case front.OP_MOD:
		return B1_Mod
	case front.OP_ADD:
		return B1_Add
	case front.OP_SUB:
		return B1_Sub
	case front.OP_BIT_LSHIFT:
		return B1_Shl
	case front.OP_BIT_RSHIFT:
		return B1_Shr
	case front.OP_LT:
		return B1_Lt
	case front.OP_GT:
		return B1_Gt
	case front.OP_LT_EQ:
		return B1_Le
	case front.OP_GT_EQ:
		return B1_Ge
	case front.OP_EQ:
		return B1_Eq
	case front.OP_NEQ:
		return B1_Ne
	case front.OP_BIT_AND:
		return B1_BitAnd
	case front.OP_BIT_XOR:
		return B1_BitXor
	case front.OP_BIT_OR:
		return B1_BitOr
	case front.OP_LOGIC_AND:
		return B1_LogicAnd
	case front.OP_LOGIC_OR:
		return B1_LogicOr
	case front.OP_COND:
		return C1_Cond
	case front.OP_INC:
		return U1_Inc
	case front.OP_DEC:
		return U1_Dec
	default:
		return -1
	}
}

// get number of operand
func getOperandNum(op A1ExprOpT) int {
	switch op {
	case -1:
		return 0
	case C1_Cond, C1_Slice:
		return 3
	case U1_Plus, U1_Minus, U1_LogicNot, U1_BitNot, U1_Inc, U1_Dec,
		U1_Ref, U1_Deref, U1_Sizeof, U1_Len, U1_Move:
		return 1
	default:
		return 2
	}
}

// convert token to A1StatT
func getAssignType(tkn front.Token) A1StatT {
	switch tkn.ObjType {
	case front.OP_ASSIGN:
		return S1_Assign
	case front.OP_ASSIGN_ADD:
		return S1_AssignAdd
	case front.OP_ASSIGN_SUB:
		return S1_AssignSub
	case front.OP_ASSIGN_MUL:
		return S1_AssignMul
	case front.OP_ASSIGN_DIV:
		return S1_AssignDiv
	case front.OP_ASSIGN_MOD:
		return S1_AssignMod
	default:
		return -1
	}
}

// parse type
func (m *A1Module) parseType(tp front.TokenProvider, cur *A1StatScope, arch int) (*A1Type, error) {
	// parse base type
	var base A1Type
	if tp.Match([]front.TokenType{front.ID, front.OP_DOT, front.ID}) { // foreign
		incNm := tp.Pop()
		tp.Pop()
		tgtNm := tp.Pop()
		decl := m.FindDecl(incNm.Text, false)
		if decl == nil || (*decl).GetObjType() != D1_Include {
			return nil, fmt.Errorf("E0201 include %s not found at %s:%d.%d", incNm.Text, m.Path, incNm.Location.Line, incNm.Location.Col)
		}
		base.Init(T1_Foreign, incNm.Location, tgtNm.Text, incNm.Text, m.Uname)

	} else if tp.Match([]front.TokenType{front.ID}) { // typedef, template, struct, enum
		tgtNm := tp.Pop()
		decl := m.FindDecl(tgtNm.Text, false)
		if decl == nil || (*decl).GetObjType() != D1_Typedef { // template, struct, enum -> name
			base.Init(T1_Name, tgtNm.Location, tgtNm.Text, "", m.Uname)
		} else { // typedef -> replace
			base = (*decl).(*A1DeclTypedef).Type.Clone()
		}

	} else if tp.CanPop(1) { // auto, primitive
		tpNm := tp.Pop()
		if tpNm.ObjType == front.KEY_AUTO {
			base.Init(T1_Auto, tpNm.Location, "auto", "", "")
			return &base, nil
		} else {
			base.Init(T1_Primitive, tpNm.Location, tpNm.Text, "", "")
		}
		switch tpNm.Text {
		case "int", "uint":
			base.Size = arch
			base.Align = arch
		case "i8", "u8", "bool":
			base.Size = 1
			base.Align = 1
		case "i16", "u16":
			base.Size = 2
			base.Align = 2
		case "i32", "u32", "f32":
			base.Size = 4
			base.Align = 4
		case "i64", "u64", "f64":
			base.Size = 8
			base.Align = 8
		case "void":
			base.Size = 0
			base.Align = 1
		default:
			return nil, fmt.Errorf("E0202 unknown type %s at %s:%d.%d", tpNm.Text, m.Path, tpNm.Location.Line, tpNm.Location.Col)
		}
	} else {
		return nil, fmt.Errorf("E0203 unexpected EOF at %s", m.Path)
	}

	// parse modifiers
	res := &base
	for tp.CanPop(1) {
		tkn := tp.Pop()
		switch tkn.ObjType {
		case front.OP_MUL: // pointer
			var ptr A1Type
			ptr.Init(T1_Ptr, tkn.Location, "*", "", "")
			ptr.Direct = res
			ptr.Size = arch
			ptr.Align = arch
			res = &ptr

		case front.OP_LBRACKET: // array, slice
			if res.Size == 0 {
				return nil, fmt.Errorf("E0204 cannot create void array/slice at %s:%d.%d", m.Path, tkn.Location.Line, tkn.Location.Col) // void array is not allowed
			}
			var arr A1Type
			if tp.Match([]front.TokenType{front.OP_RBRACKET}) { // slice
				tp.Pop()
				arr.Init(T1_Slice, tkn.Location, "[]", "", "")
				arr.Size = arch * 2
				arr.Align = arch

			} else if tp.Match([]front.TokenType{front.LIT_INT, front.OP_RBRACKET}) { // array
				lenTkn := tp.Pop()
				tp.Pop()
				l := lenTkn.Value.Value.(int64)
				if l <= 0 {
					return nil, fmt.Errorf("E0205 negative array length at %s:%d.%d", m.Path, lenTkn.Location.Line, lenTkn.Location.Col)
				}
				arr.Init(T1_Arr, tkn.Location, "[N]", "", "")
				arr.ArrLen = int(l)
				if res.Size > 0 {
					arr.Size = res.Size * arr.ArrLen
					arr.Align = res.Align
				}

			} else if tp.Match([]front.TokenType{front.ID, front.OP_RBRACKET}) { // array (defined size)
				lenTkn := tp.Pop()
				tp.Pop()
				lit := cur.FindLiteral(lenTkn.Text)
				if lit == nil || lit.ObjType != front.LitInt {
					return nil, fmt.Errorf("E0206 cannot find literal %s at %s:%d.%d", lenTkn.Text, m.Path, lenTkn.Location.Line, lenTkn.Location.Col)
				}
				l := lit.Value.(int64)
				if l <= 0 {
					return nil, fmt.Errorf("E0207 negative array length at %s:%d.%d", m.Path, lenTkn.Location.Line, lenTkn.Location.Col)
				}
				arr.Init(T1_Arr, tkn.Location, "[N]", "", "")
				arr.ArrLen = int(l)
				if res.Size > 0 {
					arr.Size = res.Size * arr.ArrLen
					arr.Align = res.Align
				}
			} else {
				return nil, fmt.Errorf("E0208 expected ']' at %s:%d.%d", m.Path, tkn.Location.Line, tkn.Location.Col)
			}

			// insert if nested array
			if res.ObjType == T1_Arr || res.ObjType == T1_Slice {
				t := res
				for t.ObjType == T1_Arr || t.ObjType == T1_Slice {
					t = t.Direct
				}
				arr.Direct = t
				t.Direct = &arr
			} else {
				arr.Direct = res
				res = &arr
			}

		case front.OP_LPAREN: // function
			var f A1Type
			f.Init(T1_Func, tkn.Location, "()", "", "")
			f.Size = arch
			f.Align = arch
			if tp.Seek().ObjType != front.OP_RPAREN {
				for tp.CanPop(1) {
					t, err := m.parseType(tp, cur, arch)
					if err != nil {
						return nil, err
					}
					f.Indirect = append(f.Indirect, *t)
					if tp.Seek().ObjType == front.OP_COMMA {
						tp.Pop()
					} else {
						break
					}
				}
			}
			if tp.Pop().ObjType != front.OP_RPAREN {
				return nil, fmt.Errorf("E0209 expected ')' at %s:%d.%d", m.Path, tkn.Location.Line, tkn.Location.Col)
			}
			f.Direct = res
			res = &f

		default: // type end
			tp.Rewind(1)
			return res, nil
		}
	}
	return res, nil
}

// check if type start
func (a1 *A1Parser) isTypeStart(tp front.TokenProvider, m *A1Module) bool {
	if front.IsPrimitive(tp.Seek().ObjType) { // primitive
		return true
	}
	if tp.Seek().ObjType == front.KEY_AUTO { // auto
		return true
	}

	if tp.Match([]front.TokenType{front.ID, front.OP_DOT, front.ID}) { // foreign
		incNm := tp.Pop()
		tp.Pop()
		tgtNm := tp.Pop()
		next := tp.Pop()
		tp.Rewind(4)
		decl := m.FindDecl(incNm.Text, false)
		if decl == nil || (*decl).GetObjType() != D1_Include {
			return false
		}
		pos := a1.FindModule((*decl).(*A1DeclInclude).TgtPath)
		if pos < 0 {
			return false
		}
		decl = a1.Modules[pos].FindDecl(tgtNm.Text, true)
		if decl == nil {
			return false
		}
		if ((*decl).GetObjType() == D1_Struct || (*decl).GetObjType() == D1_Enum || (*decl).GetObjType() == D1_Typedef) && next.ObjType != front.OP_DOT {
			return true // struct.member, enum.member is not type
		}

	} else if tp.Match([]front.TokenType{front.ID}) { // typedef, template, struct, enum
		tgtNm := tp.Pop()
		next := tp.Pop()
		tp.Rewind(2)
		decl := m.FindDecl(tgtNm.Text, false)
		if decl == nil {
			return false
		}
		if ((*decl).GetObjType() == D1_Struct || (*decl).GetObjType() == D1_Enum || (*decl).GetObjType() == D1_Typedef || (*decl).GetObjType() == D1_Template) && next.ObjType != front.OP_DOT {
			return true // struct.member, enum.member is not type
		}
	}
	return false
}
