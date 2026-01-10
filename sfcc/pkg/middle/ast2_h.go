// test787e : femto-C middle.ast2_h R0

package middle

import (
	"sync"
	"sync/atomic"

	"../front"
)

// AST2 type node
type A2TypeT int

const (
	T2_Primitive A2TypeT = iota
	T2_Ptr
	T2_Arr
	T2_Slice
	T2_Func
	T2_Struct
	T2_Enum
)

type A2Type struct {
	ObjType  A2TypeT
	Loc      front.Loc
	Name     string
	SrcUname string   // src uname for struct, enum
	Direct   *A2Type  // ptr target, array elem, func return
	Indirect []A2Type // func args
	ArrLen   int
	Size     int
	Align    int
}

func (a2 *A2Type) Init(tp A2TypeT, loc front.Loc, name string, srcUname string) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.Name = name
	a2.SrcUname = srcUname
	a2.Direct = nil
	a2.Indirect = make([]A2Type, 0)
	a2.ArrLen = -1
	a2.Size = -1
	a2.Align = -1
}

// AST2 expr node
type A2ExprT int

const (
	E2_Literal A2ExprT = iota
	E2_LitData
	E2_Op
	E2_VarName
	E2_FuncName
	E2_StructName // inside AST2 only
	E2_EnumName   // inside AST2 only
	E2_FuncCall
	E2_FptrCall
)

type A2Expr interface {
	GetLocation() front.Loc
	GetObjType() A2ExprT
	GetType() *A2Type
	CheckLvalue() bool
	CheckConst() bool
}

type A2ExprB struct {
	Loc      front.Loc
	ObjType  A2ExprT
	ExprType *A2Type
	IsLvalue bool
	IsConst  bool
}

func (b *A2ExprB) GetLocation() front.Loc { return b.Loc }
func (b *A2ExprB) GetObjType() A2ExprT    { return b.ObjType }
func (b *A2ExprB) GetType() *A2Type       { return b.ExprType }
func (b *A2ExprB) CheckLvalue() bool      { return b.IsLvalue }
func (b *A2ExprB) CheckConst() bool       { return b.IsConst }

type A2ExprLiteral struct {
	A2ExprB
	Value front.Literal
}

func (a2 *A2ExprLiteral) Init(loc front.Loc, ty *A2Type, value front.Literal) {
	a2.ObjType = E2_Literal
	a2.Loc = loc
	a2.ExprType = ty
	a2.IsLvalue = false
	a2.IsConst = false
	a2.Value = value
}

type A2ExprLitData struct {
	A2ExprB
	Elements []A2Expr
}

func (a2 *A2ExprLitData) Init(loc front.Loc, ty *A2Type) {
	a2.ObjType = E2_LitData
	a2.Loc = loc
	a2.ExprType = ty
	a2.IsLvalue = false
	a2.IsConst = false
	a2.Elements = make([]A2Expr, 0)
}

type A2ExprOpT int

const (
	// unary
	U2_Plus A2ExprOpT = iota
	U2_Minus
	U2_LogicNot
	U2_BitNot
	U2_Ref
	U2_Deref
	U2_Inc
	U2_Dec
	// binary
	B2_Dot
	B2_Arrow
	B2_Index
	B2_Mul
	B2_Div
	B2_Mod
	B2_Add
	B2_Sub
	B2_Shl
	B2_Shr
	B2_Lt
	B2_Le
	B2_Gt
	B2_Ge
	B2_Eq
	B2_Ne
	B2_BitAnd
	B2_BitXor
	B2_BitOr
	B2_LogicAnd
	B2_LogicOr
	// cubic
	C2_Slice
	C2_Cond
	// integrated functions
	U2_Sizeof
	B2_Cast
	B2_Make
	U2_Len
	U2_Move
)

type A2ExprOp struct {
	A2ExprB
	SubType     A2ExprOpT
	TypeOperand *A2Type // for sizeof
	Operand0    A2Expr
	Operand1    A2Expr
	Operand2    A2Expr
	AccessPos   int // sturct member index
}

func (a2 *A2ExprOp) Init(loc front.Loc, ty *A2Type, subType A2ExprOpT) {
	a2.ObjType = E2_Op
	a2.Loc = loc
	a2.ExprType = ty
	a2.IsLvalue = false
	a2.IsConst = false
	a2.SubType = subType
	a2.TypeOperand = nil
	a2.Operand0 = nil
	a2.Operand1 = nil
	a2.Operand2 = nil
	a2.AccessPos = -1
}

