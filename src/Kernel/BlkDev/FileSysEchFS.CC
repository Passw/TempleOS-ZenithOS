#define ECHFS_ROOT_DIR_ID  0xffffffffffffffff
#define ECHFS_END_OF_CHAIN 0xffffffffffffffff

#define ECHFS_TYPE_ANY  0xff
#define ECHFS_TYPE_FILE 0
#define ECHFS_TYPE_DIR  1

#define ECHFS_SEARCH_FAILURE 0xffffffffffffffff

#define ECHFS_BLKDEV_BLOCK_SIZE 512

#define ECHFS_BLKDEV_CACHE_NOTREADY 0
#define ECHFS_BLKDEV_CACHE_READY    1
#define ECHFS_BLKDEV_CACHE_DIRTY    2

Bool EchFSBlkDevReadBlocks(CDrive *drive, U8 *dest, U64 loc, U64 count) {
    U64 i;
    for (i = 0; i < count; i++)
        if (!BlkRead(drive, dest + i * 512, loc + i * 512, 1))
            return FALSE;
}

Bool EchFSBlkDevCache(CDrive *drive, U64 block) {
    switch (drive->echfs_id_table->dev_cache_state) {
        case ECHFS_BLKDEV_CACHE_READY:
            if (block == drive->echfs_id_table->dev_cached_block)
                return TRUE;
            // FALLTHRU
        case ECHFS_BLKDEV_CACHE_NOTREADY:
            //EchFSBlkDevReadBlocks(drive, drive->echfs_id_table->dev_cache,
            BlkRead(drive, drive->echfs_id_table->dev_cache,
                                  drive->drv_offset + block,
                                  ECHFS_BLKDEV_BLOCK_SIZE / 512);
            drive->echfs_id_table->dev_cached_block = block;
            drive->echfs_id_table->dev_cache_state  = ECHFS_BLKDEV_CACHE_READY;
            return TRUE;
        case ECHFS_BLKDEV_CACHE_DIRTY:
            "echfs cache flush\n";
            //while (1) {}
    }
}

Bool EchFSBlkDevRead(CDrive *drive, U8 *buffer, U64 loc, U64 count) {
    U64 block, chunk, offset, progress = 0;
    while (progress < count) {
        block = (loc + progress) / ECHFS_BLKDEV_BLOCK_SIZE;

        if (!EchFSBlkDevCache(drive, block))
            return FALSE;

        chunk = count - progress;
        offset = (loc + progress) % ECHFS_BLKDEV_BLOCK_SIZE;
        if (chunk > ECHFS_BLKDEV_BLOCK_SIZE - offset)
            chunk = ECHFS_BLKDEV_BLOCK_SIZE - offset;

        MemCopy(buffer + progress, &drive->echfs_id_table->dev_cache[offset], chunk);
        progress += chunk;
    }

    return TRUE;
}

Bool EchFSReadEntry(CDrive *drive, CEchFSEntry *ret, U64 entry) {
    CEchFSIdTable *id_tab = drive->echfs_id_table;
    U64 loc = id_tab->dir_start * id_tab->bytes_per_block +
              entry * sizeof(CEchFSEntry);

    if (loc >= (id_tab->dir_start + id_tab->dir_size) * id_tab->bytes_per_block)
        return FALSE;

    if (!EchFSBlkDevRead(drive, ret, loc, sizeof(CEchFSEntry)))
        return FALSE;

    return TRUE;
}

