#define ECHFS_ROOT_DIR_ID   0xFFFFFFFFFFFFFFFF
#define ECHFS_END_OF_CHAIN  0xFFFFFFFFFFFFFFFF
#define ECHFS_DELETED_ENTRY 0xFFFFFFFFFFFFFFFE

#define ECHFS_TYPE_ANY  0xFF
#define ECHFS_TYPE_FILE 0
#define ECHFS_TYPE_DIR  1

#define ECHFS_SEARCH_FAILURE 0xFFFFFFFFFFFFFFFF

#define ECHFS_BLKDEV_BLOCK_SIZE 512

#define ECHFS_BLKDEV_CACHE_NOTREADY 0
#define ECHFS_BLKDEV_CACHE_READY	1
#define ECHFS_BLKDEV_CACHE_DIRTY	2

Bool EchFSBlkDevCache(CDrive *drive, U64 block)
{//Cache a block (block) (not to be confused with an echfs block, this is just a group of sectors)
 //from a CDrive (drive) into the CDrive echfs block cache.
	switch (drive->echfs_id_table->dev_cache_state)
	{
		case ECHFS_BLKDEV_CACHE_READY:
			//Block is already cached, do nothing.
			if (block == drive->echfs_id_table->dev_cached_block)
				return TRUE;
			//FALLTHRU
		case ECHFS_BLKDEV_CACHE_NOTREADY:
loadup:
			//Cache is not ready, we fell through, or a cache flush occurred. Cache the block.
			BlkRead(drive, drive->echfs_id_table->dev_cache, drive->drv_offset + block, 1);
			drive->echfs_id_table->dev_cached_block = block;
			drive->echfs_id_table->dev_cache_state  = ECHFS_BLKDEV_CACHE_READY;
			return TRUE;
		case ECHFS_BLKDEV_CACHE_DIRTY:
			//Cache is dirty, meaning something has been written to the block's cache, but not yet
			//commited to disk. Commit change to disk, then jump to loadup and load the new block to cache.
			if (block == drive->echfs_id_table->dev_cached_block)
				return TRUE;
			BlkWrite(drive, drive->echfs_id_table->dev_cache, drive->drv_offset + drive->echfs_id_table->dev_cached_block, 1);
			goto loadup;
	}
}

Bool EchFSBlkDevRead(CDrive *drive, U8 *buffer, U64 loc, U64 count)
{//Read granularly (aka, similarily to an ANSI C fread) from a CDrive (drive), using the block cache
 //loaded using EchFSBlkDevCache.
	U64 block, chunk, offset, progress = 0;
	while (progress < count)
	{
		block = (loc + progress) / BLK_SIZE;

		if (!EchFSBlkDevCache(drive, block))
			return FALSE;

		chunk = count - progress;
		offset = (loc + progress) % BLK_SIZE;
		if (chunk > BLK_SIZE - offset)
			chunk = BLK_SIZE - offset;

		MemCopy(buffer + progress, &drive->echfs_id_table->dev_cache[offset], chunk);
		progress += chunk;
	}
	return TRUE;
}

Bool EchFSBlkDevWrite(CDrive *drive, U8 *buffer, U64 loc, U64 count)
{//Write granularly (aka, similarily to an ANSI C fwrite) from a CDrive (drive), using the block cache
 //loaded using EchFSBlkDevCache.
	U64 block, chunk, offset, progress = 0;
	while (progress < count)
	{
		block = (loc + progress) / BLK_SIZE;

		if (!EchFSBlkDevCache(drive, block))
			return FALSE;

		chunk = count - progress;
		offset = (loc + progress) % BLK_SIZE;
		if (chunk > BLK_SIZE - offset)
			chunk = BLK_SIZE - offset;

		MemCopy(&drive->echfs_id_table->dev_cache[offset], buffer + progress, chunk);
		progress += chunk;

		drive->echfs_id_table->dev_cache_state = ECHFS_BLKDEV_CACHE_DIRTY;
	}
	return TRUE;
}

