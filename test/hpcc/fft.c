#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mta_task.h>
#include <machine/runtime.h>

double timer()
{ return ((double) mta_get_clock(0) / mta_clock_freq()); }

#pragma mta inline
void btrfly(j, wk1r, wk1i, wk2r, wk2i, wk3r, wk3i, a, b, c, d)
  int j;
  double wk1r, wk1i, wk2r, wk2i, wk3r, wk3i, *a, *b, *c, *d;
{ double x0r = a[j    ] + b[j    ];
  double x0i = a[j + 1] + b[j + 1];
  double x1r = a[j    ] - b[j    ];
  double x1i = a[j + 1] - b[j + 1];
  double x2r = c[j    ] + d[j    ];
  double x2i = c[j + 1] + d[j + 1];
  double x3r = c[j    ] - d[j    ];
  double x3i = c[j + 1] - d[j + 1];

  a[j    ] = x0r + x2r;
  a[j + 1] = x0i + x2i;
  x0r -= x2r;
  x0i -= x2i;
  c[j    ] = wk2r * x0r - wk2i * x0i;
  c[j + 1] = wk2r * x0i + wk2i * x0r;
  x0r = x1r - x3i;
  x0i = x1i + x3r;
  b[j    ] = wk1r * x0r - wk1i * x0i;
  b[j + 1] = wk1r * x0i + wk1i * x0r;
  x0r = x1r + x3i;
  x0i = x1i - x3r;
  d[j    ] = wk3r * x0r - wk3i * x0i;
  d[j + 1] = wk3r * x0i + wk3i * x0r;
}

double * bit_reverse(int n, double *w) {
  unsigned int i, mask, shift;
  double *v = new double[2 * n];

  mask  = 0x0102040810204080;
  shift = (int) (log(n) / log(2));

#pragma mta use 100 streams
#pragma mta assert no dependence
  for (i = 0; i < n; i++) {
      int ndx = MTA_BIT_MAT_OR(mask, MTA_BIT_MAT_OR(i, mask));
      ndx = MTA_ROTATE_LEFT(ndx, shift);
      v[2 * ndx]     = w[2 * i];
      v[2 * ndx + 1] = w[2 * i + 1];
  }

  free(w);
  return(v);
}

void twiddles(int n, double *w)
{ int i;
  double delta = atan(1.0) / n;

  w[0]     = 1;
  w[1]     = 0;
  w[n]     = cos(delta * n);
  w[n + 1] = w[n];

#pragma mta assert no dependence
  for (i = 2; i < n; i += 2) {
      double x = cos(delta * i);
      double y = sin(delta * i);
      w[i]             = x;
      w[i + 1]         = y;
      w[2 * n - i]     = y;
      w[2 * n - i + 1] = x;
} }

void cft1st(int n, double *a, double *w)
{ int j, k1;

  double *v   = w + 1;
  double *b   = a + 2;
  double *c   = a + 4;
  double *d   = a + 6;
  double wk1r = w[2];

  double x0r = a[0] + a[2];
  double x0i = a[1] + a[3];
  double x1r = a[0] - a[2];
  double x1i = a[1] - a[3];
  double x2r = a[4] + a[6];
  double x2i = a[5] + a[7];
  double x3r = a[4] - a[6];
  double x3i = a[5] - a[7];

  a[0] = x0r + x2r;
  a[1] = x0i + x2i;
  a[4] = x0r - x2r;
  a[5] = x0i - x2i;
  a[2] = x1r - x3i;
  a[3] = x1i + x3r;
  a[6] = x1r + x3i;
  a[7] = x1i - x3r;

  x0r  = a[8]  + a[10];
  x0i  = a[9]  + a[11];
  x1r  = a[8]  - a[10];
  x1i  = a[9]  - a[11];
  x2r  = a[12] + a[14];
  x2i  = a[13] + a[15];
  x3r  = a[12] - a[14];
  x3i  = a[13] - a[15];
  a[8] = x0r + x2r;
  a[9] = x0i + x2i;
  a[12] = x2i - x0i;
  a[13] = x0r - x2r;
  x0r   = x1r - x3i;
  x0i   = x1i + x3r;
  a[10] = wk1r * (x0r - x0i);
  a[11] = wk1r * (x0r + x0i);
  x0r   = x3i + x1r;
  x0i   = x3r - x1i;
  a[14] = wk1r * (x0i - x0r);
  a[15] = wk1r * (x0i + x0r);

#pragma mta use 100 streams
#pragma mta no scalar expansion
#pragma mta assert no dependence
  for (j = 16, k1 = 2; j < n; j += 16, k1 += 2) {
      double wk2r = w[k1];
      double wk2i = v[k1];
      double wk1r = w[k1 + k1];
      double wk1i = v[k1 + k1];
      double wk3r = wk1r - 2 * wk2i * wk1i;
      double wk3i = 2 * wk2i * wk1r - wk1i;

      btrfly (j, wk1r, wk1i, wk2r, wk2i, wk3r, wk3i, a, b, c, d);

      wk1r = w[k1 + k1 + 2];
      wk1i = v[k1 + k1 + 2];
      wk3r = wk1r - 2 * wk2r * wk1i;
      wk3i = 2 * wk2r * wk1r - wk1i;

      btrfly (j + 8, wk1r, wk1i, - wk2i, wk2r, wk3r, wk3i, a, b, c, d);
} }

