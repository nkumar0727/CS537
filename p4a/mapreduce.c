#include "mapreduce.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define LOAD_FACTOR 0.7

//#define TEST_COUNT 500
//#define INIT_SIZE 999331
//#define INIT_SIZE 5381
#define INIT_SIZE 19937
//#define NUM_THREADS 5

//////////////////////////////////////////////////////////////////////
// LINKED LIST OF STRINGS -- SUPPORTS CONCURRENT HEAD INSERTION
//////////////////////////////////////////////////////////////////////

typedef struct __LinkedValue {
	struct __LinkedValue* next;
	char* value;
} LinkedValue;

typedef struct __LinkedList {
	pthread_mutex_t lock;
	LinkedValue* head;
	LinkedValue* nextRecord;
} LinkedList;

int
LinkedList_Init(LinkedList* L) {
	if(!L)
		return -1;
	if(pthread_mutex_init(&L->lock, NULL) != 0)
		return -1;
	L->head = NULL;
	L->nextRecord = NULL;
	return 1;
}

// threadsafe
int
LinkedList_Insert(LinkedList* L, char* value) {
	if(!L)
		return -1;
	LinkedValue* newLink = (LinkedValue*) malloc(sizeof(LinkedValue));
	if(!newLink)
		return -1;
	pthread_mutex_lock(&L->lock);
	newLink->value = strdup(value);
	newLink->next = L->head;
	L->head = newLink;
	L->nextRecord = L->head;
	pthread_mutex_unlock(&L->lock);
	return 1;
}

LinkedValue*
LinkedList_NextRecord(LinkedList* L) {
	LinkedValue* nextRecord = NULL;
	if(L) {
		nextRecord = L->nextRecord;
		if(nextRecord)
			L->nextRecord = nextRecord->next;
	}
	return nextRecord;
}

// not thread safe
void
LinkedList_Destroy(LinkedList* L) {
	pthread_mutex_destroy(&L->lock);
	LinkedValue* curr = L->head;
	LinkedValue* n;
	while(curr) {
		n = curr->next;
		free(curr->value);
		free(curr);
		curr = n;
	}
}

// not threadsafe
void
LinkedList_Print(LinkedList* L) {
	if(!L)
		return;
	LinkedValue* curr = L->head;
	while(curr) {
		printf("%s ", curr->value);		
		curr = curr->next;
	}
	printf("\n");
}

//////////////////////////////////////////////////////////////////////
// KEY/VALUE LIST (LINKED LIST) 
//////////////////////////////////////////////////////////////////////

typedef struct __KeyValueList {
	char* key;
	LinkedList valueList;
} KeyValueList;

int
KeyValueList_Init(KeyValueList* kvl) {
	if(!kvl)
		return -1;
	kvl->key = NULL;
	return LinkedList_Init(&kvl->valueList);
}

int
KeyValueList_Insert(KeyValueList* kvl, char* value) {
	if(!kvl)
		return -1;
	return LinkedList_Insert(&kvl->valueList, value);
}

void
KeyValueList_Destroy(KeyValueList* kvl) {
	if(!kvl)
		return;
	free(kvl->key);
	LinkedList_Destroy(&kvl->valueList);
}

LinkedValue*
KeyValueList_NextRecord(KeyValueList* kvl) {
	return LinkedList_NextRecord(&kvl->valueList);	
}

void
KeyValueList_Print(KeyValueList* kvl) {
	printf("Key: %s | Values: ", kvl->key);
	LinkedList_Print(&kvl->valueList);
}


//////////////////////////////////////////////////////////////////////
// CONCURRENT RESIZING HASHTABLE (LinkedList Buckets)
//////////////////////////////////////////////////////////////////////

// hash function used when hashing within a partition
// number of buckets is specified as # of partition elements
long unsigned int
djb2Hash(char *str, long unsigned int buckets) {
  long unsigned int hash = 5381;
  int c;
  size_t len = strlen(str);
  for(size_t i = 0; i < len; ++i) {
    c = str[i];
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash % buckets;
}

typedef struct __ConcurrentHashTable {
	long unsigned int lfsr;
	long unsigned int size;
	long unsigned int elements;
	long unsigned int nextSlot;
	long unsigned int editors;
	long unsigned int expanders;
	pthread_mutex_t masterLock;
	pthread_mutex_t* elementLock;
	pthread_cond_t editQ;
	pthread_cond_t expandQ;
	KeyValueList* table;	
} ConcurrentHashTable;

int
ConcurrentHashTable_Init(ConcurrentHashTable* cht) {
	if(!cht)
		return -1;
	cht->size = INIT_SIZE;
	cht->elements = 0;
	cht->nextSlot = 0;
	cht->lfsr = 1;
	cht->editors = 0;
	cht->expanders = 0;
	if(pthread_cond_init(&cht->editQ, NULL) != 0 || pthread_cond_init(&cht->expandQ, NULL) != 0)
		return -1;
	if(pthread_mutex_init(&cht->masterLock, NULL) != 0)
		return -1;
	cht->table = (KeyValueList*) malloc(sizeof(KeyValueList) * INIT_SIZE);
	if(!cht->table)
		return -1;
	cht->elementLock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t) * INIT_SIZE);
	if(!cht->elementLock) {
		free(cht->table);
		return -1;
	}
	for(unsigned long int i = 0; i < INIT_SIZE; ++i) {
		pthread_mutex_init(&cht->elementLock[i], NULL);
		KeyValueList_Init(&cht->table[i]);
	}
	return 0;
}

