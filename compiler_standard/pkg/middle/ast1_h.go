package middle

import "../front"

// AST1 type node
type A1TypeT int

const (
	T_Auto1 A1TypeT = iota
	T_Primitive1
	T_Ptr1
	T_Arr1
	T_Slice1
	T_Func1
	T_Name1    // struct, enum, template name
	T_Foreign1 // from other module
)

type A1Type struct {
	ObjType  A1TypeT
	Loc      front.Loc
	Name     string
	IncName  string   // for Foreign
	SrcUname string   // src uname
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

func (a1 *A1Type) Clone() A1Type {
	var direct *A1Type
	if a1.Direct != nil {
		t := a1.Direct.Clone()
		direct = &t
	}
	indirect := make([]A1Type, len(a1.Indirect))
	for i, r := range a1.Indirect {
		indirect[i] = r.Clone()
	}
	return A1Type{
		ObjType:  a1.ObjType,
		Loc:      a1.Loc,
		Name:     a1.Name,
		IncName:  a1.IncName,
		SrcUname: a1.SrcUname,
		Direct:   direct,
		Indirect: indirect,
		ArrLen:   a1.ArrLen,
		Size:     a1.Size,
		Align:    a1.Align,
	}
}

// AST1 expression node
type A1ExprT int

const (
	E_Literal1 A1ExprT = iota
	E_LitData1
	E_Name1
	E_Op1
	E_FCall1
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
	a1.ObjType = E_Literal1
	a1.Loc = loc
	a1.Value = value
}

type A1ExprLitData struct {
	A1ExprB
	Elements []A1Expr
}

func (a1 *A1ExprLitData) Init(loc front.Loc) {
	a1.ObjType = E_LitData1
	a1.Loc = loc
	a1.Elements = make([]A1Expr, 0)
}

type A1ExprName struct {
	A1ExprB
	Name string
}

func (a1 *A1ExprName) Init(loc front.Loc, name string) {
	a1.ObjType = E_Name1
	a1.Loc = loc
	a1.Name = name
}

type A1ExprOpT int

const (
	B_Dot1 A1ExprOpT = iota
	B_Index1
	C_Slice1
	U_Plus1
	U_Minus1
	U_LogicNot1
	U_BitNot1
	U_Ref1
	U_Deref1
	B_Mul1
	B_Div1
	B_Mod1
	B_Add1
	B_Sub1
	B_Shl1
	B_Shr1
	B_Lt1
	B_Le1
	B_Gt1
	B_Ge1
	B_Eq1
	B_Ne1
	B_BitAnd1
	B_BitXor1
	B_BitOr1
	B_LogicAnd1
	B_LogicOr1
	C_Cond1
	// Integrated functions
	U_Sizeof1
	B_Cast1
	B_Make1
	U_Len1
)

type A1ExprOp struct {
	A1ExprB
	SubType     A1ExprOpT
	TypeOperand *A1Type // for sizeof, cast
	Operand0    *A1Expr
	Operand1    *A1Expr
	Operand2    *A1Expr
}

func (a1 *A1ExprOp) Init(loc front.Loc, subType A1ExprOpT) {
	a1.ObjType = E_Op1
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
	a1.ObjType = E_FCall1
	a1.Loc = loc
	a1.Func = funcExpr
	a1.Args = make([]A1Expr, 0)
}

// AST1 statement node
type A1StatT int

const (
	RawC1 A1StatT = iota
	RawIR1
	Expr1
	Decl1
	Assign1
	AssignAdd1
	AssignSub1
	AssignMul1
	AssignDiv1
	AssignMod1
	Return1
	Defer1
	Break1
	Continue1
	Fall1
	Scope1
	If1
	While1
	For1
	Switch1
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
	a1.ObjType = RawC1
	a1.Loc = loc
	a1.Code = code
}

type A1StatExpr struct {
	A1StatB
	Expr A1Expr
}

func (a1 *A1StatExpr) Init(loc front.Loc, expr A1Expr) {
	a1.ObjType = Expr1
	a1.Loc = loc
	a1.Expr = expr
}

type A1StatDecl struct {
	A1StatB
	Decl A1Decl
}

func (a1 *A1StatDecl) Init(loc front.Loc, decl A1Decl) {
	a1.ObjType = Decl1
	a1.Loc = loc
	a1.Decl = decl
}

type A1StatAssign struct {
	A1StatB
	Left  A1Expr
	Right A1Expr
}

func (a1 *A1StatAssign) Init(loc front.Loc, left A1Expr, right A1Expr) {
	a1.ObjType = Assign1
	a1.Loc = loc
	a1.Left = left
	a1.Right = right
}

type A1StatCtrl struct {
	A1StatB
	Body *A1Expr // return value or defer body
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
}

func (a1 *A1StatScope) Init(tp A1StatT, loc front.Loc, parent *A1StatScope) {
	a1.ObjType = tp
	a1.Loc = loc
	a1.Parent = parent
	a1.Body = make([]A1Stat, 0)
}

type A1StatIf struct {
	A1StatB
	Cond     A1Expr
	ThenBody *A1StatScope
	ElseBody *A1StatScope
}

func (a1 *A1StatIf) Init(loc front.Loc, cond A1Expr, thenBody *A1StatScope, elseBody *A1StatScope) {
	a1.ObjType = If1
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
	a1.ObjType = While1
	a1.Loc = loc
	a1.Cond = cond
	a1.Body = body
}

type A1StatFor struct {
	A1StatB
	Cond *A1Expr
	Step *A1Stat
	Body *A1StatScope
}

func (a1 *A1StatFor) Init(loc front.Loc, cond *A1Expr, step *A1Stat, body *A1StatScope) {
	a1.ObjType = For1
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
	a1.ObjType = For1
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
	a1.ObjType = Switch1
	a1.Loc = loc
	a1.Cond = cond
	a1.CaseConds = make([]int64, 0)
	a1.CaseBodies = make([][]A1Stat, 0)
	a1.DefaultBody = make([]A1Stat, 0)
}

// AST1 declaration node
type A1DeclT int

const (
	D_RawC1 A1DeclT = iota
	D_RawIR1
	D_Typedef1
	D_Include1
	D_Template1
	D_Var1
	D_Func1
	D_Struct1
	D_Enum1
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
	d.ObjType = D_Include1
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
	d.ObjType = D_Typedef1
	d.Loc = loc
	d.Name = name
	d.Type = t
	d.IsExported = isExported
}

type A1DeclTemplate struct {
	A1DeclB
}

func (d *A1DeclTemplate) Init(loc front.Loc, name string) {
	d.ObjType = D_Template1
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
	d.ObjType = D_Var1
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
	d.ObjType = D_Func1
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
	d.ObjType = D_Struct1
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
	d.ObjType = D_Enum1
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
}

func (m *A1Module) Init(path string, uname string) {
	m.Path = path
	m.Uname = uname
	m.ChunkID = -1
	m.Code = nil
	m.IsFinished = false
}

// A1Parser represents a parser for a single project
type A1Parser struct {
	Logger     front.CplrMsg
	Arch       int
	ChunkCount int
	Modules    []A1Module
	SrcLocSave front.SrcLocSave
}

func (p *A1Parser) Init(arch int, level int) {
	if arch > 0 {
		p.Arch = arch
	} else {
		p.Arch = 8
	}
	if level > 0 {
		p.Logger.Init(level)
	} else {
		p.Logger.Init(3)
	}
	p.ChunkCount = 0
	p.Modules = make([]A1Module, 0, 16)
	p.SrcLocSave.Init()
}
