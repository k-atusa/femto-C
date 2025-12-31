package middle

import (
	"strings"

	"../front"
)

// AST1 type node
type A1TypeT int

const (
	T1_Auto A1TypeT = iota
	T1_Primitive
	T1_Ptr
	T1_Arr
	T1_Slice
	T1_Func
	T1_Name    // struct, enum, template name
	T1_Foreign // from other module
)

type A1Type struct {
	ObjType  A1TypeT
	Loc      front.Loc
	Name     string
	IncName  string   // for Foreign
	SrcUname string   // src uname for struct, enum
	Direct   *A1Type  // ptr target, array elem, func return
	Indirect []A1Type // func args
	ArrLen   int
	Size     int
	Align    int
}

func (a1 *A1Type) Init(tp A1TypeT, loc front.Loc, name string, incName string, srcUname string) {
	a1.ObjType = tp
	a1.Loc = loc
	a1.Name = name
	a1.IncName = incName
	a1.SrcUname = srcUname
	a1.Direct = nil
	a1.Indirect = make([]A1Type, 0)
	a1.ArrLen = -1
	a1.Size = -1
	a1.Align = -1
}

// AST1 expression node
type A1ExprT int

const (
	E1_Literal A1ExprT = iota
	E1_LitData
	E1_Name
	E1_Op
	E1_FCall
)

type A1Expr interface {
	GetLocation() front.Loc
	GetObjType() A1ExprT
}

type A1ExprB struct {
	Loc     front.Loc
	ObjType A1ExprT
}

func (b *A1ExprB) GetLocation() front.Loc { return b.Loc }
func (b *A1ExprB) GetObjType() A1ExprT    { return b.ObjType }

type A1ExprLiteral struct {
	A1ExprB
	Value front.Literal
}

func (a1 *A1ExprLiteral) Init(loc front.Loc, value front.Literal) {
	a1.ObjType = E1_Literal
	a1.Loc = loc
	a1.Value = value
}

type A1ExprLitData struct {
	A1ExprB
	Elements []A1Expr
}

func (a1 *A1ExprLitData) Init(loc front.Loc) {
	a1.ObjType = E1_LitData
	a1.Loc = loc
	a1.Elements = make([]A1Expr, 0)
}

type A1ExprName struct {
	A1ExprB
	Name string
}

func (a1 *A1ExprName) Init(loc front.Loc, name string) {
	a1.ObjType = E1_Name
	a1.Loc = loc
	a1.Name = name
}

type A1ExprOpT int

const (
	// unary
	U1_Plus A1ExprOpT = iota
	U1_Minus
	U1_LogicNot
	U1_BitNot
	U1_Ref
	U1_Deref
	U1_Inc
	U1_Dec
	// binary
	B1_Dot
	B1_Index
	B1_Mul
	B1_Div
	B1_Mod
	B1_Add
	B1_Sub
	B1_Shl
	B1_Shr
	B1_Lt
	B1_Le
	B1_Gt
	B1_Ge
	B1_Eq
	B1_Ne
	B1_BitAnd
	B1_BitXor
	B1_BitOr
	B1_LogicAnd
	B1_LogicOr
	// cubic
	C1_Slice
	C1_Cond
	// integrated functions
	U1_Sizeof
	B1_Cast
	B1_Make
	U1_Len
	U1_Move
)

type A1ExprOp struct {
	A1ExprB
	SubType     A1ExprOpT
	TypeOperand *A1Type // for sizeof, cast
	Operand0    A1Expr
	Operand1    A1Expr
	Operand2    A1Expr
}

func (a1 *A1ExprOp) Init(loc front.Loc, subType A1ExprOpT) {
	a1.ObjType = E1_Op
	a1.Loc = loc
	a1.SubType = subType
	a1.TypeOperand = nil
	a1.Operand0 = nil
	a1.Operand1 = nil
	a1.Operand2 = nil
}

type A1ExprFCall struct {
	A1ExprB
	Func A1Expr
	Args []A1Expr
}

func (a1 *A1ExprFCall) Init(loc front.Loc, funcExpr A1Expr) {
	a1.ObjType = E1_FCall
	a1.Loc = loc
	a1.Func = funcExpr
	a1.Args = make([]A1Expr, 0)
}

// AST1 statement node
type A1StatT int