int
ConcurrentHashTable_Expand(ConcurrentHashTable* cht) {
	//printf("Size[%lu], Elems[%lu]-------------------NEED TO EXPAND----------------------\n",
	//cht->size, cht->elements);
	long unsigned int oldSize = cht->size;
	long unsigned int newSize = oldSize * 2;
	cht->table = (KeyValueList*) realloc(cht->table, newSize * sizeof(KeyValueList));
	if(!cht->table)
		return -1;
	memset(&cht->table[oldSize], 0, oldSize * sizeof(KeyValueList));
	cht->elementLock = (pthread_mutex_t*) realloc(cht->elementLock, newSize * sizeof(pthread_mutex_t));
	if(!cht->elementLock) {
		free(cht->table);
		return -1;
	}
	for(long unsigned int i = oldSize; i < newSize; ++i) {
		pthread_mutex_init(&cht->elementLock[i], NULL);
	}
	for(long unsigned int i = oldSize; i < newSize; ++i) {
		KeyValueList_Init(&cht->table[i]);
	}
	cht->size = newSize;
	return 1;
}

void
ConcurrentHashTable_Insert(ConcurrentHashTable* cht, char* key, char* value) {
	long unsigned int bucket;
	char stop = 0;
	char collide = 0;

	// prevent any editors when expansion is taking place
	pthread_mutex_lock(&cht->masterLock);
	while(cht->expanders > 0) {
		pthread_cond_wait(&cht->editQ, &cht->masterLock);
	}
	pthread_mutex_unlock(&cht->masterLock);

	long double currLF = cht->elements / (long double)cht->size;
	if(currLF >= LOAD_FACTOR) {
		// prevent any expanders when editing is taking place
		pthread_mutex_lock(&cht->masterLock);
		cht->expanders++;
		while(cht->editors > 0) {
			pthread_cond_wait(&cht->expandQ, &cht->masterLock);
		}
		ConcurrentHashTable_Expand(cht);
		cht->expanders--;
		// tell asll the potential editors that they may edit now that expansion is over
		pthread_cond_broadcast(&cht->editQ);
		pthread_mutex_unlock(&cht->masterLock);
	}

	// one more editor in shared data
	pthread_mutex_lock(&cht->masterLock);
	cht->editors++;
	pthread_mutex_unlock(&cht->masterLock);

	while(!stop) {
		bucket = djb2Hash(key, cht->size);
		if(collide) {
			bucket = (bucket + cht->lfsr) % cht->size;
			collide = 0;
		}
		KeyValueList* target = &cht->table[bucket]; 
		pthread_mutex_lock(&cht->elementLock[bucket]);
		if(!target->key || strcmp(target->key, key) == 0) {
			if(!target->key) {
				pthread_mutex_lock(&cht->masterLock);
				cht->elements++;
				pthread_mutex_unlock(&cht->masterLock);
				target->key = strdup(key);
			}
			KeyValueList_Insert(target, value);
			pthread_mutex_unlock(&cht->elementLock[bucket]);
			stop = 1;
		}
		else {
			pthread_mutex_unlock(&cht->elementLock[bucket]);
			collide = 1;
			pthread_mutex_lock(&cht->masterLock);
      cht->lfsr = ((cht->lfsr >> 1) ^ (unsigned)((0 - (cht->lfsr & 1u)) & 0xd0000001u));
			pthread_mutex_unlock(&cht->masterLock);
		}
	}

	// one less editor in shared data
	pthread_mutex_lock(&cht->masterLock);
	cht->editors--;
	// tell all the potential expanders that they may expand now that all edits are over
	if(cht->editors == 0)
		pthread_cond_broadcast(&cht->expandQ);
	pthread_mutex_unlock(&cht->masterLock);
}