type A2ExprName struct {
	A2ExprB
	Decl A2Decl // points to var, func, struct, enum
}

func (a2 *A2ExprName) Init(tp A2ExprT, loc front.Loc, decl A2Decl) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.ExprType = decl.GetType()
	a2.IsLvalue = true
	a2.IsConst = false
	a2.Decl = decl
}

type A2ExprCall struct {
	A2ExprB
	FuncDecl *A2DeclFunc // for static function call
	FuncExpr A2Expr      // for function ptr call
	Args     []A2Expr
}

func (a2 *A2ExprCall) Init(tp A2ExprT, loc front.Loc, funcDecl *A2DeclFunc, funcExpr A2Expr) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.IsLvalue = false
	a2.IsConst = false
	a2.FuncDecl = funcDecl
	a2.FuncExpr = funcExpr
	a2.Args = make([]A2Expr, 0)

	if funcDecl != nil {
		a2.ExprType = funcDecl.GetType().Direct
	}
	if funcExpr != nil {
		a2.ExprType = funcExpr.GetType().Direct
	}
}

// AST2 statement node
type A2StatT int

const (
	S2_RawC A2StatT = iota
	S2_RawIR
	S2_Expr
	S2_Decl
	S2_Assign
	S2_AssignAdd
	S2_AssignSub
	S2_AssignMul
	S2_AssignDiv
	S2_AssignMod
	S2_Return
	S2_Break
	S2_Continue
	S2_Scope
	S2_If
	S2_While
	S2_For
	S2_Foreach
	S2_Switch
)

type A2Stat interface {
	GetLocation() front.Loc
	GetObjType() A2StatT
	GetUid() int64
	CheckReturn() bool
}

type A2StatB struct {
	Loc       front.Loc
	ObjType   A2StatT
	Uid       int64
	IsReturns bool
}

func (b *A2StatB) GetLocation() front.Loc { return b.Loc }
func (b *A2StatB) GetObjType() A2StatT    { return b.ObjType }
func (b *A2StatB) GetUid() int64          { return b.Uid }
func (b *A2StatB) CheckReturn() bool      { return b.IsReturns }

type A2StatLoop interface {
	A2Stat
	SetLoopBody(body A2Stat)
}

type A2StatJump struct {
	Scope *A2StatScope // scope to escape
	Index int          // defer index
}

func (a2 *A2StatJump) Init(scope *A2StatScope, index int) {
	a2.Scope = scope
	a2.Index = index
}

type A2StatRaw struct {
	A2StatB
	Code string
}

func (a2 *A2StatRaw) Init(tp A2StatT, loc front.Loc, uid int64, code string) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Code = code
}

type A2StatExpr struct {
	A2StatB
	Expr A2Expr
}

func (a2 *A2StatExpr) Init(tp A2StatT, loc front.Loc, uid int64, expr A2Expr) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Expr = expr
}

type A2StatDecl struct {
	A2StatB
	Decl A2Decl
}

func (a2 *A2StatDecl) Init(tp A2StatT, loc front.Loc, decl A2Decl) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.Uid = decl.GetUid()
	a2.IsReturns = false
	a2.Decl = decl
}

type A2StatAssign struct {
	A2StatB
	Left  A2Expr
	Right A2Expr
}

func (a2 *A2StatAssign) Init(tp A2StatT, loc front.Loc, uid int64, left A2Expr, right A2Expr) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Left = left
	a2.Right = right
}

type A2StatCtrl struct {
	A2StatB
	RetValue A2Expr     // for return
	Loop     A2StatLoop // break, continue tgt
	Escape   A2StatJump // escape tgt
}

func (a2 *A2StatCtrl) Init(tp A2StatT, loc front.Loc, uid int64) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = tp == S2_Return
	a2.RetValue = nil
	a2.Loop = nil
	a2.Escape.Init(nil, -1)
}

type A2StatScope struct {
	A2StatB
	// name finder
	Parent *A2StatScope
	Decls  map[string]A2Decl

	// control infos
	Defers     []A2Expr   // saved same as declared order (non-stacked now)
	Escape     A2StatJump // escape tgt
	IsFuncBody bool
	IsLoopBody bool

	Body []A2Stat
}

