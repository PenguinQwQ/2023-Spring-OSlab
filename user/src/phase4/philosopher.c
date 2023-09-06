#include "philosopher.h"

// TODO: define some sem if you need
int count, sig[6];

void init() {
  // init some sem if you need
  //TODO();
  count = sem_open(4);
  sig[0]  = sem_open(1);
  sig[1]  = sem_open(1);
  sig[2]  = sem_open(1);
  sig[3]  = sem_open(1);
  sig[4]  = sem_open(1);
}

void philosopher(int id) {
  // implement philosopher, remember to call `eat` and `think`
  while (1) {
   // TODO();
    sem_p(count);
    sem_p(sig[id]);
    sem_p(sig[(id + 1) % 5]);
    eat(id);
    sem_v(sig[id]);
    sem_v(sig[(id + 1) % 5]);
    sem_v(count);
    think(id);
  }
}