Bool EchFSReadEntry(CDrive *drive, CEchFSEntry *ret, U64 entry)
{//Read an entry (entry) from the filesystem's directory, and write it to output argument ret.
	CEchFSIdTable *id_tab = drive->echfs_id_table;
	U64 loc = id_tab->dir_start * id_tab->bytes_per_block + entry * sizeof(CEchFSEntry);

	if (loc >= (id_tab->dir_start + id_tab->dir_size) * id_tab->bytes_per_block)
		return FALSE;

	if (!EchFSBlkDevRead(drive, ret, loc, sizeof(CEchFSEntry)))
		return FALSE;

	return TRUE;
}

Bool EchFSWriteEntry(CDrive *drive, CEchFSEntry *ptr, U64 entry)
{//Write an entry (entry) to the filesystem's directory, reading it from ptr.
	CEchFSIdTable *id_tab = drive->echfs_id_table;
	U64 loc = id_tab->dir_start * id_tab->bytes_per_block + entry * sizeof(CEchFSEntry);

	if (loc >= (id_tab->dir_start + id_tab->dir_size) * id_tab->bytes_per_block)
		return FALSE;

	if (!EchFSBlkDevWrite(drive, ptr, loc, sizeof(CEchFSEntry)))
		return FALSE;

	return TRUE;
}

U64 EchFSSearch(CDrive *drive, CEchFSEntry *entry, U8 *name, U64 parent, U8 type)
{//Returns directory entry number of file "name", with parent "parent", of type "type".
 //Returns ECHFS_SEARCH_FAILURE upon failure/not found.
 //On success, the entry is written to output argument "entry".
	CEchFSIdTable *id_tab = drive->echfs_id_table;
	U64 i;

	for (i = 0; i < id_tab->dir_size * id_tab->dir_entries_per_block; i++)
	{
		if (!EchFSReadEntry(drive, entry, i))
			return ECHFS_SEARCH_FAILURE;

		if (!entry->parent_id)
			return ECHFS_SEARCH_FAILURE;

		if (entry->parent_id == parent &&
			(entry->type == type || type == ECHFS_TYPE_ANY) &&
			!StrCompare(entry->name, name))
			return i;
	}
	return ECHFS_SEARCH_FAILURE;
}

Bool EchFSResolvePath(CDrive *drive, CEchFSPathResult *ret, U8 *path, U8 type, U64 parent)
{//Resolve a hierarchical path to an item into a CEchFSPathResult structure, which provides information
 //about the parent of the requested item, and the item itself in case it exists.
 //Returns FALSE in case the path has a directory element (other than the target one) which does not exist.
 //Otherwise, ret is always validly return.
 //ret->not_found is set to TRUE in case the path is valid except for the last item (the target). Even if
 //ret->not_found is set, the information regarding the parent of the not found target is valid.
	U8	 name[201];
	Bool last = FALSE;
	U64	 i, search_res;

	MemSet(ret, 0, sizeof(CEchFSPathResult));

	if (!StrCompare(path, "/"))
	{
		switch (type)
		{
			case ECHFS_TYPE_DIR:
				//If looking for a directory called "/", return root, as it does not have an entry.
				ret->target.type	= ECHFS_TYPE_DIR;
				ret->target.payload = ECHFS_ROOT_DIR_ID;
				ret->parent.type	= ECHFS_TYPE_DIR;
				ret->parent.payload = ECHFS_ROOT_DIR_ID;
				return TRUE;
			case ECHFS_TYPE_FILE:
				//Fail if looking for a file named "/"
				return FALSE;
		}
	}
	//Start with the parent
	ret->parent.payload = parent;

	if (*path == '/')
		path++;
next:
	for (i = 0; *path != '/'; path++)
	{
		if (!*path)
		{
			last = TRUE;
			break;
		}
		name[i++] = *path;
	}
	name[i] = 0;
	path++;

	if (!last)
	{
		search_res = EchFSSearch(drive, &ret->parent, name, ret->parent.payload, ECHFS_TYPE_DIR);
		if (search_res == ECHFS_SEARCH_FAILURE)
			return FALSE;
	}
	else
	{
		search_res = EchFSSearch(drive, &ret->target, name, ret->parent.payload, type);
		if (search_res == ECHFS_SEARCH_FAILURE)
			ret->not_found = TRUE;
		else
			ret->target_entry = search_res;
		return TRUE;
	}
	goto next;
}

