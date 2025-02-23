#include <stdio.h>
#include <stdlib.h>
#include "uthash.h"

// Define the structure for the hash table
typedef struct {
    int id;            // Key
    int value;         // Value
    UT_hash_handle hh; // Hash handle
} HashTableItem;

int main() {
    HashTableItem *hashTable = NULL; // Initialize the hash table
    HashTableItem *item, *tmp;

    // Add an integer to the hash table
    int key = 0;
    int val = 42;
    item = (HashTableItem *)malloc(sizeof(HashTableItem));
    item->id = key;
    item->value = val;
    HASH_ADD_INT(hashTable, &key, item);

    // Retrieve the integer from the hash table
    HASH_FIND_INT(hashTable, &key, tmp);
    if (tmp) {
        printf("Found: key = %d, value = %d\n", tmp->id, tmp->value);
    } else {
        printf("Key not found\n");
    }

    // Clean up
    HASH_ITER(hh, hashTable, item, tmp) {
        HASH_DEL(hashTable, item);
        free(item);
    }

    return 0;
}
