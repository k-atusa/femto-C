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
func getUnaryOpType(tknType front.TokenType) A1ExprOpT {
	switch tknType {
	case front.OP_ADD:
		return U1_Plus
	case front.OP_SUB:
		return U1_Minus
	case front.OP_LOGIC_NOT:
		return U1_LogicNot
	case front.OP_BIT_NOT:
		return U1_BitNot
	case front.OP_INC:
		return U1_Inc
	case front.OP_DEC:
		return U1_Dec
	case front.OP_MUL:
		return U1_Deref
	case front.OP_BIT_AND:
		return U1_Ref
	default:
		return -1
	}
}

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
func (m *A1Module) parseType(tp *front.TokenProvider, cur *A1StatScope, arch int) (*A1Type, error) {
	// parse base type
	var base A1Type
	if tp.Match([]front.TokenType{front.ID, front.OP_DOT, front.ID}) { // foreign
		incNm := tp.Pop()
		tp.Pop()
		tgtNm := tp.Pop()
		decl := m.FindDecl(incNm.Text, false)
		if decl == nil || decl.GetObjType() != D1_Include {
			return nil, fmt.Errorf("E0201 include %s not found at %s:%d.%d", incNm.Text, m.Path, incNm.Location.Line, incNm.Location.Col)
		}
		base.Init(T1_Foreign, incNm.Location, tgtNm.Text, incNm.Text, m.Uname)

	} else if tp.Match([]front.TokenType{front.ID}) { // typedef, template, struct, enum
		tgtNm := tp.Pop()
		decl := m.FindDecl(tgtNm.Text, false)
		if decl == nil || decl.GetObjType() != D1_Typedef { // template, struct, enum -> name
			base.Init(T1_Name, tgtNm.Location, tgtNm.Text, "", m.Uname)
		} else { // typedef -> replace
			base = *decl.(*A1DeclTypedef).Type
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
func (a1 *A1Parser) isTypeStart(tp *front.TokenProvider, m *A1Module) bool {
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
		if decl == nil || decl.GetObjType() != D1_Include {
			return false
		}
		pos := a1.FindModule(decl.(*A1DeclInclude).TgtPath)
		if pos < 0 {
			return false
		}
		decl = a1.Modules[pos].FindDecl(tgtNm.Text, true)
		if decl == nil {
			return false
		}
		if (decl.GetObjType() == D1_Struct || decl.GetObjType() == D1_Enum || decl.GetObjType() == D1_Typedef) && next.ObjType != front.OP_DOT {
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
		if (decl.GetObjType() == D1_Struct || decl.GetObjType() == D1_Enum || decl.GetObjType() == D1_Typedef || decl.GetObjType() == D1_Template) && next.ObjType != front.OP_DOT {
			return true // struct.member, enum.member is not type
		}
	}
	return false
}

// fold node to literal
func (a1 *A1Parser) foldNode(tgt A1Expr, m *A1Module, cur *A1StatScope) *front.Literal {
	if tgt == nil {
		return nil
	}
	if tgt.GetObjType() == E1_Literal { // literal node
		return &tgt.(*A1ExprLiteral).Value
	}
	if tgt.GetObjType() == E1_Name { // defined literal
		return cur.FindLiteral(tgt.(*A1ExprName).Name)
	}
	if tgt.GetObjType() != E1_Op {
		return nil
	}

	// fold operands first
	op := tgt.(*A1ExprOp)
	o0 := a1.foldNode(op.Operand0, m, cur)
	o1 := a1.foldNode(op.Operand1, m, cur)
	o2 := a1.foldNode(op.Operand2, m, cur)
	if o0 != nil {
		var l A1ExprLiteral
		l.Init(op.Loc, *o0)
		op.Operand0 = &l
	}
	if o1 != nil {
		var l A1ExprLiteral
		l.Init(op.Loc, *o1)
		op.Operand1 = &l
	}
	if o2 != nil {
		var l A1ExprLiteral
		l.Init(op.Loc, *o2)
		op.Operand2 = &l
	}

	// fold operation
	var res front.Literal
	switch op.SubType {
	case U1_Plus:
		if o0 != nil {
			if o0.ObjType == front.LitInt || o0.ObjType == front.LitFloat {
				return o0
			}
		}
	case U1_Minus:
		if o0 != nil {
			if o0.ObjType == front.LitInt {
				res.Init(-o0.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(-o0.Value.(float64))
				return &res
			}
		}
	case U1_LogicNot:
		if o0 != nil && o0.ObjType == front.LitBool {
			res.Init(!o0.Value.(bool))
			return &res
		}
	case U1_BitNot:
		if o0 != nil && o0.ObjType == front.LitInt {
			res.Init(^o0.Value.(int64))
			return &res
		}
	case B1_Mul:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) * o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) * o1.Value.(float64))
				return &res
			}
		}
	case B1_Div:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				if o1.Value.(int64) == 0 {
					a1.Logger.Log(fmt.Sprintf("E0210 division by zero at %s", a1.Logger.GetLoc(op.Loc)), 5, true)
					return nil
				}
				res.Init(o0.Value.(int64) / o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				if o1.Value.(float64) == 0.0 {
					a1.Logger.Log(fmt.Sprintf("E0211 division by zero at %s", a1.Logger.GetLoc(op.Loc)), 5, true)
					return nil
				}
				res.Init(o0.Value.(float64) / o1.Value.(float64))
				return &res
			}
		}
	case B1_Mod:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				if o1.Value.(int64) == 0 {
					a1.Logger.Log(fmt.Sprintf("E0212 division by zero at %s", a1.Logger.GetLoc(op.Loc)), 5, true)
					return nil
				}
				res.Init(o0.Value.(int64) % o1.Value.(int64))
				return &res
			}
		}
	case B1_Add:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) + o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) + o1.Value.(float64))
				return &res
			}
		}
	case B1_Sub:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) - o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) - o1.Value.(float64))
				return &res
			}
		}
	case B1_Shl:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				sh := o1.Value.(int64)
				if sh < 0 || sh > 63 {
					a1.Logger.Log(fmt.Sprintf("E0213 shift(%d) overflow at %s", sh, a1.Logger.GetLoc(op.Loc)), 5, true)
					return nil
				}
				res.Init(o0.Value.(int64) << sh)
				return &res
			}
		}
	case B1_Shr:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				sh := o1.Value.(int64)
				if sh < 0 || sh > 63 {
					a1.Logger.Log(fmt.Sprintf("E0214 shift(%d) overflow at %s", sh, a1.Logger.GetLoc(op.Loc)), 5, true)
					return nil
				}
				res.Init(o0.Value.(int64) >> sh)
				return &res
			}
		}
	case B1_Lt:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) < o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) < o1.Value.(float64))
				return &res
			}
		}
	case B1_Le:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) <= o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) <= o1.Value.(float64))
				return &res
			}
		}
	case B1_Gt:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) > o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) > o1.Value.(float64))
				return &res
			}
		}
	case B1_Ge:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) >= o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) >= o1.Value.(float64))
				return &res
			}
		}
	case B1_Eq:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) == o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) == o1.Value.(float64))
				return &res
			} else if o0.ObjType == front.LitBool {
				res.Init(o0.Value.(bool) == o1.Value.(bool))
				return &res
			} else if o0.ObjType == front.LitNptr {
				res.Init(true)
				return &res
			}
		}
	case B1_Ne:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) != o1.Value.(int64))
				return &res
			} else if o0.ObjType == front.LitFloat {
				res.Init(o0.Value.(float64) != o1.Value.(float64))
				return &res
			} else if o0.ObjType == front.LitBool {
				res.Init(o0.Value.(bool) != o1.Value.(bool))
				return &res
			} else if o0.ObjType == front.LitNptr {
				res.Init(false)
				return &res
			}
		}
	case B1_BitAnd:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) & o1.Value.(int64))
				return &res
			}
		}
	case B1_BitXor:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) ^ o1.Value.(int64))
				return &res
			}
		}
	case B1_BitOr:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitInt {
				res.Init(o0.Value.(int64) | o1.Value.(int64))
				return &res
			}
		}
	case B1_LogicAnd:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitBool {
				res.Init(o0.Value.(bool) && o1.Value.(bool))
				return &res
			}
		}
	case B1_LogicOr:
		if o0 != nil && o1 != nil && o0.ObjType == o1.ObjType {
			if o0.ObjType == front.LitBool {
				res.Init(o0.Value.(bool) || o1.Value.(bool))
				return &res
			}
		}
	case C1_Cond:
		if o0 != nil && o1 != nil && o2 != nil && o0.ObjType == front.LitBool && o1.ObjType == o2.ObjType {
			if o1.Value.(bool) {
				res = *o1
			} else {
				res = *o2
			}
			return &res
		}
	case U1_Sizeof:
		if o0 != nil {
			switch o0.ObjType {
			case front.LitInt, front.LitFloat:
				res.Init(int64(8))
			case front.LitBool:
				res.Init(int64(1))
			case front.LitString:
				res.Init(int64(a1.Arch * 2))
			case front.LitNptr:
				res.Init(int64(a1.Arch))
			}
			return &res
		} else if op.TypeOperand != nil {
			switch op.TypeOperand.ObjType {
			case T1_Primitive:
				res.Init(int64(op.TypeOperand.Size))
			case T1_Ptr, T1_Func:
				res.Init(int64(a1.Arch))
			case T1_Arr:
				if op.TypeOperand.Size > 0 {
					res.Init(int64(op.TypeOperand.Size))
				} else {
					return nil
				}
			case T1_Slice:
				res.Init(int64(a1.Arch * 2))
			default:
				return nil
			}
			return &res
		}
	case B1_Dot: // enum.member, inc.name, inc.enum.member
		if op.Operand0.GetObjType() == E1_Name {
			name0 := op.Operand0.(*A1ExprName).Name
			decl := m.FindDecl(name0, false)
			if decl == nil {
				return nil
			}
			if decl.GetObjType() == D1_Enum && op.Operand1.GetObjType() == E1_Name { // enum.member
				name1 := op.Operand1.(*A1ExprName).Name
				return m.FindLiteral(name0+"."+name1, false)
			}
			if decl.GetObjType() == D1_Include {
				pos := a1.FindModule(decl.(*A1DeclInclude).TgtPath)
				if pos < 0 {
					return nil
				}
				if op.Operand1.GetObjType() == E1_Name { // inc.name
					name1 := op.Operand1.(*A1ExprName).Name
					return a1.Modules[pos].FindLiteral(name1, true)

				} else if op.Operand1.GetObjType() == E1_Op { // inc.enum.member
					o := op.Operand1.(*A1ExprOp)
					if o.SubType == B1_Dot && o.Operand0.GetObjType() == E1_Name && o.Operand1.GetObjType() == E1_Name {
						name1 := o.Operand0.(*A1ExprName).Name
						name2 := o.Operand1.(*A1ExprName).Name
						return a1.Modules[pos].FindLiteral(name1+"."+name2, true)
					}
				}
			}
		}
	case B1_Index, C1_Slice, U1_Ref, U1_Deref, U1_Inc, U1_Dec, B1_Cast, B1_Make, U1_Len, U1_Move: // not foldable operator
		return nil
	default:
		return nil
	}
	return nil
}