Bool EchFSCd(U8 *name, I64 cur_dir_clus)
{//Returns TRUE if directory name in cur_dir_cluster is valid, else FALSE.
	CEchFSPathResult pres;
	Bool unlock, ret;

	try
	{
		unlock = DriveLock(Fs->cur_dv);
		ret = EchFSResolvePath(Fs->cur_dv, &pres, name, ECHFS_TYPE_DIR, cur_dir_clus(U64));
		if (unlock)
			DriveUnlock(Fs->cur_dv);
		if (!ret || pres.not_found)
			return FALSE;
	}
	catch
	{
		if (unlock)
			DriveUnlock(Fs->cur_dv);
	}
	return TRUE;
}

Bool EchFSFindEmptyEntry(CDrive *drive, U64 *_free_entry, U64 *_free_dir_id)
{//Finds an empty directory entry on drive, and returns its index in return argument _free_entry.
 //Additionally, it returns a free directory ID in return argument _free_dir_id.
	CEchFSIdTable *id_tab = drive->echfs_id_table;
	CEchFSEntry entry;
	U64 i;

	*_free_entry  = 0;
	*_free_dir_id = 1;

	for (i = 0; id_tab->dir_size * id_tab->dir_entries_per_block; i++)
	{
		if (!EchFSReadEntry(drive, &entry, i))
			return FALSE;

		if (!entry.parent_id)
		{
			if (!*_free_entry)
				*_free_entry = i;
			return TRUE;
		}

		if (entry.parent_id == ECHFS_DELETED_ENTRY)
			if (!*_free_entry)
				*_free_entry = i;
		else
			if (entry.type == ECHFS_TYPE_DIR && entry.payload == *_free_dir_id)
				(*_free_dir_id)++;
	}
	return FALSE;
}

Bool EchFSMkDir(CDrive *drive, U8 *cur_dir, U8 *name)
{//Creates directory "name" within directory "cur_dir" on drive.
 //Returns FALSE on failure.
	U8 *full_path;
	CEchFSPathResult pres;
	CEchFSEntry entry;
	U64 entry_id, free_dir_id;
	Bool unlock;

	full_path = MAlloc(StrLen(cur_dir) + 1 + StrLen(name) + 1);
	StrCopy(full_path, cur_dir);
	if (cur_dir[StrLen(cur_dir)-1] != '/')
		StrCopy(full_path + StrLen(full_path), "/");
	StrCopy(full_path + StrLen(full_path), name);

	try
	{
		unlock = DriveLock(drive);

		if (!EchFSResolvePath(drive, &pres, full_path, ECHFS_TYPE_ANY, ECHFS_ROOT_DIR_ID))
		{
			Free(full_path);
			if (unlock)
				DriveUnlock(drive);
			return FALSE;
		}

		Free(full_path);

		if (!pres.not_found || !EchFSFindEmptyEntry(drive, &entry_id, &free_dir_id))
		{
			if (unlock)
				DriveUnlock(drive);
			return FALSE;
		}

		MemSet(&entry, 0, sizeof(CEchFSEntry));

		StrCopy(entry.name, name);
		entry.parent_id = pres.parent.payload;
		entry.type	  = ECHFS_TYPE_DIR;
		entry.payload   = free_dir_id;

		if (!EchFSWriteEntry(drive, &entry, entry_id))
		{
			if (unlock)
				DriveUnlock(drive);
			return FALSE;
		}
		if (unlock)
			DriveUnlock(drive);
		return TRUE;
	}
	catch
	{
		if (unlock)
			DriveUnlock(drive);
		return FALSE;
	}
}