int
ConcurrentHashTable_Destroy(ConcurrentHashTable* cht) {
	pthread_mutex_destroy(&cht->masterLock);
	pthread_cond_destroy(&cht->editQ);
	pthread_cond_destroy(&cht->expandQ);
	for(long unsigned int i = 0; i < cht->size; ++i) {
		pthread_mutex_destroy(&cht->elementLock[i]);
	}
	free(cht->elementLock);
	for(long unsigned int i = 0; i < cht->size; ++i) {
		KeyValueList_Destroy(&cht->table[i]);
	}
	free(cht->table);
	return 0;
}

void
ConcurrentHashTable_Print(ConcurrentHashTable* cht) {
	for(long unsigned int i = 0; i < cht->size; ++i) {
		KeyValueList* target = &cht->table[i];
		if(target->key)
			printf("Entry[%lu] ::: Key: %s\n", i, target->key);
		/*
		else
			printf("Entry[%lu] ::: Key: NULL\n", i);
		*/
	}
}


//////////////////////////////////////////////////////////////////////
// MAP-REDUCE GLOBAL VARIABLES 
//////////////////////////////////////////////////////////////////////

ConcurrentHashTable* partitionTables;		// each parition gets a hashtable
pthread_mutex_t jobLock;								// lock for jobNumber counter for mappers
long unsigned int numPartitions;			// # of partitions specified by user
long unsigned int jobNumber;						// # of files that have been assigned to mappers
Partitioner partitionFunction;					// partition function used by MR_Emit
Reducer reducerFunction;								// reducer function provided by user
Mapper mapperFunction;									// mapper function provided by user


//////////////////////////////////////////////////////////////////////
// MAP-REDUCE AUXILIARY FUNCTIONS
//////////////////////////////////////////////////////////////////////

unsigned long 
MR_DefaultHashPartition(char *key, int num_partitions) {
  unsigned long hash = 5381;
  int c;
  while ((c = *key++) != '\0')
    hash = hash * 33 + c;
  return hash % num_partitions;
}

// comparison function for <key, value_list> pairs
int
KVL_Compare(const void* p1, const void* p2) {
	KeyValueList* pair1 = (KeyValueList*)p1;
	KeyValueList* pair2 = (KeyValueList*)p2;
	if(!pair1->key)
		return 1;
	else if(!pair2->key)
		return -1;
	return strcmp(pair1->key, pair2->key);
}

char*
get_next(char* key, int partition_number) {
	ConcurrentHashTable* cht = &partitionTables[partition_number];
	KeyValueList* target = &cht->table[cht->nextSlot];
	LinkedValue* targetValue = KeyValueList_NextRecord(target); 
	if(!targetValue) {
		cht->nextSlot += 1;
		return NULL;
	}
	return targetValue->value;
}


//////////////////////////////////////////////////////////////////////
// MAP-REDUCE THREAD-WORKER FUNCTIONS/STRUCTURES
//////////////////////////////////////////////////////////////////////

// struct for passing arguments to worker threads by void*
typedef struct __WorkerArgs {
	int argc;
	char** argv;
} WorkerArgs;

void*
Mapper_Worker(void* args) {
	WorkerArgs* workerArgs = (WorkerArgs*) args;
	int workerJob;
	while(1) {
		pthread_mutex_lock(&jobLock);
		workerJob = jobNumber++;
		if(workerJob >= workerArgs->argc) {
			pthread_mutex_unlock(&jobLock);
			break;
		}
		pthread_mutex_unlock(&jobLock);
		mapperFunction(workerArgs->argv[workerJob]);
	}
	return NULL;
}

void*
Sorter_Worker(void* args) {
	long unsigned int workerPartition = (long unsigned int) args;
	ConcurrentHashTable* cht = &partitionTables[workerPartition];
	qsort((void*)cht->table, cht->size, sizeof(KeyValueList), KVL_Compare);
	return NULL;
}

