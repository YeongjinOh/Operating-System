#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define INDIRECT_BLOCK_SIZE 128*512    // 128*BLOCK_SECTOR_SIZE //
#define MAXIMUM_SIZE 8*1024*1024 // We assume that file system partition will not be larger than 8 MB.
struct inode_disk* single_to_double_indirect (struct inode_disk *disk_inode);

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;  
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[122];               /* Not used, To make the size of inode same as 512, adjust this value */
					/* Especially, in the case of inode_create ASSERTION */
    block_sector_t sector;
    block_sector_t indirect_blocks_sector;
    block_sector_t double_indirect_blocks_sector;

  };

struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    struct lock lock;
  };

struct indirect_block
{
    block_sector_t block_sectors[128];  // indirect_block.block_sectors[0] : 첫번 째 direct block의 sector. 총 128개의 direct block의 sector값 보유.
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size) 
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);  // BLOCK_SECTOR_SIZE = 512 bytes.
}


/* Returns the number of indirect blocks to allocate for and inode SIZE bytes long. */
static inline size_t
bytes_to_indirect_blocks (off_t size)
{
  return DIV_ROUND_UP(size, INDIRECT_BLOCK_SIZE);  // 
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  int total_id = inode->data.length/BLOCK_SECTOR_SIZE; 
  
  if (inode->data.length == 0)
      return -1;

  if (pos <= inode->data.length)
  
  {
  	int id = pos/BLOCK_SECTOR_SIZE;
	int indirect_id = id/128;
	struct indirect_block indirect;
  	if ((total_id/128)*128 == (id/128)*128)  // check indirect or double_indirect
	{
		block_read(fs_device, inode->data.indirect_blocks_sector, &indirect);
	}
	else
	{
		block_read(fs_device, inode->data.double_indirect_blocks_sector, &indirect);
		block_read(fs_device, indirect.block_sectors[indirect_id], &indirect);
	}
	return indirect.block_sectors[id%128];
  }
  else
    return -1;
}


struct inode_disk* single_to_double_indirect (struct inode_disk *disk_inode)
{
	static char zero_block[BLOCK_SECTOR_SIZE];
	struct indirect_block indirect;

	if (disk_inode->double_indirect_blocks_sector == 0)
	{
		if (free_map_allocate(1, &disk_inode->double_indirect_blocks_sector))
		{
			block_write(fs_device, disk_inode->double_indirect_blocks_sector, zero_block);
		}
		block_read(fs_device, disk_inode->double_indirect_blocks_sector, &indirect);
		indirect.block_sectors[0] = disk_inode->indirect_blocks_sector;
		disk_inode-> indirect_blocks_sector = 0;
		block_write(fs_device, disk_inode->double_indirect_blocks_sector, &indirect);
	}
	else
	{
		block_read(fs_device, disk_inode->double_indirect_blocks_sector, &indirect);
		size_t double_index=0;
		while(indirect.block_sectors[double_index] == 0){
			double_index++;
		}
		indirect.block_sectors[double_index] = disk_inode->indirect_blocks_sector;
		disk_inode->indirect_blocks_sector = 0;
		block_write(fs_device, disk_inode->double_indirect_blocks_sector, &indirect);
	}
	return disk_inode;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
	list_init (&open_inodes);
}


