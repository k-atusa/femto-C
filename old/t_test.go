package tsll_test

import (
	"fmt"
	"os"
	"testing"

	tsll "example.com"
)

func TestMain(m *testing.M) {
	path := "gt2.txt"
	d, _ := os.ReadFile(path)
	t, e := tsll.Tokenize(d, path)
	if e != nil {
		fmt.Println(e)
	}

	var prov tsll.TokenProvider
	prov.Set(t, path)
	tnode, e := tsll.GetTypeNode(&prov, 8)
	if e != nil {
		fmt.Println(e)
	}

	fmt.Println(tnode.Print(true))
	fmt.Println(*tnode)
}
