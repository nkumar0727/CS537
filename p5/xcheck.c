#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
//////////////////////////////////////////////////////////////////////////////////
// ERROR MESSAGES
//////////////////////////////////////////////////////////////////////////////////
#define USAGE_BAD "Usage: xcheck <file_system_image>\n"
#define IMG_FOUND "image not found.\n"

// file system checker error messages
#define BAD_INODE "ERROR: bad inode.\n"
#define BAD_DA "ERROR: bad direct address in inode.\n"
#define BAD_IA "ERROR: bad indirect address in inode.\n"
#define ROOT_EXIST "ERROR: root directory does not exist.\n"
#define DIR_FORMAT "ERROR: directory not properly formatted.\n"
#define ADDR_FREE "ERROR: address used by inode but marked free in bitmap.\n"
#define BLOCK_USE "ERROR: bitmap marks block in use but it is not in use.\n"
#define DA_USE "ERROR: direct address used more than once.\n"
#define IA_USE "ERROR: indirect address used more than once.\n"
#define INODE_FOUND "ERROR: inode marked use but not found in a directory.\n"
#define INODE_FREE "ERROR: inode referred to in directory but marked free.\n"
#define BAD_REF "ERROR: bad reference count for file.\n"
#define DIR_APPEAR "ERROR: directory appears more than once in file system.\n"

#define ERR_UNEXPECTED "unexpected error.\n"

//////////////////////////////////////////////////////////////////////////////////
// FILESYSTEM STUFF FROM XV6
//////////////////////////////////////////////////////////////////////////////////
// On-disk file system format.
// Both the kernel and user programs use this header file.

// Block 0 is unused.
// Block 1 is super block.
// Inodes start at block 2.''
#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device

#define ROOTINO 1  // root i-number
#define BSIZE (512)  // block size

// File system super block
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

//#define NUMDIRS ((BSIZE) / (sizeof(struct dirent)))
#define NUMDIRS 32

void* fsImage = NULL;
void* inodeStart = NULL;
void* superStart = NULL;
void* bitmapStart = NULL;

void*
getBlockAddr(uint blockNo) {
  return fsImage + BSIZE * blockNo;
}

struct dinode*
getInodeAddr(uint inodeNo) {
  return (struct dinode*) (inodeStart + sizeof(struct dinode) * inodeNo);
}

uint
checkValidBit(uint bit) {
  uint byteNum = bit / 8;
  uint bitNum = bit % 8;
  //uint mask = 1 << (8 - bitNum);
  uint mask = 1 << (bitNum);
  void* byteLocation = bitmapStart + byteNum;
  uint targetByte = *((uint*) byteLocation);
  uint result = mask & targetByte;
  return result;
}
// cleanup memory
void
cleanup(void* fsImage, size_t imageSize, uint* blockRefs, uint* inodeRefs, uint* linkRefs, uint* dirRefs) {
  if(fsImage != MAP_FAILED)
    munmap(fsImage, imageSize);
  free(blockRefs);
  free(inodeRefs);
  free(linkRefs);
  free(dirRefs);
}

// perform cleanup and exit based on error message
void
exitOnError(const char* error, void* fsImage, size_t imageSize,
  uint* blockRefs, uint* inodeRefs, uint* linkRefs, uint* dirRefs) {
  cleanup(fsImage, imageSize, blockRefs, inodeRefs, linkRefs, dirRefs);
  fprintf(stderr, "%s", error);
  exit(1);
}

