开发过程记录

- 重复生成汇编指令

比如说这两条 IR 指令:

```
%1 = add 1, 2
ret %1
```

basic block 里面会有 %1 = add 1, 2 和 ret %1 两条指令，遍历 basic block 会处理一次

ret 指令里面会指向 %1 = add 1, 2 ，也会处理一次

如果不做判断，会导致 %1 = add 1, 2 被处理两次

- 段错误

代码生成中，全局变量 fp 忘记赋值，导致段错误

整个 debug 过程一直在折腾 autotest 脚本，耗费大量时间，但是收效甚微

为此，加了一些方便测试 debug 的脚本，能够手动编译执行

- 段错误

寄存器存在一个数组里面，用的太多越界了

这个问题困难的地方在于，越界的地方没有报错，而是在另外一个地方报的错，完全让人摸不着头脑

这种以后要加个断言判断

其实也可以用 valgrind ，检查出内存的非法操作

## Lv4.2 变量和赋值

整个的难点在于目标代码生成，特别是 % 开头的临时变量的偏移量。
新加变量记录局部变量 (locals) 和临时变量计数 (stack_index) ，有返回值的 IR 需要增加临时变量计数，通过这种方式正确地实现了偏移计算。

临时变量如果使用压栈弹栈的方式，感觉实现起来更简单。

## Lv5 语句块和作用域

一是 parse 里面的比较有问题，比如 const 和 c 本来不相等，但是给判断相等了。

二是碰到老朋友——段错误，原因是结构体没有设置初始化值。

三是优化 AST 时候，没有考虑作用域，导致最后一个测试（6_complex_scopes）失败，排查了蛮久时间。

## Lv6 if 语句

短路求值一开始想简单了,发现根本不行(IR 里面不能多次赋值).最后按文中的思路,引入中间变量(利用之前变量的实现来进行多次赋值操作),将逻辑表达式翻译成多条语句,再将这多条语句翻译成 IR .

## Lv7 while 语句

开头就是重构，增加 TOKEN_KEYWORD 类型，又在 strcmp 上面踩坑，经常忘记后面加等于 0 的判断。
因为目标代码生成没有变动，问题其实就是 ast 的解析，这个写过很多次了，没什么难度。

## Lv8 函数和全局变量

刚开始想了蛮久，要怎么检查函数体内所有分支都有 return 。
后面发现，语义规范是应当，没有作强制要求，emm...

写得太乱了，毫无章法。写 IR 的时候，发现 parse 部分还没有写完（函数参数、函数调用都没有支持）。
C 语言标准库里面没有现成的数组，字符串拼接，整个写代码的过程很麻烦。

函数调用目标代码生成写了快 2 个小时，很多细节问题弄错了，导致 debug 了好久。

我怎么感觉直接从 AST 生成目标代码更简单，这个 IR 目标代码生成好复杂。主要原因是根本不懂这个 IR ，以及他为什么要这么设计。

或许我应该换一种思路来生成目标代码（表达式不用现在这种递归的方式处理，按平铺处理，记录指针与栈对应关系），等全部写完再试试。

全局变量也写了 2 个小时左右，写了一堆 bug ，基本都是在 debug 。

## Lv9 数组

数组这个生成，人快被搞晕了。自己写的程序快要看不懂了。。。

这个 Koopa IR 内存表示好复杂，每个结构都套了好多层。

对这个 IR 是真的不熟悉，各种错误。尤其是 WRANG ANSWER ，好难排查。

目标代码排查经验

1. 缩小测试代码，一步步加回原样，可以确定是那部分代码编译有问题。

2. 确定目标后就是查看生成的汇编代码，人肉执行看看。

3. 看不出来，那就用 gdb debug 。

怀疑 riscv 生成应该还有问题，后面全部完成之后，要重构一遍。

多维数组的初始化值解析好复杂，这是哪个天才想出来的天才设计。

昨天花了一个小时理解，今天花了两个小时写实现

多维数组的测试通过，看了一下测试用例，他没有测试多维数组的初始化是否正确 sad

发现多维数组的类型写错了，应该用 [[i32, 2], 3] 这样的嵌套结构，文档里面没有说，完全想叉了（用的还是一维数组的样式，把多维数组展开）。

经过无数次的 debug ，终于 IR 测试都通过了。

经过无数次的 debug ，终于 RISC-V 测试都通过了。

写得不够细心，导致后续 debug 花费了大量时间。

数组这一章写了一周，指针操作复杂，叠加代码的混乱，真的难搞[摸头]。

wow PASSED (130/130)
