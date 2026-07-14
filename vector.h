#define MIN_SIZE 64 //so just the magic number without of enough sense
		     
typedef struct {
    void **body;
    int len;
    int nalloc;
} Vector;

static int max(int a, int b) { return a > b ? a : b; }

static int roundup(int n) {
  if (n == 0) return 0;
  int r = 1;
  while (n > r) r *= 2;
  return r;
}

static void extend(Vector* vec, int delta) {
  if (vec->len + delta <= vec->nalloc) return;
  int nelem = max(roundup(vec->len + delta), MIN_SIZE);
  void* newbody = malloc(sizeof(void*) * nelem);
  memcpy(newbody, vec->body, sizeof(void*) * vec->len);
  if (vec->nalloc != 0) free(vec->body);
  vec->body = newbody;
  vec->nalloc = nelem;
}

void vec_push(Vector* vec, void* elem) {
  extend(vec, 1);
  vec->body[vec->len++] = elem;
}

Vector* do_make_vector(int size) {
  Vector* r = malloc(sizeof(Vector));
  size = roundup(size);
  if (size > 0) r->body = malloc(sizeof(void*) * size);
  r->len = 0;
  r->nalloc = size;
  return r;
}

Vector* make_vector() { return do_make_vector(0); }

Vector* make_vector1(void* e) {
  Vector* r = do_make_vector(0);
  vec_push(r, e);
  return r;
}

Vector* vec_copy(Vector* src) {
  Vector* r = do_make_vector(src->len);
  memcpy(r->body, src->body, sizeof(void*) * src->len);
  if (src->nalloc != 0) free(src->body);
  r->len = src->len;
  return r;
}

void vec_append(Vector* a, Vector* b) {
  extend(a, b->len);
  memcpy(a->body + a->len, b->body, sizeof(void*) * b->len);
  a->len += b->len;
}

void* vec_pop(Vector* vec) {
  assert(vec->len > 0,"the vector len too small to pop\n");
  return vec->body[--vec->len];
}

void* vec_get(Vector* vec, int index) {
  assert(index < vec->len,"the vector index too smal to get with it\n");
  return vec->body[index];
}

void vec_set(Vector* vec, int index, void* val) {
  assert(0 <= index && index < vec->len,"the vector index is negative\n");
  vec->body[index] = val;
}

void* vec_head(Vector* vec) {
  assert(vec->len > 0,"the vector len is lesser of zero when try to get its head\n");
  return vec->body[0];
}

void* vec_tail(Vector* vec) {
  assert(vec->len > 0,"the vector len is lesser of zero when try to get its tail\n");
  return vec->body[vec->len - 1];
}

Vector* vec_reverse(Vector* vec) {
  Vector* r = do_make_vector(vec->len);
  for (int i = 0; i < vec->len; i++) r->body[i] = vec->body[vec->len - i - 1];
  r->len = vec->len;
  return r;
}

void* vec_body(Vector* vec) { return vec->body; }

int vec_len(Vector* vec) { return vec->len; }

void vec_free(Vector* vec) {
  if (vec->nalloc != 0) free(vec->body);
  free(vec);
}

