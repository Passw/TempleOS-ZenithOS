U0 MountInt13hAuto()
{
	CRMRegs r;
	CInt13hParams *params;
	U64 cur_drive;
	CBlkDev *blk;

	for (cur_drive = 0x80; cur_drive < 0xE0; cur_drive++)
	{
		MemSet(&r, 0, sizeof(CRMRegs));

		//Issue BIOS interrupt 13h, AH=48h: Ext Read Drive Params
		r.eax = 0x4800;
		r.edx = cur_drive;

		params = BAlloc(sizeof(CInt13hParams), 16);

		r.esi = params(U32);

		RealModeInt(0x13, &r, &r);

		if (r.eflags & 1 || r.eax != 0)
		{//If carry flag is set, it's an error
			goto continue;
		}

		"Int 13h: Detected disk drive %u\n", cur_drive;
		"Int 13h: Sector count: %u\n", params->sector_count;

		blk = BlkDevNextFreeSlot(blkdev.first_int13h_drive_let, BDT_INT13H);
		blk->bios_drive = cur_drive;
		blk->max_blk = params->sector_count - 1;
		BlkDevAdd(blk, , FALSE, FALSE);

continue:
		BFree(sizeof(CInt13hParams));
	}
}

Bool Int13hTransferBlk(U64 drive, U8 *buf, U64 block, Bool write)
{
	U8 *tmp;
	CInt13hDAP *dap;
	CRMRegs r;

	MemSet(&r, 0, sizeof(CRMRegs));

	dap = BAlloc(sizeof(CInt13hDAP), 16);

	tmp = BAlloc(BLK_SIZE, 16);
	if (write)
		MemCopy(tmp, buf, BLK_SIZE);

	dap->size  = 16;
	dap->count = 1;
	dap->off   = tmp(U16);
	dap->seg   = 0;
	dap->lba   = block;

	if (write)
		r.eax = 0x4300;
	else
		r.eax = 0x4200;
	r.edx = drive;
	r.esi = dap(U32);

	RealModeInt(0x13, &r, &r);

	if (r.eflags & 1)
	{
		BFree(BLK_SIZE + sizeof(CInt13hDAP));
		return FALSE;
	}

	if (!write)
		MemCopy(buf, tmp, BLK_SIZE);

	BFree(BLK_SIZE + sizeof(CInt13hDAP));

	return TRUE;
}

Bool Int13hTransferBlksFromBlkDev(CBlkDev *bd, U8 *buf, U64 start_blk, U64 count, Bool write) {
	Bool unlock;
	U64 bios_drive;

	unlock = BlkDevLock(bd);

	bios_drive = bd->bios_drive;

	while (count-- > 0)
	{
		if (Int13hTransferBlk(bios_drive, buf, start_blk++, write) == FALSE) {
			if (unlock)
				BlkDevUnlock(bd);
			throw('Int13h');
			return FALSE;
		}
		buf += BLK_SIZE;
	}

	if (unlock)
		BlkDevUnlock(bd);
	return TRUE;


}

Bool Int13hTransferBlksFromDrive(CDrive *drive, U8 *buf, U64 start_blk, U64 count, Bool write) {
	return Int13hTransferBlksFromBlkDev(drive->bd, buf, start_blk, count, write);

}