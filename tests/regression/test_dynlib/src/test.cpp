// Create a memory dependence through two function calls to standard library

extern "C" {
  void my_memcpy(void *dst, void *src, int size);
}

#define ITER_NUM 10
// #define ITER_NUM 1000000
int main() {
  int *p = new int[10];
  int *q = new int[10];

  for (int i = 0; i < 10; i++) {
    p[i] = i;
  }

  int sum = 0;

  for (int i = 0; i < ITER_NUM; i++) {
    my_memcpy(q, p, 10 * sizeof(int)); // the dependence is killed
    sum += q[i % 10]; // read from q
    q[i % 10] = sum;  // write to q
  }
}