func (a2 *A2StatScope) Init(loc front.Loc, uid int64, parent *A2StatScope) {
	a2.ObjType = S2_Scope
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Parent = parent
	a2.Decls = make(map[string]A2Decl)
	a2.Defers = make([]A2Expr, 0)
	a2.IsFuncBody = false
	a2.IsLoopBody = false
	a2.Body = make([]A2Stat, 0)
	a2.Escape.Init(nil, -1)
}

type A2StatIf struct {
	A2StatB
	Cond A2Expr
	Then A2Stat
	Else A2Stat
}

func (a2 *A2StatIf) Init(loc front.Loc, uid int64, cond A2Expr, thenStat A2Stat, elseStat A2Stat) {
	a2.ObjType = S2_If
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Cond = cond
	a2.Then = thenStat
	a2.Else = elseStat
}

type A2StatWhile struct {
	A2StatB
	Cond A2Expr
	Body A2Stat
}

func (a2 *A2StatWhile) Init(loc front.Loc, uid int64, cond A2Expr, bodyStat A2Stat) {
	a2.ObjType = S2_While
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Cond = cond
	a2.Body = bodyStat
}

func (a2 *A2StatWhile) SetLoopBody(bodyStat A2Stat) {
	a2.Body = bodyStat
}

type A2StatFor struct {
	A2StatB // for_init is at upper scope
	Cond    A2Expr
	Step    A2Expr
	Body    A2Stat
}

func (a2 *A2StatFor) Init(loc front.Loc, uid int64, cond A2Expr, step A2Expr, bodyStat A2Stat) {
	a2.ObjType = S2_For
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Cond = cond
	a2.Step = step
	a2.Body = bodyStat
}

func (a2 *A2StatFor) SetLoopBody(bodyStat A2Stat) {
	a2.Body = bodyStat
}

type A2StatForeach struct {
	A2StatB
	Var_i *A2DeclVar
	Var_r *A2DeclVar
	Iter  A2Expr
	Body  A2Stat
}

func (a2 *A2StatForeach) Init(loc front.Loc, uid int64, var_i *A2DeclVar, var_r *A2DeclVar, iter A2Expr, bodyStat A2Stat) {
	a2.ObjType = S2_Foreach
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Var_i = var_i
	a2.Var_r = var_r
	a2.Iter = iter
	a2.Body = bodyStat
}

func (a2 *A2StatForeach) SetLoopBody(bodyStat A2Stat) {
	a2.Body = bodyStat
}

type A2StatSwitch struct {
	A2StatB
	Cond        A2Expr
	CaseConds   []int64
	CaseFalls   []bool
	CaseBodies  []A2StatScope
	DefaultBody *A2StatScope
}

func (a2 *A2StatSwitch) Init(loc front.Loc, uid int64, cond A2Expr) {
	a2.ObjType = S2_Switch
	a2.Loc = loc
	a2.Uid = uid
	a2.IsReturns = false
	a2.Cond = cond
	a2.CaseConds = make([]int64, 0)
	a2.CaseFalls = make([]bool, 0)
	a2.CaseBodies = make([]A2StatScope, 0)
	a2.DefaultBody = nil
}

// AST2 declaration node
type A2DeclT int

const (
	D2_RawC A2DeclT = iota
	D2_RawIR
	D2_Var
	D2_Func
	D2_Struct
	D2_Enum
)

type A2Decl interface {
	GetLocation() front.Loc
	GetObjType() A2DeclT
	GetName() string
	GetSrcUname() string
	GetType() *A2Type
	GetUid() int64
	CheckExported() bool
}

type A2DeclB struct {
	Loc        front.Loc
	ObjType    A2DeclT
	Name       string
	SrcUname   string
	DeclType   *A2Type
	Uid        int64
	IsExported bool
}

func (b *A2DeclB) GetLocation() front.Loc { return b.Loc }
func (b *A2DeclB) GetObjType() A2DeclT    { return b.ObjType }
func (b *A2DeclB) GetName() string        { return b.Name }
func (b *A2DeclB) GetSrcUname() string    { return b.SrcUname }
func (b *A2DeclB) GetType() *A2Type       { return b.DeclType }
func (b *A2DeclB) GetUid() int64          { return b.Uid }
func (b *A2DeclB) CheckExported() bool    { return b.IsExported }

type A2DeclRaw struct {
	A2DeclB
	Code string
}

func (a2 *A2DeclRaw) Init(tp A2DeclT, loc front.Loc, uid int64, code string) {
	a2.ObjType = tp
	a2.Loc = loc
	a2.Name = ""
	a2.SrcUname = ""
	a2.DeclType = nil
	a2.Uid = uid
	a2.IsExported = false
	a2.Code = code
}

