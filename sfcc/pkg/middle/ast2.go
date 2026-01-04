// test787f : femto-C middle.ast2 R0

package middle

import (
	"fmt"
	"strings"

	"../front"
)

// type helper functions
func (a2 *A2Type) Print() string {
	switch a2.ObjType {
	case T2_Primitive:
		return a2.Name
	case T2_Ptr:
		return a2.Direct.Print() + "*"
	case T2_Slice:
		return a2.Direct.Print() + "[]"
	case T2_Arr:
		return fmt.Sprintf("%s[%d]", a2.Direct.Print(), a2.ArrLen)
	case T2_Func:
		temp := make([]string, len(a2.Indirect))
		for i := range a2.Indirect {
			temp[i] = a2.Indirect[i].Print()
		}
		return fmt.Sprintf("%s(%s)", a2.Direct.Print(), strings.Join(temp, ","))
	case T2_Struct, T2_Enum:
		return a2.Name
	}
	return "?"
}

func (a2 *A2Type) Equals(tp *A2Type) bool {
	if a2.ObjType != tp.ObjType {
		return false
	}
	switch a2.ObjType {
	case T2_Primitive:
		return a2.Name == tp.Name
	case T2_Ptr, T2_Slice:
		if a2.Direct == nil || tp.Direct == nil {
			return false
		}
		return a2.Direct.Equals(tp.Direct)
	case T2_Arr:
		if a2.Direct == nil || tp.Direct == nil || a2.ArrLen != tp.ArrLen {
			return false
		}
		return a2.Direct.Equals(tp.Direct)
	case T2_Func:
		if a2.Direct == nil || tp.Direct == nil || len(a2.Indirect) != len(tp.Indirect) {
			return false
		}
		if !a2.Direct.Equals(tp.Direct) {
			return false
		}
		for i := 0; i < len(a2.Indirect); i++ {
			if !a2.Indirect[i].Equals(&tp.Indirect[i]) {
				return false
			}
		}
		return true
	case T2_Struct, T2_Enum:
		return a2.SrcUname == tp.SrcUname && a2.Name == tp.Name
	}
	return true
}

func (a2 *A2Type) IsThis(class string) bool {
	f0 := false // signed int
	f1 := false // unsigned int
	f2 := false // float
	switch a2.Name {
	case "int", "i8", "i16", "i32", "i64":
		f0 = true
	case "uint", "u8", "u16", "u32", "u64":
		f1 = true
	case "f32", "f64":
		f2 = true
	}
	switch class {
	case "primitive":
		return a2.ObjType == T2_Primitive
	case "ptr":
		return a2.ObjType == T2_Ptr
	case "slice":
		return a2.ObjType == T2_Slice
	case "arr":
		return a2.ObjType == T2_Arr
	case "func":
		return a2.ObjType == T2_Func
	case "struct":
		return a2.ObjType == T2_Struct
	case "enum":
		return a2.ObjType == T2_Enum
	case "sint":
		return a2.ObjType == T2_Primitive && f0
	case "uint":
		return a2.ObjType == T2_Primitive && f1
	case "int":
		return a2.ObjType == T2_Primitive && (f0 || f1)
	case "float":
		return a2.ObjType == T2_Primitive && f2
	case "bool":
		return a2.ObjType == T2_Primitive && a2.Name == "bool"
	case "void":
		return a2.ObjType == T2_Primitive && a2.Name == "void"
	}
	return false
}

// basic helper functions
func callArgCheck(f *A2Type, isVa bool, isVa_ad bool, args []*A2Type, loc string) error {
	c0 := len(f.Indirect) // minimum args required
	c1 := len(args)       // given args
	if isVa {
		c0--
	}
	if isVa_ad {
		c0--
	}
	if (isVa && c0 > c1) || (!isVa && c0 != c1) {
		return fmt.Errorf("E0901 need %d args, got %d args at %s", c0, c1, loc)
	}
	for i := range c0 {
		if !f.Indirect[i].Equals(args[i]) {
			return fmt.Errorf("E0902 arg %d type mismatch at %s", i, loc)
		}
	}
	return nil
}

