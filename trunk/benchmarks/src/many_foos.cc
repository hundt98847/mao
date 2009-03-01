#define SINGLE(x,y) \
int foo##x##y(int i, int j, int k) { \
  for (j = 0; j < 10; j++) { \
    i+=j; i*=j; i-=j; i/=j; \
    for (k=i; k < j; k++) { \
      i+=k; i*=k; i-=k; i/=k; \
    } \
  } \
  return i; \
}

#define M10(x)  SINGLE(x,0) SINGLE(x,1) SINGLE(x,2) SINGLE(x,3) SINGLE(x,4) \
                SINGLE(x,5) SINGLE(x,6) SINGLE(x,7) SINGLE(x,8) SINGLE(x,9)

#define M100(x) M10(x##0) M10(x##1) M10(x##2) M10(x##3) M10(x##4) \
                M10(x##5) M10(x##6) M10(x##7) M10(x##8) M10(x##9)

M100(1)
M100(2)
M100(3)
M100(4)
M100(5)
#ifdef ALL
M100(6)
M100(7)
M100(8)
M100(9)
M100(10)

M100(11)
M100(12)
M100(13)
M100(14)
M100(15)
M100(16)
M100(17)
M100(18)
M100(19)
M100(20)

M100(21)
M100(22)
M100(23)
M100(24)
M100(25)
M100(26)
M100(27)
M100(28)
M100(29)
M100(30)

M100(31)
M100(32)
M100(33)
M100(34)
M100(35)
M100(36)
M100(37)
M100(38)
M100(39)
M100(40)

M100(41)
M100(42)
M100(43)
M100(44)
M100(45)
M100(46)
M100(47)
M100(48)
M100(49)
M100(50)
#endif
