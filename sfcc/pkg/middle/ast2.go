// test787f : femto-C middle.ast2 R0

package middle

import (
	"fmt"
	"slices"
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
func (a2 *A2Analyzer) convExpr(src A1Expr, ct *A2Context, forcedTp *A2Type) A2Expr {
	if src == nil {
		return nil
	}
	switch src.GetObjType() {
	case E1_Literal:
		return a2.convExprLiteral(src.(*A1ExprLiteral), forcedTp)

	case E1_LitData:
		return a2.convExprLitData(src.(*A1ExprLitData), ct, forcedTp)

	case E1_Op:
		op := src.(*A1ExprOp)
		if op.SubType == B1_Dot {
			return a2.convExprDotOp(op, ct, forcedTp)
		} else {
			return a2.convExprOp(op, ct, forcedTp)
		}

	case E1_FCall:
		return a2.convExprFcall(src.(*A1ExprFCall), ct, forcedTp)

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
		a2.Logger.Log(fmt.Sprintf("E1105 need type %s, got %s at %s", forcedTp.Print(), litType.Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
	}
	res.Init(src.Loc, litType, src.Value)
	if forcedTp.IsThis("enum") {
		res.Value.EnumInfo = forcedTp.SrcUname + "." + forcedTp.Name
	}
	return &res
}

func (a2 *A2Analyzer) convExprLitData(src *A1ExprLitData, ct *A2Context, forcedTp *A2Type) *A2ExprLitData {
	var res A2ExprLitData
	if forcedTp == nil && len(src.Elements) == 0 { // cannot infer type
		a2.Logger.Log(fmt.Sprintf("E1106 cannot infer type at %s", a2.Logger.GetLoc(src.Loc)), 5, true)
		return nil
	}

	if forcedTp == nil { // auto array
		var tp A2Type
		tp.Init(T2_Arr, src.Loc, fmt.Sprintf("[%d]", len(src.Elements)), "")
		tp.ArrLen = len(src.Elements)
		res.Init(src.Loc, &tp)
		res.Elements = append(res.Elements, a2.convExpr(src.Elements[0], ct, nil))
		res.ExprType.Direct = res.Elements[0].GetType()
		for i := 1; i < len(src.Elements); i++ {
			res.Elements = append(res.Elements, a2.convExpr(src.Elements[i], ct, res.ExprType.Direct))
		}
		return &res

	} else if forcedTp.IsThis("arr") || forcedTp.IsThis("slice") { // arr, slice
		if forcedTp.IsThis("arr") && forcedTp.ArrLen < len(src.Elements) {
			a2.Logger.Log(fmt.Sprintf("E1107 arr[%d] cannot contain %d elements at %s", forcedTp.ArrLen, len(src.Elements), a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		res.Init(src.Loc, forcedTp)
		for _, r := range src.Elements {
			res.Elements = append(res.Elements, a2.convExpr(r, ct, forcedTp.Direct))
		}
		return &res

	} else if forcedTp.IsThis("struct") { // struct
		pos := a2.FindModule(forcedTp.SrcUname)
		if pos < 0 {
			a2.Logger.Log(fmt.Sprintf("E1108 cannot find module %s at %s", forcedTp.SrcUname, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		decl := a2.Modules[pos].FindDecl(forcedTp.Name)
		if decl == nil || decl.GetObjType() != D2_Struct {
			a2.Logger.Log(fmt.Sprintf("E1109 cannot find struct %s.%s at %s", forcedTp.SrcUname, forcedTp.Name, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		sDecl := decl.(*A2DeclStruct)
		if len(src.Elements) != len(sDecl.MemTypes) {
			a2.Logger.Log(fmt.Sprintf("E1110 struct %s.%s has %d members, got %d at %s", forcedTp.SrcUname, forcedTp.Name, len(sDecl.MemTypes), len(src.Elements), a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		res.Init(src.Loc, forcedTp)
		for i, r := range src.Elements {
			res.Elements = append(res.Elements, a2.convExpr(r, ct, &sDecl.MemTypes[i]))
		}
		return &res

	} else { // invalid
		a2.Logger.Log(fmt.Sprintf("E1111 invalid type %s for literal data at %s", forcedTp.Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
		return nil
	}
}

func (a2 *A2Analyzer) convExprDotOp(src *A1ExprOp, ct *A2Context, forcedTp *A2Type) A2Expr {
	var lhs A2Expr
	rname := src.Operand1.(*A1ExprName).Name

	if src.Operand1.GetObjType() == E1_Name { // LHS is name
		lname := src.Operand0.(*A1ExprName).Name
		switch ct.FindDomain(lname) {
		case 0, 1: // var.x, func.x
			lhs = a2.convExpr(src.Operand0, ct, nil)
		case 2: // struct.x
			decl := ct.CurModule.FindDecl(lname).(*A2DeclStruct)
			var temp A2ExprName
			temp.Init(E2_StructName, src.Operand0.GetLocation(), decl)
			lhs = &temp
		case 3: // enum.x
			decl := ct.CurModule.FindDecl(lname).(*A2DeclEnum)
			var temp A2ExprName
			temp.Init(E2_EnumName, src.Operand0.GetLocation(), decl)
			lhs = &temp
		default: // inc.x
			d1 := a2.a1.Modules[a2.a1.GetModule(ct.CurModule.Uname)].FindDecl(lname, false)
			if d1 == nil || d1.GetObjType() != D1_Include {
				a2.Logger.Log(fmt.Sprintf("E1201 cannot find include name %s at %s", lname, a2.Logger.GetLoc(src.Loc)), 5, true)
				return nil
			}
			pos := a2.FindModule(d1.(*A1DeclInclude).TgtUname)
			if pos < 0 {
				a2.Logger.Log(fmt.Sprintf("E1202 cannot find module %s at %s", d1.(*A1DeclInclude).TgtUname, a2.Logger.GetLoc(src.Loc)), 5, true)
				return nil
			}
			d2 := a2.Modules[pos].FindDecl(rname)
			if d2 == nil {
				a2.Logger.Log(fmt.Sprintf("E1203 cannot find %s.%s at %s", lname, rname, a2.Logger.GetLoc(src.Loc)), 5, true)
				return nil
			}

			var temp A2ExprName
			switch d2.GetObjType() {
			case D2_Var:
				temp.Init(E2_VarName, src.Operand0.GetLocation(), d2)
			case D2_Func:
				temp.Init(E2_FuncName, src.Operand0.GetLocation(), d2)
			case D2_Struct:
				temp.Init(E2_StructName, src.Operand0.GetLocation(), d2)
			case D2_Enum:
				temp.Init(E2_EnumName, src.Operand0.GetLocation(), d2)
			default:
				a2.Logger.Log(fmt.Sprintf("E1204 cannot find %s.%s at %s", lname, rname, a2.Logger.GetLoc(src.Loc)), 5, true)
				return nil
			}
			lhs = &temp
		}

	} else { // LHS is expr
		lhs = a2.convExpr(src.Operand0, ct, nil)
	}
	if lhs == nil {
		return nil
	}

	switch lhs.GetObjType() {
	case E2_VarName, E2_FuncName, E2_LitData, E2_Op, E2_FuncCall, E2_FptrCall: // inst.member
		// check struct type
		var structType *A2Type
		op := B2_Dot
		if lhs.GetType().IsThis("struct") {
			structType = lhs.GetType()
		} else if lhs.GetType().IsThis("ptr") && lhs.GetType().Direct.IsThis("struct") {
			op = B2_Arrow
			structType = lhs.GetType().Direct
		} else {
			a2.Logger.Log(fmt.Sprintf("E1205 invalid access .%s at %s", rname, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}

		// check visibility
		if rname[0] < 'A' || rname[0] > 'Z' {
			if rname[0] == '_' { // private
				if lhs.GetType().SrcUname != ct.CurModule.Uname || lhs.GetType().Name != ct.CurFunc.StructNm {
					a2.Logger.Log(fmt.Sprintf("E1206 member %s is private at %s", rname, a2.Logger.GetLoc(src.Loc)), 5, true)
				}
			} else { // protected
				if lhs.GetType().SrcUname != ct.CurModule.Uname {
					a2.Logger.Log(fmt.Sprintf("E1207 member %s is protected at %s", rname, a2.Logger.GetLoc(src.Loc)), 5, true)
				}
			}
		}

		// find member
		pos := a2.FindModule(structType.SrcUname)
		decl := a2.Modules[pos].FindDecl(structType.Name)
		if decl == nil || decl.GetObjType() != D2_Struct {
			a2.Logger.Log(fmt.Sprintf("E1208 cannot find struct %s at %s", structType.Name, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		sDecl := decl.(*A2DeclStruct)
		pos = slices.Index(sDecl.MemNames, rname)
		if pos < 0 {
			a2.Logger.Log(fmt.Sprintf("E1209 cannot find %s.%s at %s", structType.Name, rname, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}

		// return op
		var res A2ExprOp
		res.Init(src.Loc, &sDecl.MemTypes[pos], op)
		res.Operand0 = lhs
		res.AccessPos = pos
		return &res

	case E2_StructName: // struct.method
		// check visibility
		if rname[0] < 'A' || rname[0] > 'Z' {
			if rname[0] == '_' { // private
				if lhs.GetType().SrcUname != ct.CurModule.Uname || lhs.GetType().Name != ct.CurFunc.StructNm {
					a2.Logger.Log(fmt.Sprintf("E1210 method %s is private at %s", rname, a2.Logger.GetLoc(src.Loc)), 5, true)
				}
			} else { // protected
				if lhs.GetType().SrcUname != ct.CurModule.Uname {
					a2.Logger.Log(fmt.Sprintf("E1211 method %s is protected at %s", rname, a2.Logger.GetLoc(src.Loc)), 5, true)
				}
			}
		}

		// find method
		pos := a2.FindModule(lhs.GetType().SrcUname)
		fname := lhs.GetType().Name + "." + rname
		decl := a2.Modules[pos].FindDecl(fname)
		if decl == nil || decl.GetObjType() != D2_Func {
			a2.Logger.Log(fmt.Sprintf("E1212 cannot find %s at %s", fname, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}

		// return func_name
		var temp A2ExprName
		temp.Init(E2_FuncName, lhs.GetLocation(), decl)
		return &temp

	case E2_EnumName: // enum.member
		// check visibility
		if (rname[0] < 'A' || rname[0] > 'Z') && lhs.GetType().SrcUname != ct.CurModule.Uname {
			a2.Logger.Log(fmt.Sprintf("E1213 member %s is protected at %s", rname, a2.Logger.GetLoc(src.Loc)), 5, true)
		}

		// find member
		pos := a2.FindModule(lhs.GetType().SrcUname)
		decl := a2.Modules[pos].FindDecl(lhs.GetType().Name)
		if decl == nil || decl.GetObjType() != D2_Enum {
			a2.Logger.Log(fmt.Sprintf("E1214 cannot find enum %s at %s", lhs.GetType().Name, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		i, ok := decl.(*A2DeclEnum).Members[rname]
		nm := lhs.GetType().Name + "." + rname
		if !ok {
			a2.Logger.Log(fmt.Sprintf("E1215 cannot find %s at %s", nm, a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}

		// return literal
		var l front.Literal
		l.Init(i)
		l.EnumInfo = nm
		var temp A2ExprLiteral
		temp.Init(src.Loc, decl.GetType(), l)
		return &temp
	}
	return nil
}

func (a2 *A2Analyzer) convExprOp(src *A1ExprOp, ct *A2Context, forcedTp *A2Type) *A2ExprOp {
	var res A2ExprOp
	var op A2ExprOpT
	exptp := ""
	switch src.SubType {
	case U1_Plus:
		op = U2_Plus
		fallthrough
	case U1_Minus:
		op = U2_Minus
		res.Init(src.Loc, nil, op)
		res.Operand0 = a2.convExpr(src.Operand0, ct, forcedTp)
		res.ExprType = res.Operand0.GetType()

		// check type
		if !res.ExprType.IsThis("int") && !res.ExprType.IsThis("float") {
			a2.Logger.Log(fmt.Sprintf("E1301 invalid type %s for (+, -) at %s", res.ExprType.Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		return &res

	case U1_Inc:
		op = U2_Inc
		exptp = "int"
		fallthrough
	case U1_Dec:
		op = U2_Dec
		exptp = "int"
		fallthrough
	case U1_LogicNot:
		op = U2_LogicNot
		exptp = "bool"
		fallthrough
	case U1_BitNot:
		op = U2_BitNot
		exptp = "int"
		res.Init(src.Loc, nil, op)
		res.Operand0 = a2.convExpr(src.Operand0, ct, forcedTp)
		res.ExprType = res.Operand0.GetType()

		// check type
		if !res.ExprType.IsThis(exptp) {
			a2.Logger.Log(fmt.Sprintf("E1302 invalid type %s for (++, --, !, ~) at %s", res.ExprType.Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		return &res

	case U1_Ref: // if operand0 is rvalue, temp var is created
		if forcedTp == nil {
			res.Init(src.Loc, nil, U2_Ref)
			res.Operand0 = a2.convExpr(src.Operand0, ct, nil)
			var temp A2Type
			temp.Init(T2_Ptr, src.Loc, "*", "")
			temp.Direct = res.Operand0.GetType()
			res.ExprType = &temp
		} else if forcedTp.IsThis("ptr") {
			res.Init(src.Loc, forcedTp, U2_Ref)
			res.Operand0 = a2.convExpr(src.Operand0, ct, forcedTp.Direct)
		} else {
			a2.Logger.Log(fmt.Sprintf("E1303 invalid type %s for & at %s", forcedTp.Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
			return nil
		}
		res.IsLvalue = true
		res.IsConst = res.Operand0.CheckConst()
		return &res

	case U1_Deref:
		res.Init(src.Loc, nil, U2_Deref)
		res.Operand0 = a2.convExpr(src.Operand0, ct, nil)
		if !res.Operand0.GetType().IsThis("ptr") {
			a2.Logger.Log(fmt.Sprintf("E1304 invalid type %s for * at %s", res.Operand0.GetType().Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
		}
		res.ExprType = res.Operand0.GetType().Direct
		if res.ExprType.IsThis("void") {
			a2.Logger.Log(fmt.Sprintf("E1305 cannot dereference void* at %s", a2.Logger.GetLoc(src.Loc)), 5, true)
		}
		res.IsLvalue = true
		res.IsConst = res.Operand0.CheckConst()
		return &res

	case B1_Index: // op0[op1]
		res.Init(src.Loc, nil, B2_Index)
		res.Operand0 = a2.convExpr(src.Operand0, ct, nil)
		res.Operand1 = a2.convExpr(src.Operand1, ct, nil)

		// check type, lvalue
		if res.Operand0.GetType().IsThis("arr") || res.Operand0.GetType().IsThis("slice") || res.Operand0.GetType().IsThis("ptr") {
			res.ExprType = res.Operand0.GetType().Direct
			res.IsLvalue = true
			res.IsConst = res.Operand0.CheckConst()
		} else {
			a2.Logger.Log(fmt.Sprintf("E1306 invalid type %s for [] at %s", res.Operand0.GetType().Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
		}
		if !res.Operand1.GetType().IsThis("int") {
			a2.Logger.Log(fmt.Sprintf("E1307 invalid type %s for [] at %s", res.Operand1.GetType().Print(), a2.Logger.GetLoc(src.Loc)), 5, true)
		}
		if res.ExprType.IsThis("void") {
			a2.Logger.Log(fmt.Sprintf("E1308 cannot index void* at %s", a2.Logger.GetLoc(src.Loc)), 5, true)
		}
		return &res

	case B1_Mul:
	case B1_Div:
	case B1_Mod:
	case B1_Add:
	case B1_Sub:
	case B1_Shl:
	case B1_Shr:
	case B1_Lt:
	case B1_Le:
	case B1_Gt:
	case B1_Ge:
	case B1_Eq:
	case B1_Ne:
	case B1_BitAnd:
	case B1_BitXor:
	case B1_BitOr:
	case B1_LogicAnd:
	case B1_LogicOr:

	case C1_Slice: // op0[op1:op2]

	case C1_Cond: // op0? op1: op2

	case U1_Sizeof: // sizeof(op0)
	case B1_Cast: // cast<tp>(op0)
	case B1_Make: // make(op0, op1)
	case U1_Len: // len(op0)
	case U1_Move: // move(op0)
	default:
		return nil
	}
}

func (a2 *A2Analyzer) convExprFcall(src *A1ExprFCall, ct *A2Context, forcedTp *A2Type) *A2ExprCall {
	return nil
}
