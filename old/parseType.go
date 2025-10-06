// test784b : TSLL-C compiler AST parser

package tsll

import (
	"fmt"
	"strconv"
)

type TypeNode struct {
	NdType      int
	SubType     int
	Name        string
	Size        int
	DirectRel   *TypeNode
	IndirectRel []TypeNode
}

func (node *TypeNode) Print(isRecursive bool) string {
	switch node.NdType {
	case 1: // primitive
		switch node.SubType {
		case 1:
			return "i8"
		case 2:
			return "i16"
		case 3:
			return "i32"
		case 4:
			return "i64"
		case 5:
			return "u8"
		case 6:
			return "u16"
		case 7:
			return "u32"
		case 8:
			return "u64"
		case 9:
			return "f32"
		case 10:
			return "f64"
		case 11:
			return "void"
		}
	case 2: // pointer/array
		if node.SubType == 0 {
			return node.DirectRel.Print(isRecursive) + "*"
		} else {
			return fmt.Sprintf("%s[%d]", node.DirectRel.Print(isRecursive), node.SubType)
		}
	case 3: // function
		s := node.DirectRel.Print(isRecursive) + "<"
		for _, r := range node.IndirectRel {
			s = s + r.Print(isRecursive) + ","
		}
		if len(node.IndirectRel) > 0 {
			s = s[:len(s)-1]
		}
		return s + ">"
	case 4: // struct
		s := "struct " + node.Name
		if isRecursive {
			s = s + " {\n"
			for _, r := range node.IndirectRel {
				s = s + "  " + r.Print(true) + "\n"
			}
			s = s + "  }"
		}
		return s
	case 5: // enum
		return "enum " + node.Name
	}
	return "unknown"
}

/*
NodeType: 1: primitive, 2: pointer/array, 3: function, 4: struct, 5: enum
SubType primitive: 1i8 2i16 3i32 4i64 5u8 6u16 7u32 8u64 9f32 10f64 11void
SubType pointer/array: 0: ptr, >0: array size
Size: -1 unknown, 0 void, >0 size in bytes
DirectRel: for pointer/array, points to the base type & function return type
IndirectRel: for function, points to parameter types & struct members
*/

func getPrimitive(token Token, filename string) (*TypeNode, error) {
	if token.TknType != 3 || token.SubType > 11 {
		return nil, fmt.Errorf("E0201 cannot parse primitive type at %s:%d", filename, token.Line) // E0201
	}
	var res TypeNode
	res.NdType = 1
	res.SubType = token.SubType
	res.Name = token.Data
	res.Size = 0
	switch token.SubType {
	case 1, 5:
		res.Size = 1
	case 2, 6:
		res.Size = 2
	case 3, 7, 9:
		res.Size = 4
	case 4, 8, 10:
		res.Size = 8
	}
	return &res, nil
}

func getPointer(prov *TokenProvider, archSz int, base *TypeNode) (*TypeNode, error) {
	if prov.IsEnd() {
		return nil, fmt.Errorf("E0202 no token available to parse type") // E0202
	}
	var res TypeNode
	res.NdType = 2
	res.SubType = 0
	res.Size = archSz
	res.Name = "pointer"
	res.DirectRel = base
	tgt := prov.Pop()
	if tgt.TknType == 2 && tgt.SubType == 3 { // T*
		return &res, nil
	} else if tgt.TknType == 2 && tgt.SubType == 30 { // T[
		if prov.IsEnd() {
			return nil, fmt.Errorf("E0203 no token available to parse type") // E0203
		}
		tgt = prov.Pop()
		if tgt.TknType == 2 && tgt.SubType == 31 { // T[]
			return &res, nil
		} else if tgt.TknType == 1 && tgt.SubType == 1 { // T[N
			res.SubType, _ = strconv.Atoi(tgt.Data)
			res.Size = base.Size * res.SubType
			res.Name = "array"
			if prov.IsEnd() {
				return nil, fmt.Errorf("E0204 no token available to parse type") // E0204
			}
			tgt = prov.Pop()
			if tgt.TknType == 2 && tgt.SubType == 31 { // T[N]
				return &res, nil
			} else {
				return nil, fmt.Errorf("E0205 expected ] but %s at %s:%d", tgt.Data, prov.Filename, tgt.Line) // E0205
			}
		} else { // invalid length
			return nil, fmt.Errorf("E0206 array length should be literal int10 at %s:%d", prov.Filename, tgt.Line) // E0206
		}
	} else { // invalid syntax
		return nil, fmt.Errorf("E0207 expected [ but %s at %s:%d", tgt.Data, prov.Filename, tgt.Line) // E0207
	}
}

func getFunction(prov *TokenProvider, archSz int, base *TypeNode) (*TypeNode, error) {
	var res TypeNode
	res.NdType = 3
	res.Name = "function"
	res.Size = archSz
	res.DirectRel = base
	res.IndirectRel = make([]TypeNode, 0)
	for { // get parameter types
		if prov.IsEnd() {
			return nil, fmt.Errorf("E0208 no token available to parse type") // E0208
		}
		tgt := prov.Pop()
		if tgt.TknType == 2 && tgt.SubType == 8 { // end of parms
			break
		}
		parm, err := getPrimitive(*tgt, prov.Filename) // parm value type base
		if err != nil {
			return nil, err
		}
		for {
			if prov.IsEnd() {
				return nil, fmt.Errorf("E0209 no token available to parse type") // E0209
			}
			tgt = prov.Seek()
			if tgt.TknType == 2 && tgt.SubType == 24 { // comma
				res.IndirectRel = append(res.IndirectRel, *parm)
				prov.Pop()
				break
			}
			if tgt.TknType == 2 && tgt.SubType == 8 { // end of parms
				res.IndirectRel = append(res.IndirectRel, *parm)
				break
			}
			if tgt.TknType == 2 && tgt.SubType == 6 { // function type parm
				prov.Pop()
				parm, err = getFunction(prov, archSz, parm)
				if err != nil {
					return nil, err
				}
			} else { // parm pointer extend
				parm, err = getPointer(prov, archSz, parm)
				if err != nil {
					return nil, err
				}
			}
		}
	}
	return &res, nil
}

// get type node, archSz 4 for 32bit / 8 for 64bit
func GetTypeNode(prov *TokenProvider, archSz int) (*TypeNode, error) {
	if prov.IsEnd() {
		return nil, fmt.Errorf("E0210 no token available to parse type") // E0210
	}
	res, err := getPrimitive(*prov.Pop(), prov.Filename) // get base type
	if err != nil {
		return nil, err
	}
	for { // type extend
		if prov.IsEnd() {
			return res, nil
		}
		tgt := prov.Seek()
		if tgt.TknType == 2 && (tgt.SubType == 3 || tgt.SubType == 30) { // pointer extend
			res, err = getPointer(prov, archSz, res)
			if err != nil {
				return nil, err
			}
		} else if tgt.TknType == 2 && tgt.SubType == 6 { // function extend
			prov.Pop()
			res, err = getFunction(prov, archSz, res)
			if err != nil {
				return nil, err
			}
		} else { // finish extend
			break
		}
	}
	return res, nil
}
