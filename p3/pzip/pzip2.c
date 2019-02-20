#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

/////////////////////////////////////////////////
// IMPORTANT MACROS
/////////////////////////////////////////////////

#define MAX_FRQ (~0)			// limit for char frequency
#define UINT_SZ sizeof(unsigned)
#define CH_SZ sizeof(char)
#define NL '\n'
/** #define PAGE_SIZE 20 */
#define PAGE_SIZE (1<<20)
/** #define PAGE_SIZE 32786 */
/** #define PAGE_SIZE 65572 */
/** #define PAGE_SIZE (100000000000)	// file chunk size */
/** #define PAGE_SIZE 4294967296 */
/** #define MAXQ 100					// max work queue size */
#define BLK_PAIRS (1<<15) // (int, char) pairs in block
#define REC_LEN ((sizeof(unsigned)) + (sizeof(char)))				// size of (int, char) pair

/////////////////////////////////////////////////
// STORAGE BLOCKS FOR RLE CHUNKS (non binary)
/////////////////////////////////////////////////

typedef struct ZipBuffer {
  size_t n_chunks;
  size_t* size_list;
  size_t* cap_list;
  void** zip_chunks;
} ZipBuffer;
ZipBuffer zbuffer;

int
init_chunks(size_t n_chunks, size_t init_pairs) {
  zbuffer.zip_chunks = (void**) malloc(sizeof(void*) * n_chunks);
  zbuffer.size_list = (size_t*) calloc(n_chunks, sizeof(size_t));
  zbuffer.cap_list = (size_t*) malloc(sizeof(size_t) * n_chunks);
  if(!zbuffer.cap_list || !zbuffer.size_list || !zbuffer.zip_chunks) {
    free(zbuffer.cap_list); free(zbuffer.size_list);
    free(zbuffer.zip_chunks); return -1;
  }
  for(size_t i = 0; i < n_chunks; ++i) {
    zbuffer.cap_list[i] = init_pairs;
  }
  for(size_t i = 0; i < n_chunks; ++i) {
    zbuffer.zip_chunks[i] = (void*) malloc(REC_LEN * init_pairs);
    if(!zbuffer.zip_chunks[i]) {
      for(size_t j = i; j >= 0; --j) {
        free(zbuffer.zip_chunks[j]);
      }
      free(zbuffer.zip_chunks); free(zbuffer.cap_list);
      free(zbuffer.size_list); zbuffer.zip_chunks = NULL;
      return -1;
    }
  }
  zbuffer.n_chunks = n_chunks;
  return 0;
}

void
delete_chunks() {
  for(size_t i = 0; i < zbuffer.n_chunks; ++i) {
    free(zbuffer.zip_chunks[i]);
  }
  free(zbuffer.zip_chunks); free(zbuffer.cap_list);
  free(zbuffer.size_list); zbuffer.zip_chunks = NULL;
  zbuffer.cap_list = NULL; zbuffer.size_list = NULL;
  zbuffer.n_chunks = 0;
}

int
insert_chunks(size_t chunk_n, unsigned freq, char enc) {
  int ret_value = 0;
  size_t curr_sz = zbuffer.size_list[chunk_n];
  size_t curr_cap = zbuffer.cap_list[chunk_n];
  if(curr_sz >= curr_cap) {
    size_t new_bytes = curr_cap * 2 * REC_LEN;
    zbuffer.zip_chunks[chunk_n] = (void*) realloc(zbuffer.zip_chunks[chunk_n], new_bytes);
    if(!zbuffer.zip_chunks[chunk_n])
      return -1;
    zbuffer.cap_list[chunk_n] *= 2;
    ret_value = 2;
  }
  *((unsigned*)(zbuffer.zip_chunks[chunk_n] + (curr_sz * REC_LEN))) = freq;
  *((char*)((zbuffer.zip_chunks[chunk_n] + (curr_sz * REC_LEN)) + UINT_SZ)) = enc;
  zbuffer.size_list[chunk_n] += 1;
  return ret_value;
}

void
print_chunks() {
	/** int bufferCount = 0;
		* void* buffer[]; */
  for(size_t i = 0; i < zbuffer.n_chunks; ++i) {
		for(size_t j = 0; j < zbuffer.size_list[i]; ++j) {
      printf("%u%c",
			*((unsigned*)(zbuffer.zip_chunks[i] + (j * REC_LEN))),
        *((char*)((zbuffer.zip_chunks[i] + (j * REC_LEN)) + UINT_SZ)));
		}
		/** if(zbuffer.size_list[i] > 0) {
			*   fwrite(zbuffer.zip_chunks[i], REC_LEN, zbuffer.size_list[i], stdout);
			* } */
  }
}