const (
	S1_RawC A1StatT = iota
	S1_RawIR
	S1_Expr
	S1_Decl
	S1_Assign
	S1_AssignAdd
	S1_AssignSub
	S1_AssignMul
	S1_AssignDiv
	S1_AssignMod
	S1_Return
	S1_Defer
	S1_Break
	S1_Continue
	S1_Fall
	S1_Scope
	S1_If
	S1_While
	S1_For
	S1_Foreach
	S1_Switch
)

type A1Stat interface {
	GetLocation() front.Loc
	GetObjType() A1StatT
}

type A1StatB struct {
	Loc     front.Loc
	ObjType A1StatT
}

func (b *A1StatB) GetLocation() front.Loc { return b.Loc }
func (b *A1StatB) GetObjType() A1StatT    { return b.ObjType }

type A1StatRaw struct {
	A1StatB
	Code string
}

func (a1 *A1StatRaw) Init(loc front.Loc, code string) {
	a1.ObjType = S1_RawC
	a1.Loc = loc
	a1.Code = code
}

type A1StatExpr struct {
	A1StatB
	Expr A1Expr
}

func (a1 *A1StatExpr) Init(loc front.Loc, expr A1Expr) {
	a1.ObjType = S1_Expr
	a1.Loc = loc
	a1.Expr = expr
}

type A1StatDecl struct {
	A1StatB
	Decl A1Decl
}

func (a1 *A1StatDecl) Init(loc front.Loc, decl A1Decl) {
	a1.ObjType = S1_Decl
	a1.Loc = loc
	a1.Decl = decl
}

type A1StatAssign struct {
	A1StatB
	Left  A1Expr
	Right A1Expr
}

func (a1 *A1StatAssign) Init(loc front.Loc, left A1Expr, right A1Expr) {
	a1.ObjType = S1_Assign
	a1.Loc = loc
	a1.Left = left
	a1.Right = right
}

type A1StatCtrl struct {
	A1StatB
	Body A1Expr // return value or defer body
}

func (a1 *A1StatCtrl) Init(tp A1StatT, loc front.Loc) {
	a1.ObjType = tp
	a1.Loc = loc
	a1.Body = nil
}

type A1StatScope struct {
	A1StatB
	Parent *A1StatScope
	Body   []A1Stat
	Decls  map[string]A1Decl
}

func (a1 *A1StatScope) Init(tp A1StatT, loc front.Loc, parent *A1StatScope) {
	a1.ObjType = tp
	a1.Loc = loc
	a1.Parent = parent
	a1.Body = make([]A1Stat, 0)
	a1.Decls = make(map[string]A1Decl)
}

func (a1 *A1StatScope) FindDecl(name string) A1Decl {
	if decl, ok := a1.Decls[name]; ok {
		return decl
	}
	if a1.Parent != nil {
		return a1.Parent.FindDecl(name)
	}
	return nil
}

func (a1 *A1StatScope) FindLiteral(name string) *front.Literal {
	if decl, ok := a1.Decls[name]; ok {
		if decl.GetObjType() == D1_Var {
			t, ok := decl.(*A1DeclVar)
			if ok && t.IsDefine {
				return &t.InitExpr.(*A1ExprLiteral).Value
			}
		}
	}
	if a1.Parent != nil {
		return a1.Parent.FindLiteral(name)
	}
	return nil
}

type A1StatIf struct {
	A1StatB
	Cond     A1Expr
	ThenBody *A1StatScope
	ElseBody *A1StatScope
}

func (a1 *A1StatIf) Init(loc front.Loc, cond A1Expr, thenBody *A1StatScope, elseBody *A1StatScope) {
	a1.ObjType = S1_If
	a1.Loc = loc
	a1.Cond = cond
	a1.ThenBody = thenBody
	a1.ElseBody = elseBody
}

type A1StatWhile struct {
	A1StatB
	Cond A1Expr
	Body *A1StatScope
}

func (a1 *A1StatWhile) Init(loc front.Loc, cond A1Expr, body *A1StatScope) {
	a1.ObjType = S1_While
	a1.Loc = loc
	a1.Cond = cond
	a1.Body = body
}

type A1StatFor struct {
	A1StatB
	Cond A1Expr
	Step A1Stat
	Body *A1StatScope
}

func (a1 *A1StatFor) Init(loc front.Loc, cond A1Expr, step A1Stat, body *A1StatScope) {
	a1.ObjType = S1_For
	a1.Loc = loc
	a1.Cond = cond
	a1.Step = step
	a1.Body = body
}

