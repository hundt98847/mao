int foo(int val, int l1, int l2) {
  int sum = 0, i, j, k;
  for (i = 0; i < l1; i++) {
    for (j = 0; j < l2; j++) {
      sum += i + j;
    }
  }

  for (k = 0; k < l1+l2; k++) {
    sum -= k*l1;
  }

  return sum;
}
