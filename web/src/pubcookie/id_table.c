#include "id_table.h"
#include "wp_uidmapper_lib.h"

#include <sys/types.h> /* opendir */
#include <sys/stat.h> /* stat */
#include <stdlib.h> /* mallloc,free,strtol */
#include <string.h> /* memcpy */
#include <errno.h> /* errno */

#include <dirent.h> /* opendir */
#include <unistd.h> /* stat */

unsigned long hash_func(char *string,unsigned long num_buckets) {
  unsigned long i;
  char *p;
  for(i = 0, p = string; *p; p++) i = (9 * i) + *p;
  return i % num_buckets;
}

struct id_table_entry {
  struct id_table_entry **pself;
  struct id_table_entry *next;
  int id;
  char tmp;
  char name[1];
};

struct key_hash_entry {
  unsigned key[WP_KEY_LEN];
  id_table_entry *e;
  struct key_hash_entry **pself;
  struct key_hash_entry *next;
};

id_table_range *id_table_range_new(char *str) {
  id_table_range *rhead,*rtail;
  char *p,*pn;
  long s,e;
  
  rhead = rtail = 0;
  for(p = str; *p; p = *pn ? pn + 1 : pn) {
    s = strtoul(p,&pn,0);
    if(pn > p) {
      if(*pn == '-') {
	p = pn + 1;
	e = strtoul(p,&pn,0);
	if(pn == p) e = s;
      } else {
	e = s;
      }

      if(rtail) {
	rtail->next = (id_table_range*)malloc(sizeof(id_table_range));
	rtail = rtail->next;
      } else {
	rhead = rtail = (id_table_range*)malloc(sizeof(id_table_range));
      }
      rtail->start = s;
      rtail->end = e;
    }
  }
  if(rtail) rtail->next = 0;
  return rhead;
}

void id_table_range_delete(id_table_range *range) {
  id_table_range *r;
  while(r = range) {
    range = range->next;
    free(r);
  }
}

int id_table_init(id_table *t,id_table_range *range) {
  id_table_range *r;
  unsigned long array_end,u;
  
  if(!range) return -1;
  t->array_start = range->start;
  array_end = range->end;
  for(r = range->next; r; r = r->next) {
    if(r->start < t->array_start) t->array_start = r->start;
    if(r->end > array_end) array_end = r->end;
  }
  if(t->array_start >= array_end) return -1;  
  t->array_size = array_end + 1 - t->array_start;
  t->hash_size = t->array_size * 2 + 1;
  t->key_hash_size = t->array_size * 2 + 1;
  
  t->array = (id_table_entry**)malloc(sizeof(id_table_entry*) * t->array_size);
  t->hash = (id_table_entry**)malloc(sizeof(id_table_entry*) * t->hash_size);
  t->key_hash = (key_hash_entry**)malloc(sizeof(key_hash_entry *) * t->key_hash_size);
  if(!t->hash || !t->array || !t->key_hash) {
    if(t->hash) free(t->hash);
    if(t->key_hash) free(t->key_hash);
    if(t->array) free(t->array);
    return -1;
  }
  for(u = 0; u < t->array_size; u++) t->array[u] = (id_table_entry*)-1;
  t->max_fill = 0;
  for(r = range; r; r = r->next) {
    t->max_fill += r->end + 1 - r->start;    
    for(u = r->start; u <= r->end; u++) t->array[u - t->array_start] = 0;
  }
  bzero(t->hash,sizeof(id_table_entry*) * t->hash_size);
  bzero(t->key_hash, sizeof(key_hash_entry*) * t->key_hash_size);
  
  t->array_ptr = 0;
  t->fill = 0;
  return 0;
}

void id_table_destroy(id_table *t) {
  unsigned long u;
  for(u = 0; u < t->array_size; u++)
    if(t->array[u] && (t->array[u] != (id_table_entry*)-1)) free(t->array[u]);
  free(t->hash);
  free(t->array);
}