bool add_inode_size (struct inode_disk *disk_inode, off_t new_size)
{
	bool result = false; // Set default return value as false

	ASSERT(new_size >= disk_inode->length);

	size_t new_sector = bytes_to_sectors (new_size) - bytes_to_sectors(disk_inode->length); // 새로 추가해줘야하는 sector 수 환산.
	size_t previous_sector = bytes_to_sectors(disk_inode->length);
	int indirect_index = (int)previous_sector%128;
	int double_index = bytes_to_indirect_blocks(disk_inode->length);
	static char zero_block[BLOCK_SECTOR_SIZE];
	
	/* The case that it doesn't have to be expanded. */
	if (new_sector == 0){
		disk_inode->length = new_size; // The length would be the same with new_size even though they have the same number of sectors.
		block_write(fs_device, disk_inode->sector, disk_inode);
		return true; // Since this function terminated well, return true.
	}
	
	struct indirect_block indirect;
	
	/* Initialize an indirect block if it didn't have */
	if (disk_inode->length == 0){ 
		free_map_allocate(1, &disk_inode->indirect_blocks_sector);  // indirect_blocks_sector에 1칸만큼 할당된 sector 값을 넣는다.
		block_write(fs_device, disk_inode->indirect_blocks_sector, zero_block); // indirect_blocks_sector에 해당하는 block에 초기값 write
	}
	block_read(fs_device, disk_inode->indirect_blocks_sector, &indirect);  // indirect_blocks_sector에 있던 값을 block buffer에 저장.
	int i;
	for (i=0;i<new_sector;i++)
		{
			/* Initialize an indirect block if it didn't have */
			if (disk_inode->indirect_blocks_sector == 0)
			{
				free_map_allocate(1, &disk_inode->indirect_blocks_sector);
				block_write(fs_device, disk_inode->indirect_blocks_sector, zero_block);
				block_read(fs_device, disk_inode->indirect_blocks_sector, &indirect);
			}

			/* Add initialized direct blocks */
			if (free_map_allocate(1, &indirect.block_sectors[(indirect_index+i)%128]))
			{
				block_write(fs_device, indirect.block_sectors[(indirect_index+i)%128], zero_block);
			}

			
			if ((indirect_index+i+1)%128 == 0)
			{
				block_write(fs_device, disk_inode->indirect_blocks_sector, &indirect);
				disk_inode = single_to_double_indirect(disk_inode);
			}
		}
	block_write(fs_device, disk_inode->indirect_blocks_sector, &indirect);
	disk_inode->length = new_size;
	block_write(fs_device, disk_inode->sector, disk_inode);
	result = true;
	return result;
}


/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);
  if(length > MAXIMUM_SIZE)
	length = MAXIMUM_SIZE;

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
//  printf("sizeof *disk_inode : %d, BLOCK_SERCTOR_SIZE = %d\n\n",sizeof *disk_inode, BLOCK_SECTOR_SIZE);
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->magic = INODE_MAGIC;
      disk_inode->sector = sector;
      disk_inode->length = 0;
      disk_inode->indirect_blocks_sector = 0;
      disk_inode->double_indirect_blocks_sector = 0;
	  if (add_inode_size (disk_inode, length))
	  {
	  	  success = true;
	  }
	  free (disk_inode);

	}
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  lock_init(&inode->lock);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
//          free_map_release (inode->data.start, bytes_to_sectors (inode->data.length)); 

          int i, j;
	  struct indirect_block indirect;
	  struct indirect_block double_indirect;
	  if (inode->data.indirect_blocks_sector != 0)
 	  {
               block_read (fs_device, inode->data.indirect_blocks_sector, &indirect);
	       for (i=0;i<128;i++)
	       {
	  	  if (indirect.block_sectors[i] != 0)
		  {
	            free_map_release(indirect.block_sectors[i], 1);
		  }
	       }
	  }
	  if (inode->data.double_indirect_blocks_sector != 0)
	  {
	      block_read (fs_device, inode->data.double_indirect_blocks_sector, &indirect);
	      for (i=0;i<128;i++)
	      {
	          if (indirect.block_sectors[i] != 0)
	          {
		      block_read (fs_device, indirect.block_sectors[i], &double_indirect);
		      for (j=0;j<128;j++)
		          if (double_indirect.block_sectors[j] != 0)
			      free_map_release(double_indirect.block_sectors[j], 1);
		  }
	      }
	  }
        }
        free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (offset + size > inode->data.length)
  {
  	  lock_acquire(&inode->lock);
  	  add_inode_size(&inode->data, offset + size);
  	  lock_release(&inode->lock);  
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
