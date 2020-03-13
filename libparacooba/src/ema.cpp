#include "../include/paracooba/ema.hpp"

namespace paracooba {

void EMA::update(double y)
{
  value += beta * (y - value);
  if (beta <= alpha || wait--)
    return;
  wait = period = 2 * (period + 1) - 1;
  beta *= 0.5;
  if(beta < alpha)
    beta = alpha;
}
}