// expression parser
func (a1 *A1Parser) parseAtomicExpr(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) A1Expr {
	tkn := tp.Pop()
	switch tkn.ObjType {
	case front.LIT_INT, front.LIT_FLOAT, front.LIT_STR: // literal
		var res A1ExprLiteral
		res.Init(tkn.Location, tkn.Value)
		return &res

	case front.KEY_TRUE, front.KEY_FALSE, front.KEY_NULL: // keyword literal
		var res A1ExprLiteral
		var l front.Literal
		if tkn.ObjType == front.KEY_TRUE {
			l.Init(true)
		} else if tkn.ObjType == front.KEY_FALSE {
			l.Init(false)
		} else {
			l.Init(nil)
		}
		res.Init(tkn.Location, l)
		return &res

	case front.ID: // name
		var res A1ExprName
		res.Init(tkn.Location, tkn.Text)
		return &res

	case front.OP_LPAREN: // '(' expression
		res := a1.parsePrattExpr(tp, m, cur, 0)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0301 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		return res

	case front.OP_LBRACE: // '{' data literal
		var res A1ExprLitData
		res.Init(tkn.Location)
		for tp.CanPop(1) {
			res.Elements = append(res.Elements, a1.parseExpr(tp, m, cur))
			tkn = tp.Seek()
			if tkn.ObjType == front.OP_COMMA {
				tp.Pop()
				if tp.Seek().ObjType == front.OP_RBRACE {
					break
				}
			} else if tkn.ObjType == front.OP_RBRACE {
				break
			} else {
				a1.Logger.Log(fmt.Sprintf("E0302 expected '}', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
		}
		return &res

	case front.OP_ADD, front.OP_SUB, front.OP_BIT_NOT, front.OP_LOGIC_NOT, front.OP_MUL, front.OP_BIT_AND: // unary operator
		var res A1ExprOp
		res.Init(tkn.Location, getUnaryOpType(tkn.ObjType))
		res.Operand0 = a1.parsePrattExpr(tp, m, cur, getPrattPrecedence(tkn.ObjType, true))
		return &res

	case front.IFUNC_SIZEOF:
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LPAREN {
			a1.Logger.Log(fmt.Sprintf("E0303 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		var res A1ExprOp
		res.Init(tkn.Location, U1_Sizeof)
		if a1.isTypeStart(tp, m) {
			var err error
			res.TypeOperand, err = m.parseType(tp, cur, a1.Arch)
			if err != nil {
				a1.Logger.Log(err.Error(), 5, true)
			}
		} else {
			res.Operand0 = a1.parsePrattExpr(tp, m, cur, 0)
		}
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0304 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		return &res

	case front.IFUNC_CAST:
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LT {
			a1.Logger.Log(fmt.Sprintf("E0305 expected '<', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		var res A1ExprOp
		res.Init(tkn.Location, B1_Cast)
		var err error
		res.TypeOperand, err = m.parseType(tp, cur, a1.Arch)
		if err != nil {
			a1.Logger.Log(err.Error(), 5, true)
		}
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_GT {
			a1.Logger.Log(fmt.Sprintf("E0306 expected '>', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LPAREN {
			a1.Logger.Log(fmt.Sprintf("E0307 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		res.Operand0 = a1.parsePrattExpr(tp, m, cur, 0)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0308 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		return &res

	case front.IFUNC_MAKE:
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LPAREN {
			a1.Logger.Log(fmt.Sprintf("E0309 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		var res A1ExprOp
		res.Init(tkn.Location, B1_Make)
		res.Operand0 = a1.parsePrattExpr(tp, m, cur, 0)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_COMMA {
			a1.Logger.Log(fmt.Sprintf("E0310 expected ',', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		res.Operand1 = a1.parsePrattExpr(tp, m, cur, 0)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0311 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		return &res

	case front.IFUNC_LEN, front.IFUNC_MOVE:
		obj := tkn.ObjType
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LPAREN {
			a1.Logger.Log(fmt.Sprintf("E0312 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		var res A1ExprOp
		if obj == front.IFUNC_LEN {
			res.Init(tkn.Location, U1_Len)
		} else {
			res.Init(tkn.Location, U1_Move)
		}
		res.Operand0 = a1.parsePrattExpr(tp, m, cur, 0)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0313 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		return &res

	default:
		a1.Logger.Log(fmt.Sprintf("E0314 invalid atomic expr start %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	return nil
}

func (a1 *A1Parser) parsePrattExpr(tp *front.TokenProvider, m *A1Module, cur *A1StatScope, level int) A1Expr {
	lhs := a1.parseAtomicExpr(tp, m, cur)
	if lhs == nil {
		return nil
	}
	for tp.CanPop(1) {
		curLevel := getPrattPrecedence(tp.Seek().ObjType, false)
		if curLevel < level {
			break // higher precedence, end of expression
		}

		op := tp.Pop() // operator can be binary/cubic or postfix unary
		switch op.ObjType {
		case front.OP_DOT: // dot operator
			tkn := tp.Pop()
			if tkn.ObjType != front.ID {
				a1.Logger.Log(fmt.Sprintf("E0315 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			var nm A1ExprName
			nm.Init(tkn.Location, tkn.Text)
			var res A1ExprOp
			res.Init(op.Location, B1_Dot)
			res.Operand0 = lhs
			res.Operand1 = &nm
			lhs = &res

		case front.OP_LPAREN: // func call
			var res A1ExprFCall
			res.Init(op.Location, lhs)
			if tp.Seek().ObjType != front.OP_RPAREN {
				for tp.CanPop(1) {
					res.Args = append(res.Args, a1.parsePrattExpr(tp, m, cur, 0))
					tkn := tp.Seek()
					if tkn.ObjType == front.OP_COMMA {
						tp.Pop()
					} else if tkn.ObjType == front.OP_RPAREN {
						break
					} else {
						a1.Logger.Log(fmt.Sprintf("E0316 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
					}
				}
			}
			tkn := tp.Pop()
			if tkn.ObjType != front.OP_RPAREN {
				a1.Logger.Log(fmt.Sprintf("E0317 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			lhs = &res

		case front.OP_LBRACKET: // indexing, slicing
			isIdx := true
			var left A1Expr = nil
			var right A1Expr = nil
			if tp.Seek().ObjType != front.OP_COLON {
				left = a1.parsePrattExpr(tp, m, cur, 0)
			}
			if tp.Seek().ObjType == front.OP_COLON { // slicing
				isIdx = false
				tp.Pop()
				if tp.Seek().ObjType != front.OP_RBRACKET {
					right = a1.parsePrattExpr(tp, m, cur, 0)
				}
			}
			tkn := tp.Pop()
			if tkn.ObjType != front.OP_RBRACKET {
				a1.Logger.Log(fmt.Sprintf("E0318 expected ']', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			var res A1ExprOp
			if isIdx {
				res.Init(op.Location, B1_Index)
				res.Operand0 = lhs
				res.Operand1 = left
			} else {
				res.Init(op.Location, C1_Slice)
				res.Operand0 = lhs
				res.Operand1 = left
				res.Operand2 = right
			}
			lhs = &res

		case front.OP_COND: // conditional operator
			var res A1ExprOp
			res.Init(op.Location, C1_Cond)
			res.Operand0 = lhs
			res.Operand1 = a1.parsePrattExpr(tp, m, cur, 0)
			tkn := tp.Pop()
			if tkn.ObjType != front.OP_COLON {
				a1.Logger.Log(fmt.Sprintf("E0319 expected ':', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			res.Operand2 = a1.parsePrattExpr(tp, m, cur, 0)
			lhs = &res

		default: // binary op
			var res A1ExprOp
			res.Init(op.Location, getBinaryOpType(op.ObjType))
			res.Operand0 = lhs
			res.Operand1 = a1.parsePrattExpr(tp, m, cur, 0)
			lhs = &res
		}
	}
	return lhs
}

func (a1 *A1Parser) parseExpr(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) A1Expr {
	res := a1.parsePrattExpr(tp, m, cur, 0)
	if res == nil {
		return nil
	}
	lit := a1.foldNode(res, m, cur)
	if lit == nil {
		return res
	} else {
		var l A1ExprLiteral
		l.Init(res.GetLocation(), *lit)
		return &l
	}
}

// raw, struct, enum, func, typedef
