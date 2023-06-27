#include "filesys/inode.h"
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

#ifdef EFILESYS
	#include "filesys/fat.h"
#endif

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* Returns the disk sector that contains byte offset POS within
 * INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) {
	ASSERT (inode != NULL);
	if (pos < inode->data.length) {
		#ifdef EFILESYS
			cluster_t clst = sector_to_cluster(inode->data.start);
			for (unsigned i = 0; i < (pos / DISK_SECTOR_SIZE); i++) {
				clst = fat_get(clst);
				if (clst == 0) return -1;
			}
			return cluster_to_sector(clst);
		#else
			return inode->data.start + pos / DISK_SECTOR_SIZE;
		#endif
	}
	else
		return -1;
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) {
	list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * disk.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool is_dir) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL) {
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		disk_inode->is_dir = is_dir;

#ifdef EFILESYS
		cluster_t clst = sector_to_cluster(sector); // 아이노드가 저장될 디스크의 클러스터 번호
		cluster_t new_clst = clst;

		if (sectors == 0) {
			disk_inode->start = cluster_to_sector(fat_create_chain(new_clst));
		}

		// disk inode가 가리키는 파일이 저장될 클러스터들의 정보를 FAT테이블에 업데이트하면서
		// 각각의 클러스터를 클러스터 체인에 저장
		int i;
		for (int i = 0; i < sectors; i++) {
			new_clst = fat_create_chain(new_clst);
			if (new_clst == 0) {  // chaining 실패하면 다 지워버린다.
				fat_remove_chain(clst, 0);
				free(disk_inode);
				return false;
			}
			// 아이노드의 시작 클러스터를 아이노드 내에 저장한다.
			if (i == 0) {
				clst = new_clst;  // 아이노드의 시작점 clst
				disk_inode->start = cluster_to_sector(new_clst); // 시작
			}
		}

		/* disk inode의 내용을 디스크에 저장. */
		disk_write(filesys_disk, sector, disk_inode);
		/* 파일의 데이터가 저장될 데이터 영역의 디스크 자리를 할당한 다음 0으로 채워놓는다. */
		if (sectors > 0) {
			static char zeros[DISK_SECTOR_SIZE];
			for (i = 0; i < sectors; i++) {
				ASSERT(clst != 0 || clst != EOChain);
				disk_write(filesys_disk, cluster_to_sector(clst), zeros);
				clst = fat_get(clst);
			}
		}
		success = true;
#else
		if (free_map_allocate (sectors, &disk_inode->start)) {
			disk_write (filesys_disk, sector, disk_inode);
			if (sectors > 0) {
				static char zeros[DISK_SECTOR_SIZE];
				size_t i;

				for (i = 0; i < sectors; i++) 
					disk_write (filesys_disk, disk_inode->start + i, zeros); 
			}
			success = true; 
		} 
#endif
		free (disk_inode);
	}
	return success;
}

/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) {
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			e = list_next (e)) {
		inode = list_entry (e, struct inode, elem);
		if (inode->sector == sector) {
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
	disk_read (filesys_disk, inode->sector, &inode->data);
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode) {
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode) {
	return inode->sector;
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) {
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);

		/* Deallocate blocks if removed. */
		if (inode->removed) { // 지워져야 할 아이노드라면 할당된 클러스터를 다 반환
			#ifdef EFILESYS
				fat_remove_chain(sector_to_cluster(inode->sector), 0); // 클러스터 할당 여부 false로
			#else
				free_map_release (inode->sector, 1);
				free_map_release (inode->data.start,
						bytes_to_sectors (inode->data.length)); 
			#endif
		}
		free (inode); // 아이노드 구조체도 메모리에서 반환
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void
inode_remove (struct inode *inode) {
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	while (size > 0) {
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
		} else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read (filesys_disk, sector_idx, bounce);
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
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
		off_t offset) {
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	bool grow = false;               // extend marker
	uint8_t zero[DISK_SECTOR_SIZE];  // buffer for zero padding

	// 해당 파일이 WRITE 작업을 허용하지 않으면 0을 리턴
	if (inode->deny_write_cnt) return 0;

	// 아이노드의 데이터 영역에 충분한 공간이 있는지를 확인
	disk_sector_t sector_idx = byte_to_sector(inode, offset + size);

#ifdef EFILESYS
	/* 디스크에 충분한 공간이 없다면 파일을 EXTEND한다.
	   EXTEND 시, EOF부터 WRITE를 끝내는 지점까지의 모든 데이터를 0으로 초기화한다. */
	while (sector_idx == -1) {
		grow = true;  // 파일 확장이 일어난다는 것을 표시
		off_t inode_len = inode_length(inode);  // 아이노드에 해당하는 파일의 데이터 영역 길이

		// Extending file
		// 파일 데이터 영역의 가장 끝 데이터 클러스터의 섹터 번호를 불러온다.
		cluster_t endclst = sector_to_cluster(byte_to_sector(inode, inode_len - 1));
		// endclst의 뒤에 클러스터 하나를 새로 만든다.
		cluster_t newclst = inode_len == 0 ? endclst : fat_create_chain(endclst);
		if (newclst == 0) {
			break;
		}

		/* EOF부터 OFFSET+SIZE까지의 디스크 공간들을 ZERO PADDING 해준다. */
		memset (zero, 0, DISK_SECTOR_SIZE);

		// 이전 EOF에서부터 EOF가 있는 클러스터의 끝까지를 디스크에 추가한다.
		off_t inode_ofs = inode_len % DISK_SECTOR_SIZE;
		if (inode_ofs != 0)
			inode->data.length += DISK_SECTOR_SIZE - inode_ofs;

		// 우선 write해야하는 디스크 섹터를 0으로 다 만들어준다.
		disk_write (filesys_disk, cluster_to_sector(newclst), zero);
		if (inode_ofs != 0){  
			disk_read (filesys_disk, cluster_to_sector(newclst), zero);
			memset(zero + inode_ofs + 1, 0, DISK_SECTOR_SIZE - inode_ofs); 
			// 이전 EOF와 WRITE 시작 위치 사이의 간격은 0으로 채워져야 한다.
			disk_write(filesys_disk, cluster_to_sector(endclst), zero);
			/*
					endclst          newclst (extended)
				 ---------------     ------------
				| data  0 0 0 0 | - | 0 0 0 0 0 |
				 ---------------     -----------
						↑ zero padding here!
			*/
		}
		inode->data.length += DISK_SECTOR_SIZE;  // 파일 길이 추가
		sector_idx = byte_to_sector(inode, offset + size);  
	}
#endif
	sector_idx = byte_to_sector (inode, offset); // OFFSET에 해당되는 SECTOR부터 write 시작

	while (size > 0) {
		/* Sector to write, starting byte offset within sector. */
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Write full sector directly to disk. */
			disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
		} else {
			/* We need a bounce buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) 
				disk_read (filesys_disk, sector_idx, bounce);
			else
				memset (bounce, 0, DISK_SECTOR_SIZE);
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			disk_write (filesys_disk, sector_idx, bounce); 
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;

		disk_sector_t sector_idx = byte_to_sector (inode, offset);
	}
#ifdef EFILESYS
	if (grow == true) inode->data.length = offset; // correct inode length
#endif
	free (bounce);
	disk_write(filesys_disk, inode->sector, &inode->data);  // 데이터를 디스크에 저장

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
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) {
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode) {
	return inode->data.length;
}