type A2DeclVar struct {
	A2DeclB
	InitExpr   A2Expr
	IsDefine   bool
	IsConst    bool
	IsVolatile bool
	IsExtern   bool
	IsParam    bool
}

func (a2 *A2DeclVar) Init(loc front.Loc, uid int64, name string, srcUname string, ty *A2Type) {
	a2.ObjType = D2_Var
	a2.Loc = loc
	a2.Name = name
	a2.SrcUname = srcUname
	a2.DeclType = ty
	a2.Uid = uid
	a2.IsExported = false

	a2.InitExpr = nil
	a2.IsDefine = false
	a2.IsConst = false
	a2.IsVolatile = false
	a2.IsExtern = false
	a2.IsParam = false
}

type A2DeclFunc struct {
	A2DeclB
	StructNm   string // for method
	FuncNm     string // for method
	Params     []string
	Body       *A2StatScope
	IsVaArg    bool // va_arg flag
	IsVaArg_ad bool // extended va_arg
}

func (a2 *A2DeclFunc) Init(loc front.Loc, uid int64, name string, srcUname string, ty *A2Type) {
	a2.ObjType = D2_Func
	a2.Loc = loc
	a2.Name = name
	a2.SrcUname = srcUname
	a2.DeclType = ty
	a2.Uid = uid
	a2.IsExported = false

	a2.StructNm = ""
	a2.FuncNm = ""
	a2.Params = make([]string, 0)
	a2.Body = nil
	a2.IsVaArg = false
	a2.IsVaArg_ad = false
}

type A2DeclStruct struct {
	A2DeclB
	MemTypes   []A2Type
	MemNames   []string
	MemOffsets []int
}

func (a2 *A2DeclStruct) Init(loc front.Loc, uid int64, name string, srcUname string, ty *A2Type) {
	a2.ObjType = D2_Struct
	a2.Loc = loc
	a2.Name = name
	a2.SrcUname = srcUname
	a2.DeclType = ty
	a2.Uid = uid
	a2.IsExported = false

	a2.MemTypes = make([]A2Type, 0)
	a2.MemNames = make([]string, 0)
	a2.MemOffsets = make([]int, 0)
}

type A2DeclEnum struct {
	A2DeclB
	Members map[string]int64
}

func (a2 *A2DeclEnum) Init(loc front.Loc, uid int64, name string, srcUname string, ty *A2Type) {
	a2.ObjType = D2_Enum
	a2.Loc = loc
	a2.Name = name
	a2.SrcUname = srcUname
	a2.DeclType = ty
	a2.Uid = uid
	a2.IsExported = false
	a2.Members = make(map[string]int64)
}

// AST2 module
type A2Module struct {
	PathIdx int
	Uname   string
	ChunkID int
	Code    *A2StatScope
}

func (a2 *A2Module) Init(pathIdx int, uname string, chunkID int) {
	a2.PathIdx = pathIdx
	a2.Uname = uname
	a2.ChunkID = chunkID
	a2.Code = nil
}

func (a2 *A2Module) FindDecl(name string) A2Decl {
	if decl, ok := a2.Code.Decls[name]; ok {
		return decl // global declaration
	}
	return nil
}

// AST2 analyzer
type A2Context struct { // context for function analysis
	Mt_idx    int
	CurModule *A2Module
	CurFunc   *A2DeclFunc
	CurRType  *A2Type

	TopScope *A2StatScope   // toplevel scope
	Scopes   []*A2StatScope // local scope
	Loops    []A2StatLoop   // loop info
	Escape   A2StatJump     // escape tgt
}

func (c *A2Context) Init(mt_idx int, a2 *A2Analyzer) {
	c.Mt_idx = mt_idx
	c.CurModule = &a2.Modules[mt_idx]
	c.CurFunc = nil
	c.CurRType = nil
	c.TopScope = c.CurModule.Code
	c.Scopes = make([]*A2StatScope, 0, 16)
	c.Loops = make([]A2StatLoop, 0, 8)
	c.Escape.Init(nil, -1)
}

func (c *A2Context) FindVar(name string) *A2DeclVar {
	for i := len(c.Scopes) - 1; i >= 0; i-- {
		if decl, ok := c.Scopes[i].Decls[name]; ok {
			if decl.GetObjType() == D2_Var {
				return decl.(*A2DeclVar) // local variable
			}
		}
	}
	if decl, ok := c.TopScope.Decls[name]; ok {
		if decl.GetObjType() == D2_Var {
			return decl.(*A2DeclVar) // global variable
		}
	}
	return nil
}