type A1StatForeach struct {
	A1StatB
	Iter A1Expr
	Body *A1StatScope
}

func (a1 *A1StatForeach) Init(loc front.Loc, iter A1Expr, body *A1StatScope) {
	a1.ObjType = S1_Foreach
	a1.Loc = loc
	a1.Iter = iter
	a1.Body = body
}

type A1StatSwitch struct {
	A1StatB
	Cond        A1Expr
	CaseConds   []int64
	CaseBodies  [][]A1Stat
	DefaultBody []A1Stat
}

func (a1 *A1StatSwitch) Init(loc front.Loc, cond A1Expr) {
	a1.ObjType = S1_Switch
	a1.Loc = loc
	a1.Cond = cond
	a1.CaseConds = make([]int64, 0)
	a1.CaseBodies = make([][]A1Stat, 0)
	a1.DefaultBody = make([]A1Stat, 0)
}

// AST1 declaration node
type A1DeclT int

const (
	D1_RawC A1DeclT = iota
	D1_RawIR
	D1_Typedef
	D1_Include
	D1_Template
	D1_Var
	D1_Func
	D1_Struct
	D1_Enum
)

type A1Decl interface {
	GetLocation() front.Loc
	GetObjType() A1DeclT
	GetName() string
	GetType() *A1Type
	GetIsExported() bool
}

type A1DeclB struct {
	Loc        front.Loc
	ObjType    A1DeclT
	Name       string
	Type       *A1Type
	IsExported bool
}

func (d *A1DeclB) GetLocation() front.Loc { return d.Loc }
func (d *A1DeclB) GetObjType() A1DeclT    { return d.ObjType }
func (d *A1DeclB) GetName() string        { return d.Name }
func (d *A1DeclB) GetType() *A1Type       { return d.Type }
func (d *A1DeclB) GetIsExported() bool    { return d.IsExported }

type A1DeclRaw struct {
	A1DeclB
	Code string
}

func (d *A1DeclRaw) Init(tp A1DeclT, loc front.Loc, code string) {
	d.ObjType = tp
	d.Loc = loc
	d.Name = ""
	d.Type = nil
	d.IsExported = false
	d.Code = code
}

type A1DeclInclude struct {
	A1DeclB
	TgtPath  string
	TgtUname string
	ArgTypes []A1Type
}

func (d *A1DeclInclude) Init(loc front.Loc, name string, tgtPath string, tgtUname string, argTypes []A1Type) {
	d.ObjType = D1_Include
	d.Loc = loc
	d.Name = name
	d.Type = nil
	d.IsExported = false
	d.TgtPath = tgtPath
	d.TgtUname = tgtUname
	d.ArgTypes = argTypes
}

type A1DeclTypedef struct {
	A1DeclB
}

func (d *A1DeclTypedef) Init(loc front.Loc, name string, t *A1Type, isExported bool) {
	d.ObjType = D1_Typedef
	d.Loc = loc
	d.Name = name
	d.Type = t
	d.IsExported = isExported
}

type A1DeclTemplate struct {
	A1DeclB
}

func (d *A1DeclTemplate) Init(loc front.Loc, name string) {
	d.ObjType = D1_Template
	d.Loc = loc
	d.Name = name
	d.Type = nil
	d.IsExported = false
}

type A1DeclVar struct {
	A1DeclB
	InitExpr   A1Expr
	IsDefine   bool
	IsConst    bool
	IsVolatile bool
	IsExtern   bool
	IsParam    bool
}

func (d *A1DeclVar) Init(loc front.Loc, name string, t *A1Type, initExpr A1Expr, isExported bool) {
	d.ObjType = D1_Var
	d.Loc = loc
	d.Name = name
	d.Type = t
	d.IsExported = isExported
	d.InitExpr = initExpr

	d.IsDefine = false
	d.IsConst = false
	d.IsVolatile = false
	d.IsExtern = false
	d.IsParam = false
}

type A1DeclFunc struct {
	A1DeclB
	StructNm   string
	FuncNm     string
	ParamTypes []A1Type
	ParamNames []string
	RetType    *A1Type
	Body       *A1StatScope
	IsVaArg    bool
}