void cftmd0(int n, int l, double *a, double *w)
{ int j, m = l << 2;

  double wk1r = w[2];

  double *v = w + 1;
  double *b = a + l;
  double *c = a + l + l;
  double *d = a + l + l + l;

#pragma mta use 100 streams
#pragma mta assert no dependence
  for (j = 0; j < l; j += 2)
      btrfly(j, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, a, b, c, d);

#pragma mta use 100 streams
#pragma mta assert no dependence
  for (j = m; j < l + m; j += 2)
      btrfly(j, wk1r, wk1r, 0.0, 1.0, - wk1r, wk1r, a, b, c, d);
}

void cftmd1(int n, int l, double *a, double *w)
{ int j, k, k1;

  int m  = l << 2;
  int m2 = 2 * m;

  double *v = w + 1;
  double *b = a + l;
  double *c = a + l + l;
  double *d = a + l + l + l;

  cftmd0(n, l, a, w);

#pragma mta use 100 streams
#pragma mta no scalar expansion
#pragma mta assert no dependence
  for (k = m2, k1 = 2; k < n; k += m2, k1 += 2) {
      double wk2r = w[k1];
      double wk2i = v[k1];
      double wk1r = w[k1 + k1];
      double wk1i = v[k1 + k1];
      double wk3r = wk1r - 2 * wk2i * wk1i;
      double wk3i = 2 * wk2i * wk1r - wk1i;

      for (j = k; j < l + k; j += 2)
          btrfly (j, wk1r, wk1i, wk2r, wk2i, wk3r, wk3i, a, b, c, d);

      wk1r = w[k1 + k1 + 2];
      wk1i = v[k1 + k1 + 2];
      wk3r = wk1r - 2 * wk2r * wk1i;
      wk3i = 2 * wk2r * wk1r - wk1i;

      for (j = k + m; j < l + k + m; j += 2)
          btrfly (j, wk1r, wk1i, - wk2i, wk2r, wk3r, wk3i, a, b, c, d);
} }

void cftmd21(int n, int l, double *a, double *w)
{ int j, k, k1;
  int m  = l << 2;
  int m2 = 2 * m;
  int m3 = 3 * m;

  double *v = w + 1;
  double *b = a + l;
  double *c = a + l + l;
  double *d = a + l + l + l;

  for (k = m2, k1 = 2; k < n; k += m2, k1 += 2) {
      double wk2r = w[k1];
      double wk2i = v[k1];
      double wk1r = w[k1 + k1];
      double wk1i = v[k1 + k1];
      double wk3r = wk1r - 2 * wk2i * wk1i;
      double wk3i = 2 * wk2i * wk1r - wk1i;

#pragma mta use 100 streams
#pragma mta assert no dependence
  for (j = k; j < k + l; j += 2)
      btrfly (j, wk1r, wk1i, wk2r, wk2i, wk3r, wk3i, a, b, c, d);

      wk1r = w[k1 + k1 + 2];
      wk1i = v[k1 + k1 + 2];
      wk3r = wk1r - 2 * wk2r * wk1i;
      wk3i = 2 * wk2r * wk1r - wk1i;

#pragma mta use 100 streams
#pragma mta assert no dependence
  for (j = k + m; j < k + m + l; j += 2)
      btrfly (j, wk1r, wk1i, - wk2i, wk2r, wk3r, wk3i, a, b, c, d);

} }