// type conversion
func (a2 *A2Analyzer) convType(src *A1Type, ct *A2Context) *A2Type {
	if src.ObjType != T1_Primitive && src.Size <= 0 {
		a2.Logger.Log(fmt.Sprintf("E1001 incomplete type at %s", a2.Logger.GetLoc(src.Loc)), 5, true)
	}
	var res A2Type
	switch src.ObjType {
	case T1_Primitive:
		res.Init(T2_Primitive, src.Loc, src.Name, "")
		res.Size = src.Size
		res.Align = src.Align

	case T1_Ptr:
		res.Init(T2_Ptr, src.Loc, "*", "")
		res.Direct = a2.convType(src.Direct, ct)
		res.Size = a2.Arch
		res.Align = a2.Arch

	case T1_Slice:
		res.Init(T2_Slice, src.Loc, "[]", "")
		res.Direct = a2.convType(src.Direct, ct)
		res.Size = a2.Arch * 2
		res.Align = a2.Arch

	case T1_Arr:
		res.Init(T2_Arr, src.Loc, src.Name, "")
		res.Direct = a2.convType(src.Direct, ct)
		res.ArrLen = src.ArrLen
		res.Size = res.Direct.Size * res.ArrLen
		res.Align = res.Direct.Align

	case T1_Func:
		res.Init(T2_Func, src.Loc, "()", "")
		res.Direct = a2.convType(src.Direct, ct)
		for i := range src.Indirect {
			res.Indirect = append(res.Indirect, *a2.convType(&src.Indirect[i], ct))
		}
		res.Size = a2.Arch
		res.Align = a2.Arch

	case T1_Name:
		pos := a2.FindModule(src.SrcUname)
		if pos == -1 {
			a2.Logger.Log(fmt.Sprintf("E1002 cannot find module %s at %s", src.SrcUname, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		mod := a2.Modules[pos]
		decl := mod.FindDecl(src.Name)
		if decl == nil {
			a2.Logger.Log(fmt.Sprintf("E1003 cannot find name %s.%s at %s", src.SrcUname, src.Name, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		switch decl.GetObjType() {
		case D2_Struct:
			res.Init(T2_Struct, src.Loc, src.Name, src.SrcUname)
		case D2_Enum:
			res.Init(T2_Enum, src.Loc, src.Name, src.SrcUname)
		default: // typedef, template are replaced while AST1 parsing
			a2.Logger.Log(fmt.Sprintf("E1004 name %s.%s cannot be a type at %s", src.SrcUname, src.Name, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		res.Size = decl.GetType().Size
		res.Align = decl.GetType().Align

	case T1_Foreign:
		// find original src
		pos := a2.a1.GetModule(src.SrcUname)
		if pos == -1 {
			a2.Logger.Log(fmt.Sprintf("E1005 cannot find module %s at %s", src.SrcUname, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		incDecl := a2.a1.Modules[pos].FindDecl(src.IncName, false)
		if incDecl == nil {
			a2.Logger.Log(fmt.Sprintf("E1006 cannot find include %s.%s at %s", src.SrcUname, src.IncName, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}

		// make new name node
		var temp A1Type
		temp.Init(T1_Name, src.Loc, src.Name, "", incDecl.(*A1DeclInclude).TgtUname)
		return a2.convType(&temp, ct)

	default:
		return nil
	}
	return &res
}

// expression conversion
func (a2 *A2Analyzer) convExpr(src A1Expr, ct *A2Context, forcedTp *A2Type, isResUsed bool, assignTo string) A2Expr {
	if src == nil {
		return nil
	}
	switch src.GetObjType() {
	case E1_Literal:
		if isResUsed {
			return a2.convExprLiteral(src.(*A1ExprLiteral), forcedTp)
		} else {
			return nil
		}
	case E1_LitData:
		return a2.convExprLitData(src.(*A1ExprLitData), ct, forcedTp, isResUsed, assignTo)
	case E1_Op:
		op := src.(*A1ExprOp)
		if op.SubType == B1_Dot {
			return a2.convExprDotOp(op, ct, forcedTp, isResUsed, assignTo)
		} else {
			return a2.convExprOp(op, ct, forcedTp, isResUsed, assignTo)
		}
	case E1_FCall:
		return a2.convExprFcall(src.(*A1ExprFCall), ct, forcedTp, isResUsed, assignTo)
	case E1_Name: // single name is var/func inside module
		var res A2ExprName
		nm := src.(*A1ExprName)
		vDecl := ct.FindVar(nm.Name)
		if vDecl == nil { // func name
			fDecl := ct.CurModule.FindDecl(nm.Name)
			if fDecl == nil || fDecl.GetObjType() != D2_Func {
				a2.Logger.Log(fmt.Sprintf("E1101 cannot find function %s at %s", nm.Name, a2.Logger.GetLoc(nm.Loc)), 5, true)
				return nil
			}
			res.Init(E2_FuncName, nm.Loc, fDecl)
			res.IsConst = true
			res.IsLvalue = true
		} else { // var name
			res.Init(E2_VarName, nm.Loc, vDecl)
			res.IsConst = vDecl.IsConst || vDecl.IsDefine
			res.IsLvalue = !vDecl.IsDefine
		}
		if forcedTp != nil && !res.ExprType.Equals(forcedTp) { // check type
			a2.Logger.Log(fmt.Sprintf("E1102 need type %s, got %s at %s", forcedTp.Print(), res.ExprType.Print(), a2.Logger.GetLoc(nm.Loc)), 5, true)
		}
		return &res
	default:
		a2.Logger.Log(fmt.Sprintf("E1103 invalid expression type at %s", a2.Logger.GetLoc(src.GetLocation())), 5, true)
	}
	return nil
}

func (a2 *A2Analyzer) convExprLiteral(src *A1ExprLiteral, forcedTp *A2Type) *A2ExprLiteral {
	var res A2ExprLiteral
	var litType *A2Type

	// infer type, check convertability
	nonConv := false
	switch src.Value.ObjType {
	case front.LitInt:
		if forcedTp == nil {
			litType = a2.LoadType("int")
		} else {
			switch {
			case forcedTp.IsThis("int"): // int -> int
				litType = forcedTp
			case forcedTp.IsThis("enum"): // int -> enum
				litType = forcedTp
			default:
				litType = a2.LoadType("int")
				nonConv = true
			}
		}

	case front.LitFloat:
		if forcedTp == nil {
			litType = a2.LoadType("f64")
		} else {
			switch {
			case forcedTp.IsThis("float"): // float -> float
				litType = forcedTp
			default:
				litType = a2.LoadType("f64")
				nonConv = true
			}
		}

	case front.LitString:
		if forcedTp == nil {
			litType = a2.LoadType("u8[]")
		} else {
			switch {
			case forcedTp.IsThis("slice") && forcedTp.Direct.IsThis("u8"): // string -> u8[]
				litType = forcedTp
			case forcedTp.IsThis("ptr") && forcedTp.Direct.IsThis("u8"): // string -> u8*
				litType = forcedTp
			case forcedTp.IsThis("arr") && forcedTp.Direct.IsThis("u8") && forcedTp.ArrLen > len(src.Value.Value.(string)): // string -> u8[N]
				litType = forcedTp
			default:
				litType = a2.LoadType("u8[]")
				nonConv = true
			}
		}

	case front.LitBool:
		if forcedTp == nil {
			litType = a2.LoadType("bool")
		} else {
			switch {
			case forcedTp.IsThis("bool"): // bool -> bool
				litType = forcedTp
			default:
				litType = a2.LoadType("bool")
				nonConv = true
			}
		}

	case front.LitNptr:
		if forcedTp == nil {
			litType = a2.LoadType("void*")
		} else {
			switch {
			case forcedTp.IsThis("ptr"): // null -> ptr
				litType = forcedTp
			case forcedTp.IsThis("slice"): // null -> slice
				litType = forcedTp
			case forcedTp.IsThis("func"): // null -> func
				litType = forcedTp
			default:
				litType = a2.LoadType("void*")
				nonConv = true
			}
		}

	default:
		a2.Logger.Log(fmt.Sprintf("E1104 invalid literal at %s", a2.Logger.GetLoc(src.Loc)), 5, true)
		return nil
	}

	if nonConv {
		a2.Logger.Log(fmt.Sprintf("E1104 need type %s, got %s at %s", forcedTp.Print(), litType.Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
	}
	res.Init(src.Loc, litType, src.Value)
	return &res
}

func (a2 *A2Analyzer) convExprLitData(src *A1ExprLitData, ct *A2Context, forcedTp *A2Type, isResUsed bool, assignTo string) *A2ExprLitData {
	return nil
}

func (a2 *A2Analyzer) convExprDotOp(src *A1ExprOp, ct *A2Context, forcedTp *A2Type, isResUsed bool, assignTo string) *A2ExprOp {
	return nil
}

func (a2 *A2Analyzer) convExprOp(src *A1ExprOp, ct *A2Context, forcedTp *A2Type, isResUsed bool, assignTo string) *A2ExprOp {
	return nil
}

func (a2 *A2Analyzer) convExprFcall(src *A1ExprFCall, ct *A2Context, forcedTp *A2Type, isResUsed bool, assignTo string) *A2ExprCall {
	return nil
}