int id_table_create_id(id_table *t,char *name,unsigned *key) {
  id_table_entry **pe,*e;
  unsigned long size;

  if(name && strlen(name)){
      for(pe = t->hash + hash_func(name,t->hash_size); *pe; pe = &(*pe)->next)
	if(!strcmp(name,(*pe)->name)){
	    if(key[0]){
		if((e = key_hash_get_entry(t, key)) == NULL){
		    if(key_hash_create_entry(t, *pe, key) == NULL){
			return -1;
		    }
		}
		else if(e->id != (*pe)->id){
		    return -1;
		}
	    }

	    return (*pe)->id;
	}

      /* no matching name found */
  }
  else {
      if(e = key_hash_get_entry(t,key))
	return e->id;

      /* MUST have seen name/key pair at least once */
      errno = EINVAL;
      return -1;
  }

  /* need to add new entry */
  if(t->fill == t->max_fill) {
    id_table_remove_stale(t);
    if(t->fill == t->max_fill) {
      errno = ENOSPC;
      return -1;
    }
    for(pe = t->hash + hash_func(name,t->hash_size); *pe; pe = &(*pe)->next);
  }
  size = strlen(name);
  e = (id_table_entry*)malloc(sizeof(id_table_entry) + size);
  if(!e) return -1;

  while(t->array[t->array_ptr])
    t->array_ptr = (t->array_ptr + 1) % t->array_size;
  
  e->pself = pe;
  e->next = 0;
  e->id = (int)(t->array_ptr + t->array_start);
  memcpy(e->name,name,size + 1);
  if(key[0] && key_hash_create_entry(t, e,key) == NULL)
    return -1;
  
  *pe = e;
  t->array[t->array_ptr] = e;
  t->array_ptr = (t->array_ptr + 1) % t->array_size;
  t->fill++;
  return e->id;
}

static id_table_entry *id_table_get_entry(id_table *t,unsigned long id) {
  if(id >= t->array_start) {
    id -= t->array_start;
    if(id < t->array_size)
      if(t->array[id] && (t->array[id] != (id_table_entry*)-1))	
	return t->array[id];
  }
  return 0;
}

char *id_table_get_name(id_table *t,int id) {
  id_table_entry *e = id_table_get_entry(t,(unsigned long)id);
  return e ? e->name : 0;
}
  
int id_table_remove_stale(id_table *t) {
  id_table_entry *e;
  unsigned long u;
  DIR *d;
  struct dirent *de;
  struct stat st;
  char path[NAME_MAX + 7];
  
  /*
   * flag all uids inactive
   */

  for(u = 0; u < t->array_size; u++)
    if(t->array[u] && (t->array[u] != (id_table_entry*)-1)) 
      t->array[u]->tmp = 0;

  /*
   * go through list of processes, finding active uids
   */
  
  memcpy(path,"/proc/",6);
  if(d = opendir("/proc")) {
    while(de = readdir(d)) {
      strcpy(path + 6,de->d_name);
      if(!stat(path,&st)) {
	if(e = id_table_get_entry(t,(unsigned long)st.st_uid)) e->tmp = 1;
      }
    }
    closedir(d);
  }

  /*
   * remove ones still marked inactive
   */
  
  for(u = 0; u < t->array_size; u++) {
    e = t->array[u];
    if(e && (e != (id_table_entry*)-1)) {
      if(!e->tmp) {
	if(e->next) e->next->pself = e->pself;
	*e->pself = e->next;	
	t->array[u] = 0;
	t->array_ptr = u;
	t->fill--;
	key_hash_delete_keys(t, e);
	free(e);
      }
    }
  }
  return 0;
}

id_table_entry *key_hash_get_entry(id_table *t, unsigned *key) {
    key_hash_entry **ke;

    for(ke = t->key_hash + (key[0] % t->key_hash_size); *ke; ke = &(*ke)->next)
      if(key[0] && (*ke)->key[0] == key[0] && !memcmp(key,(*ke)->key,WP_KEY_LEN * sizeof(unsigned)))
	return (*ke)->e;

    return NULL;
}

key_hash_entry *key_hash_create_entry(id_table *t, id_table_entry *pe, unsigned *key) {
    key_hash_entry **ke, *kep;

    for(ke = &t->key_hash[key[0] % t->key_hash_size]; *ke; ke = &(*ke)->next)
      if(!memcmp(key,(*ke)->key,WP_KEY_LEN * sizeof(unsigned)))
	return NULL;

    if(kep = malloc(sizeof(key_hash_entry))){
	memcpy(kep->key,key,(WP_KEY_LEN * sizeof(unsigned int)));
	kep->e = pe;
	kep->pself = ke;
	kep->next = NULL;
	*ke = kep;
    }

    return kep;
}

void key_hash_delete_keys(id_table *t, id_table_entry *e) {
    key_hash_entry *kp, *kd;
    int u;

    for(u = 0; u < t->key_hash_size; u++){
	for(kp = t->key_hash[u]; kp; )
	  if (kp->e == e){
	      kd = kp;
	      if(kp->next) kp->next->pself = kp->pself;
	      *kp->pself = kp->next;
	      kp = kp->next;
	      free(kd);
	  }
	  else
	    kp = kp->next;
    }
}
