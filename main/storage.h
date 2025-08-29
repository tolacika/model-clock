#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

typedef struct {
  uint32_t model_ts;
  uint32_t real_ts;
  uint32_t timescale;
} storage_data_t;

void storage_init(void);

void storage_load(void);
void storage_save(void);

void storage_write(storage_data_t *data);
void storage_read(storage_data_t *data);

#endif