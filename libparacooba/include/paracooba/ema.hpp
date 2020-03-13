#ifndef EMA_HPP
#define EMA_HPP

#include <bits/stdint-intn.h>
#include <cassert>

namespace paracooba {

// Based on CaDiCaL's implementation of EMAs
class EMA {
public:
EMA()
  : value(0)
  , alpha(0)
  , beta(0)
  , wait(0)
  , period(0)
{}

EMA(double a)
  : value(0)
  , alpha(a)
  , beta(1.0)
  , wait(0)
  , period(0)
{
  assert(0 <= alpha), assert(alpha <= beta), assert(beta <= 1);
}

operator double() const { return value; }
void update(double y);


EMA init_ema(double window)
{
  assert(window >= 1);
  double alpha = 1.0 / window;
  EMA e (alpha);
  return e;
}

private:
double value;  // current average value
double alpha;  // percentage contribution of new values
double beta;   // current upper approximation of alpha
int64_t wait;  // count-down using 'beta' instead of 'alpha'
int64_t period;// length of current waiting phase
};
}

#endif
