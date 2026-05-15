// test.c
typedef struct {
  int x;
  int y;
} Food;

Food better(Food a, Food b) {
  Food c;
  c.x = a.x + b.x;
  c.y = a.y + b.y;
  return c;
}