//////////////////////////////////////////////////////////////////////////////////
// MAIN
//////////////////////////////////////////////////////////////////////////////////
int
main(int argc, char** argv) {

  // This array is sized NUMBLOCKS in the file_system
  // if a block is in use, then it will be marked present in this
  // array (refCount is incremented). All refCounts are 0 to start with.
  uint* blockRefs = NULL;
  uint* briggsRefs = NULL;
  uint* kumarRefs = NULL;
  uint* chamariaRefs = NULL; // not really

  // if wrong number of arguments provided, print USAGE to stderr
  // w/ error code 1 (should only be 1 arg)
  if(argc != 2) {
    fprintf(stderr, USAGE_BAD);
    exit(1);
  }
  int fsImageFD = open(argv[1], O_RDONLY);

  // if file system image does not exist, print NOT_FOUND to stderr
  // w/ error code 1
  if(fsImageFD < 0) {
    fprintf(stderr, IMG_FOUND);
    exit(1);
  }

  // get image info
  struct stat fsImageInfo;
  if(fstat(fsImageFD, &fsImageInfo) < 0) {
    fprintf(stderr, "program messed up: FSTAT\n");  // should never happen!!
    close(fsImageFD);
    exit(1);
  }

  // mmap the file system image
  fsImage = mmap(NULL, fsImageInfo.st_size, PROT_READ,
    MAP_PRIVATE, fsImageFD, 0);
  close(fsImageFD);
  if(fsImage == MAP_FAILED)
    exitOnError(ERR_UNEXPECTED, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);

  // Get important filesystem locations
  superStart = fsImage + BSIZE;

  // superblock information
  struct superblock superBlock;
  superBlock.size = *((uint*)superStart);
  superBlock.nblocks = *(((uint*)superStart) + 1);
  superBlock.ninodes = *(((uint*)superStart) + 2);

  inodeStart = (void*)(fsImage + 2*BSIZE);
  bitmapStart = fsImage + (BSIZE * BBLOCK(0, superBlock.ninodes));

  // This array is sized NUMBLOCKS in the file_system
  // if a block is in use, then it will be marked present in this
  // array (refCount is incremented). All refCounts are 0 to start with.
  blockRefs = (uint*) calloc(superBlock.nblocks, sizeof(uint));
  briggsRefs = (uint*) calloc(superBlock.ninodes, sizeof(uint));
  kumarRefs = (uint*) calloc(superBlock.ninodes, sizeof(uint));
  chamariaRefs = (uint*) calloc(superBlock.ninodes, sizeof(uint));
  if(!blockRefs || !briggsRefs || !kumarRefs || !chamariaRefs)
    exitOnError(ERR_UNEXPECTED, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);

  uint bitmapBlock = BBLOCK(1, superBlock.ninodes);

  //////////////////////////////////////////////////////////////////////////////////
  // DO FILESYSTEMS CHECKING STUFF
  //////////////////////////////////////////////////////////////////////////////////

  // Check # 3
  // Root directory exists, its inode number is 1, and the parent of the
  // root directory is itself. If not, print ERROR: root directory does not exist.
  struct dinode* rootInode = getInodeAddr(ROOTINO);
  if(rootInode->type != T_DIR)
    exitOnError(ROOT_EXIST, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
  struct dirent* currDir = getBlockAddr(rootInode->addrs[0]);
  struct dirent* parentDir = currDir + 1;
  if(currDir->inum != ROOTINO || parentDir->inum != ROOTINO)
    exitOnError(ROOT_EXIST, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);


  for(uint inodeNo = 1; inodeNo < superBlock.ninodes; inodeNo++) {
    struct dinode* currInode = getInodeAddr(inodeNo);

    // Check #1
    // Each inode is either unallocated or one of the valid types
    // (T_FILE, T_DIR, T_DEV). If not, print ERROR: bad inode.
    if(currInode->type == 0 || currInode->type == T_FILE || currInode->type == T_DIR|| currInode->type == T_DEV) {
      // Check # 2
      // for in-use inodes, each address that is used by the inode is valid.
      // report invalid direct/indirect addresses

      // skip any invalid inodes
      if(currInode->type == 0)
        continue;

      // Check # 4
      // Each directory contains . and .. entries, and the . entry points to the directory itself. If not, print ERROR: directory not properly formatted.
      if(currInode->type == T_DIR) { // DIRECTORY INDOE

        // skip any invalid blocks
        if(currInode->addrs[0] >= superBlock.nblocks)
          continue;

        // current working and parent directory corresponding to directory inode
        struct dirent* currDir = getBlockAddr(currInode->addrs[0]);
        struct dirent* parentDir = currDir + 1;

        // make sure the . and .. exist, and that inumber corresponds with inode number
        if(strcmp(currDir->name, ".") != 0 || strcmp(parentDir->name, "..") != 0 || currDir->inum != inodeNo)
          exitOnError(DIR_FORMAT, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
      }

      // Check # 5
      // For in-use inodes, each address in use is also marked in use in the bitmap.
      // If not, print ERROR: address used by inode but marked free in bitmap.

      // This for-loop checks the 12 Direct Block Addresses, as well as the
      // Indirect Block Address itself
      for(uint addrNum = 0; addrNum <= NDIRECT; ++addrNum) {
        uint block = currInode->addrs[addrNum];
        if(block <= bitmapBlock)
          continue;
        // make sure inode references blocks within range of filesystem
        if(block >= superBlock.nblocks)
          exitOnError(BAD_DA, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);

        // CHeck # 7
        if(blockRefs[block] == 1)
          exitOnError(DA_USE, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);

        blockRefs[block] = 1;

        // for each dirent in block, we need to check if inum is valid
        // if so, then we set the briggsRefs array entry to 1
        if(currInode->type == T_DIR && addrNum != NDIRECT) {
          struct dirent* blockDirs = getBlockAddr(block);
          for(uint dir = 0; dir < NUMDIRS; ++dir) {
            struct dirent* currDir = blockDirs + dir;
            if(currDir->inum < superBlock.ninodes && currDir->inum != 0) {
              struct dinode* fileNode = getInodeAddr(currDir->inum);
              if(fileNode->type == T_FILE)
                kumarRefs[currDir->inum] += 1;
              else if(fileNode->type == T_DIR && strcmp(currDir->name, ".") != 0 && strcmp(currDir->name, "..") != 0) {
                if(chamariaRefs[currDir->inum] >= 1)
                  exitOnError(DIR_APPEAR, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
                else
                  chamariaRefs[currDir->inum] += 1;
              }
              briggsRefs[currDir->inum] = 1;
            }
          }
        }
      }
      // This for-loop checks all the Direct Block Address within the
      // Indirect Data Block
      void* indirBlock = getBlockAddr(currInode->addrs[NDIRECT]);
      for(uint indirAddr = 0; indirAddr < NINDIRECT; ++indirAddr) {
        uint block = *(((uint*) indirBlock) + indirAddr);
        if(block <= bitmapBlock)
          continue;
        // make sure indirect block addresses are within range of filesystem
        if(block >= superBlock.nblocks)
          exitOnError(BAD_IA, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);

        // Check # 8
        if(blockRefs[block] == 1)
          exitOnError(IA_USE, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);

        blockRefs[block] = 1;
        // for each dirent in block, we need to check if inum is valid
        // if so, then we set the briggsRefs array entry to 1
        if(currInode->type == T_DIR) {
          struct dirent* blockDirs = getBlockAddr(block);
          for(uint dir = 0; dir < NUMDIRS; ++dir) {
            struct dirent* currDir = blockDirs + dir;
            if(currDir->inum < superBlock.ninodes && currDir->inum != 0) {
              struct dinode* fileNode = getInodeAddr(currDir->inum);
              if(fileNode->type == T_FILE)
                kumarRefs[currDir->inum] += 1;
              else if(fileNode->type == T_DIR && strcmp(currDir->name, ".") != 0 && strcmp(currDir->name, "..") != 0) {
                if(chamariaRefs[currDir->inum] >= 1)
                  exitOnError(DIR_APPEAR, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
                else
                  chamariaRefs[currDir->inum] += 1;
              }
              briggsRefs[currDir->inum] = 1;
            }
          }
        }
      }
    }
    else
      exitOnError(BAD_INODE, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
  }

  // Check # 5-6
  // scan the blockRefs array to perform checks 5 and 6
  for(uint blk = bitmapBlock + 1; blk < superBlock.nblocks; ++blk) {
    uint bitmapResult = checkValidBit(blk);
    uint scanResult = blockRefs[blk];
    // check 5
    if(scanResult == 1 && bitmapResult == 0)
      exitOnError(ADDR_FREE, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);

    // check 6
    if(scanResult == 0 && bitmapResult > 0)
      exitOnError(BLOCK_USE, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
  }

  // Check # 9-10
  // scan the valid inodes to perform checks 9-10
  for(uint inodeNo = 1; inodeNo < superBlock.ninodes; inodeNo++) {
    // CHeck 9
    struct dinode* currInode = getInodeAddr(inodeNo);
    if(currInode->type == T_FILE || currInode->type == T_DIR || currInode->type == T_DEV) {
      if(briggsRefs[inodeNo] != 1)
        exitOnError(INODE_FOUND, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
    }
  }
  for(uint inodeNo = 1; inodeNo < superBlock.ninodes; inodeNo++) {
    struct dinode* currInode = getInodeAddr(inodeNo);
    if(briggsRefs[inodeNo] == 1 && currInode->type == 0) {
        exitOnError(INODE_FREE, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
    }
  }

  // CHeck # 11-12
  for(uint inodeNo = 1; inodeNo < superBlock.ninodes; inodeNo++) {
    struct dinode* currInode = getInodeAddr(inodeNo);
    if(currInode->type == T_FILE && currInode->nlink != kumarRefs[inodeNo])
      exitOnError(BAD_REF, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
  }
  for(uint inodeNo = 2; inodeNo < superBlock.ninodes; inodeNo++) {
    struct dinode* currInode = getInodeAddr(inodeNo);
    if(currInode->type == T_DIR && currInode->nlink > 1)
      exitOnError(DIR_APPEAR, fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);

  }
  // perform normal cleanup
  cleanup(fsImage, fsImageInfo.st_size, blockRefs, briggsRefs, kumarRefs, chamariaRefs);
  exit(0);
}
