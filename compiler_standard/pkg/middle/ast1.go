package middle

import "../front"

// return pratt precedence, -1 if not operator
func getPrattPrecedence(tknType front.TokenType, isUnary bool) int {
	if isUnary {
		switch tknType {
		case front.OP_ADD, front.OP_SUB,
			front.OP_LOGIC_NOT, front.OP_BIT_NOT,
			front.OP_MUL, front.OP_BIT_AND: // * (deref), & (addr)
			return 15
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
		return B_Mul1
	case front.OP_DIV:
		return B_Div1
	case front.OP_MOD:
		return B_Mod1
	case front.OP_ADD:
		return B_Add1
	case front.OP_SUB:
		return B_Sub1
	case front.OP_BIT_LSHIFT:
		return B_Shl1
	case front.OP_BIT_RSHIFT:
		return B_Shr1
	case front.OP_LT:
		return B_Lt1
	case front.OP_GT:
		return B_Gt1
	case front.OP_LT_EQ:
		return B_Le1
	case front.OP_GT_EQ:
		return B_Ge1
	case front.OP_EQ:
		return B_Eq1
	case front.OP_NEQ:
		return B_Ne1
	case front.OP_BIT_AND:
		return B_BitAnd1
	case front.OP_BIT_XOR:
		return B_BitXor1
	case front.OP_BIT_OR:
		return B_BitOr1
	case front.OP_LOGIC_AND:
		return B_LogicAnd1
	case front.OP_LOGIC_OR:
		return B_LogicOr1
	case front.OP_COND:
		return C_Cond1
	default:
		return -1
	}
}

// get number of operand
func getOperandNum(op A1ExprOpT) int {
	switch op {
	case -1:
		return 0
	case C_Cond1, C_Slice1:
		return 3
	case U_Plus1, U_Minus1, U_LogicNot1, U_BitNot1,
		U_Ref1, U_Deref1, U_Sizeof1, U_Len1:
		return 1
	default:
		return 2
	}
}

// convert token to A1StatT
func getAssignType(tkn front.Token) A1StatT {
	switch tkn.ObjType {
	case front.OP_ASSIGN:
		return Assign1
	case front.OP_ASSIGN_ADD:
		return AssignAdd1
	case front.OP_ASSIGN_SUB:
		return AssignSub1
	case front.OP_ASSIGN_MUL:
		return AssignMul1
	case front.OP_ASSIGN_DIV:
		return AssignDiv1
	case front.OP_ASSIGN_MOD:
		return AssignMod1
	default:
		return -1
	}
}

// parseType 구현
