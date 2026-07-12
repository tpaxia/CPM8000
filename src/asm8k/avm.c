#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"

static	char	ident[] = "@(#)a.vm.c	3.1";

/*
 * getbuf - Returns a pointer to a buffer which contains the specified
 * virtual address.  Pages the buffers on a LRU basis.
 */
struct vmbuf *
getbuf(adr) vmadr adr; {

static int limblk = 0;			/* file high water mark */
reg struct vmbuf *cb, *pb;
reg uns vbn;

#ifdef	STATS
	vmgct++;
#endif
	vbn = adr>>BLKLOG;
	pb = &vmhead;  cb = vmhead;
	while(cb->vm_blk!=vbn && cb->vm_lnk) {
		pb = cb;  cb = cb->vm_lnk;
	}
	if(cb->vm_blk!=vbn) { /* must commandeer a buffer */
		if(cb->vm_flg&VMDIR) { /* write out old contents */
			lseek(vmfd,(long)cb->vm_blk<<BLKLOG,0);
			write(vmfd,cb->vm_mem,1<<BLKLOG);
			if(cb->vm_blk >= limblk) limblk = cb->vm_blk+1;
			cb->vm_flg &= ~VMDIR;
#ifdef	STATS
			vmwct++;
#endif
		}
		if(vbn < limblk) {  /* read from disk */
			lseek(vmfd,(long)vbn<<BLKLOG,0);
			read(vmfd,cb->vm_mem,1<<BLKLOG);
#ifdef	STATS
			vmrct++;
#endif
		}
		cb->vm_blk = vbn;
	}
	pb->vm_lnk = cb->vm_lnk;  cb->vm_lnk = vmhead;  vmhead = cb;
	return(cb);
}

/*
 * rfetch - Fetches a specified virtual address for reading, and returns
 * a pointer to the in-core copy of the address.
 */
char *
rfetch(adr) vmadr adr; {

	return(&getbuf(adr)->vm_mem[adr&(1<<BLKLOG)-1]);
}

/*
 * valloc - Allocates a region of virtual memory of the specified size,
 * and returns the vm address of it.  The region is guaranteed to reside
 * entirely within a single block.
 */
vmadr
valloc(size) uns size; {

vmadr	base, blklim;

	blklim = (virtop|(1<<BLKLOG)-1)+1;
	if(virtop+size > blklim) virtop = blklim;
	base = virtop;  virtop += size;
	return(base);
}

/*
 * vinit - Opens the virtual memory file.
 */
vinit() {

	close(creatb("VM.TMP",0666));
	if((vmfd=openb("VM.TMP",2)) == -1) {
		fprintf(ERROR,"Cannot reopen vm file\n");
		exit(1);
	}
}

/*
 * wfetch - Fetches a specified virtual address for writing, and returns
 * a pointer to the in-core copy of the address.
 */
char *
wfetch(adr) vmadr adr; {

reg struct vmbuf *bp;

	(bp=getbuf(adr))->vm_flg |= VMDIR;
	return(&bp->vm_mem[adr&(1<<BLKLOG)-1]);
}