// Returns unique entry #, ECHFS_SEARCH_FAILURE upon failure/not found
U64 EchFSSearch(CDrive *drive, CEchFSEntry *entry, U8 *name, U64 parent, U8 type) {
    CEchFSIdTable *id_tab = drive->echfs_id_table;
    U64 i;

    for (i = 0; i < id_tab->dir_size * id_tab->dir_entries_per_block; i++) {
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

Bool EchFSResolvePath(CDrive *drive, CEchFSPathResult *ret, U8 *path, U8 type,
                      U64 parent) {
    U8          name[201];
    Bool        last = FALSE;
    U64         i, search_res;

    MemSet(ret, 0, sizeof(CEchFSPathResult));

    if (!StrCompare(path, "/")) {
        switch (type) {
            case ECHFS_TYPE_DIR:
                // If looking for a directory called "/", return root, as it does
                // not have an entry
                ret->target.type    = ECHFS_TYPE_DIR;
                ret->target.payload = ECHFS_ROOT_DIR_ID;
                ret->parent.type    = ECHFS_TYPE_DIR;
                ret->parent.payload = ECHFS_ROOT_DIR_ID;
                return TRUE;
            case ECHFS_TYPE_FILE:
                // Fail if looking for a file named "/"
                return FALSE;
        }
    }

    // Start with the parent
    ret->parent.payload = parent;

    if (*path == '/')
        path++;

next:
    for (i = 0; *path != '/'; path++) {
        if (!*path) {
            last = TRUE;
            break;
        }
        name[i++] = *path;
    }
    name[i] = 0;
    path++;

    if (!last) {
        search_res = EchFSSearch(drive, &ret->parent, name, ret->parent.payload,
                                     ECHFS_TYPE_DIR);
        if (search_res == ECHFS_SEARCH_FAILURE)
            return FALSE;
    } else {
        search_res = EchFSSearch(drive, &ret->target, name, ret->parent.payload,
                                     type);
        if (search_res == ECHFS_SEARCH_FAILURE)
            ret->not_found = TRUE;
        else
            ret->target_entry = search_res;
        return TRUE;
    }

    goto next;
}

Bool EchFSCd(U8 *name, I64 cur_dir_clus) {
    CEchFSPathResult pres;

    if (!EchFSResolvePath(Fs->cur_dv, &pres, name, ECHFS_TYPE_DIR, cur_dir_clus(U64)))
        return FALSE;

    if (pres.not_found)
        return FALSE;

    return TRUE;
}

Bool EchFSMkDir(CDrive *drive, U8 *cur_dir, U8 *name, I64 entry_count) {
	"echfs mkdir\n";
	//while (1) {}
}

U0 EchFSInit(CDrive *drive){
	"EchFSInit called\n";

	Bool unlock;

	try {
		unlock = DriveLock(drive);

		drive->echfs_id_table = MAlloc(sizeof(CEchFSIdTable));
		BlkRead(drive, drive->echfs_id_table, drive->drv_offset, 1);

		if (drive->echfs_id_table->signature != '_ECH_FS_') {
			"echfs: error: signature mismatch\n";
            //while (1) {}
		}
        "echfs: valid signature\n";

        "echfs: Block count: %u\n", drive->echfs_id_table->block_count;

        drive->echfs_id_table->fat_size =
            (drive->echfs_id_table->block_count * sizeof(U64) +
             drive->echfs_id_table->bytes_per_block - 1) /
            drive->echfs_id_table->bytes_per_block;
        "echfs: FAT size: %u\n", drive->echfs_id_table->fat_size;

        drive->echfs_id_table->fat_start = 16;
        "echfs: FAT start: %u\n", drive->echfs_id_table->fat_start;

        drive->echfs_id_table->dir_start = 16 + drive->echfs_id_table->fat_size;
        "echfs: Directory start: %u\n", drive->echfs_id_table->dir_start;

        drive->echfs_id_table->sectors_per_block =
            drive->echfs_id_table->bytes_per_block / 512;
        "echfs: Bytes per block: %u\n", drive->echfs_id_table->bytes_per_block;
        "echfs: Sectors per block: %u\n", drive->echfs_id_table->sectors_per_block;

        drive->echfs_id_table->dir_entries_per_block =
            drive->echfs_id_table->sectors_per_block * 2;

        drive->echfs_id_table->dev_cache_state = ECHFS_BLKDEV_CACHE_NOTREADY;
        drive->echfs_id_table->dev_cache       = MAlloc(ECHFS_BLKDEV_BLOCK_SIZE);

		drive->fs_type   = FSt_ECHFS;
        drive->root_clus = -1;

		if (unlock) {
			DriveUnlock(drive);
		}
	} catch {
		if (unlock) {
			DriveUnlock(drive);
		}
	}
}

U8 *EchFSFileWrite(CDrive *drive, U8 *cur_dir, U8 *filename,
				  U8 *buf, I64 size, CDate cdt, I64 attr) {
	"echfs write: %s %s\n", cur_dir, filename;
	//while (1) {}
}

U8 *EchFSFileRead(CDrive *drive, U8 *cur_dir, U8 *filename,
				  I64 *size, I64 *attr) {
	U8 *full_path, *buf, *p;
    CEchFSPathResult pres;
    U64 cur_block, fat_addr;
    CEchFSIdTable *id_tab = drive->echfs_id_table;

    full_path = MAlloc(StrLen(cur_dir) + 1 + StrLen(filename) + 1);
    StrCopy(full_path, cur_dir);
    if (cur_dir[StrLen(cur_dir)-1] != '/')
        StrCopy(full_path + StrLen(full_path), "/");
    StrCopy(full_path + StrLen(full_path), filename);

    if (!EchFSResolvePath(drive, &pres, full_path, ECHFS_TYPE_FILE, ECHFS_ROOT_DIR_ID)) {
        Free(full_path);
        return NULL;
    }

    Free(full_path);

    if (pres.not_found)
        return NULL;

    buf = MAlloc(CeilI64(pres.target.size, id_tab->bytes_per_block));
    p = buf;

    fat_addr = id_tab->fat_start * id_tab->bytes_per_block;

    for (cur_block = pres.target.payload; cur_block != ECHFS_END_OF_CHAIN;
         EchFSBlkDevRead(drive, &cur_block, fat_addr + cur_block * sizeof(U64),
                         sizeof(U64))) {
        EchFSBlkDevRead(drive, buf, cur_block * id_tab->bytes_per_block,
                        id_tab->bytes_per_block);
        buf += id_tab->bytes_per_block;
    }

    return p;
}

CDirEntry *EchFSFilesFind(U8 *files_find_mask, I64 fuf_flags, CDirEntry *parent=NULL) {
    // This function creates a tree of directory Fs->cur_dir.
    // CDirEntry* returned points to the first entry of a linked list describing
    // the entries of Fs->cur_dir.
    // ->next points to the next entry in the same dir.
    // ->sub is set for entries of directory type IF fuf_flag "FUf_RECURSE" is set
    // else it is set to NULL.
    // ->sub will point to the linked list describing the entries of the entry's
    // described directory.
    // ->parent is NULL for all entries at the base level (Fs->cur_dir).
    // ->parent points to the beginning of the parent directory's linked list
    // for all entries of subdirectories of Fs->cur_dir (see ->sub).
    // Furthermore, for each entry found in a given directory, one must add the
    // the entry to the tree if and only if a call to FilesFindMatch() with arguments
    // FilesFindMatch(entry_name, files_find_mask, fuf_flag) returns TRUE.

    CDrive *drive = Fs->cur_dv;
    CDirEntry *node, *ret = NULL;
    CEchFSIdTable *id_tab;
    CEchFSEntry entry;
    U64 i, parent_id;
    U16 attr;
    CEchFSPathResult pres;

    if (fuf_flags & ~FUG_FILES_FIND)
        throw('FUF');

    try {
        DriveLock(drive);

        id_tab = drive->echfs_id_table;

        if (parent == NULL) {
            if (!EchFSResolvePath(drive, &pres, Fs->cur_dir, ECHFS_TYPE_DIR, ECHFS_ROOT_DIR_ID))
                return NULL;

            if (pres.not_found)
                return NULL;

            parent_id = pres.target.payload;
        } else {
            parent_id = parent->clus(U64);
        }

        for (i = 0; i < id_tab->dir_size * id_tab->dir_entries_per_block; i++) {
            if (!EchFSReadEntry(drive, &entry, i))
                return ret;

            if (!entry.parent_id)
                return ret;

            if (entry.parent_id == parent_id &&
                FilesFindMatch(entry.name, files_find_mask, fuf_flags)) {
                node = CAlloc(sizeof(CDirEntry));

                switch (entry.type) {
                    case ECHFS_TYPE_DIR:
                        attr |= RS_ATTR_DIR;
                }

                MemCopy(node->name, entry.name, CDIR_FILENAME_LEN - 1);
                node->name[CDIR_FILENAME_LEN-1] = 0;
                node->clus = entry.payload;
                node->size = entry.size;
                node->attr = attr;

                if (Bt(&fuf_flags, FUf_RECURSE) && attr & RS_ATTR_DIR) {
                    DriveUnlock(drive);
                    node->sub = EchFSFilesFind(files_find_mask, fuf_flags, node);
                    DriveLock(drive);
                }

                node->parent = parent;

                node->next = ret;
                ret = node;
            }
        }
    } catch {
        DriveUnlock(drive);
    }
}

Bool EchFSFileFind(CDrive *drive,I64 cur_dir_clus,U8 *name,
			       CDirEntry *_res,I64 fuf_flags=0) {
    CEchFSPathResult pres;
    U64 type;
    U16 attr = 0;

    type = ECHFS_TYPE_ANY;

    if (fuf_flags & FUF_JUST_DIRS && !(fuf_flags & FUF_JUST_FILES))
        type = ECHFS_TYPE_DIR;
    else if (fuf_flags & FUF_JUST_FILES && !(fuf_flags & FUF_JUST_DIRS))
        type = ECHFS_TYPE_FILE;

    if (!EchFSResolvePath(drive, &pres, name, type, cur_dir_clus(U64)))
        return FALSE;

    switch (pres.target.type) {
        case ECHFS_TYPE_DIR:
            attr |= RS_ATTR_DIR;
    }

    MemSet(_res, 0, sizeof(CDirEntry));

    MemCopy(_res->name, pres.target.name, CDIR_FILENAME_LEN - 1);
    _res->name[CDIR_FILENAME_LEN-1] = 0;
    _res->clus = pres.target.payload;
    _res->size = pres.target.size;
    _res->attr = attr;

    return TRUE;
}