Bool EchFSDirNew(CDrive *drive, U8 *cur_dir, CDirEntry *entry)
{//Same as EchFSMkDir but creates a directory based on a CDirEntry struct.
	return EchFSMkDir(drive, cur_dir, entry->name);
}

U0 EchFSInit(CDrive *drive)
{//Initialises the filesystem context on drive.
	"EchFSInit called\n";

	Bool unlock;

	try
	{
		unlock = DriveLock(drive);

		drive->echfs_id_table = MAlloc(sizeof(CEchFSIdTable));
		BlkRead(drive, drive->echfs_id_table, drive->drv_offset, 1);

		if (drive->echfs_id_table->signature != '_ECH_FS_')
		{
			RawPrint(3000, "echfs: error: signature mismatch\n");
			while (1);
		}
		"echfs: Valid signature\n";

		"echfs: Block count: %u\n", drive->echfs_id_table->block_count;

		drive->echfs_id_table->fat_size = (drive->echfs_id_table->block_count * sizeof(U64) +
				 drive->echfs_id_table->bytes_per_block - 1) / drive->echfs_id_table->bytes_per_block;
		"echfs: FAT size: %u\n", drive->echfs_id_table->fat_size;

		drive->echfs_id_table->fat_start = 16;
		"echfs: FAT start: %u\n", drive->echfs_id_table->fat_start;

		drive->echfs_id_table->dir_start = 16 + drive->echfs_id_table->fat_size;
		"echfs: Directory start: %u\n", drive->echfs_id_table->dir_start;

		drive->echfs_id_table->sectors_per_block = drive->echfs_id_table->bytes_per_block / 512;
		"echfs: Bytes per block: %u\n", drive->echfs_id_table->bytes_per_block;
		"echfs: Sectors per block: %u\n", drive->echfs_id_table->sectors_per_block;

		drive->echfs_id_table->dir_entries_per_block = drive->echfs_id_table->sectors_per_block * 2;

		drive->echfs_id_table->dev_cache_state = ECHFS_BLKDEV_CACHE_NOTREADY;
		drive->echfs_id_table->dev_cache	   = MAlloc(BLK_SIZE);

		drive->fs_type   = FSt_ECHFS;
		drive->root_clus = -1;

		drive->data_area = drive->drv_offset;
		drive->spc = drive->echfs_id_table->sectors_per_block(I64);

		if (unlock)
			DriveUnlock(drive);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
}

Bool EchFSFreeBlockChain(CDrive *drive, U64 block)
{//Frees a blockchain from drive starting at block for use.
	U64 fat_addr, cur_block, free_block = 0;
	CEchFSIdTable *id_tab;

	id_tab   = drive->echfs_id_table;
	fat_addr = id_tab->fat_start * id_tab->bytes_per_block;

	while (block != ECHFS_END_OF_CHAIN)
	{
		EchFSBlkDevRead(drive, &cur_block, fat_addr + block * sizeof(U64), sizeof(U64));
		EchFSBlkDevWrite(drive, &free_block, fat_addr + block * sizeof(U64), sizeof(U64));
		block = cur_block;
	}

	return TRUE;
}

Bool EchFSAllocBlockChainContiguous(CDrive *drive, U64 *_block, U64 count)
{//Allocate a contiguous count long chain of blocks on drive, return the first block of the chain in return
 //argument _block.
 //Returns FALSE on failure, which could indicate we're either out of space, or there is no contiguous chain
 //long enough for the allocation.
	U64 fat_addr, cur_block, i, c;
	CEchFSIdTable *id_tab;
	Bool unlock;

	if (!count)
		return TRUE;

	try
	{
		unlock = DriveLock(drive);

		id_tab   = drive->echfs_id_table;
		fat_addr = id_tab->fat_start * id_tab->bytes_per_block;

		c = count;
		for (i = 0; i < id_tab->block_count; i++)
		{
			EchFSBlkDevRead(drive, &cur_block, fat_addr + i * sizeof(U64), sizeof(U64));
			if (!cur_block && !--c)
			{
				*_block = i - count + 1;
				if (unlock)
					DriveUnlock(drive);
				return TRUE;
			}
			else
			{
				c = count;
			}
		}

		if (unlock)
			DriveUnlock(drive);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
	return FALSE;
}

Bool EchFSAllocBlockChain(CDrive *drive, U64 *_block, U64 count)
{//Allocate a count long chain of blocks on drive, return the first block of the chain in return argument _block.
 //Returns FALSE on failure, which could indicate we're out of space on the disk.
	U64 fat_addr, cur_block, i;
	CEchFSIdTable *id_tab;
	Bool unlock;

	if (!count)
		return TRUE;

	try
	{
		unlock = DriveLock(drive);

		id_tab   = drive->echfs_id_table;
		fat_addr = id_tab->fat_start * id_tab->bytes_per_block;

		*_block  = ECHFS_END_OF_CHAIN;

		for (i = 0; i < id_tab->block_count; i++)
		{
			EchFSBlkDevRead(drive, &cur_block, fat_addr + i * sizeof(U64), sizeof(U64));
			if (!cur_block)
			{
				EchFSBlkDevWrite(drive, _block, fat_addr + i * sizeof(U64), sizeof(U64));
				*_block = i;

				if (--count == 0)
				{
					if (unlock)
						DriveUnlock(drive);
					return TRUE;
				}
			}
		}
		if (unlock)
			DriveUnlock(drive);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
	return FALSE;
}

I64 EchFSFilesDel(CDrive *drive, U8 *cur_dir, U8 *files_find_mask, I64 fuf_flags, Bool del_dir, Bool print_msg)
{//Delete one or more files from cur_dir of drive, based on the files_find_mask matches and fuf_flags.
 //If del_dir is set, recursively delete matching directories.
 //if print_msg is set, print a message for each deleted file/dir.
	Bool unlock_break, unlock;
	CEchFSIdTable *id_tab;
	CDirEntry *tree = NULL, *entry;
	CEchFSPathResult pres;
	U64 ret = 0;
	U8 *saved_cur_dir;

	try
	{
		unlock_break = BreakLock;
		unlock       = DriveLock(drive);

		id_tab = drive->echfs_id_table;

		saved_cur_dir = MAlloc(StrLen(Fs->cur_dir) + 1);
		StrCopy(saved_cur_dir, Fs->cur_dir);

		StrCopy(Fs->cur_dir, cur_dir);
		tree = EchFSFilesFind(files_find_mask, fuf_flags & ~FUF_RECURSE);
		StrCopy(Fs->cur_dir, saved_cur_dir);

		Free(saved_cur_dir);

		if (!tree)
			goto unlock_and_quit;

		entry = tree;
		while (TRUE)
		{
			if (entry->attr & RS_ATTR_DIR)
			{
				if (!del_dir)
					goto continue;
				ret += EchFSFilesDel(drive, &entry->full_name[2], "*", 0, TRUE, print_msg);
			}

			if (!EchFSResolvePath(drive, &pres, &entry->full_name[2], ECHFS_TYPE_ANY, ECHFS_ROOT_DIR_ID) || pres.not_found)
				goto continue;

			if (print_msg)
				"Del %s\n", pres.target.name;

			ret++;

			if (!(entry->attr & RS_ATTR_DIR))
				EchFSFreeBlockChain(drive, pres.target.payload);

			pres.target.parent_id = ECHFS_DELETED_ENTRY;
			EchFSWriteEntry(drive, &pres.target, pres.target_entry);

continue:
			if (entry->next)
				entry = entry->next;
			else
				break;
		}
		DirTreeDel(tree);
	}
	catch
		if (tree)
			DirTreeDel(tree);
unlock_and_quit:
	if (unlock_break)
		BreakUnlock;
	if (unlock)
		DriveUnlock(drive);
	return ret;
}

I64 EchFSFileWrite(CDrive *drive, U8 *cur_dir, U8 *filename, U8 *buf, I64 size, CDate cdt, I64 attr)
{//Write file to cur_dir to drive wholly from a memory buffer (buf) of size "size".
 //TODO: cdt and attrs are currently ignored.
	U8 *full_path;
	CEchFSPathResult pres;
	U64 cur_block, start_block, fat_addr, size_in_blocks, entry_id, dir_id;
	CEchFSIdTable *id_tab;
	Bool unlock;
	CEchFSEntry entry;

	if (size < 0)
		size = 0;

	if (drive->fs_type != FSt_ECHFS)
	{
		PrintErr("Not EchFS Drive\n");
		return 0;
	}

	if (!filename || !*filename)
	{
		PrintErr("Invalid filename\n");
		return 0;
	}

	full_path = MAlloc(StrLen(cur_dir) + 1 + StrLen(filename) + 1);
	StrCopy(full_path, cur_dir);
	if (cur_dir[StrLen(cur_dir)-1] != '/')
		StrCopy(full_path + StrLen(full_path), "/");
	StrCopy(full_path + StrLen(full_path), filename);

	try {
		unlock = DriveLock(drive);

		id_tab = drive->echfs_id_table;

		if (!EchFSResolvePath(drive, &pres, full_path, ECHFS_TYPE_FILE, ECHFS_ROOT_DIR_ID))
		{
			Free(full_path);
			if (unlock)
				DriveUnlock(drive);
			return 0;
		}

		Free(full_path);

		if (!pres.not_found)
			EchFSFilesDel(drive, cur_dir, filename, 0, FALSE, FALSE);

		size_in_blocks = (size + id_tab->bytes_per_block - 1) / id_tab->bytes_per_block;

		if (!EchFSAllocBlockChain(drive, &start_block, size_in_blocks))
		{
			if (unlock)
				DriveUnlock(drive);
			return 0;
		}

		fat_addr = id_tab->fat_start * id_tab->bytes_per_block;

		for (cur_block = start_block; cur_block != ECHFS_END_OF_CHAIN;
			 EchFSBlkDevRead(drive, &cur_block, fat_addr + cur_block * sizeof(U64),  sizeof(U64)))
		{
			EchFSBlkDevWrite(drive, buf, cur_block * id_tab->bytes_per_block, id_tab->bytes_per_block);
			buf += id_tab->bytes_per_block;
		}

		MemSet(&entry, 0, sizeof(CEchFSEntry));

		entry.parent_id = pres.parent.payload;
		entry.type      = ECHFS_TYPE_FILE;
		StrCopy(entry.name, filename);
		entry.payload   = start_block;
		entry.size      = size;

		EchFSFindEmptyEntry(drive, &entry_id, &dir_id);
		EchFSWriteEntry(drive, &entry, entry_id);

		if (unlock)
			DriveUnlock(drive);

		return start_block(I64);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
	return 0;
}

U8 *EchFSFileRead(CDrive *drive, U8 *cur_dir, U8 *filename, I64 *_size, I64 *_attr)
{//Read file from cur_dir from drive wholly into a MAlloc allocated memory buffer, and return this buffer.
 //Return file size and file attribute in _size and _attr return arguments, respectively.
 //Return NULL if file not found or error.
	U8 *full_path, *buf, *p;
	CEchFSPathResult pres;
	U64 i, fat_addr, size_in_blocks, *block_chain;
	CEchFSIdTable *id_tab;
	Bool unlock;

	*_size = 0;
	*_attr = 0;

	if (drive->fs_type != FSt_ECHFS)
	{
		PrintErr("Not EchFS Drive\n");
		return NULL;
	}
	if (!filename || !*filename)
	{
		PrintErr("Invalid filename\n");
		return NULL;
	}

	full_path = MAlloc(StrLen(cur_dir) + 1 + StrLen(filename) + 1);
	StrCopy(full_path, cur_dir);
	if (cur_dir[StrLen(cur_dir)-1] != '/')
		StrCopy(full_path + StrLen(full_path), "/");
	StrCopy(full_path + StrLen(full_path), filename);

	try
	{
		unlock = DriveLock(drive);
		id_tab = drive->echfs_id_table;

		if (!EchFSResolvePath(drive, &pres, full_path, ECHFS_TYPE_FILE, ECHFS_ROOT_DIR_ID))
		{
			Free(full_path);
			if (unlock)
				DriveUnlock(drive);
			return NULL;
		}

		Free(full_path);

		if (pres.not_found)
		{
			if (unlock)
				DriveUnlock(drive);
			return NULL;
		}

		size_in_blocks = (pres.target.size + id_tab->bytes_per_block - 1) / id_tab->bytes_per_block;

		buf = MAlloc(size_in_blocks * id_tab->bytes_per_block + 1);
		p = buf;

		fat_addr = id_tab->fat_start * id_tab->bytes_per_block;

		block_chain = MAlloc((size_in_blocks + 1) * sizeof(U64));

		block_chain[0] = pres.target.payload;
		for (i = 1; block_chain[i-1] != ECHFS_END_OF_CHAIN; i++)
		{
			EchFSBlkDevRead(drive, &block_chain[i], fat_addr + block_chain[i-1] * sizeof(U64), sizeof(U64));
		}

		for (i = 0; block_chain[i] != ECHFS_END_OF_CHAIN; i++)
		{
			EchFSBlkDevRead(drive, buf, block_chain[i] * id_tab->bytes_per_block, id_tab->bytes_per_block);
			buf += id_tab->bytes_per_block;
		}

		Free(block_chain);

		p[pres.target.size] = 0;

		*_size = pres.target.size(I64);
		*_attr = 0;

		if (unlock)
			DriveUnlock(drive);
	}
	catch 
		if (unlock)
			DriveUnlock(drive);
	return p;
}

CDirEntry *EchFSFilesFind(U8 *files_find_mask, I64 fuf_flags, CDirEntry *parent=NULL)
{//This function creates a tree of directory Fs->cur_dir.
 //CDirEntry* returned points to the first entry of a linked list describing the entries of Fs->cur_dir.
 //->next$UL,0$ points to the next entry in the same dir.
 //$UL$->sub$UL,0$ is set for entries of directory type IF fuf_flag $LK,"FUf_RECURSE",A="MN:FUf_RECURSE"$ is set else it is set to NULL.
 //It will point to the linked list describing the entries of the entry's described directory.
 //$UL$->parent$UL,0$ is NULL for all entries at the base level (Fs->cur_dir).
 //It points to the beginning of the parent directory's linked list.
 //For all entries of subdirectories of Fs->cur_dir (see ->sub).
 //Furthermore, for each entry found in a given directory,
 //one must add the the entry to the tree if and only if a call to FilesFindMatch() with arguments
 //FilesFindMatch(entry_name, files_find_mask, fuf_flags) returns TRUE.
	CDrive *drive = Fs->cur_dv;
	CDirEntry *node, *ret = NULL;
	CEchFSIdTable *id_tab;
	CEchFSEntry entry;
	U64 i, parent_id;
	CEchFSPathResult pres;
	Bool unlock;

	if (fuf_flags & ~FUG_FILES_FIND)
		throw('FUF');

	try
	{
		unlock = DriveLock(drive);

		id_tab = drive->echfs_id_table;

		if (!EchFSResolvePath(drive, &pres, Fs->cur_dir, ECHFS_TYPE_DIR, ECHFS_ROOT_DIR_ID) || pres.not_found)
		{
			if (unlock)
				DriveUnlock(drive);
			return NULL;
		}

		parent_id = pres.target.payload;

		if (pres.target.payload == ECHFS_ROOT_DIR_ID || fuf_flags & FUF_RECURSE)
			goto no_dot_entries;

		if (FilesFindMatch(".", files_find_mask, fuf_flags))
		{
			node = CAlloc(sizeof(CDirEntry));
			node->attr = RS_ATTR_DIR;
			node->clus = pres.target.payload;
			StrCopy(node->name, ".");

			node->full_name = FileNameAbs(".");

			node->next = ret;
			ret = node;
		}

		if (FilesFindMatch("..", files_find_mask, fuf_flags))
		{
			node = CAlloc(sizeof(CDirEntry));
			node->attr = RS_ATTR_DIR;
			node->clus = pres.parent.payload;
			StrCopy(node->name, "..");

			node->full_name = FileNameAbs("..");

			node->next = ret;
			ret = node;
		}

no_dot_entries:
		for (i = 0; i < id_tab->dir_size * id_tab->dir_entries_per_block; i++) {
			if (!EchFSReadEntry(drive, &entry, i) || !entry.parent_id)
			{
				if (unlock)
					DriveUnlock(drive);
				return ret;
			}

			if (entry.parent_id == parent_id)
			{
				node = CAlloc(sizeof(CDirEntry));

				if (entry.type == ECHFS_TYPE_DIR)
					node->attr |= RS_ATTR_DIR;

				MemCopy(node->name, entry.name, CDIR_FILENAME_LEN - 1);
				node->name[CDIR_FILENAME_LEN-1] = 0;
				node->clus = entry.payload;
				node->size = entry.size;
				node->parent = parent;

				if (fuf_flags & FUF_RECURSE && node->attr & RS_ATTR_DIR)
				{
					node->full_name = DirNameAbs(node->name);
					if (Cd(node->name))
					{
						node->sub = EchFSFilesFind(files_find_mask, fuf_flags, node);
						Cd("..");
					}
					node->next = ret;
					ret = node;
				}
				else
				{
					node->full_name = FileNameAbs(entry.name);
					if ((node->attr & RS_ATTR_DIR || !(fuf_flags & FUF_JUST_DIRS)) &&
						FilesFindMatch(node->full_name, files_find_mask, fuf_flags))
					{
						node->next = ret;
						ret = node;
					}
					else
					{
						DirEntryDel(node);
					}
				}
			}
		}
		if (unlock)
			DriveUnlock(drive);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
}

Bool EchFSFileFind(CDrive *drive, I64 cur_dir_clus, U8 *filename, CDirEntry *_res, I64 fuf_flags=0)
{//Find a single file in drive, based on its filename, the parent directory entry (cur_dir_clus), and fuf_flags
 //and return a CDirEntry in return argument _res.
 //Return FALSE if file not found or error.
	U8 name[CDIR_FILENAME_LEN];
	CEchFSPathResult pres;
	U64 type;
	U16 attr = 0;
	Bool unlock;

	if (fuf_flags & ~FUG_FILE_FIND)
		throw('FUF');

	MemSet(_res, 0, sizeof(CDirEntry));

	if (drive->fs_type != FSt_ECHFS)
	{
		PrintErr("Not EchFS Drive\n");
		return FALSE;
	}
	if (!CFileNameTo(name, filename))
	{
		PrintErr("Invalid filename: %s\n", filename);
		return FALSE;
	}

	type = ECHFS_TYPE_ANY;

	try
	{
		unlock = DriveLock(drive);

		if (fuf_flags & FUF_JUST_DIRS && !(fuf_flags & FUF_JUST_FILES))
			type = ECHFS_TYPE_DIR;
		else if (fuf_flags & FUF_JUST_FILES && !(fuf_flags & FUF_JUST_DIRS))
			type = ECHFS_TYPE_FILE;

		if (!EchFSResolvePath(drive, &pres, name, type, cur_dir_clus(U64)) ||
			pres.not_found)
		{
			if (unlock)
				DriveUnlock(drive);
			return FALSE;
		}

		if (pres.target.type == ECHFS_TYPE_DIR)
			attr |= RS_ATTR_DIR;

		MemCopy(_res->name, pres.target.name, CDIR_FILENAME_LEN - 1);
		_res->name[CDIR_FILENAME_LEN-1] = 0;
		_res->clus = pres.target.payload;
		_res->size = pres.target.size;
		_res->attr = attr;

		if (unlock)
			DriveUnlock(drive);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
	return TRUE;
}
