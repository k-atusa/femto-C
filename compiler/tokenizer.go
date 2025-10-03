// test000a : TSLL-C compiler tokenizer

package main

type Token struct {
	Data    string
	TknType int
	Line    int
}

func Tokenize(text []byte) ([]Token, error) {
	res := make([]Token, 0, 100)
	status := 0 // 0: normal, 1: word, 2: short comment, 3: long comment, 4: operator, 5: string, 6: str escape
	readPos := 0
	buffer := make([]byte, 0, 10)

	for true {

	}
}