// all chunks, write
// on last chunk, write everything up till last pair
void
print_most_chunks() {
	size_t last_chunk = zbuffer.n_chunks-1;
	for(size_t i = zbuffer.n_chunks - 1; i >= 0; --i) {
		if(zbuffer.size_list[i] > 0) {
			last_chunk = i;
			break;
		}
	}
	for(size_t i = 0; i < zbuffer.n_chunks; ++i) {
		if(i == last_chunk) {
			for(size_t j = 0; j < zbuffer.size_list[i] -1; ++j) {
				printf("%u%c",
				*((unsigned*)(zbuffer.zip_chunks[i] + (j * REC_LEN))),
					*((char*)((zbuffer.zip_chunks[i] + (j * REC_LEN)) + UINT_SZ)));
			}
		}
		else {
			for(size_t j = 0; j < zbuffer.size_list[i]; ++j) {
				printf("%u%c",
				*((unsigned*)(zbuffer.zip_chunks[i] + (j * REC_LEN))),
					*((char*)((zbuffer.zip_chunks[i] + (j * REC_LEN)) + UINT_SZ)));
			}
		}
		/** if(zbuffer.size_list[i] > 0) {
			*   if(i == last_chunk)
			*     fwrite(zbuffer.zip_chunks[i], REC_LEN, zbuffer.size_list[i]-1, stdout);
			*   else
			*     fwrite(zbuffer.zip_chunks[i], REC_LEN, zbuffer.size_list[i], stdout);
			* } */
	}
}

void
stitch_chunks(size_t n_chunks) {
	size_t lhs_last = 0;
	unsigned sum = 0;
	for(size_t i = 0; i < n_chunks - 1; ++i) {
		lhs_last = zbuffer.size_list[i] - 1;
		char lhs_char = *((char*)(zbuffer.zip_chunks[i] + (lhs_last * REC_LEN) + UINT_SZ)); 
		char rhs_char = *((char*)(zbuffer.zip_chunks[i+1] + UINT_SZ));
		unsigned lhs_freq = *((unsigned*)(zbuffer.zip_chunks[i] + (lhs_last * REC_LEN)));
		unsigned rhs_freq = *((unsigned*)(zbuffer.zip_chunks[i+1]));
		if(lhs_char == rhs_char) {
			sum = lhs_freq + rhs_freq;
			if(sum < lhs_freq) {
				*((unsigned*)(zbuffer.zip_chunks[i] + (lhs_last * REC_LEN))) = MAX_FRQ;
			}
			else {
				if(zbuffer.size_list[i] > 0) {
					zbuffer.size_list[i] -= 1;
				}
			}
			*((unsigned*)(zbuffer.zip_chunks[i+1])) = sum;
		}
	}
}

/////////////////////////////////////////////////
// MORE STRUCTS
/////////////////////////////////////////////////

// record
typedef struct pair {
	unsigned freq;
	char enc;
} pair;

// file information
typedef struct f_info {
	void* start;
	void* end;
	size_t n_chunks;
	size_t chunks_done;
} f_info;

// job description
typedef struct job_t {
	void* start;
	void* end;
	size_t chunk_num;
} job_t;

/////////////////////////////////////////////////
// GLOBAL VARIABLES
/////////////////////////////////////////////////

f_info info;					// file information
pthread_mutex_t lock;
size_t proc_count;		// number of consumer threads
pair old_pair;		//These variables are used for File stitching
pair new_pair;

/////////////////////////////////////////////////
// PRODUCER / CONSUMER
/////////////////////////////////////////////////

int
my_zip(job_t job) {
	if(job.chunk_num >= info.n_chunks)
		return -1;

	size_t f_size = (size_t)(job.end - job.start); 
	// do run length encoding
	size_t pos = 0;
	char curr, enc;
	curr = 0; enc = -1;
	unsigned freq = 1;
	curr = *(((char*)job.start) + pos++);
	if(enc == -1)
		enc = curr;
	while(pos < f_size) { 
		curr = *(((char*)job.start) + pos++);
		if(curr != enc) {
			assert(insert_chunks(job.chunk_num, freq, enc) != -1);
			enc = curr;
			freq = 1;
		}
		else if(freq < MAX_FRQ)
			++freq;
		else {
			assert(insert_chunks(job.chunk_num, freq, enc) != -1);
			freq = 0;
		}
	}
	if(f_size == 1 || freq >= 1)
		assert(insert_chunks(job.chunk_num, freq, enc) != -1);
	return 0;
}

void*
compress(void* arg) {
	job_t job;
	while(1) {
		pthread_mutex_lock(&lock);
		job.chunk_num = info.chunks_done++;
		pthread_mutex_unlock(&lock);

		job.start = info.start + (job.chunk_num * PAGE_SIZE);
		job.end = info.start + ((job.chunk_num + 1) * PAGE_SIZE);
		job.end = info.end < job.end ? info.end : job.end;

		if(my_zip(job) == -1)
			break;
	}
	return NULL;
}

/////////////////////////////////////////////////
// MAIN THREAD
/////////////////////////////////////////////////