void cftmd2(int n, int l, double *a, double *w)
{ int j, k, k1;

  int m  = l << 2;
  int m2 = 2 * m;

  double *v = w + 1;
  double *b = a + l;
  double *c = a + l + l;
  double *d = a + l + l + l;

  cftmd0(n, l, a, w);

  if (m2 >= n) return;
  if (m2 >= n / 8) {cftmd21(n, l, a, w); return;}

#pragma mta use 100 streams
#pragma mta assert no dependence
  for (j = 0; j < l; j += 2)  {
#pragma mta assert no dependence
  for (k = m2, k1 = 2; k < n; k += m2, k1 += 2) {
      double wk2r = w[k1];
      double wk2i = v[k1];
      double wk1r = w[k1 + k1];
      double wk1i = v[k1 + k1];
      double wk3r = wk1r - 2 * wk2i * wk1i;
      double wk3i = 2 * wk2i * wk1r - wk1i;

      btrfly (j + k, wk1r, wk1i, wk2r, wk2i, wk3r, wk3i, a, b, c, d);
  }

#pragma mta assert no dependence
  for (k = m2, k1 = 2; k < n; k += m2, k1 += 2) {
      double wk2r = w[k1];
      double wk2i = v[k1];
      double wk1r = w[k1 + k1 + 2];
      double wk1i = v[k1 + k1 + 2];
      double wk3r = wk1r - 2 * wk2r * wk1i;
      double wk3i = 2 * wk2r * wk1r - wk1i;

      btrfly (j + k + m, wk1r, wk1i, - wk2i, wk2r, wk3r, wk3i, a, b, c, d);

} } }

void dfft(int n, int logn, double *a, double *w)
{ int i, l, j;
  double *v, *b, *c, *d;

  cft1st(n, a, w);

  i = 4; l = 8;

  for ( ; i <= logn / 2; i += 2, l *= 4) cftmd1(n, l, a, w);
  for ( ; i <= logn - 1; i += 2, l *= 4) cftmd2(n, l, a, w);

  v = w + 1;
  b = a + l;
  c = a + l + l;
  d = a + l + l + l;

  if ((l << 2) == n) {

#pragma mta use 100 streams
#pragma mta assert no dependence
     for (j = 0; j < l; j += 2)
         btrfly(j, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, a, b, c, d);

  } else {

#pragma mta use 100 streams
#pragma mta assert no dependence
     for (j = 0; j < l; j += 2) {
         double x0r = a[j    ];
         double x0i = a[j + 1];
         double x1r = b[j    ];
         double x1i = b[j + 1];
         a[j    ]   = x0r + x1r;
         a[j + 1]   = x0i + x1i;
         b[j    ]   = x0r - x1r;
         b[j + 1]   = x0i - x1i;
} }  }

int main(int argc, char *argv[])
{ int i;
  double gflop, maxerr, time;

  int logN  = atoi(argv[1]);
  int N     = 1 << logN;

  double EPS = pow(2.0, -51.0);
  double THRESHOLD = 16.0;

  double *a = new double[N * 2];
  double *b = new double[N * 2];
  double *w = new double[N / 2];

  int N2 = 2 * N;
  prand_(&N2, a);

/* save a for verification step */
  for (i = 0; i < N2; i++) b[i] = a[i];

  twiddles(N / 4, w);
  w = bit_reverse(N / 4, w);

/* conjugate data */
#pragma mta assert parallel
  for (i = 1; i < N2; i += 2) a[i] = -a[i];

  a = bit_reverse(N, a);
  dfft(N2, logN, a, w);

/* conjugate and scale data */
#pragma mta assert parallel
  for (i = 0; i < N2; i += 2)
      {a[i] = a[i] / N; a[i + 1] = -a[i + 1] / N;}

  time = timer();

  a = bit_reverse(N, a);
  dfft(N2, logN, a, w);

  time  = timer() - time;
  gflop = 5.0 * N * logN / 1000000000.0;

/* verify fft */
  for (i = 0, maxerr = 0.0; i < N2; i += 2) {
      double tmp1 = b[i]     - a[i];
      double tmp2 = b[i + 1] - a[i + 1];
      double tmp3 = sqrt(tmp1 * tmp1 + tmp2 * tmp2);
      maxerr = (tmp3 > maxerr) ? tmp3 : maxerr;
  }

  maxerr = maxerr / logN / EPS;
  if (maxerr < THRESHOLD) printf("SUCCESS, error = %lf\n", maxerr);
  else                    printf("FAILURE, error = %lf\n", maxerr);

  printf("\n\n");
  printf("N      = %d\n", N);
  printf("Time   = %lf\n", time);
  printf("GFlops = %lf\n", gflop / time);
}
