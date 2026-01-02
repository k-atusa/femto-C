// test787d : femto-C middle.ast1 R0

package middle

import (
	"fmt"
	"slices"

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
		decl := cur.FindDecl(tgtNm.Text)
		if decl == nil { // name use before define
			base.Init(T1_Name, tgtNm.Location, tgtNm.Text, "", m.Uname)
		} else if decl.GetObjType() == D1_Typedef { // local typedef -> replace
			base = *decl.(*A1DeclTypedef).Type
		} else if decl.GetObjType() == D1_Template { // local template -> replace
			base = *decl.(*A1DeclTemplate).Type
		} else { // struct, enum -> name
			base.Init(T1_Name, tgtNm.Location, tgtNm.Text, "", m.Uname)
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
				for t.Direct != nil && (t.Direct.ObjType == T1_Arr || t.Direct.ObjType == T1_Slice) {
					t = t.Direct
				}
				arr.Direct = t.Direct
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
			tp.Rewind(1)
		}
		return res

	case front.OP_LBRACE: // '{' data literal
		var res A1ExprLitData
		res.Init(tkn.Location)
		for tp.CanPop(1) {
			if tp.Seek().ObjType == front.OP_RBRACE {
				break
			}
			res.Elements = append(res.Elements, a1.parseExpr(tp, m, cur))
			tkn = tp.Pop()
			if tkn.ObjType == front.OP_COMMA {
				continue
			} else if tkn.ObjType == front.OP_RBRACE {
				break
			} else {
				a1.Logger.Log(fmt.Sprintf("E0302 expected '}', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
				tp.Rewind(1)
			}
		}
		return &res

	case front.OP_ADD, front.OP_SUB, front.OP_BIT_NOT, front.OP_LOGIC_NOT, front.OP_MUL, front.OP_BIT_AND: // unary operator
		var res A1ExprOp
		res.Init(tkn.Location, getUnaryOpType(tkn.ObjType))
		res.Operand0 = a1.parsePrattExpr(tp, m, cur, getPrattPrecedence(tkn.ObjType, true))
		return &res

	case front.IFUNC_SIZEOF: // sizeof(expr), sizeof(type)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LPAREN {
			a1.Logger.Log(fmt.Sprintf("E0303 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
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
			tp.Rewind(1)
		}
		return &res

	case front.IFUNC_CAST: // cast<type>(expr)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LT {
			a1.Logger.Log(fmt.Sprintf("E0305 expected '<', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
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
			tp.Rewind(1)
		}
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LPAREN {
			a1.Logger.Log(fmt.Sprintf("E0307 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
		}
		res.Operand0 = a1.parsePrattExpr(tp, m, cur, 0)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0308 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
		}
		return &res

	case front.IFUNC_MAKE: // make(expr, expr)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LPAREN {
			a1.Logger.Log(fmt.Sprintf("E0309 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
		}
		var res A1ExprOp
		res.Init(tkn.Location, B1_Make)
		res.Operand0 = a1.parsePrattExpr(tp, m, cur, 0)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_COMMA {
			a1.Logger.Log(fmt.Sprintf("E0310 expected ',', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
		}
		res.Operand1 = a1.parsePrattExpr(tp, m, cur, 0)
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0311 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
		}
		return &res

	case front.IFUNC_LEN, front.IFUNC_MOVE: // len(expr), move(expr)
		obj := tkn.ObjType
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_LPAREN {
			a1.Logger.Log(fmt.Sprintf("E0312 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
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
			tp.Rewind(1)
		}
		return &res

	default:
		a1.Logger.Log(fmt.Sprintf("E0314 invalid atomic expr start %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	return nil
}

func (a1 *A1Parser) parsePrattExpr(tp *front.TokenProvider, m *A1Module, cur *A1StatScope, level int) A1Expr {
	lhs := a1.parseAtomicExpr(tp, m, cur) // LHS is left root of expression
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
		case front.OP_INC, front.OP_DEC: // postfix unary
			var res A1ExprOp
			if op.ObjType == front.OP_INC {
				res.Init(op.Location, U1_Inc)
			} else {
				res.Init(op.Location, U1_Dec)
			}
			res.Operand0 = lhs
			lhs = &res

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
						tp.Rewind(1)
						break
					}
				}
			}
			tkn := tp.Pop()
			if tkn.ObjType != front.OP_RPAREN {
				a1.Logger.Log(fmt.Sprintf("E0317 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
				tp.Rewind(1)
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
				tp.Rewind(1)
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
				tp.Rewind(1)
			}
			res.Operand2 = a1.parsePrattExpr(tp, m, cur, 0)
			lhs = &res

		default: // binary op
			var res A1ExprOp
			res.Init(op.Location, getBinaryOpType(op.ObjType))
			res.Operand0 = lhs
			res.Operand1 = a1.parsePrattExpr(tp, m, cur, level+1)
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

// toplevel declaration parser
func (a1 *A1Parser) parseToplevel(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) []A1Decl {
	isDefine := false
	isConst := false
	isVolatile := false
	isExtern := false
	isExported := false
	isVaArg := false
	for tp.CanPop(1) {
		tkn := tp.Seek()
		switch tkn.ObjType {
		case front.KEY_INCLUDE: // include
			tp.Pop()
			return []A1Decl{a1.parseInclude(tp, m, cur)}

		case front.KEY_TEMPLATE: // template
			tp.Pop()
			return a1.parseTemplate(tp, m, cur)

		case front.OP_SEMICOLON: // empty statement
			tp.Pop()
			return nil

		case front.KEY_STRUCT: // struct
			tp.Pop()
			return []A1Decl{a1.parseDeclStruct(tp, m, cur, isExported)}

		case front.KEY_ENUM: // enum
			tp.Pop()
			return []A1Decl{a1.parseDeclEnum(tp, m, cur, isExported)}

		// tags
		case front.KEY_DEFINE:
			tp.Pop()
			isDefine = true
		case front.KEY_CONST:
			tp.Pop()
			isConst = true
		case front.KEY_VOLATILE:
			tp.Pop()
			isVolatile = true
		case front.KEY_EXTERN:
			tp.Pop()
			isExtern = true
		case front.KEY_EXPORT:
			tp.Pop()
			isExported = true
		case front.KEY_VA_ARG:
			tp.Pop()
			isVaArg = true

		case front.KEY_RAW_C, front.KEY_RAW_IR: // raw code
			tkn = tp.Pop()
			code := tp.Pop()
			if code.ObjType != front.LIT_STR {
				a1.Logger.Log(fmt.Sprintf("E0401 expected string, got %s at %s", code.Text, a1.Logger.GetLoc(code.Location)), 5, true)
			}
			var res A1DeclRaw
			if tkn.ObjType == front.KEY_RAW_C {
				res.Init(D1_RawC, tkn.Location, code.Text)
			} else {
				res.Init(D1_RawIR, tkn.Location, code.Text)
			}
			return []A1Decl{&res}

		case front.KEY_TYPEDEF: // typedef
			tkn = tp.Pop()
			if tkn.ObjType != front.ID {
				a1.Logger.Log(fmt.Sprintf("E0402 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			if !m.IsNameUsable(tkn.Text) {
				a1.Logger.Log(fmt.Sprintf("E0403 name %s is not usable at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			t, err := m.parseType(tp, cur, a1.Arch)
			if err != nil {
				a1.Logger.Log(err.Error(), 5, true)
			}
			var res A1DeclTypedef
			res.Init(tkn.Location, tkn.Text, t, isExported)
			cur.Decls[tkn.Text] = &res
			return []A1Decl{&res}

		default:
			t, err := m.parseType(tp, cur, a1.Arch)
			if err != nil {
				a1.Logger.Log(err.Error(), 5, true)
			}
			if tp.Match([]front.TokenType{front.ID, front.OP_SEMICOLON}) || tp.Match([]front.TokenType{front.ID, front.OP_ASSIGN}) { // var declaration
				v := a1.parseDeclVar(tp, m, cur, t, isDefine, isConst, isVolatile, isExtern, isExported)
				if !m.IsNameUsable(v.Name) {
					a1.Logger.Log(fmt.Sprintf("E0404 name %s is not usable at %s", v.Name, a1.Logger.GetLoc(tkn.Location)), 5, true)
				}
				if v.InitExpr != nil && v.InitExpr.GetObjType() != E1_Literal {
					a1.Logger.Log(fmt.Sprintf("E0405 global var must be constexpr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
				}
				return []A1Decl{v}
			} else { // func declaration
				return []A1Decl{a1.parseDeclFunc(tp, m, cur, t, isVaArg, isExported)}
			}
		}
	}
	return nil
}

// statement parser
func (a1 *A1Parser) parseStat(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) A1Stat {
	isDefine := false
	isConst := false
	isVolatile := false
	isExtern := false
	for tp.CanPop(1) {
		tkn := tp.Seek()
		switch tkn.ObjType {
		// tags
		case front.KEY_DEFINE:
			tp.Pop()
			isDefine = true
		case front.KEY_CONST:
			tp.Pop()
			isConst = true
		case front.KEY_VOLATILE:
			tp.Pop()
			isVolatile = true
		case front.KEY_EXTERN:
			tp.Pop()
			isExtern = true

		// control flow
		case front.OP_LBRACE:
			return a1.parseStatScope(tp, m, cur)
		case front.KEY_IF:
			tp.Pop()
			return a1.parseStatIf(tp, m, cur)
		case front.KEY_WHILE:
			tp.Pop()
			return a1.parseStatWhile(tp, m, cur)
		case front.KEY_FOR:
			tp.Pop()
			return a1.parseStatFor(tp, m, cur)
		case front.KEY_SWITCH:
			tp.Pop()
			return a1.parseStatSwitch(tp, m, cur)

		case front.OP_SEMICOLON: // empty statement
			tp.Pop()
			return nil

		case front.KEY_RAW_C, front.KEY_RAW_IR: // raw code
			tkn = tp.Pop()
			code := tp.Pop()
			if code.ObjType != front.LIT_STR {
				a1.Logger.Log(fmt.Sprintf("E0406 expected string, got %s at %s", code.Text, a1.Logger.GetLoc(code.Location)), 5, true)
			}
			var res A1StatRaw
			if tkn.ObjType == front.KEY_RAW_C {
				res.Init(S1_RawC, tkn.Location, code.Text)
			} else {
				res.Init(S1_RawIR, tkn.Location, code.Text)
			}
			return &res

		case front.KEY_BREAK: // break
			tp.Pop()
			var res A1StatCtrl
			res.Init(S1_Break, tkn.Location, nil)
			return &res

		case front.KEY_CONTINUE: // continue
			tp.Pop()
			var res A1StatCtrl
			res.Init(S1_Continue, tkn.Location, nil)
			return &res

		case front.KEY_RETURN: // return
			tp.Pop()
			var expr A1Expr = nil
			if tp.Seek().ObjType != front.OP_SEMICOLON {
				expr = a1.parseExpr(tp, m, cur)
			}
			tkn := tp.Pop()
			if tkn.ObjType != front.OP_SEMICOLON {
				a1.Logger.Log(fmt.Sprintf("E0407 expected ';', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
				tp.Rewind(1)
			}
			var res A1StatCtrl
			res.Init(S1_Return, tkn.Location, expr)
			return &res

		case front.KEY_DEFER: // defer
			tp.Pop()
			var res A1StatCtrl
			res.Init(S1_Defer, tkn.Location, a1.parseExpr(tp, m, cur))
			tkn := tp.Pop()
			if tkn.ObjType != front.OP_SEMICOLON {
				a1.Logger.Log(fmt.Sprintf("E0408 expected ';', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
				tp.Rewind(1)
			}
			return &res

		case front.KEY_TYPEDEF: // typedef
			tkn = tp.Pop()
			if tkn.ObjType != front.ID {
				a1.Logger.Log(fmt.Sprintf("E0409 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			t, err := m.parseType(tp, cur, a1.Arch)
			if err != nil {
				a1.Logger.Log(err.Error(), 5, true)
			}
			var d A1DeclTypedef
			d.Init(tkn.Location, tkn.Text, t, false)
			var res A1StatDecl
			res.Init(tkn.Location, &d)
			cur.Decls[tkn.Text] = &d
			return &res

		default:
			if a1.isTypeStart(tp, m) { // var declaration
				t, err := m.parseType(tp, cur, a1.Arch)
				if err != nil {
					a1.Logger.Log(err.Error(), 5, true)
				}
				tkn := tp.Pop()
				var res A1StatDecl
				res.Init(tkn.Location, a1.parseDeclVar(tp, m, cur, t, isDefine, isConst, isVolatile, isExtern, false))
				return &res
			} else {
				left := a1.parseExpr(tp, m, cur)
				if left == nil {
					return nil
				}
				tkn = tp.Pop()
				if tkn.ObjType == front.OP_SEMICOLON { // expr statement
					var res A1StatExpr
					res.Init(tkn.Location, left)
					return &res
				} else { // assign statement
					return a1.parseStatAssign(tp, m, cur, left, tkn.ObjType, front.OP_SEMICOLON)
				}
			}
		}
	}
	return nil
}

// declaration specific case parser
func (a1 *A1Parser) parseDeclVar(tp *front.TokenProvider, m *A1Module, cur *A1StatScope, varType *A1Type, isDefine bool, isConst bool, isVolatile bool, isExtern bool, isExported bool) *A1DeclVar {
	defer a1.Logger.Log(fmt.Sprintf("parsed var decl at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	// check type, name
	if varType.Size == 0 {
		a1.Logger.Log(fmt.Sprintf("E0501 variable type cannot be void at %s", a1.Logger.GetLoc(tp.Seek().Location)), 5, true)
	}
	tkn := tp.Pop()
	if tkn.ObjType != front.ID {
		a1.Logger.Log(fmt.Sprintf("E0502 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	if _, ok := cur.Decls[tkn.Text]; ok {
		a1.Logger.Log(fmt.Sprintf("E0503 name %s is already defined inside scope at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}

	// get variable init expr
	var init A1Expr = nil
	tkn = tp.Pop()
	if tkn.ObjType == front.OP_ASSIGN {
		init = a1.parseExpr(tp, m, cur)
		tkn = tp.Pop()
	}
	if tkn.ObjType != front.OP_SEMICOLON {
		a1.Logger.Log(fmt.Sprintf("E0504 expected ';', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	var res A1DeclVar
	res.Init(tkn.Location, tkn.Text, varType, init, isExported)
	res.IsDefine = isDefine
	res.IsConst = isConst
	res.IsVolatile = isVolatile
	res.IsExtern = isExtern

	// check var tag, register to scope
	if isDefine && (init == nil || init.GetObjType() != E1_Literal) {
		a1.Logger.Log(fmt.Sprintf("E0505 define variable must be initialized with constexpr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	if isConst && init == nil {
		a1.Logger.Log(fmt.Sprintf("E0506 const variable must be initialized at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	if isExtern && init != nil {
		a1.Logger.Log(fmt.Sprintf("E0507 extern variable cannot be initialized at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	cur.Decls[tkn.Text] = &res
	return &res
}

func (a1 *A1Parser) parseDeclFunc(tp *front.TokenProvider, m *A1Module, cur *A1StatScope, retType *A1Type, isVaArg bool, isExported bool) *A1DeclFunc {
	defer a1.Logger.Log(fmt.Sprintf("parsed func decl at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	// parse name
	tkn := tp.Seek()
	var body A1StatScope
	body.Init(tkn.Location, cur)
	var res A1DeclFunc
	if tp.Match([]front.TokenType{front.ID, front.OP_DOT, front.ID}) { // method
		nm0 := tp.Pop().Text
		tp.Pop()
		nm1 := tp.Pop().Text
		res.Init(tkn.Location, nm0+"."+nm1, nm0, nm1, &body, isExported)
	} else if tp.Seek().ObjType == front.ID { // function
		nm := tp.Pop().Text
		res.Init(tkn.Location, nm, "", "", &body, isExported)
	} else {
		a1.Logger.Log(fmt.Sprintf("E0508 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	if !m.IsNameUsable(res.Name) {
		a1.Logger.Log(fmt.Sprintf("E0509 name %s is not usable at %s", res.Name, a1.Logger.GetLoc(res.Loc)), 5, true)
	}

	// parse params
	var ft A1Type
	ft.Init(T1_Func, tkn.Location, "()", "", "")
	ft.Direct = retType
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_LPAREN {
		a1.Logger.Log(fmt.Sprintf("E0510 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	if tp.Seek().ObjType != front.OP_RPAREN {
		for tp.CanPop(1) {
			// push to func type
			t, err := m.parseType(tp, cur, a1.Arch)
			if err != nil {
				a1.Logger.Log(err.Error(), 5, true)
			}
			tkn = tp.Pop()
			if tkn.ObjType != front.ID {
				a1.Logger.Log(fmt.Sprintf("E0511 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			res.Params = append(res.Params, tkn.Text)
			ft.Indirect = append(ft.Indirect, *t)

			// check param name
			if slices.Contains(res.Params, tkn.Text) {
				a1.Logger.Log(fmt.Sprintf("E0512 name %s is already used at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			}

			// push to func body
			var d A1DeclVar
			d.Init(tkn.Location, tkn.Text, t, nil, false)
			d.IsParam = true
			body.Decls[tkn.Text] = &d
			var s A1StatDecl
			s.Init(tkn.Location, &d)
			body.Body = append(body.Body, &s)
		}
	}
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_RPAREN {
		a1.Logger.Log(fmt.Sprintf("E0513 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	res.Type = &ft

	// parse body, check func
	body.Body = append(body.Body, a1.parseStatScope(tp, m, &body))
	if res.StructNm != "" { // method
		d := m.FindDecl(res.StructNm, false)
		if d == nil || d.GetObjType() != D1_Struct { // check struct exists
			a1.Logger.Log(fmt.Sprintf("E0514 method struct %s not exists at %s", res.StructNm, a1.Logger.GetLoc(res.Loc)), 5, true)
		}
		isValid := true // first arg of method is struct*
		if len(res.Type.Indirect) == 0 {
			isValid = false
		} else {
			isValid = res.Type.Indirect[0].Check([]A1TypeT{T1_Ptr, T1_Name})
		}
		if isValid {
			isValid = res.Type.Indirect[0].Direct.Name == res.StructNm
		}
		if !isValid {
			a1.Logger.Log(fmt.Sprintf("E0515 first arg of method must be %s* at %s", res.StructNm, a1.Logger.GetLoc(res.Loc)), 5, true)
		}
	}
	if isVaArg { // last arg is void*[] or void*[] int[]
		isVa := true
		isVa_ad := true
		if len(res.Type.Indirect) < 1 {
			isVa = false
		} else {
			isVa = res.Type.Indirect[len(res.Type.Indirect)-1].Check([]A1TypeT{T1_Slice, T1_Ptr, T1_Primitive})
			if isVa {
				isVa = res.Type.Indirect[len(res.Type.Indirect)-1].Direct.Direct.Name == "void"
			}
		}
		if len(res.Type.Indirect) < 2 {
			isVa_ad = false
		} else {
			isVa_ad = res.Type.Indirect[len(res.Type.Indirect)-2].Check([]A1TypeT{T1_Slice, T1_Ptr, T1_Primitive}) && res.Type.Indirect[len(res.Type.Indirect)-1].Check([]A1TypeT{T1_Slice, T1_Primitive})
			if isVa_ad {
				isVa_ad = res.Type.Indirect[len(res.Type.Indirect)-2].Direct.Direct.Name == "void" && res.Type.Indirect[len(res.Type.Indirect)-1].Direct.Name == "int"
			}
		}
		if !isVa && !isVa_ad {
			a1.Logger.Log(fmt.Sprintf("E0516 last arg of va_func must be void*[] at %s", a1.Logger.GetLoc(res.Loc)), 5, true)
		}
		res.IsVaArg = true
		res.IsVaArg_ad = isVa_ad
	}

	// register to scope
	cur.Decls[tkn.Text] = &res
	return &res
}

func (a1 *A1Parser) parseDeclStruct(tp *front.TokenProvider, m *A1Module, cur *A1StatScope, isExported bool) *A1DeclStruct {
	defer a1.Logger.Log(fmt.Sprintf("parsed struct decl at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	// parse name, {
	tkn := tp.Pop()
	if tkn.ObjType != front.ID {
		a1.Logger.Log(fmt.Sprintf("E0517 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	if !m.IsNameUsable(tkn.Text) {
		a1.Logger.Log(fmt.Sprintf("E0518 name %s is not usable at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	var res A1DeclStruct
	res.Init(tkn.Location, tkn.Text, isExported)
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_LBRACE {
		a1.Logger.Log(fmt.Sprintf("E0519 expected '{', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}

	// parse members, }
	for tp.CanPop(1) {
		t, err := m.parseType(tp, cur, a1.Arch)
		if err != nil {
			a1.Logger.Log(err.Error(), 5, true)
		}
		tkn = tp.Pop()
		if tkn.ObjType != front.ID {
			a1.Logger.Log(fmt.Sprintf("E0520 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		if slices.Contains(res.MemNames, tkn.Text) {
			a1.Logger.Log(fmt.Sprintf("E0521 duplicate member %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		res.MemTypes = append(res.MemTypes, *t)
		res.MemNames = append(res.MemNames, tkn.Text)
		res.MemOffsets = append(res.MemOffsets, -1)
		tkn = tp.Seek()
		if tkn.ObjType == front.OP_RBRACE {
			break
		} else if tkn.ObjType == front.OP_SEMICOLON || tkn.ObjType == front.OP_COMMA {
			tp.Pop()
			if tp.Seek().ObjType == front.OP_RBRACE {
				break
			}
		} else {
			a1.Logger.Log(fmt.Sprintf("E0522 expected ';', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
	}

	// finish parsing, register to scope
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_RBRACE {
		a1.Logger.Log(fmt.Sprintf("E0523 expected '}', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	var t A1Type
	t.Init(T1_Name, tkn.Location, res.Name, "", m.Uname)
	res.Type = &t
	cur.Decls[res.Name] = &res
	return &res
}

func (a1 *A1Parser) parseDeclEnum(tp *front.TokenProvider, m *A1Module, cur *A1StatScope, isExported bool) *A1DeclEnum {
	defer a1.Logger.Log(fmt.Sprintf("parsed enum decl at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	// parse name, {
	tkn := tp.Pop()
	if tkn.ObjType != front.ID {
		a1.Logger.Log(fmt.Sprintf("E0524 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	if !m.IsNameUsable(tkn.Text) {
		a1.Logger.Log(fmt.Sprintf("E0525 name %s is not usable at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	var res A1DeclEnum
	res.Init(tkn.Location, tkn.Text, isExported)
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_LBRACE {
		a1.Logger.Log(fmt.Sprintf("E0526 expected '{', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}

	// parse members, }
	var value int64 = 0
	var maxV int64 = 0
	var minV int64 = 0
	for tp.CanPop(1) {
		// check name
		tkn = tp.Pop()
		if tkn.ObjType != front.ID {
			a1.Logger.Log(fmt.Sprintf("E0527 expected name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		nm := tkn.Text
		if _, ok := res.Members[nm]; ok {
			a1.Logger.Log(fmt.Sprintf("E0528 name %s is already used at %s", nm, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}

		// init with value
		tkn = tp.Seek()
		if tkn.ObjType == front.OP_ASSIGN {
			tp.Pop()
			v := a1.parseExpr(tp, m, cur)
			if v == nil || v.GetObjType() != E1_Literal || v.(*A1ExprLiteral).Value.ObjType != front.LitInt {
				a1.Logger.Log(fmt.Sprintf("E0529 expected int constexpr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			} else {
				value = v.(*A1ExprLiteral).Value.Value.(int64)
			}
		}

		// set member value
		res.Members[nm] = value
		if value > maxV {
			maxV = value
		}
		if value < minV {
			minV = value
		}
		value++
		tkn = tp.Seek()
		if tkn.ObjType == front.OP_RBRACE {
			break
		} else if tkn.ObjType == front.OP_SEMICOLON || tkn.ObjType == front.OP_COMMA {
			tp.Pop()
			if tp.Seek().ObjType == front.OP_RBRACE {
				break
			}
		} else {
			a1.Logger.Log(fmt.Sprintf("E0530 expected ',', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
	}

	// finish parsing, register to scope
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_RBRACE {
		a1.Logger.Log(fmt.Sprintf("E0531 expected '}', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	var t A1Type
	t.Init(T1_Name, tkn.Location, res.Name, "", m.Uname)
	if maxV <= 127 && minV >= -128 {
		t.Size = 1
	} else if maxV <= 32767 && minV >= -32768 {
		t.Size = 2
	} else if maxV <= 2147483647 && minV >= -2147483648 {
		t.Size = 4
	} else {
		t.Size = 8
	}
	t.Align = t.Size
	res.Type = &t
	cur.Decls[res.Name] = &res
	return &res
}

// statement specific case parser
func (a1 *A1Parser) parseStatAssign(tp *front.TokenProvider, m *A1Module, cur *A1StatScope, left A1Expr, asn front.TokenType, expEnd front.TokenType) *A1StatAssign {
	var assignTp A1StatT
	switch asn {
	case front.OP_ASSIGN:
		assignTp = S1_Assign
	case front.OP_ASSIGN_ADD:
		assignTp = S1_AssignAdd
	case front.OP_ASSIGN_SUB:
		assignTp = S1_AssignSub
	case front.OP_ASSIGN_MUL:
		assignTp = S1_AssignMul
	case front.OP_ASSIGN_DIV:
		assignTp = S1_AssignDiv
	case front.OP_ASSIGN_MOD:
		assignTp = S1_AssignMod
	default:
		assignTp = S1_Assign
		a1.Logger.Log(fmt.Sprintf("E0601 invalid assignment operator at %s", a1.Logger.GetLoc(tp.Seek().Location)), 5, true)
	}
	var res A1StatAssign
	res.Init(assignTp, left.GetLocation(), left, a1.parseExpr(tp, m, cur))
	tkn := tp.Pop()
	if tkn.ObjType != expEnd {
		a1.Logger.Log(fmt.Sprintf("E0602 invalid assignment end at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	return &res
}

func (a1 *A1Parser) parseStatScope(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) *A1StatScope {
	a1.Logger.Log(fmt.Sprintf("start parsing scope at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	defer a1.Logger.Log(fmt.Sprintf("end parsing scope at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)

	tkn := tp.Pop()
	if tkn.ObjType != front.OP_LBRACE {
		a1.Logger.Log(fmt.Sprintf("E0603 expected '{', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	var res A1StatScope
	res.Init(tkn.Location, cur)
	for tp.CanPop(1) {
		tkn = tp.Seek()
		if tkn.ObjType == front.OP_RBRACE {
			tp.Pop()
			break
		}
		st := a1.parseStat(tp, m, &res)
		if st != nil {
			res.Body = append(res.Body, st)
		}
	}
	return &res
}

func (a1 *A1Parser) parseStatIf(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) *A1StatIf {
	defer a1.Logger.Log(fmt.Sprintf("parsed if stat at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	tkn := tp.Pop()
	if tkn.ObjType != front.OP_LPAREN {
		a1.Logger.Log(fmt.Sprintf("E0604 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	cond := a1.parseExpr(tp, m, cur)
	if cond == nil {
		a1.Logger.Log(fmt.Sprintf("E0605 expected condition expr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_RPAREN {
		a1.Logger.Log(fmt.Sprintf("E0606 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	thenStat := a1.parseStat(tp, m, cur)
	if thenStat == nil {
		a1.Logger.Log(fmt.Sprintf("E0607 expected then statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	var elseStat A1Stat = nil
	if tp.Seek().ObjType == front.KEY_ELSE {
		tp.Pop()
		elseStat = a1.parseStat(tp, m, cur)
		if elseStat == nil {
			a1.Logger.Log(fmt.Sprintf("E0608 expected else statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
	}
	var res A1StatIf
	res.Init(cond.GetLocation(), cond, thenStat, elseStat)
	return &res
}

func (a1 *A1Parser) parseStatWhile(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) *A1StatWhile {
	defer a1.Logger.Log(fmt.Sprintf("parsed while stat at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	tkn := tp.Pop()
	if tkn.ObjType != front.OP_LPAREN {
		a1.Logger.Log(fmt.Sprintf("E0609 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	cond := a1.parseExpr(tp, m, cur)
	if cond == nil {
		a1.Logger.Log(fmt.Sprintf("E0610 expected condition expr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_RPAREN {
		a1.Logger.Log(fmt.Sprintf("E0611 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	body := a1.parseStat(tp, m, cur)
	if body == nil {
		a1.Logger.Log(fmt.Sprintf("E0612 expected body statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	var res A1StatWhile
	res.Init(cond.GetLocation(), cond, body)
	return &res
}

func (a1 *A1Parser) parseStatFor(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) A1Stat {
	defer a1.Logger.Log(fmt.Sprintf("parsed for stat at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	// make var_decl scope
	tkn := tp.Pop()
	if tkn.ObjType != front.OP_LPAREN {
		a1.Logger.Log(fmt.Sprintf("E0613 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	var res A1Stat = nil
	var scope A1StatScope
	scope.Init(tkn.Location, cur)

	if tp.Match([]front.TokenType{front.KEY_AUTO, front.ID, front.OP_COMMA, front.ID, front.OP_COLON}) { // auto i, r :
		// get var i, r names
		tp.Pop()
		tkn_i := tp.Pop()
		tp.Pop()
		tkn_r := tp.Pop()
		tp.Pop()
		var di A1DeclVar
		di.Init(tkn_i.Location, tkn_i.Text, nil, nil, false)
		var si A1StatDecl
		si.Init(tkn.Location, &di)
		var dr A1DeclVar
		dr.Init(tkn_r.Location, tkn_r.Text, nil, nil, false)
		var sr A1StatDecl
		sr.Init(tkn.Location, &dr)
		scope.Body = append(scope.Body, &si)
		scope.Body = append(scope.Body, &sr)
		scope.Decls[tkn_i.Text] = &di
		scope.Decls[tkn_r.Text] = &dr
		scope.IsForeach = true // foreach var is declared in scope
		if tkn_i.Text == tkn_r.Text {
			a1.Logger.Log(fmt.Sprintf("E0614 foreach var names are same at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		}

		// parse iterator, body
		iter := a1.parseExpr(tp, m, cur)
		if iter == nil {
			a1.Logger.Log(fmt.Sprintf("E0615 expected iterator expr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0616 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
		}
		body := a1.parseStat(tp, m, &scope)
		if body == nil {
			a1.Logger.Log(fmt.Sprintf("E0617 expected body statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		var f A1StatForeach
		f.Init(tkn.Location, tkn_i.Text, tkn_r.Text, iter, body)
		scope.Body = append(scope.Body, &f)
		res = &scope

	} else if tp.Match([]front.TokenType{front.ID, front.OP_COMMA, front.ID, front.OP_COLON}) { // i, r :
		// get var i, r names
		tkn_i := tp.Pop()
		tp.Pop()
		tkn_r := tp.Pop()
		tp.Pop()
		if tkn_i.Text == tkn_r.Text {
			a1.Logger.Log(fmt.Sprintf("E0618 foreach var names are same at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		}

		// parse iterator, body
		iter := a1.parseExpr(tp, m, cur)
		if iter == nil {
			a1.Logger.Log(fmt.Sprintf("E0619 expected iterator expr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_RPAREN {
			a1.Logger.Log(fmt.Sprintf("E0620 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
		}
		body := a1.parseStat(tp, m, cur) // no pre-decl scope required
		if body == nil {
			a1.Logger.Log(fmt.Sprintf("E0621 expected body statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		var f A1StatForeach
		f.Init(tkn.Location, tkn_i.Text, tkn_r.Text, iter, body)
		res = &f

	} else { // classic for
		initSt := a1.parseStat(tp, m, &scope) // init var is registered in scope
		cond := a1.parseExpr(tp, m, &scope)   // cond_expr is in scope
		tkn = tp.Pop()
		if tkn.ObjType != front.OP_SEMICOLON {
			a1.Logger.Log(fmt.Sprintf("E0622 expected ';', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
		}

		// parse inc_stat
		var incSt A1Stat = nil
		if tp.Seek().ObjType != front.OP_RPAREN {
			left := a1.parseExpr(tp, m, &scope)
			tkn = tp.Pop()
			if tkn.ObjType == front.OP_RPAREN { // inc_stat is expr
				var st A1StatExpr
				st.Init(tkn.Location, left)
				incSt = &st
			} else { // inc_stat is assign
				incSt = a1.parseStatAssign(tp, m, &scope, left, tkn.ObjType, front.OP_RPAREN)
			}
		}

		// parse body
		if initSt == nil { // no init -> no scope
			body := a1.parseStat(tp, m, cur)
			if body == nil {
				a1.Logger.Log(fmt.Sprintf("E0623 expected body statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			var f A1StatFor
			f.Init(tkn.Location, cond, incSt, body)
			res = &f
		} else { // var init is in scope
			scope.Body = append(scope.Body, initSt)
			body := a1.parseStat(tp, m, &scope)
			if body == nil {
				a1.Logger.Log(fmt.Sprintf("E0624 expected body statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			var f A1StatFor
			f.Init(tkn.Location, cond, incSt, body)
			scope.Body = append(scope.Body, &f)
			res = &scope
		}
	}
	return res
}

func (a1 *A1Parser) parseStatSwitch(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) A1Stat {
	defer a1.Logger.Log(fmt.Sprintf("parsed switch stat at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	// get switch condition, make switch scope
	tkn := tp.Pop()
	if tkn.ObjType != front.OP_LPAREN {
		a1.Logger.Log(fmt.Sprintf("E0625 expected '(', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	cond := a1.parseExpr(tp, m, cur)
	if cond == nil {
		a1.Logger.Log(fmt.Sprintf("E0626 expected condition expr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_RPAREN {
		a1.Logger.Log(fmt.Sprintf("E0627 expected ')', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	var res A1StatSwitch
	res.Init(tkn.Location, cond)

	// get switch body
	tkn = tp.Pop()
	if tkn.ObjType != front.OP_LBRACE {
		a1.Logger.Log(fmt.Sprintf("E0628 expected '{', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	wasFall := false
	wasDefault := false
	for tp.CanPop(1) {
		tkn = tp.Seek()
		switch tkn.ObjType {
		case front.KEY_CASE: // case comes before default
			if wasDefault {
				a1.Logger.Log(fmt.Sprintf("E0629 case cannot be after default at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			cond := a1.parseExpr(tp, m, cur)
			if cond == nil || cond.GetObjType() != E1_Literal || cond.(*A1ExprLiteral).Value.ObjType != front.LitInt {
				a1.Logger.Log(fmt.Sprintf("E0630 case must be int constexpr at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			i := cond.(*A1ExprLiteral).Value.Value.(int64)
			for _, v := range res.CaseConds {
				if v == i {
					a1.Logger.Log(fmt.Sprintf("E0631 duplicate case %d at %s", i, a1.Logger.GetLoc(tkn.Location)), 5, true)
				}
			}
			tkn = tp.Pop()
			if tkn.ObjType != front.OP_COLON {
				a1.Logger.Log(fmt.Sprintf("E0632 expected ':', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
				tp.Rewind(1)
			}
			res.CaseConds = append(res.CaseConds, i)
			res.CaseFalls = append(res.CaseFalls, false)
			var s A1StatScope
			s.Init(tkn.Location, cur)
			res.CaseBodies = append(res.CaseBodies, s)
			wasFall = false

		case front.KEY_DEFAULT: // default comes after case
			if wasDefault {
				a1.Logger.Log(fmt.Sprintf("E0633 double default at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			var s A1StatScope
			s.Init(tkn.Location, cur)
			res.DefaultBody = &s
			wasDefault = true
			wasFall = false

		case front.KEY_FALL: // fall can be last stat of chunk
			if len(res.CaseConds) == 0 {
				a1.Logger.Log(fmt.Sprintf("E0634 fall before case at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			if wasFall {
				a1.Logger.Log(fmt.Sprintf("E0635 double fall at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			if wasDefault {
				a1.Logger.Log(fmt.Sprintf("E0636 cannot fall inside default at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			res.CaseFalls[len(res.CaseFalls)-1] = true
			wasFall = true

		case front.OP_RBRACE: // end of switch
			tp.Pop()
			return &res

		default: // body statement
			if len(res.CaseConds) == 0 && !wasDefault {
				a1.Logger.Log(fmt.Sprintf("E0637 statement before case at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			if wasFall {
				a1.Logger.Log(fmt.Sprintf("E0638 statement after fall at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
			}
			if wasDefault {
				body := a1.parseStat(tp, m, res.DefaultBody)
				if body == nil {
					a1.Logger.Log(fmt.Sprintf("E0639 expected body statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
				} else {
					res.DefaultBody.Body = append(res.DefaultBody.Body, body)
				}
			} else {
				pos := len(res.CaseBodies) - 1
				body := a1.parseStat(tp, m, &res.CaseBodies[pos])
				if body == nil {
					a1.Logger.Log(fmt.Sprintf("E0640 expected body statement at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
				} else {
					res.CaseBodies[pos].Body = append(res.CaseBodies[pos].Body, body)
				}
			}
		}
	}

	// check name decl with fall
	for i := range res.CaseConds {
		if res.CaseFalls[i] && len(res.CaseBodies[i].Decls) > 0 {
			a1.Logger.Log(fmt.Sprintf("E0641 name declaration inside falling case at %s", a1.Logger.GetLoc(res.CaseBodies[i].Loc)), 5, true)
		}
	}
	return &res
}

// module parsing
func (a1 *A1Parser) parseInclude(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) *A1DeclInclude {
	defer a1.Logger.Log(fmt.Sprintf("parsed include at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	tkn := tp.Pop()
	args := make([]A1Type, 0)
	if tkn.ObjType == front.OP_LT { // template include
		for tp.CanPop(1) {
			t, err := m.parseType(tp, cur, a1.Arch)
			if err != nil {
				a1.Logger.Log(err.Error(), 5, true)
			}
			args = append(args, *t)
			tkn = tp.Pop()
			if tkn.ObjType == front.OP_GT {
				break
			} else if tkn.ObjType != front.OP_COMMA {
				a1.Logger.Log(fmt.Sprintf("E0701 expected '>', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
				tp.Rewind(1)
				break
			}
		}
		tkn = tp.Pop()
	}

	// get path, name
	if tkn.ObjType != front.LIT_STR {
		a1.Logger.Log(fmt.Sprintf("E0702 expected module path, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	nm := tp.Pop()
	if tkn.ObjType != front.ID {
		a1.Logger.Log(fmt.Sprintf("E0703 expected module name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
	}
	if !m.IsNameUsable(nm.Text) {
		a1.Logger.Log(fmt.Sprintf("E0704 module name %s is not usable at %s", nm.Text, a1.Logger.GetLoc(nm.Location)), 5, true)
	}
	path, err := front.AbsPath(tkn.Text, front.GetWorkingDir(m.Path))
	if err != nil {
		a1.Logger.Log(err.Error(), 5, true)
	}

	// make new module
	var res A1DeclInclude
	pos := a1.FindModule(path)
	if pos >= 0 && !a1.Modules[pos].IsFinished {
		a1.Logger.Log(fmt.Sprintf("E0705 import cycle with source %s at %s", path, a1.Logger.GetLoc(tkn.Location)), 5, true)
		return nil
	}
	if pos >= 0 && len(args) == 0 { // non-template include is reused
		res.Init(tkn.Location, nm.Text, path, a1.Modules[pos].Uname, args)
	} else { // should parse new module
		if len(args) == 0 {
			a1.ChunkCount++ // template include must use same chunkID
		}
		mm := a1.parseSrc(path, args, a1.ChunkCount)
		if mm == nil {
			a1.Logger.Log(fmt.Sprintf("E0706 failed to parse module %s at %s", path, a1.Logger.GetLoc(tkn.Location)), 5, true)
			return nil
		}
		a1.Modules = append(a1.Modules, *mm)
		res.Init(tkn.Location, nm.Text, path, mm.Uname, args)
	}
	cur.Decls[nm.Text] = &res
	return &res
}

func (a1 *A1Parser) parseTemplate(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) []A1Decl {
	defer a1.Logger.Log(fmt.Sprintf("parsed template at %s", a1.Logger.GetLoc(tp.Seek().Location)), 1, false)
	res := make([]A1Decl, 0)
	tkn := tp.Pop()
	if tkn.ObjType != front.OP_LT {
		a1.Logger.Log(fmt.Sprintf("E0707 expected '<', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		tp.Rewind(1)
	}
	for tp.CanPop(1) {
		// parse template name
		tkn = tp.Pop()
		if tkn.ObjType != front.ID {
			a1.Logger.Log(fmt.Sprintf("E0708 expected template name, got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}
		if !m.IsNameUsable(tkn.Text) {
			a1.Logger.Log(fmt.Sprintf("E0709 template name %s is not usable at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
		}

		// link template type
		var d A1DeclTemplate
		d.Init(tkn.Location, tkn.Text)
		if m.tmpUsed < len(m.tmpArgs) {
			d.Type = &m.tmpArgs[m.tmpUsed]
			m.tmpUsed++
		} else {
			a1.Logger.Log(fmt.Sprintf("E0710 too many template arguments at %s", a1.Logger.GetLoc(tkn.Location)), 5, true)
		}

		// register template
		cur.Decls[tkn.Text] = &d
		res = append(res, &d)
		tkn = tp.Pop()
		if tkn.ObjType == front.OP_GT {
			break
		} else if tkn.ObjType != front.OP_COMMA {
			a1.Logger.Log(fmt.Sprintf("E0711 expected '>', got %s at %s", tkn.Text, a1.Logger.GetLoc(tkn.Location)), 5, true)
			tp.Rewind(1)
			break
		}
	}
	return res
}

func (a1 *A1Parser) parseSrc(path string, args []A1Type, chunkID int) *A1Module {
	defer a1.Logger.Log(fmt.Sprintf("AST1 generated %s", path), 3, false)
	// find unique name
	fname := front.GetFileName(path)
	if len(fname) > a1.NameCut {
		fname = fname[:a1.NameCut]
	}
	count := 0
	isIn := true
	uname := fname
	for isIn {
		isIn = false
		if count > 0 {
			uname = fmt.Sprintf("%s_%d", fname, count)
		}
		for i := range a1.Modules {
			if a1.Modules[i].Uname == uname {
				isIn = true
				count++
				break
			}
		}
	}
	a1.Logger.Log(fmt.Sprintf("parsing %s as %s", path, uname), 2, false)

	// tokenize source
	src, err := front.ReadFile(path)
	if err != nil {
		a1.Logger.Log(err.Error(), 5, true)
		return nil
	}
	a1.Logger.Paths = append(a1.Logger.Paths, path)
	tkns, err := front.Tokenize(src, path, len(a1.Modules))
	if err != nil {
		a1.Logger.Log(err.Error(), 5, true)
	}
	var tp front.TokenProvider
	tp.Init(tkns)
	a1.Logger.Log(fmt.Sprintf("tokenized %s as %d tokens", uname, len(tkns)), 2, false)

	// make module
	var m A1Module
	m.Init(path, uname, chunkID, args)
	var scope A1StatScope
	scope.Init(tp.Seek().Location, nil)
	m.Code = &scope
	defer func() {
		m.IsFinished = true
	}()
	idx := make([]int, 0, 32) // pass2 index

	// pass1 : parse decl except var/func
	for tp.CanPop(1) {
		tkn := tp.Seek()
		switch tkn.ObjType {
		case front.KEY_INCLUDE, front.KEY_TEMPLATE, front.KEY_STRUCT, front.KEY_ENUM, front.KEY_TYPEDEF: // parse now
			for _, d := range a1.parseToplevel(&tp, &m, &scope) {
				var st A1StatDecl
				st.Init(d.GetLocation(), d)
				scope.Body = append(scope.Body, &st)
			}
		case front.KEY_RAW_C, front.KEY_RAW_IR:
			scope.Body = append(scope.Body, a1.parseStat(&tp, &m, &scope))
		case front.OP_SEMICOLON:
			tp.Pop()
		case front.KEY_EXPORT:
			if tp.Match([]front.TokenType{front.KEY_EXPORT, front.KEY_STRUCT}) || tp.Match([]front.TokenType{front.KEY_EXPORT, front.KEY_ENUM}) || tp.Match([]front.TokenType{front.KEY_EXPORT, front.KEY_TYPEDEF}) {
				for _, d := range a1.parseToplevel(&tp, &m, &scope) { // parse now
					var st A1StatDecl
					st.Init(d.GetLocation(), d)
					scope.Body = append(scope.Body, &st)
				}
			} else {
				idx = append(idx, tp.Pos) // add index for later parsing
			}
		case front.KEY_DEFINE, front.KEY_CONST, front.KEY_VOLATILE, front.KEY_EXTERN, front.KEY_VA_ARG:
			idx = append(idx, tp.Pos) // add index for later parsing
		default:
			idx = append(idx, tp.Pos) // var/func are added later
			a1.jumpDecl(&tp, &m, &scope)
		}
	}

	// check template arg
	if m.tmpUsed != len(m.tmpArgs) {
		a1.Logger.Log(fmt.Sprintf("E0712 template arguments do not match (%d given, %d used) for %s", len(m.tmpArgs), m.tmpUsed, uname), 5, true)
		return nil
	}

	// pass2 : calculate type size
	calcList := make([]*A1DeclStruct, 0)
	for _, st := range scope.Body {
		if st.GetObjType() == S1_Decl {
			d := st.(*A1StatDecl).Decl
			if d.GetObjType() == D1_Struct {
				calcList = append(calcList, d.(*A1DeclStruct))
			}
		}
	}
	modified := true
	for modified {
		modified = false
		for _, s := range calcList {
			modified = a1.completeStruct(s, &m) || modified
		}
	}

	// check size
	for _, s := range calcList {
		if s.Type.Size <= 0 {
			a1.Logger.Log(fmt.Sprintf("E0713 cannot decide size of %s at %s", s.Name, a1.Logger.GetLoc(s.Loc)), 5, true)
		}
	}
	a1.Logger.Log(fmt.Sprintf("calculated size for %s", uname), 2, false)

	// pass3 : parse var/func
	tp.Pos = 0
	for _, pos := range idx {
		if tp.Pos <= pos {
			tp.Pos = pos
			for _, d := range a1.parseToplevel(&tp, &m, &scope) {
				var st A1StatDecl
				st.Init(d.GetLocation(), d)
				scope.Body = append(scope.Body, &st)
			}
		}
	}
	return &m
}

func (a1 *A1Parser) jumpDecl(tp *front.TokenProvider, m *A1Module, cur *A1StatScope) {
	m.parseType(tp, cur, a1.Arch)
	if tp.Match([]front.TokenType{front.ID, front.OP_SEMICOLON}) || tp.Match([]front.TokenType{front.ID, front.OP_ASSIGN}) { // variable declaration
		for tp.CanPop(1) {
			if tp.Pop().ObjType == front.OP_SEMICOLON {
				break
			}
		}
	} else { // function declaration
		count := 0
		for tp.CanPop(1) {
			if tp.Pop().ObjType == front.OP_LBRACE {
				count++
				break
			}
		}
		for tp.CanPop(1) {
			tkn := tp.Pop()
			if tkn.ObjType == front.OP_LBRACE {
				count++
			} else if tkn.ObjType == front.OP_RBRACE {
				count--
				if count == 0 {
					break
				}
			}
		}
	}
}

// type size calculation
func (a1 *A1Parser) completeType(tp *A1Type, m *A1Module) bool {
	// complete subtypes first
	modified := false
	if tp.Direct != nil {
		modified = a1.completeType(tp.Direct, m) || modified
	}
	for i := range tp.Indirect {
		modified = a1.completeType(&tp.Indirect[i], m) || modified
	}
	if tp.Size != -1 { // already completed
		return modified
	}

	// complete type
	switch tp.ObjType {
	case T1_Arr:
		if tp.Direct.Size > 0 { // get array size by direct type
			tp.Size = tp.Direct.Size * tp.ArrLen
			tp.Align = tp.Direct.Align
			modified = true
		}

	case T1_Name:
		// 1. find module that name is used
		pos := a1.GetModule(tp.SrcUname)
		if pos < 0 {
			a1.Logger.Log(fmt.Sprintf("E0801 unknown module %s", tp.SrcUname), 5, true)
			return modified
		}
		tgtMod := &a1.Modules[pos]
		decl := tgtMod.FindDecl(tp.Name, tgtMod.Uname != m.Uname)
		if decl == nil {
			a1.Logger.Log(fmt.Sprintf("E0802 unknown name %s", tp.Name), 5, true)
			return modified
		}

		// 2. complete type by declaration
		if decl.GetObjType() == D1_Struct {
			structDecl := decl.(*A1DeclStruct)
			if structDecl.Type.Size > 0 {
				tp.Size = structDecl.Type.Size
				tp.Align = structDecl.Type.Align
				modified = true
			}
		} else if decl.GetObjType() == D1_Enum {
			enumDecl := decl.(*A1DeclEnum)
			if enumDecl.Type.Size > 0 {
				tp.Size = enumDecl.Type.Size
				tp.Align = enumDecl.Type.Align
				modified = true
			}
		} else if decl.GetObjType() == D1_Typedef {
			typedefDecl := decl.(*A1DeclTypedef) // complete typedef original type first
			modified = a1.completeType(typedefDecl.Type, tgtMod) || modified
			if typedefDecl.Type.Size > 0 {
				tp.Size = typedefDecl.Type.Size
				tp.Align = typedefDecl.Type.Align
			}
		} else if decl.GetObjType() == D1_Template {
			templateDecl := decl.(*A1DeclTemplate) // complete template arg type first
			modified = a1.completeType(templateDecl.Type, tgtMod) || modified
			if templateDecl.Type.Size > 0 {
				tp.Size = templateDecl.Type.Size
				tp.Align = templateDecl.Type.Align
			}
		} else {
			a1.Logger.Log(fmt.Sprintf("E0803 name %s is not a type", tp.Name), 5, true)
		}

	case T1_Foreign:
		// 1. find include decl that name is used
		pos := a1.GetModule(tp.SrcUname)
		if pos < 0 {
			a1.Logger.Log(fmt.Sprintf("E0804 unknown module %s", tp.SrcUname), 5, true)
			return modified
		}
		tgtMod := &a1.Modules[pos]
		decl := tgtMod.FindDecl(tp.Name, tgtMod.Uname != m.Uname)
		if decl == nil || decl.GetObjType() != D1_Include {
			a1.Logger.Log(fmt.Sprintf("E0805 unknown include %s", tp.Name), 5, true)
			return modified
		}
		includeDecl := decl.(*A1DeclInclude)

		// 2. find actual module that name is used
		pos = a1.GetModule(includeDecl.TgtUname)
		if pos < 0 {
			a1.Logger.Log(fmt.Sprintf("E0806 unknown module %s", includeDecl.TgtUname), 5, true)
			return modified
		}
		tgtMod = &a1.Modules[pos]

		// 3. make name type and complete it
		var nm_t A1Type
		nm_t.Init(T1_Name, tp.Loc, tp.Name, "", includeDecl.TgtUname)
		modified = a1.completeType(&nm_t, tgtMod) || modified
		if nm_t.Size > 0 {
			tp.Size = nm_t.Size
			tp.Align = nm_t.Align
		}
	}
	return modified
}

func (a1 *A1Parser) completeStruct(sDecl *A1DeclStruct, m *A1Module) bool {
	modified := false
	if sDecl.Type.Size > 0 { // already completed
		return false
	}
	for i := range sDecl.MemTypes { // complete member types
		if sDecl.MemTypes[i].Size <= 0 {
			modified = a1.completeType(&sDecl.MemTypes[i], m) || modified
		}
		if sDecl.MemTypes[i].Size <= 0 { // not completed yet
			return modified
		}
	}

	// calculate size and align
	sDecl.Type.Size = 0
	sDecl.Type.Align = 0
	for i := range sDecl.MemTypes {
		if sDecl.Type.Size%sDecl.MemTypes[i].Align != 0 { // align padding
			sDecl.Type.Size += sDecl.MemTypes[i].Align - sDecl.Type.Size%sDecl.MemTypes[i].Align
		}
		sDecl.MemOffsets[i] = sDecl.Type.Size // add member
		sDecl.Type.Size += sDecl.MemTypes[i].Size
		sDecl.Type.Align = max(sDecl.Type.Align, sDecl.MemTypes[i].Align)
	}
	if sDecl.Type.Size%sDecl.Type.Align != 0 { // align padding
		sDecl.Type.Size += sDecl.Type.Align - sDecl.Type.Size%sDecl.Type.Align
	}
	a1.Logger.Log(fmt.Sprintf("calculated size of struct %s at %s", sDecl.Name, a1.Logger.GetLoc(sDecl.Loc)), 1, false)
	return true
}