int 
main(int argc, char** argv) {
	proc_count = get_nprocs();

	// file variables
	struct stat f_st;
	size_t f_size;
	int fd = 0;
	info.start = info.end = NULL;
	info.n_chunks = info.chunks_done = 0;
	int seen_file = 0;

	if(argc == 1) {
		printf("my-zip: file1 [file2 ...]\n");
		exit(1);		
	}
	for(int i = 1; i < argc; ++i) {
		info.chunks_done = 0;	
		assert(pthread_mutex_init(&lock, NULL) == 0);
		if((fd = open(argv[i], O_RDONLY)) == -1) {
			fprintf(stderr, "Could not open file %s\n", argv[i]);
			break;
		}

		if(fstat(fd, &f_st) == -1) {
			fprintf(stderr, "Could not get file info\n");
			close(fd);
			exit(1);
		}

		f_size = f_st.st_size;
		if(f_size == 0) {
			assert(pthread_mutex_destroy(&lock) == 0);
			close(fd);
			if(i == argc - 1) {
				fwrite(&new_pair.freq, UINT_SZ, 1, stdout);				
				printf("%c", new_pair.enc);
			}
			continue;
		}
		
		if((info.start = mmap(NULL, f_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
			fprintf(stderr, "Could not memory map file\n");
			close(fd);
			exit(1);
		}

		// Record File Info
		info.end = info.start + f_size;
		info.n_chunks = f_size / PAGE_SIZE;
		if(f_size % PAGE_SIZE != 0)
			info.n_chunks++;

		// Allocate Data Structure Space
		assert(init_chunks(info.n_chunks, BLK_PAIRS) != -1);

		// Easy Threading
		pthread_t threads[proc_count];
		for(size_t x = 0; x < proc_count; ++x) {
			pthread_create(&threads[x], NULL, compress, (void*)x);	
		}
		for(size_t x = 0; x < proc_count; ++x) {
			pthread_join(threads[x], NULL);
		}

		stitch_chunks(info.n_chunks);

    old_pair.enc = new_pair.enc;
		old_pair.freq = new_pair.freq;

		// update new
		size_t last_chunk = info.n_chunks - 1;
		for(size_t i = info.n_chunks - 1; i >= 0; --i) {
			if(zbuffer.size_list[i] > 0) {
				last_chunk = i;
				break;
			}
		}
		size_t last_pos = zbuffer.size_list[last_chunk] - 1;
		char last_char = *((char*)(zbuffer.zip_chunks[last_chunk] + (last_pos * REC_LEN) + UINT_SZ));
		unsigned last_freq = *((unsigned*)(zbuffer.zip_chunks[last_chunk] + (last_pos * REC_LEN)));
		/** printf("Last Freq: %u, Last Char: %c\n", last_freq, last_char); */
		new_pair.enc = last_char;
    new_pair.freq = last_freq;

		// File Stitching
		if(i != 1 && seen_file == 1) {
			size_t first_chunk = 0;			
			for(size_t i = 0; i < info.n_chunks; ++i) {
				if(zbuffer.size_list[i] > 0) {
					first_chunk = i;
					break;
				}
			}
			char curr_char = *((char*)(zbuffer.zip_chunks[first_chunk] + UINT_SZ));
			unsigned curr_freq = *((unsigned*)(zbuffer.zip_chunks[first_chunk]));
			/** printf("Curr_char: %c, Curr_freq: %u\n", curr_char, curr_freq);
				* printf("Old char: %c, Old freq: %u\n", old_pair.enc, old_pair.freq); */
			if(old_pair.enc == curr_char) {
				unsigned sum = old_pair.freq + curr_freq;
				if(sum < old_pair.freq) {
					old_pair.freq = MAX_FRQ;
					/** assert(write(STDOUT_FILENO, &old_pair.freq, UINT_SZ) != -1); */
					fwrite(&old_pair.freq, UINT_SZ, 1, stdout);
				printf("%c", old_pair.enc);
					/** printf("%u%c", old_pair.freq, old_pair.enc); */
				}
				// update curr pair with sum
				*((unsigned*)(zbuffer.zip_chunks[first_chunk])) = sum;
				 new_pair.freq = sum;
			}
			else {
				/** assert(write(STDOUT_FILENO, &old_pair.freq, UINT_SZ) != -1); */
				fwrite(&old_pair.freq, UINT_SZ, 1, stdout);
			printf("%c", old_pair.enc);
				/** printf("%u%c", old_pair.freq, old_pair.enc); */
			}
		}
		
		seen_file = 1;			
		// Print contents
		if(i != argc - 1)
			print_most_chunks();
		else
			print_chunks();
		
		// Cleanup
		char stop = 0;
		if(pthread_mutex_destroy(&lock) != 0)
			stop = 1;
		delete_chunks();
		if(munmap(info.start, f_size) == -1) {
			fprintf(stderr, "Could not unmap reagion of memory\n");
			close(fd);
			exit(1);
		}

		if(stop == 1)
			break;
	}
	return 0;
}
