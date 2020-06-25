I64 ClusNumNext(CDrive *drive, I64 c, I64 count=1)
{//Return next count'th clus in chain.
	Bool unlock;
	U64 fat_addr;
	DriveCheck(drive);
	if (count <= 0)
		return c;
	try
	{
		unlock = DriveLock(drive);
		switch (drive->fs_type)
		{
			case FSt_ECHFS:
				fat_addr = drive->echfs_id_table->fat_start * drive->echfs_id_table->bytes_per_block;
				while (count-- > 0 && c != 0xFFFFFFFFFFFFFFFF)
					EchFSBlkDevRead(drive, &c, fat_addr + c * sizeof(U64), sizeof(U64));
				if (c == 0xFFFFFFFFFFFFFFFF)
					c = INVALID_CLUS;
				break;
			case FSt_REDSEA:
				c += count;
				break;
			case FSt_FAT32:
				while (count-- > 0 && 0 < c < 0x0FFFFFF8)
				{
					DriveFATBlkSet(drive, c);
					c = drive->cur_fat_blk[c & (BLK_SIZE / 4 - 1)];
				}
				if (!(0 < c < 0x0FFFFFF8))
					c = INVALID_CLUS;
				break;
			default:
				throw('Drive');
		}
		if (unlock)
			DriveUnlock(drive);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
	return c;
}

I64 Clus2Blk(CDrive *drive, I64 c)
{//Drive clus num to blk num.
	DriveCheck(drive);
	switch (drive->fs_type)
	{
		case FSt_REDSEA:
			return c;
		case FSt_FAT32:
		case FSt_ECHFS:
			return drive->data_area + c * drive->spc;
		default:
			throw('Drive');
	}
}

I64 ClusBlkRead(CDrive *drive, U8 *buf, I64 c, I64 blks)
{//Accepts blk count, so padding on last clus is not read.
	I64 i;
	Bool unlock;
	DriveCheck(drive);
	if (blks <= 0)
		return c;
	try
	{
		unlock = DriveLock(drive);
		switch (drive->fs_type)
		{
			case FSt_REDSEA:
				BlkRead(drive, buf, c, blks);
				c += blks;
				break;
			case FSt_ECHFS:
				while (blks && c != 0xFFFFFFFFFFFFFFFF)
				{
					i = blks;
					if (i > drive->spc)
						i = drive->spc;
					BlkRead(drive, buf, drive->data_area + c * drive->spc, i);
					buf += i * drive->spc;
					c = ClusNumNext(drive, c, 1);
					blks -= i;
				}
				if (blks)
					throw('Drive');
				break;
			case FSt_FAT32:
				while (blks && 0 < c < 0x0FFFFFF8)
				{
					i = blks;
					if (i > drive->spc)
						i = drive->spc;
					BlkRead(drive, buf, drive->data_area + c * drive->spc, i);
					buf += i << BLK_SIZE_BITS;
					c = ClusNumNext(drive,c,1);
					blks -= i;
				}
				if (blks)
					throw('Drive');
				break;
			default:
				throw('Drive');
		}
		if (unlock)
			DriveUnlock(drive);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
	return c;
}

I64 ClusRead(CDrive *drive, U8 *buf, I64 c, I64 count)
{//Read clus count from drv to buf.
	return ClusBlkRead(drive, buf, c, count * drive->spc);
}

I64 ClusBlkWrite(CDrive *drive, U8 *buf, I64 c, I64 blks)
{//Accepts blk count, so padding on last clus is not written.
	I64 i;
	Bool unlock;
	DriveCheck(drive);
	if (blks <= 0)
		return c;
	try
	{
		unlock = DriveLock(drive);
		switch (drive->fs_type)
		{
			case FSt_REDSEA:
				BlkWrite(drive, buf, c, blks);
				c = 0;
				break;
			case FSt_ECHFS:
				while (blks)
				{
					if (c == 0xFFFFFFFFFFFFFFFF)
						throw('Drive');
					i = blks;
					if (i > drive->spc)
						i = drive->spc;
					BlkWrite(drive, buf, drive->data_area + c * drive->spc, i);
					buf += i * drive->spc;
					c = ClusNumNext(drive, c);
					blks -= i;
				}
				break;
			case FSt_FAT32:
				while (blks)
				{
					if (!(0 < c < 0x0FFFFFF8))
						throw('Drive');
					i = blks;
					if (i > drive->spc)
						i = drive->spc;
					BlkWrite(drive, buf, drive->data_area + c * drive->spc, i);
					buf += i << BLK_SIZE_BITS;
					c = ClusNumNext(drive, c);
					blks -= i;
				}
				break;
			default:
				throw('Drive');
		}
		if (unlock)
			DriveUnlock(drive);
	}
	catch
		if (unlock)
			DriveUnlock(drive);
	return c;
}

I64 ClusWrite(CDrive *drive, U8 *buf, I64 c, I64 count)
{//Write clus count from buf to drv.
	return ClusBlkWrite(drive, buf, c, count * drive->spc);
}

I64 ClusAlloc(CDrive *drive, I64 c=0, I64 count=1, Bool contiguous=FALSE)
{//Alloc clus count into chain.
//c=0 means first clus in chain
	U64 blk, fat_addr;
	DriveCheck(drive);
	if (count <= 0)
		return c;
	switch (drive->fs_type)
	{
		case FSt_ECHFS:
			if (contiguous)
				if (!EchFSAllocBlockChainContiguous(drive, &blk, count))
					throw('Space');
			else if (!EchFSAllocBlockChain(drive, &blk, count))
				throw('Space');
			fat_addr = drive->echfs_id_table->fat_start * drive->echfs_id_table->bytes_per_block;
			EchFSBlkDevWrite(drive, blk, fat_addr + c * sizeof(U64), sizeof(U64));
			return blk;
		case FSt_REDSEA:
			return RedSeaAllocClus(drive, count);
		case FSt_FAT32:
			if (contiguous)
			{
				if (c)
					throw('File');
				return FAT32AllocContiguousClus(drive, count);
			}
			else
				return FAT32AllocClus(drive, c, count);
		default:
			throw('Drive');
	}
}