void*
Reducer_Worker(void* args) {
	long unsigned int workerPartition = (long unsigned int) args;
	ConcurrentHashTable* cht = &partitionTables[workerPartition];
	while(cht->nextSlot < cht->elements) {
		KeyValueList* target = &cht->table[cht->nextSlot];
		reducerFunction(target->key, get_next, workerPartition);
	}
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////
// MAP-REDUCE MAIN FUNCTIONS
/////////////////////////////////////////////////////////////////////////////

void 
MR_Emit(char *key, char *value) {
  int partitionNumber = partitionFunction(key, numPartitions);
	ConcurrentHashTable* cht = &partitionTables[partitionNumber];
	ConcurrentHashTable_Insert(cht, key, value);
}

void
MR_Run(int argc, char* argv[], Mapper map, int num_mappers, Reducer reduce, 
	int num_reducers, Partitioner partition) {
	
	// initialize global variables
	numPartitions = num_reducers;
	partitionFunction = (partition == NULL ? MR_DefaultHashPartition : partition);
	mapperFunction = map;
	reducerFunction = reduce;
	jobNumber = 1;
	assert(pthread_mutex_init(&jobLock, NULL) == 0);
	partitionTables = (ConcurrentHashTable*) malloc(sizeof(ConcurrentHashTable) * num_reducers);	
	assert(partitionTables != NULL);
	for(int i = 0; i < num_reducers; ++i) {
		assert(ConcurrentHashTable_Init(&partitionTables[i]) == 0);
	}

	// setup argument struct to pass to mapper worker threads
	WorkerArgs workerArgs;
	workerArgs.argc = argc;
	workerArgs.argv = argv;

	// setup threads
	pthread_t mapperThreads[num_mappers];
	//pthread_t reducerThreads[num_reducers];

	// Map-Reduce Mapping Phase
	for(int i = 0; i < num_mappers; ++i) {
		pthread_create(&mapperThreads[i], NULL, Mapper_Worker, (void*)&workerArgs);
	}
	for(int i = 0; i < num_mappers; ++i) {
		pthread_join(mapperThreads[i], NULL);
	}
	
	for(int i = 0; i < num_reducers; ++i) {
		ConcurrentHashTable_Print(&partitionTables[i]);
	}

/*
	// Map-Reduce Sorting Phase 
	for(long unsigned int i = 0; i < num_reducers; ++i) {
		pthread_create(&reducerThreads[i], NULL, Sorter_Worker, (void*)i);
	}
	for(long unsigned int i = 0; i < num_reducers; ++i) {
		pthread_join(reducerThreads[i], NULL);
	}

	// Map-Reduce Reducing Phase
	for(long unsigned int i = 0; i < num_reducers; ++i) {
		pthread_create(&reducerThreads[i], NULL, Reducer_Worker, (void*)i);
	}
	for(long unsigned int i = 0; i < num_reducers; ++i) {
		pthread_join(reducerThreads[i], NULL);
	}
*/
	// Cleanup
	assert(pthread_mutex_destroy(&jobLock) == 0);
	for(int i = 0; i < num_reducers; ++i) {
		ConcurrentHashTable_Destroy(&partitionTables[i]);
	}
	free(partitionTables);
}

void Map(char *file_name) {
    FILE *fp = fopen(file_name, "r");
    assert(fp != NULL);

    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, fp) != -1) {
        char *token, *dummy = line;
        while ((token = strsep(&dummy, " \t\n\r")) != NULL) {
            MR_Emit(token, "1");
        }
    }
    free(line);
    fclose(fp);
}

void Reduce(char *key, Getter get_next, int partition_number) {
    int count = 0;
    char *value;
    while ((value = get_next(key, partition_number)) != NULL)
        count++;
    printf("%s %d\n", key, count);
}

int main(int argc, char *argv[]) {
    MR_Run(argc, argv, Map, 10, Reduce, 10, MR_DefaultHashPartition);
}

/*
int
main() {
	ConcurrentHashTable cht;
	ConcurrentHashTable_Init(&cht);
	
	records = (KVP*) malloc(sizeof(KVP) * TEST_COUNT);
	assert(records != NULL);

	for(long unsigned i = 0; i < TEST_COUNT; ++i) {
		records[i].key = rand_str();
		records[i].value = rand_str();
	}

	printf("Records to Insert\n");
	for(long unsigned int i = 0; i < TEST_COUNT; ++i) {
		printf("KVP[%lu] ::: <%s, %s>\n", i, records[i].key, records[i].value);
	}
	printf("\n\n=================================================\n\n");

	partSize = (TEST_COUNT / NUM_THREADS) + 1;
	if(TEST_COUNT % NUM_THREADS != 0)
		partSize += 1;

	WARG wargs[NUM_THREADS];
	pthread_t threads[NUM_THREADS];
	for(long unsigned int i = 0; i < NUM_THREADS; ++i) {
		wargs[i].cht = &cht;
		wargs[i].thread_num = i;
	}

	for(long unsigned int i = 0; i < NUM_THREADS; ++i) {
		pthread_create(&threads[i], NULL, worker, (void*)&wargs[i]);
	}
	for(long unsigned int i = 0; i < NUM_THREADS; ++i) {
		pthread_join(threads[i], NULL);
	}
	printf("\n------------------PRINTING HASHTABLE------------------\n");

	ConcurrentHashTable_Print(&cht);

	for(long unsigned i = 0; i < TEST_COUNT; ++i) {
		free(records[i].key);
		free(records[i].value);
	}
	free(records);
	ConcurrentHashTable_Destroy(&cht);
	return 0;
}
*/
