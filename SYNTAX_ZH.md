# 盘古语言参考手册

## 概述

盘古（Pangu）是一门基于 LLVM 后端的编译型语言，同时支持 JIT 即时执行。
其设计目标是实现**高密度、高可读性**的程序结构：

- 顶层文件描述模块结构
- 中层文件描述管道（pipeline）和类型结构
- 底层文件包含具体算法

**自举状态：** 盘古可以编译自身。`bootstrap/` 目录中的自举编译器读取 PGL
源码、生成 C 代码，已通过三代自举固定点验证。

## 工具链

```bash
make linux            # 构建编译器
make tests            # 运行全部测试

./build/pangu run     <file.pgl|dir/> [args...]   # JIT 执行
./build/pangu compile <file.pgl|dir/> [-o path]   # 编译为原生二进制
./build/pangu emit-ir <file.pgl|dir/>              # 输出 LLVM IR
./build/pangu parse   <file.pgl|dir/>              # 解析并打印 AST
./build/pangu --help                                # 显示帮助
```

- `compile` 默认输出到 `build/<name>`，可用 `-o path` 指定路径。
- 目录参数自动发现 `.pgl` 文件并合并相同 package 的文件。

### 诊断信息

所有错误使用 clang 风格的源码定位和插入符号：

```text
path/file.pgl:4:13: error: undefined function 'foo'
    println(foo(1));
            ^~~
```

在 JIT 和 AOT 模式下，panic 和信号处理器会打印 PGL 源码级的回溯信息（文件:行号）。

## 词法结构

### 注释

```pgl
// 行注释
/* 块注释 */
```

### 字面量

```pgl
42          // 整数
0xFF        // 十六进制整数
0b1010      // 二进制整数（10）
0777        // 八进制整数（511）
1_000_000   // 下划线分隔符（忽略，用于提高可读性）
"hello\n"   // 字符串（支持 \n \t \\ \" \0 \xNN \0NNN）
`raw\nstr`  // 反引号字符串（多行、原始——不处理转义）
"${name}"   // 字符串插值——嵌入变量值
"${len(s)}" // 表达式插值——嵌入函数调用结果
"${a + b}"  // 表达式插值——嵌入算术结果
'A'         // 字符字面量 → 整数（ASCII 值）
true false  // 布尔值 → 整数（1 / 0）
[1, 2, 3]   // 数组字面量 → DynArray（整数元素）
["a", "b"]  // 数组字面量 → DynStrArray（字符串元素）
```

### 关键字

`package` `import` `as` `type` `struct` `enum` `interface` `func` `pipeline` `impl`
`if` `else` `for` `in` `while` `do` `return` `defer` `switch` `case` `default` `match`
`break` `continue` `goto` `try` `catch`
`public` `static` `const` `final` `var` `class` `worker` `switcher`

### 运算符

```
+  -  *  /  %          // 算术运算
== != > < >= <=        // 比较运算（支持 int 和 string）
&& ||  !               // 逻辑运算（短路求值）
&  |  ^  ~             // 位运算：与、或、异或、取反
++ --                   // 前缀自增/自减
=  :=                   // 赋值 / 定义赋值
+= -= *= /= %= &= |= ^=  // 复合赋值
.  ::                   // 字段访问 / 枚举变体
>> <>                   // 流推送 / 原地变换
( ) [ ] { } , ; : ->   // 分隔符
```

## 类型

### 基本类型

| 类型     | LLVM     | 描述                |
|----------|----------|---------------------|
| `int`    | `i32`    | 32 位有符号整数     |
| `char`   | `i32`    | 字符（int 的别名）  |
| `bool`   | `i32`    | 布尔值（true=1, false=0）|
| `string` | `ptr`    | C 字符串指针        |
| `ptr`    | `ptr`    | 原始指针            |

### 结构体

```pgl
type Point struct {
    x int;
    y int;
}

// 构造
p := Point{x: 1, y: 2};

// 字段访问
println(p.x);

// 字段修改
p.x = 10;

// 作为函数参数和返回值
func translate(p Point, dx int) Point {
    return Point{x: p.x + dx, y: p.y};
}

// 结构体展开——从已有结构体复制字段，覆盖部分
p2 := Point{...p, x: 10};   // p2.x=10, p2.y=p.y
```

结构体支持 `@annotation` 元数据用于反射：