func (c *A2Context) FindDomain(name string) int {
	if c.FindVar(name) != nil {
		return 0 // variable
	}
	d := c.CurModule.FindDecl(name)
	switch d.GetObjType() {
	case D2_Var:
		return 0 // variable
	case D2_Func:
		return 1 // function
	case D2_Struct:
		return 2 // struct
	case D2_Enum:
		return 3 // enum
	}
	return -1 // not found
}

type A2Analyzer struct { // global analyzer
	Logger   *front.CplrMsg
	Arch     int
	Modules  []A2Module
	ModMap   map[string]int
	UidCount int64

	// analyzer context
	a1    *A1Parser
	tpool sync.Map

	// multithread
	Mt_flag bool
	Mt_wg   sync.WaitGroup
	Mt_ret  chan bool
}

func (a2 *A2Analyzer) Init(logger *front.CplrMsg, arch int, ast1 *A1Parser, mt_cfg int) {
	a2.Logger = logger
	a2.Arch = arch
	a2.Modules = make([]A2Module, 0)
	a2.ModMap = make(map[string]int) // module uname -> index
	a2.UidCount = 0
	a2.a1 = ast1
	a2.Mt_flag = mt_cfg > 0 // set (mt_cfg > 0) to enable multithread
	a2.Mt_wg = sync.WaitGroup{}
	a2.Mt_ret = make(chan bool, mt_cfg)
	a2.initTypes()
}

func (a2 *A2Analyzer) FindModule(uname string) int {
	if idx, ok := a2.ModMap[uname]; ok {
		return idx
	}
	return -1
}

func (a2 *A2Analyzer) GetUid() int64 {
	if a2.Mt_flag {
		return atomic.AddInt64(&a2.UidCount, 1)
	} else {
		a2.UidCount++
		return a2.UidCount
	}
}

func (a2 *A2Analyzer) StoreType(name string, ty *A2Type) {
	a2.tpool.Store(name, ty)
}

func (a2 *A2Analyzer) LoadType(name string) *A2Type {
	if v, ok := a2.tpool.Load(name); ok {
		return v.(*A2Type)
	}
	return nil
}

func (a2 *A2Analyzer) initTypes() {
	getp := func(name string, size int, align int) *A2Type {
		var res A2Type
		res.Init(T2_Primitive, front.Loc{}, name, "")
		res.Size = size
		res.Align = align
		return &res
	}
	getd := func(name string, size int, align int, tp A2TypeT, d *A2Type) *A2Type {
		var res A2Type
		res.Init(tp, front.Loc{}, name, "")
		res.Size = size
		res.Align = align
		res.Direct = d
		return &res
	}

	a2.StoreType("int", getp("int", a2.Arch, a2.Arch))
	a2.StoreType("i8", getp("i8", 1, 1))
	a2.StoreType("i16", getp("i16", 2, 2))
	a2.StoreType("i32", getp("i32", 4, 4))
	a2.StoreType("i64", getp("i64", 8, 8))

	a2.StoreType("uint", getp("uint", a2.Arch, a2.Arch))
	a2.StoreType("u8", getp("u8", 1, 1))
	a2.StoreType("u16", getp("u16", 2, 2))
	a2.StoreType("u32", getp("u32", 4, 4))
	a2.StoreType("u64", getp("u64", 8, 8))

	a2.StoreType("f32", getp("f32", 4, 4))
	a2.StoreType("f64", getp("f64", 8, 8))
	a2.StoreType("bool", getp("bool", 1, 1))
	a2.StoreType("void", getp("void", 0, 1))

	a2.StoreType("u8*", getd("*", a2.Arch, a2.Arch, T2_Ptr, a2.LoadType("u8")))
	a2.StoreType("u8[]", getd("[]", 2*a2.Arch, a2.Arch, T2_Slice, a2.LoadType("u8")))
	a2.StoreType("int[]", getd("[]", 2*a2.Arch, a2.Arch, T2_Slice, a2.LoadType("int")))
	a2.StoreType("void*", getd("*", a2.Arch, a2.Arch, T2_Ptr, a2.LoadType("void")))
	a2.StoreType("void*[]", getd("[]", 2*a2.Arch, a2.Arch, T2_Slice, a2.LoadType("void*")))
}
