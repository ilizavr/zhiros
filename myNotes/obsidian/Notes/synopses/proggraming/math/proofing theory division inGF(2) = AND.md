# Prepare
for proofing, i'll provide base input data that bery important for this proofing. take:

- zero it's just yet another number in number line without "magic" properties
- division in GF<sub>2</sub> it's same as function $f(x,y) = xy^-1$ where current function delims 2 arguments in GF<sub>2</sub> field
- for comparing i'm use AND table


# proofing

- see on AND truth table. we are see him below and the table very important for futher proof:

| 1-st argument | 2-nd argument | result |
| ------------- | ------------- | ------ |
| 1             | 1             | 1      |
| 1             | 0             | 0      |
| 0             | 1             | 0      |
| 0             | 0             | 0      |
|               |               |        |
**how you can see, output bit 1 only if 2 arguments is true**

- make division in GF<sub>2</sub>:
 1. $1 * 1 ^ -1 = 1$
 2.  $1 * 0 ^ -1 = 0$ stop. now we can't continue proofing because delim 1 on 0(because negative degree it like as  $\frac{1}{n}$) is undefined behaivor. or No? see next point(proof n : 0 = 0)
 3.  $0 * 1 ^ -1 = 0$ 
 4. $0 * 0 ^ -1 = 0$
 
- proof n : 0 = 0

from prepare statement, we thinking zero it's just yet another number. Therefore we can use mostly common properties of rings:

1.  $1 : 0$ -  we want understand how many times we need multiplie to get from 0 to 1? if we computing,then we getting result: infinity,but if we represent it in Sigma form,then we see an answer:
2. $\sum_{n=0}^{\infty} = 1:n$
3. if represent in sigma form,then we get infinity summing zero,zero,zero and zero. How we know how many times you can't  summing zero, you always not getting something else instead of zero $\Rightarrow$ 1 : 0 = 0

-  check result
1. in AND table  1 * 1 = 1. in GF<sub>2</sub> result same as
2. in AND table  1 * 0 = 0. in GF<sub>2</sub> result same as
3. in AND table  0 * 1 = 0. in GF<sub>2</sub> result same as
4. in AND table  0 * 0 = 0. in GF<sub>2</sub> result same as

from this checking, we can accept: division in GF<sub>2</sub> it's same operation how AND 

*Proofed*