```pgl
@json("point_data")
type Point struct {
    @required
    x int;
    y int;
}
```

### 枚举

```pgl
// 简单枚举（序号整数）
type Color enum { RED, GREEN, BLUE }

// 枚举变体访问（序号整数：0, 1, 2）
c := Color::RED;
if (c == Color::BLUE) { println("blue"); }
```

### 带关联数据的枚举（代数数据类型）

```pgl
type Result enum {
    Ok(value int),
    Err(msg string),
}

type Option enum {
    Some(val int),
    None,
}

// 构造
r := Result::Ok(42);
o := Option::None;

// match 解构——将变体字段绑定到局部变量
v := match(r) {
    Ok(v)  => v;       // v 绑定到 int 字段
    Err(e) => 0;       // e 绑定到 string 字段（此处未使用）
};

msg := match(r) {
    Ok(v)  => "success";
    Err(e) => e;       // 使用绑定的错误消息
    _      => "unknown";
};
```

### 管道类型

```pgl
type CharToToken pipeline(c int)(kind int, text string);
```

管道声明定义了一个具有输入和输出参数的处理阶段。
详见[管道系统](#管道系统)。

## 函数

```pgl
func add(a int, b int) int {
    return a + b;
}

func greet(name string) {
    println("Hello, ${name}");
}

func main() {
    result := add(1, 2);
    greet("world");
}
```

### 多返回值

```pgl
func divmod(a int, b int) (int, int) {
    return a / b, a % b;
}

q, r := divmod(17, 5);   // q=3, r=2
```

### 全局常量

```pgl
const MAX_SIZE = 1024;
const PI_NAME = "pi";
const NEWLINE = '\n';
const DEBUG = true;
```

常量是编译期值，可在包内任何位置访问。

### Defer（延迟执行）

```pgl
func process() {
    f := open_file("data.txt");
    defer close_file(f);         // 关键字语法（推荐）
    defer(close_file(f));        // 函数调用语法（也可以）
    defer println("cleanup 2");
    // 延迟语句在函数返回时以 LIFO 顺序执行
}
```

### 类型转换别名

```pgl
n := int("42");        // string → int（str_to_int 的别名）
s := str(42);          // int → string（int_to_str 的别名）
c := chr(65);          // int → 单字符字符串（char_to_str 的别名）
```

### 类型别名

```pgl
type Name string;     // Name 是 string 的别名
type Age int;          // Age 是 int 的别名

func greet(n Name) {
    println("Hello, ${n}!");
}

name := "Pangu";
greet(name);           // string 与 Name 完全兼容
```

类型别名为已有类型创建新名称。别名与原始类型完全可互换。

### 结构体方法（impl 块）

```pgl
type Counter struct { value int; }

impl Counter {
    func increment(c Counter) Counter {
        return Counter{value: c.value + 1};
    }
}

// 通过 StructName.method(args) 调用
c := Counter{value: 0};
c = Counter.increment(c);
```

## 控制流

### If / Else

```pgl
if (x > 0) {
    println("positive");
} else if (x == 0) {
    println("zero");
} else {
    println("negative");
}
```

### While 循环

```pgl
i := 0;
while (i < 10) {
    println(i);
    i = i + 1;
    if (i == 5) { break; }
}
```

### For 循环

```pgl
// C 风格 for 循环
for (i := 0; i < 10; ++i) {
    if (i % 2 == 0) { continue; }
    println(i);
}

// For-in 范围循环（迭代 0..N-1）
for i in 5 {
    println(i);  // 输出 0, 1, 2, 3, 4
}

// For-in 配合 range(start, end)
for i in range(2, 7) {
    println(i);  // 输出 2, 3, 4, 5, 6
}

// For-in 使用变量上界
n := 10;
for i in n {
    println(i);
}

// For-in 字符串迭代
for ch in "hello" {
    println(ch);  // 输出 ASCII 码：104, 101, 108, 108, 111
}

// For-in 集合迭代
arr := [10, 20, 30];
for val in arr {
    println(val);
}

names := ["alice", "bob"];
for name in names {
    println(name);
}

m := make_map();
for key in m {
    println(key);  // 迭代 map 的键
}

// For-in 表达式（方法调用、函数调用）
for i in arr.size() {
    println(i);  // 0, 1, 2
}

for name in get_names() {
    println(name);  // 迭代返回的集合
}

// 带下标的 enumerate 形式：for index, value in collection
arr2 := ["alpha", "beta", "gamma"];
for i, name in arr2 {
    println(sprintf("%d: %s", i, name));  // 0: alpha, 1: beta, 2: gamma
}

nums := [10, 20, 30];
for idx, val in nums {
    println(sprintf("nums[%d] = %d", idx, val));
}

for i, v in range(2, 5) {
    println(sprintf("%d -> %d", i, v));  // i=0,1,2  v=2,3,4
}

// Map 键值迭代
m2 := map_of("a", "1", "b", "2");
for k, v in m2 {
    println(sprintf("%s=%s", k, v));  // a=1, b=2
}

im := int_map_of("x", 10, "y", 20);
for k, v in im {
    println(sprintf("%s=%d", k, v));  // x=10, y=20
}

// 无限循环（使用 break 退出）
for {
    if (done) { break; }
    // ...
}
```

### Switch / Case

```pgl
switch (x) {
    case 1: println("one");
    case 2: println("two");
    default: println("other");
}

// 字符串 switch（生成 if-else 链配合 str_eq）
switch (cmd) {
    case "start": { println("starting"); }
    case "stop":  { println("stopping"); }
    default:      { println("unknown command"); }
}
```

### 三元表达式

```pgl
result := x > 5 ? "big" : "small";
max := a > b ? a : b;
// 嵌套三元需要显式括号：
grade := score > 90 ? "A" : (score > 60 ? "B" : "C");
```

### Match 表达式

```pgl
type Color enum { RED, GREEN, BLUE }

name := match (c) {
    Color::RED   => "red",
    Color::GREEN => "green",
    _            => "other",
};
```

match 支持枚举变体、整数字面量、通配符 `_`、字符串比较以及带字段绑定的数据枚举解构。

### Nil 字面量

```pgl
x := maybe_get(flag);
if (x == nil) {
    println("not found");
}
```

`nil` 是空指针常量。与 nil 的比较使用指针相等，而非 strcmp。

## 索引访问与切片

```pgl
// 数组索引
arr := make_dyn_array();
arr.push(10);
x := arr[0];        // 10
arr[0] = 99;         // 索引赋值

// 字符串字符访问（返回 int）
s := "hello";
ch := s[0];          // 104 ('h')

// 字符串切片 [start:end]
sub := s[0:3];       // "hel"
```

## 模块与导入

### 包声明

```pgl
package main;
```

### Import

```pgl
import "stdlib/core/math" as math;    // 标准库模块
import "./utils" as utils;            // 相对路径
```

多文件编译：`pangu compile dir/` 自动发现 `.pgl` 文件并合并相同 package 声明的文件。

### 包管理（.pgs）

类 Go 风格的包配置，通过 `pangu.pgs`：

```pgl
module "github.com/user/myapp";
require "github.com/ProtossGenius/json_pgl" v0.1.0;
replace "github.com/ProtossGenius/json_pgl" => "./vendor/github.com/ProtossGenius/json_pgl";
```

第三方导入通过 vendor 目录的 replace 指令解析。

## 管道系统

管道是用于流式数据处理（如词法分析）的状态机模式。

### 声明

```pgl
type Signal enum { CONTINUE, FINISH, TRANSFER_FINISH, APPEND, APPEND_FINISH }
type WorkerID enum { WIdent, WNumber, WString, WSymbol }
type CharToToken pipeline(c int)(kind int, text string);
```

### Switcher 和 Workers

```pgl
// Switcher 将每个输入路由到相应 worker
impl CharToToken Switcher {
    func dispatch(c int) int {
        if (c >= 'a' && c <= 'z') { return WorkerID::WIdent; }
        if (c >= '0' && c <= '9') { return WorkerID::WNumber; }
        return WorkerID::WSymbol;
    }
}

// Workers 处理输入并返回信号
impl WIdent CharToToken worker {
    func process(c int, buf_len int, first_char int) int {
        if (c >= 'a' && c <= 'z') { return Signal::APPEND; }
        return Signal::TRANSFER_FINISH;
    }
}
```

### 自动生成的 Dispatch

当管道拥有名称匹配枚举变体的 workers 时，编译器自动生成
`PipelineName.__dispatch(wid, ...)` 来根据 worker ID 进行分派：

```pgl
// 自动生成——无需手动编写
sig := CharToToken.__dispatch(wrk, c, buf_len, first_char);
```

### 管道运行时内建函数

底层状态管理，用于手动管道实现：

| 函数 | 描述 |
|------|------|
| `pipeline_create(elem_size)` | 创建管道状态 |
| `pipeline_destroy(state)` | 释放管道状态 |
| `pipeline_cache_append(state, char)` | 向缓冲区追加字符 |
| `pipeline_cache_str(state)` | 获取缓冲区字符串并重置 |
| `pipeline_cache_reset(state)` | 清除缓冲区 |
| `pipeline_set_worker(state, id)` | 设置当前 worker |
| `pipeline_get_worker(state)` | 获取当前 worker |
| `pipeline_emit(state, elem)` | 存储输出元素 |
| `pipeline_output_count(state)` | 统计已存储的输出数量 |
| `pipeline_output_get(state, i)` | 按索引获取已存储输出 |

## 内建函数

### I/O

| 函数 | 描述 |
|------|------|
| `println(val)` | 带换行打印（自动检测 int/string） |
| `print(val)` | 不带换行打印 |
| `read_line()` | 从标准输入读取一行（返回 string） |
| `read_file(path)` | 读取文件内容为字符串 |
| `write_file(path, content)` | 将字符串写入文件 |
| `args(index)` | 获取命令行参数 |
| `args_count()` | 获取参数个数 |
| `exit(code)` | 以状态码退出 |
| `panic(msg)` | 打印错误 + 回溯，终止程序 |
| `system(cmd)` | 执行 shell 命令 |

### 字符串

| 函数 | 描述 |
|------|------|
| `str_concat(a, b)` | 拼接（也可用 `a + b`） |
| `str_len(s)` | 字符串长度 |
| `str_eq(a, b)` | 相等检查 |
| `str_substr(s, start, end)` | 子字符串 |
| `str_char_at(s, i)` | 指定索引处的字符码 |
| `char_to_str(code)` | 字符码转字符串 |
| `str_index_of(s, sub)` | 查找子串位置 |
| `str_starts_with(s, prefix)` | 前缀检查 |
| `str_ends_with(s, suffix)` | 后缀检查 |
| `str_replace(s, old, new)` | 替换子串 |
| `str_contains(s, sub)` | 检查是否包含子串 |
| `str_trim(s)` | 去除首尾空白 |
| `str_to_upper(s)` | 转大写 |
| `str_to_lower(s)` | 转小写 |
| `str_split(s, delim)` | 按分隔符拆分为 DynStrArray |
| `str_repeat(s, n)` | 重复字符串 n 次 |
| `str_count(s, sub)` | 统计出现次数 |
| `str_replace_all(s, old, new)` | 替换所有出现 |
| `int_to_str(n)` | 整数转字符串 |
| `str_to_int(s)` | 字符串转整数 |
| `sprintf(fmt, ...)` | 格式化字符串（C printf 语法） |
| `str_join(arr, sep)` | 用分隔符连接字符串数组 |

### 数组（固定大小）

| 函数 | 描述 |
|------|------|
| `make_array(size)` | 创建 int 数组 |
| `array_get(arr, i)` | 获取 int 元素 |
| `array_set(arr, i, val)` | 设置 int 元素 |
| `make_str_array(size)` | 创建 string 数组 |
| `str_array_get(arr, i)` | 获取 string 元素 |
| `str_array_set(arr, i, val)` | 设置 string 元素 |

### HashMap (string → string)

| 函数 | 描述 |
|------|------|
| `make_map()` | 创建空 map |
| `map_of("k1", "v1", ...)` | 用初始键值对创建 map |
| `map_set(m, key, val)` | 设置键值对 |
| `map_get(m, key)` | 获取值（缺失则返回空字符串） |
| `map_has(m, key)` | 检查键是否存在（1/0） |
| `map_size(m)` | 条目数量 |
| `map_delete(m, key)` | 删除条目 |
| `map_keys(m)` | 获取所有键（返回 DynStrArray） |
| `m["key"]` | 下标读取（→ map_get） |
| `m["key"] = val` | 下标写入（→ map_set） |

### IntMap (string → int)

| 函数 | 描述 |
|------|------|
| `make_int_map()` | 创建空 int map |
| `int_map_of("k1", 1, ...)` | 用初始键值对创建 int map |
| `int_map_set(m, key, val)` | 设置键值对 |
| `int_map_get(m, key)` | 获取值（缺失则返回 0） |
| `int_map_has(m, key)` | 检查键是否存在（1/0） |
| `int_map_size(m)` | 条目数量 |
| `int_map_keys(m)` | 获取所有键（返回 DynStrArray） |
| `m["key"]` | 下标读取（→ int_map_get） |
| `m["key"] = val` | 下标写入（→ int_map_set） |

### 动态数组（可调整大小的 int 数组）

| 函数 | 描述 |
|------|------|
| `make_dyn_array()` | 创建空动态数组 |
| `dyn_array_push(a, val)` | 追加值 |
| `dyn_array_get(a, i)` | 获取元素 |
| `dyn_array_set(a, i, val)` | 设置元素 |
| `dyn_array_size(a)` | 当前大小 |
| `dyn_array_pop(a)` | 移除并返回最后一个元素 |

### 动态字符串数组（可调整大小的 string 数组）

| 函数 | 描述 |
|------|------|
| `make_dyn_str_array()` | 创建空动态字符串数组 |
| `dyn_str_array_push(a, val)` | 追加字符串 |
| `dyn_str_array_get(a, i)` | 获取字符串元素 |
| `dyn_str_array_set(a, i, val)` | 设置字符串元素 |
| `dyn_str_array_size(a)` | 当前大小 |

### StringBuilder

| 函数 | 描述 |
|------|------|
| `make_str_builder()` | 创建空字符串构建器 |
| `sb_append(sb, str)` | 追加字符串 |
| `sb_append_int(sb, n)` | 追加整数为字符串 |
| `sb_append_char(sb, ch)` | 追加字符码 |
| `sb_build(sb)` | 返回构建的字符串 |
| `sb_reset(sb)` | 清除构建器 |
| `sb_len(sb)` | 当前长度 |

### 文件系统

| 函数 | 描述 |
|------|------|
| `find_pgl_files(dir, arr)` | 在目录中查找 .pgl 文件 |
| `is_directory(path)` | 检查路径是否为目录 |

### 反射

| 函数 | 描述 |
|------|------|
| `reflect_type_count()` | 已注册类型的数量 |
| `reflect_type_name(i)` | 按索引获取类型名 |
| `reflect_field_count(type)` | 类型的字段数量 |
| `reflect_field_name(type, i)` | 按索引获取字段名 |
| `reflect_field_type(type, i)` | 按索引获取字段类型 |
| `reflect_annotation_count(type)` | 注解数量 |
| `reflect_annotation_key(type, i)` | 注解键 |
| `reflect_annotation_value(type, i)` | 注解值 |

### 方法调用语法

内建类型支持方法调用语法作为语法糖：

```pgl
arr := [1, 2, 3];
arr.push(10);           // dyn_array_push(arr, 10)
arr.append(20);         // push 的别名
x := arr.get(0);        // dyn_array_get(arr, 0)
n := arr.len();          // dyn_array_size(arr)
n := arr.size();         // 与 len() 相同

names := ["alice", "bob", "charlie"];
println(names.join(", "));  // str_join(names, ", ") → "alice, bob, charlie"

m := make_map();
m.set("key", "val");    // map_set(m, "key", "val")
v := m.get("key");      // map_get(m, "key")
n := m.len();            // map_size(m)
for k in m { ... }       // 迭代键

// Map/IntMap 下标语法
m["key"] = "value";     // map_set(m, "key", "value")
v := m["key"];           // map_get(m, "key")

// Map 字面量构造器
colors := map_of("red", "#ff0000", "green", "#00ff00");
scores := int_map_of("math", 95, "eng", 87);

s := "hello";
n := s.len();            // str_len(s)
ch := s.char_at(0);      // str_char_at(s, 0)
sub := s.substr(0, 3);   // str_substr(s, 0, 3)

sb := make_str_builder();
sb.append("hello");
result := sb.build();
```

支持的类型：DynArray、DynStrArray、HashMap、IntMap、StringBuilder、string。

## 语义分析

`sema` 模块在 LLVM 降级前进行验证：

- 函数存在性和参数个数
- 导入别名有效性和被导入函数的存在性
- 变量定义（`:=` 或参数）
- 结构体类型名和枚举类型名识别
- 结构体方法调用验证（`StructName.method(args)`）

## 自举状态

**✅ 自举已实现。** `bootstrap/` 目录中的自举编译器可以编译自身，
三代编译产生相同输出：

```bash
pangu compile bootstrap/ -o build/bootstrap
build/bootstrap compile bootstrap/ -o build/bootstrap2
build/bootstrap2 compile bootstrap/ -o build/bootstrap3
# strip bootstrap2 == strip bootstrap3  （固定点已验证）
```

自举编译器读取 PGL 源码、词法分析、语法分析生成 AST，
然后生成 C 代码并用 gcc 编译。支持多文件编译、`-o` 标志和 `--help`。

### 现代化自举编译器（bootstrap2/）

`bootstrap2/` 目录包含使用现代 PGL 特性重写的第二代自举编译器：

- **枚举类型**：使用 `Enum::Variant` 语法代替魔术数字
- **`const` 声明**：使用命名常量
- 多文件编译，共享类型定义

```bash
pangu compile bootstrap2/ -o build/bootstrap2
build/bootstrap2 compile bootstrap2/ -o build/bootstrap2_gen2
build/bootstrap2_gen2 compile bootstrap2/ -o build/bootstrap2_gen3
# gen2 输出 == gen3 输出  （固定点已验证）
```

## 流运算符

PGL 提供流运算符用于数据流风格编程：

### `>>` 流推送

将值追加到字符串变量：

```pgl
buf := "";
72 >> buf;       // 追加字符 'H'（ASCII 72）到 buf
"world" >> buf;  // 追加字符串 "world" 到 buf
println(buf);    // "Hworld"
```

### `<>` 原地变换

对字符串的每个字符应用函数，原地修改：

```pgl
func to_upper(ch int) int {
    if (ch >= 97 && ch <= 122) { return ch - 32; };
    return ch;
}

s := "hello";
s <> to_upper;
println(s);  // "HELLO"
```

### `>>` 分派

通过基于谓词的分派路由值：

```pgl
c >> [is_alpha -> handle_alpha, is_digit -> handle_digit];
```

按顺序求值谓词。第一个匹配的调用对应处理器。
返回处理器的结果，如果没有谓词匹配则返回 0。

### `==>` 管道链（计划中）

将管道阶段链接起来进行端到端数据处理。

## 闭包

闭包从其外围作用域捕获变量：

```pgl
func make_adder(n int) func {
    return func(x int) int {
        return x + n;
    };
}

func main() {
    add5 := make_adder(5);
    println(add5(10));  // 15
}
```

所有 lambda 使用环境指针约定：`{ ptr func, ptr env }`。
不捕获变量的 lambda 和函数引用的 `env = null`。

## 泛型

泛型函数使用类型参数和单态化：

```pgl
func identity[T](x T) T {
    return x;
}

func first[T](a T, b T) T {
    return a;
}

func main() {
    println(identity(42));         // int 特化
    println(identity("hello"));   // string 特化
    println(first(10, 20));       // 推断 T=int
}
```

类型从参数推断。每个唯一的类型组合生成一个特化函数。

## 接口

接口定义方法契约，使用 vtable 分派：

```pgl
type Shape interface {
    func area(self Shape) int;
    func name(self Shape) string;
}

type Rect struct {
    w int;
    h int;
}

impl Rect Shape {
    func area(self Rect) int {
        return self.w * self.h;
    }
    func name(self Rect) string {
        return "rect";
    }
}

func print_info(s Shape) {
    println(s.name());
    println(s.area());
}

func main() {
    r := Rect{w: 3, h: 4};
    s := Shape(r);       // 包装为接口胖指针
    print_info(s);       // 通过 vtable 分派
}
```

接口值是胖指针：`{ ptr data, ptr vtable }`。
`Shape(concrete_value)` 将具体类型包装到接口中。

## 函数引用

函数可以作为一等公民值使用：

```pgl
fn := add_one;        // 存储函数引用
println(fn(5));       // 通过变量间接调用
println(apply(fn, 3)); // 作为参数传递
```

## 计划中 / 尚未实现

- match 表达式中的范围模式
- 带 Result 类型的错误处理
- 自动生成的管道 `run` 状态机
- 完整管道体语法（`type X pipeline { def in T; ... }`）
