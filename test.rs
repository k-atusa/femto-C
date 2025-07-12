include std;

struct Node<T> {
    prev: *Node<T>; // 이전 노드에 대한 원시 포인터
    next: ?box Node<T>; // 실제 소유권은 한 방향으로만 흐름
    value: T;
}

func (self: box Node<T>) __new(v: T): box Node<T> {
    self.prev = void;
    self.next = void;
    self.value = v;
    return self;
}

struct LinkedList<T> {
    head: ?box Node<T>; // 머리 노드를 소유
    tail: *Node<T>; // 꼬리 노드에 대한 포인터
    length: int;
}

func (self: box LinkedList<T>) __new(): box LinkedList<T> {
    self.head = void;
    self.tail = void;
    self.length = 0;
    return self;
}

func (self: &LinkedList<T>) push_back_(value: T) {
    var node = new Node<T>(value); // 새 노드 box Node<T> 생성
    if (self.length == 0) {
        self.tail = (node as *Node<T>);
        self.head = node; // 소유권 이동
    } else {
        var prev = self.tail; // *Node<T> 포인터
        self.tail = (node as *Node<T>); // 리스트의 꼬리 정보 업데이트
        node.prev = prev; // 노드의 prev 설정
        prev.next = node; // 이때 노드의 소유권이 넘어간다 (컴파일러는 원시 포인터가 가르키는 구조체는 계속 존재할 것이라 가정)
    }
    self.length = self.length + 1;
}

func (self: &LinkedList<T>) index_(i: int): *T {
    if (i < 0 || i >= self.length || self.head == void) {
        return void; // 잘못된 인덱스
    }
    var node: *Node<T> = (self.head as *Node<T>); // 소유권 규칙으로 인해 원시 포인터로 탐색
    for (var pos = 0; pos < i; pos = pos + 1) {
        if (node.next == void) {
            return void;
        }
        node = (node.next as *Node<T>);
    }
    return &node.value;
}

func main_(): void {
    var list = new LinkedList<f64>; // box 변수라 main이 끝나며 free
    list.push_back_(-16.8);
    list.push_back_(205.8);
    list.push_back_(3.14);
    list.push_back_(0);
    for (var i = 0; i < 5; i = i + 1) {
        var v = list.index_(i); // *f64형 스택변수
        if (v != void) {
            std.print_float(*v);
        } else {
            std.print("invalid index!"); // i가 4일때 출력 예상
        }
    }
}
