/* ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef _ID_TABLE_H_
#define _ID_TABLE_H_

struct id_table_entry;
typedef struct id_table_entry id_table_entry;

struct key_hash_entry;
typedef struct key_hash_entry key_hash_entry;

typedef struct id_table_range {
  unsigned long start;
  unsigned long end;
  struct id_table_range *next;
} id_table_range;

typedef struct id_table {
  id_table_entry **array;  
  unsigned long array_start;
  unsigned long array_size;
  unsigned long array_ptr;
  
  id_table_entry **hash;
  unsigned long hash_size;

  key_hash_entry **key_hash;
  unsigned long key_hash_size;

  unsigned long max_fill;
  unsigned long fill;
} id_table;

id_table_range *id_table_range_new(char *str);
void id_table_range_delete(id_table_range *range);

int id_table_init(id_table *t,id_table_range *range);
void id_table_destroy(id_table *t);

int id_table_create_id(id_table *t,char *name,unsigned int *key);
char *id_table_get_name(id_table *t,int id);
int id_table_remove_stale(id_table *t);

id_table_entry *key_hash_get_entry(id_table *, unsigned *key);
key_hash_entry *key_hash_create_entry(id_table *, id_table_entry *pe, unsigned *key);
void key_hash_delete_keys(id_table *t, id_table_entry *e);

#endif
