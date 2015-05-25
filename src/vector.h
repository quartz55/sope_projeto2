/*
 * Custom made class to represent a dynamic
 * sized array (aka a vector)
 */

#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define VECTOR_DEFAULT_CAPACITY 100

typedef struct Vector_t
{
    int size;
    int capacity;
    void **data;
}Vector_t;

Vector_t* Vector_new();
void Vector_destroy(Vector_t *vec);

int Vector_size(Vector_t *vec);
void Vector_push(Vector_t *vec, void *element);
void* Vector_get(Vector_t *vec, int i);
void* Vector_last(Vector_t *vec);

inline Vector_t* Vector_new()
{
    Vector_t* vec = (Vector_t *)malloc(sizeof(Vector_t));

    if(!vec) return NULL;

    vec->size = 0;
    vec->capacity = VECTOR_DEFAULT_CAPACITY;
    vec->data = (void **)malloc(vec->capacity*sizeof(void *));

    if(!vec->data)
        if(vec){
            free(vec);
            return NULL;
        }

    return vec;
}

inline void Vector_destroy(Vector_t *vec)
{
    int i;
    for(i = 0; i < vec->size; i++)
        free(vec->data[i]);
    free(vec->data);
    free(vec);
}

inline int Vector_size(Vector_t *vec)
{
    assert(vec != NULL);

    return vec->size;
}

inline void Vector_push(Vector_t *vec, void *element)
{
    assert(vec != NULL);
    assert(vec->data != NULL);

    if(vec->size >= vec->capacity){
        vec->capacity *= 2;
        void** newData = (void **)realloc(vec->data, vec->capacity*sizeof(*vec->data));
        vec->data = newData;
    }

    vec->data[vec->size++] = element;
}

inline void* Vector_get(Vector_t *vec, int i)
{
    assert(vec != NULL);
    assert(vec->data != NULL);
    if(i >= vec->size || i < 0){
        printf("#ERROR# Index out of bounds: %d\n", i);
        abort();
    }

    return vec->data[i];
}
inline void* Vector_last(Vector_t *vec)
{
    assert(vec != NULL);
    assert(vec->data != NULL);

    return vec->data[vec->size-1];
}
#endif /* VECTOR_H */