func (d *A1DeclFunc) Init(loc front.Loc, fullNm string, t *A1Type, structNm string, funcNm string, isExported bool) {
	d.ObjType = D1_Func
	d.Loc = loc
	d.Name = fullNm
	d.Type = t
	d.StructNm = structNm
	d.FuncNm = funcNm
	d.ParamTypes = make([]A1Type, 0)
	d.ParamNames = make([]string, 0)
	d.RetType = nil
	d.IsExported = isExported
	d.Body = nil
	d.IsVaArg = false
}

type A1DeclStruct struct {
	A1DeclB
	MemTypes   []A1Type
	MemNames   []string
	MemOffsets []int
}

func (d *A1DeclStruct) Init(loc front.Loc, name string, isExported bool) {
	d.ObjType = D1_Struct
	d.Loc = loc
	d.Name = name
	d.Type = nil
	d.IsExported = isExported
	d.MemTypes = make([]A1Type, 0)
	d.MemNames = make([]string, 0)
	d.MemOffsets = make([]int, 0)
}

type A1DeclEnum struct {
	A1DeclB
	MemNames  []string
	MemValues []int64
}

func (d *A1DeclEnum) Init(loc front.Loc, name string, isExported bool) {
	d.ObjType = D1_Enum
	d.Loc = loc
	d.Name = name
	d.Type = nil
	d.IsExported = isExported
	d.MemNames = make([]string, 0)
	d.MemValues = make([]int64, 0)
}

// A1Module represents a single source file AST
type A1Module struct {
	Path       string
	Uname      string
	ChunkID    int
	Code       *A1StatScope
	IsFinished bool

	tp  *front.TokenProvider // token provider for pass3
	idx []int                // pass3 index
}

func (m *A1Module) Init(path string, uname string) {
	m.Path = path
	m.Uname = uname
	m.ChunkID = -1
	m.Code = nil
	m.IsFinished = false
	m.tp = nil
	m.idx = make([]int, 0, 32)
}

func (m *A1Module) FindDecl(name string, chkExported bool) A1Decl {
	d := m.Code.FindDecl(name)
	if d == nil || !chkExported {
		return nil
	}
	switch d.GetObjType() {
	case D1_Var, D1_Struct, D1_Enum, D1_Typedef:
		nm := d.GetName()
		if 'A' <= nm[0] && nm[0] <= 'Z' {
			return d
		}
	case D1_Func:
		t := d.(*A1DeclFunc)
		if t.StructNm == "" {
			if 'A' <= t.Name[0] && t.Name[0] <= 'Z' { // global function
				return d
			}
		} else {
			if 'A' <= t.StructNm[0] && t.StructNm[0] <= 'Z' && 'A' <= t.FuncNm[0] && t.FuncNm[0] <= 'Z' { // methods
				return d
			}
		}
	default:
		return nil
	}
	return nil
}

func (m *A1Module) FindLiteral(name string, chkExported bool) *front.Literal {
	if strings.Contains(name, ".") { // enum member
		enumNm := strings.Split(name, ".")[0]
		memberNm := strings.Split(name, ".")[1]
		if chkExported && !('A' <= enumNm[0] && enumNm[0] <= 'Z' && 'A' <= memberNm[0] && memberNm[0] <= 'Z') {
			return nil
		}
		d := m.Code.FindDecl(enumNm)
		if d.GetObjType() != D1_Enum {
			return nil
		}
		e := d.(*A1DeclEnum)
		for i, nm := range e.MemNames {
			if nm == memberNm {
				var l front.Literal
				l.Init(e.MemValues[i])
				return &l
			}
		}
		return nil

	} else { // defined literal
		if chkExported && !('A' <= name[0] && name[0] <= 'Z') {
			return nil
		}
		return m.Code.FindLiteral(name)
	}
}

func (m *A1Module) IsNameUsable(name string) bool {
	_, ok := m.Code.Decls[name] // fullname(enum, struct.func, ...) only
	return !ok
}

// A1Parser represents a parser for a single project
type A1Parser struct {
	Logger     *front.CplrMsg
	Arch       int
	ChunkCount int
	Modules    []A1Module
}

func (p *A1Parser) Init(arch int, log *front.CplrMsg) {
	if arch > 0 {
		p.Arch = arch
	} else {
		p.Arch = 8
	}
	p.Logger = log
	p.ChunkCount = 0
	p.Modules = make([]A1Module, 0, 16)
}

func (p *A1Parser) FindModule(path string) int {
	for i, m := range p.Modules {
		if m.Path == path {
			return i
		}
	}
	return -1
}
