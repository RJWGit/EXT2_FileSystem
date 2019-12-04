/*********** util.c file ****************/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256];

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}   

int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
}   

int tokenize(char *pathname)
{
  // tokenize pathname in GLOBAL gpath[]
  char *s; 
  strcpy(gpath, pathname); 
  n = 0; 
  s = strtok(gpath, "/"); 
  while(s){ 
    name[n++] = s;
    s = strtok(0, "/"); 
  }

}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, disp;
  INODE *ip;

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->dev == dev && mip->ino == ino){
       mip->refCount++;
       //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       return mip;
    }
  }
    
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount == 0){
      //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
       mip->refCount = 1;
       mip->dev = dev;
       mip->ino = ino;

       // get INODE of ino to buf    
       blk  = (ino-1) / 8 + inode_start;
       disp = (ino-1) % 8;

       //printf("iget: ino=%d blk=%d disp=%d\n", ino, blk, disp);

       get_block(dev, blk, buf);
       ip = (INODE *)buf + disp;
       // copy INODE to mp->INODE
       mip->INODE = *ip;

       return mip;
    }
  }   
  printf("PANIC: no more free minodes\n");
  return 0;
}

int iput(MINODE *mip)
{
 int i, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 if (mip==0) 
     return 1;

 mip->refCount--;
 
 //TODO
 //Figure out out how to deal with this with root ref count > 0 at all times, 
 //impossible to update root inode with this line enabled
 //if (mip->refCount > 0) return 1;
 if (mip->dirty == 0)       return 1;
 
 /* write back */
 //printf("iput: dev=%d ino=%d\n", mip->dev, mip->ino); 

 block =  ((mip->ino - 1) / 8) + inode_start;
 offset =  (mip->ino - 1) % 8;

 /* first get the block containing this inode */
 get_block(mip->dev, block, buf);

 ip = (INODE *)buf + offset;
 *ip = mip->INODE;

 put_block(mip->dev, block, buf);

} 

int search(MINODE *mip, char *name)
{
  int i; 
  char *cp, temp[256], sbuf[BLKSIZE]; 
  DIR *dp; 

  for (i=0; i<12; i++){ // search DIR direct blocks only 
    if (mip->INODE.i_block[i] == 0) 
      return 0; 

    get_block(mip->dev, mip->INODE.i_block[i], sbuf); 
    dp = (DIR *)sbuf; 
    cp = sbuf; 

    while (cp < sbuf + BLKSIZE){ 
      strncpy(temp, dp->name, dp->name_len); 
      temp[dp->name_len] = 0; 
      //printf("%8d%8d%8u %s\n", dp->inode, dp->rec_len, dp->name_len, temp); 
    
      if (strcmp(name, temp)==0){ 
        printf("found %s : inumber = %d\n", name, dp->inode); 
        return dp->inode; 
        } 
      
      cp += dp->rec_len; 
      dp = (DIR *)cp; 
      } 
  } 
    return 0;
}

int get_myname(MINODE *parent_minode, int my_ino, char *my_name){
  char *cp, temp[256], sbuf[BLKSIZE]; 
  DIR *dp; 
  int i;
    for (i=0; i<12; i++){
    if (parent_minode->INODE.i_block[i] == 0) 
      return 1; 

    get_block(parent_minode->dev, parent_minode->INODE.i_block[i], sbuf); 
    dp = (DIR *)sbuf; 
    cp = sbuf; 

    while (cp < sbuf + BLKSIZE){ 
      strncpy(temp, dp->name, dp->name_len); 
      temp[dp->name_len] = 0; 
    
      if (my_ino == dp->inode){ 
        //printf("found Dir %s\n", dp->name); 
        strcpy(my_name, temp);
        return 0; 
        } 
      
      cp += dp->rec_len; 
      dp = (DIR *)cp; 
      } 
    }
    return 0;
}

int get_myino(MINODE *mip){
  int i; 
  char *cp, temp[256], sbuf[BLKSIZE]; 
  DIR *dp; 

  for (i=0; i<12; i++){ // search DIR direct blocks only 
    if (mip->INODE.i_block[i] == 0) 
      return 0; 

    get_block(mip->dev, mip->INODE.i_block[i], sbuf); 
    dp = (DIR *)sbuf; 
    cp = sbuf; 

    while (cp < sbuf + BLKSIZE){ 
      strncpy(temp, dp->name, dp->name_len); 
      temp[dp->name_len] = 0; 
    
      if (strcmp("..", temp)==0){ 
        printf("found %s : ParentIno = %d\n", temp, dp->inode); 
        return dp->inode; 
        } 
      
      cp += dp->rec_len; 
      dp = (DIR *)cp; 
      } 
  } 
    return 0;
}


int getino(char *pathname)
{
  int i, ino, blk, disp;
  INODE *ip;
  MINODE *mip;

  printf("getino: pathname=%s\n", pathname);
  if (strcmp(pathname, "/")==0)
      return 2;

  if (pathname[0]=='/')
    mip = iget(dev, 2);
  else
    mip = iget(running->cwd->dev, running->cwd->ino);

  tokenize(pathname);

  for (i=0; i<n; i++){
      printf("===========================================\n");
      ino = search(mip, name[i]);

      if (ino==0){
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);
      mip = iget(dev, ino);
   }
   return ino;
}

int tst_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  if (buf[i] & (1 << j))
     return 1;
  return 0;
}

int set_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] |= (1 << j);
}

int clr_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] &= ~(1 << j);
}


int decFreeInodes(int dev)
{
  char buf[BLKSIZE];
  // dec free inodes count by 1 in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count--;
  put_block(dev, 2, buf);
}

int ialloc(int dev)  // allocate an inode number
{
  int  i;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, imap, buf);

  for (i=0; i < ninodes; i++){
    if (tst_bit(buf, i)==0){
       set_bit(buf,i);
       put_block(dev, imap, buf);
       decFreeInodes(dev);
       return i+1;
    }
  }
  return 0;
}

int decFreeBlocks(int dev)
{
  char buf[BLKSIZE];
  
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_blocks_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_blocks_count--;
  put_block(dev, 2, buf);
}

int balloc(int dev)  // allocate disk block
{
  int  i;
  char buf[BLKSIZE];

  // read blocks_bitmap block
  get_block(dev, bmap, buf);

  for (i=0; i < nblocks; i++){
    if (tst_bit(buf, i)==0){
       set_bit(buf,i);
       put_block(dev, bmap, buf);
       decFreeBlocks(dev);
       return i+1;
    }
  }
  return 0;
}

int createInodeDir(int ino, int blk){

  MINODE *mip = iget(dev,ino);
  INODE *ip = &mip->INODE; 
  ip->i_mode = 0x41ED; // 040755: DIR type and permissions 
  ip->i_uid = running->uid; // owner uid 
  ip->i_gid = running->pid; // group Id 
  ip->i_size = BLKSIZE; // size in bytes 
  ip->i_links_count = 2; // links count=2 because of . and .. 
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L); 
  ip->i_blocks = 2; // LINUX: Blocks count in 512-byte chunks 
  ip->i_block[0] = blk; // new DIR has one data block 
  for(int i = 1; i < 15; i++){
    ip->i_block[i] = 0;
  }
  mip->dirty = 1; // mark minode dirty 
  iput(mip); // write INODE to disk
}

int createInodeFile(int ino, int blk){

  MINODE *mip = iget(dev,ino);
  INODE *ip = &mip->INODE; 
  ip->i_mode = 0x81A4;
  ip->i_uid = running->uid; // owner uid 
  ip->i_gid = running->pid; // group Id 
  ip->i_size = 0; // size in bytes 
  ip->i_links_count = 1;
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L); 
  ip->i_blocks = 2; // LINUX: Blocks count in 512-byte chunks 
  //ip->i_block[0] = blk; // new DIR has one data block 
  for(int i = 0; i < 15; i++){
    ip->i_block[i] = 0;
  }
  mip->dirty = 1; // mark minode dirty 
  iput(mip); // write INODE to disk
}

int createDataBlockStartEntries(int ino, int blk, int pino){
  char buf[BLKSIZE];
  bzero(buf, BLKSIZE); // optional: clear buf[ ] to 0 
  DIR *dp = (DIR *)buf; 

  // make . entry 
  dp->inode = ino; 
  dp->rec_len = 12; 
  dp->name_len = 1; 
  dp->name[0] = '.'; 

  // make .. entry: pino=parent DIR ino, blk=allocated block 
  dp = (char *)dp + 12; 
  dp->inode = pino; 
  dp->rec_len = BLKSIZE-12; // rec_len spans block 
  dp->name_len = 2; 
  dp->name[0] = dp->name[1] = '.'; 
  put_block(dev, blk, buf); // write to blk on diks

}

int newDirEntry(MINODE *mip, int ino, char *name, char type){
  int i, remain, need_len, ideal_len; 
  char *cp, sbuf[BLKSIZE], newbuf[BLKSIZE]; 
  DIR *dp; 

  for (i=0; i<12; i++){ // search DIR direct blocks only 
    if (mip->INODE.i_block[i] == 0) 
      break; 

    get_block(mip->dev, mip->INODE.i_block[i], sbuf); 
    dp = (DIR *)sbuf; 
    cp = sbuf; 

    while (cp + dp->rec_len < sbuf + BLKSIZE){ 
      cp += dp->rec_len; 
      dp = (DIR *)cp; 
      } 

    need_len = 4*((8 + getSizeofString(name) + 3)/4); 
    ideal_len =  4*((8 + dp->name_len + 3)/4); 
    remain = dp->rec_len - ideal_len;


    printf("need:%d\n",need_len);
    printf("ideal:%d\n",ideal_len);
    printf("remain:%d\n",remain);

    if(remain >= need_len){
      //Update now second to last dir
      dp->rec_len = ideal_len;
      cp += dp->rec_len;
      dp = (DIR *)cp; 

      //Add new dir entry
      dp->rec_len = remain;
      dp->inode = ino;
      strcpy(dp->name,name);
      dp->name_len = getSizeofString(name);
      //dp->file_type = 0x41ED;

      put_block(mip->dev, mip->INODE.i_block[i], sbuf);

      if(type == 'd'){     
        // INODE *ip = &mip->INODE; 
        // ip->i_links_count += 1;
        mip->INODE.i_links_count += 1;
      }
      
      mip->dirty = 1;
      iput(mip);
     
      return 0;
    } 
  } 

  //Exit if INODE ran out i_blocks
  if(i == 12){
    printf("Inode ran out of direct block!\n");
    return 1;
  }
  
  //If dir blocks are full then create new one if INODE has space
  //BUG: NOT WORKING AFTER QUIT AND RELOAD 

  //Allocate new i_block for INODE
  mip->INODE.i_block[i] = balloc(mip->dev);
  //get_block(mip->dev, mip->INODE.i_block[i], newbuf); 

  dp = (DIR *)newbuf; 
  //cp = newbuf;
  

  //Add new dir entry to new i_block
  dp->rec_len = BLKSIZE;
  dp->inode = ino;
  strcpy(dp->name,name);
  dp->name_len = getSizeofString(name);

  put_block(mip->dev, mip->INODE.i_block[i], newbuf);

  //Increase parent size
  mip->INODE.i_size += BLKSIZE;

  //If parent is a directory then increase count
  if(type == 'd')
    mip->INODE.i_links_count += 1;

  
  return 0;
}

//Not including null term
int getSizeofString(char *str){

  for(int i = 0; i < 255; i++){
    if(str[i] == '\0'){
      return i;
    }
  }


  printf("Error: getSizeofString\n Name too big\n");
  exit(1);
}

int incFreeInodes(int dev)
{
  char buf[BLKSIZE];

  // inc free inodes count in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buf);
}

int idalloc(int dev, int ino)  // deallocate an ino number
{
  int i;  
  char buf[BLKSIZE];

  if (ino > ninodes){
    printf("inumber %d out of range\n", ino);
    return 1;
  }

  // get inode bitmap block
  get_block(dev, imap, buf);
  clr_bit(buf, ino-1);

  // write buf back
  put_block(dev, imap, buf);

  // update free inode count in SUPER and GD
  incFreeInodes(dev);
}

int incFreeBlocks(int dev)
{
  char buf[BLKSIZE];
  
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_blocks_count++;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_blocks_count++;
  put_block(dev, 2, buf);
}


int bdalloc(int dev, int blk) // deallocate a blk number
{
  int i;  
  char buf[BLKSIZE];

  if (blk > nblocks){
    printf("blk %d out of range\n", blk);
    return 1;
  }

  // get blk bitmap block
  get_block(dev, bmap, buf);
  clr_bit(buf, blk-1);

  // write buf back
  put_block(dev, bmap, buf);

  // update free inode count in SUPER and GD
  incFreeBlocks(dev);
}

int isDirEmpty(MINODE *mip){
  char *cp, temp[256], sbuf[BLKSIZE]; 
  DIR *dp; 
  int i;
    for (i=0; i<12; i++){
    if (mip->INODE.i_block[i] == 0) 
      return 0; 

    get_block(mip->dev, mip->INODE.i_block[i], sbuf); 
    dp = (DIR *)sbuf; 
    cp = sbuf; 

    while (cp < sbuf + BLKSIZE){ 
      strncpy(temp, dp->name, dp->name_len); 
      temp[dp->name_len] = 0; 
    
      if(strcmp(temp,".") != 0 && strcmp(temp,"..") != 0 ){ 
          return 1;
        } 
      
      cp += dp->rec_len; 
      dp = (DIR *)cp; 
      } 
    }
    return 0;
}

int rmChild(MINODE *pmip, char *name){
  int i;
  char *cp,*lp, sbuf[BLKSIZE], newbuf[BLKSIZE], temp[256]; 
  DIR *dp; 

  //Search by name for desired entry in buffer
  for (i=0; i<12; i++){ // search DIR direct blocks only 
    if (pmip->INODE.i_block[i] == 0 || strcmp(temp, name) == 0 )
      break; 

    get_block(pmip->dev, pmip->INODE.i_block[i], sbuf); 
    dp = (DIR *)sbuf; 
    cp = sbuf; 

    while (cp + dp->rec_len < sbuf + BLKSIZE){ 
      strncpy(temp, dp->name, dp->name_len); 
      temp[dp->name_len] = 0; 

      // printf("temp: %s\n", temp);
      // printf("name: %s\n", name);
      // printf("dp reclen: %d\n", dp->rec_len);

      if(strcmp(temp, name) == 0){ 
          printf("FOUND\n");
          break;
          }

      //Only used in case where deleting last entry of block; stores second to last entry
      lp = cp; 

      cp += dp->rec_len; 
      dp = (DIR *)cp; 
      } 
  } 
  //i should now match what block name was found in INODE
  i--;


  // printf("\ndp reclen: %d\n", dp->rec_len);
  // printf("counter: %d\n", counter);
  // printf("i: %d\n", i);

  


  int deleted_rec_len = dp->rec_len;

  //Check if only 1 entry in block; deallocate blk if true
  if(deleted_rec_len == BLKSIZE){
    bdalloc(pmip->dev,pmip->INODE.i_block[i]);
    pmip->INODE.i_links_count -= 1;
    pmip->INODE.i_size -= BLKSIZE;
    //TO DO
    //Shift parent INODE.i_block array so no 'gaps' of 0 exist ---> [1,1,0,1] Check and shift so all blocks are in consectuive row
  }

  //Check if last entry
  else if(cp + dp->rec_len == sbuf + BLKSIZE){

    //Move pointer back to second to last entry
    cp -= cp - lp;
    dp = (DIR *)cp; 
    
    dp->rec_len += deleted_rec_len;
  }


  else{
    int size = 0;
    cp += dp->rec_len; 

    //Get size from start of next entry of the one going to be deleted to end of buffer
    size = (sbuf + BLKSIZE) - cp;

    // printf("size: %d\n",size); 
    // printf("name_len: %d\n",dp->name_len);
    
    //Shift all other entries, starting where deleted entry was
    memcpy(dp,cp,size);

    //Move back pointer
    cp -= deleted_rec_len;

    // printf("name_len: %d\n",dp->name_len);


    //Find last INODE in parent
    while (cp + dp->rec_len < sbuf + BLKSIZE - deleted_rec_len){ 
      cp += dp->rec_len; 
      dp = (DIR *)cp; 
    }

    //Add deleted rec length to last INODE in parent
    dp->rec_len += deleted_rec_len;
  
  }

   //Update Block and link count
    pmip->INODE.i_links_count -= 1;
    iput(pmip);
    put_block(pmip->dev, pmip->INODE.i_block[i], sbuf);
}

//Check if file is open
int isFileOpen(MINODE *mip){
  for(int i=0; i<8; i++){
    if(running->fd[i] != 0){
      if (running->fd[i]->mptr == mip){
        if (running->fd[i]->mode>-1){
          printf("Error: File already open\n");
          return 1;
        }
      }
    }
	}

  return 0;
}

//Check if running process has room for new file and return open slot if so
int falloc(){
  int i;

  for(i = 0;i<8;i++){
    if(running->fd[i] == 0)
      return i;
  }

  return -1;
}

//TODO: 
int truncate(MINODE *mip){
  int *indirect, *dindirect;
  char buf[BLKSIZE], buf2[BLKSIZE];

  //Free direct blocks
  for(int i=0; i<12; i++){
    if(mip->INODE.i_block[i] != 0){
      bdalloc(mip->dev,mip->INODE.i_block[i]);
      mip->INODE.i_block[i] = 0;
    }
  }

  //Free indirect blocks
  if(mip->INODE.i_block[12] != 0){
    get_block(dev,mip->INODE.i_block[12],buf);
    indirect = (int *)buf;

    for(int i=0; i<256; i++){
      if(*indirect != 0){
        bdalloc(mip->dev,*indirect);
        *indirect = 0;
        indirect++;
      }
    }

    mip->INODE.i_block[12] = 0;
  }

  //Free double indirect blocks
  if(mip->INODE.i_block[13] != 0){
    get_block(dev,mip->INODE.i_block[13],buf);
    dindirect = (int *)buf;

    for(int i=0; i<255; i++){
      if(*dindirect != 0){
       get_block(dev,*dindirect,buf2);
       indirect = (int *)buf2;
       
       for(int j=0; j<255; j++){
         if(*indirect != 0){
          bdalloc(mip->dev,*indirect);
          *indirect = 0;
         }
        indirect++;
       }
      }
      dindirect++;
    }
    mip->INODE.i_block[13] = 0;
  }

  mip->INODE.i_atime = time(0L);
  mip->INODE.i_mtime = time(0L); 
  mip->INODE.i_size = 0;
  mip->dirty = 1;
}

int findFdByName(char *filename){
  int i;
  int dev, ino;
  MINODE *mip;

  if(filename[0] == '\0'){
    printf("No filename given\n");
    return 1;
  }

  if(filename[0] == '/')
    dev = root->dev;
  else
    dev = running->cwd->dev;

  ino = getino(filename);

  if(ino == 0){
    printf("Error: Could not find inode of given name\n");
    return 1;
  }
  
  mip = iget(dev,ino);

  for(i = 0;i<8;i++){
    if(running->fd[i]->mptr == mip)
      return i;
  }

  return -1;
}

//Not working
int findInodeName(MINODE *mip,int ino, char *filename){
  char sbuf[BLKSIZE];
  char *cp; 
  char temp[255];
  DIR *dp; 

  for(int i = 0; i < 12; i++){
    if (mip->INODE.i_block[i] == 0){ 
      return 0;}
    
    get_block(dev,mip->INODE.i_block[i],sbuf);
    dp = (DIR *)sbuf; 
    cp = sbuf; 

    while (cp < sbuf + BLKSIZE){ 
      strncpy(temp, dp->name, dp->name_len); 
      temp[dp->name_len] = 0; 

      if(dp->inode == ino){
        printf("FOUND\n");
        strcpy(filename,temp);

        return 0;
      }

      if(strcmp(filename, " ")== 0){
        MINODE *child = iget(dev,dp->inode);
        findInodeName(child,ino, &filename);
        iput(child);
      }
      cp += dp->rec_len; 
      dp = (DIR *)cp; 
      } 

  }
}

int lseekFile(int fd, int postion){
  int originalpos;

  if(running->fd[fd]->mptr->INODE.i_size < postion){
    printf("Postion is outside of file\n");
    return 1;
  }

  if(postion < 0){
    printf("Position can not be negative\n");
    return 1;
  }

  originalpos = running->fd[fd]->offset;
  running->fd[fd]->offset = postion;

  return originalpos;

  
}

int myread(int fd, char *buf, int nbytes){
  int count = 0;
  int filesize = running->fd[fd]->mptr->INODE.i_size;
  int offset =running->fd[fd]->offset;
  int avil = filesize - offset;
  int lbk, startbyte, blk;
  int remain;
  int *indirect, *dindirect;
  char *cq = buf;
  char *cp;
  char readbuf[BLKSIZE];
  char indirbuf[BLKSIZE], dindirbuf[BLKSIZE];


  while(nbytes > 0 && avil >0){
    lbk = offset / BLKSIZE;
    startbyte = offset % BLKSIZE;

    if(lbk<12){
      blk = running->fd[fd]->mptr->INODE.i_block[lbk];
      printf("direct\n");
    }
    //  indirect blocks 
    else if (lbk >= 12 && lbk < 256 + 12) { 
      get_block(running->fd[fd]->mptr->dev,running->fd[fd]->mptr->INODE.i_block[12],indirbuf);
      indirect = (int *)indirbuf;
      blk = *(indirect+lbk-12);
      printf("indirect\n");

    }

    else{ 
      get_block(running->fd[fd]->mptr->dev,running->fd[fd]->mptr->INODE.i_block[13],dindirbuf);
      dindirect = (int *)dindirbuf;

      blk = *(dindirect + ((lbk-268)/256));
      get_block(running->fd[fd]->mptr->dev,blk,indirbuf);
  
      indirect = (int *)indirbuf;
      blk = *(indirect+((lbk-268)%256));
      printf("dindirect\n");

    } 

    get_block(running->fd[fd]->mptr->dev, blk, readbuf);

    cp = readbuf + startbyte;
    remain = BLKSIZE - startbyte;

    while(remain > 0){
      *cq = *cp;
      cq++;
      cp++;
      // printf("%c", *cp);
      running->fd[fd]->offset++;
      count++;
      avil--;
      nbytes--;
      remain--;

      if(nbytes <= 0 || avil <= 0){
        break;
      }

    }
  }
  printf("myread: read %d char from file descriptor %d\n", count, fd);  
  return count;
}

int mywrite(int fd, char *buf, int nbytes){
  int offset =running->fd[fd]->offset;
  int lbk, startbyte, blk;
  int remain;
  int *indirect, *dindirect;
  char *cq = buf;
  char *cp;
  char writebuf[BLKSIZE];
  char indirbuf[BLKSIZE], dindirbuf[BLKSIZE];
  printf("%s\n", buf);


  while(nbytes > 0){
    lbk = offset / BLKSIZE;
    startbyte = offset % BLKSIZE;

    if(lbk<12){
      if(running->fd[fd]->mptr->INODE.i_block[lbk] == 0){
        running->fd[fd]->mptr->INODE.i_block[lbk] = balloc(running->fd[fd]->mptr->dev);
      }
      blk = running->fd[fd]->mptr->INODE.i_block[lbk];
      printf("direct\n");
    }
    // //  indirect blocks 
    // else if (lbk >= 12 && lbk < 256 + 12) { 
    //   get_block(running->fd[fd]->mptr->dev,running->fd[fd]->mptr->INODE.i_block[12],indirbuf);
    //   indirect = (int *)indirbuf;
    //   blk = *(indirect+lbk-12);
    //   printf("indirect\n");

    // }

    // else{ 
    //   get_block(running->fd[fd]->mptr->dev,running->fd[fd]->mptr->INODE.i_block[13],dindirbuf);
    //   dindirect = (int *)dindirbuf;

    //   blk = *(dindirect + ((lbk-268)/256));
    //   get_block(running->fd[fd]->mptr->dev,blk,indirbuf);
  
    //   indirect = (int *)indirbuf;
    //   blk = *(indirect+((lbk-268)%256));
    //   printf("dindirect\n");

    // } 

    get_block(running->fd[fd]->mptr->dev, blk, writebuf);

    cp = writebuf + startbyte;
    remain = BLKSIZE - startbyte;

    while(remain > 0){
      *cp++ = *cq++;
      running->fd[fd]->offset++;
      nbytes--;
      remain--;

      if (running->fd[fd]->offset > running->fd[fd]->mptr->INODE.i_size){
        running->fd[fd]->mptr->INODE.i_size++;
      }

      if(nbytes <= 0){
        break;
      }

    }

    put_block(running->fd[fd]->mptr->dev,blk,writebuf);
  }

  running->fd[fd]->mptr->dirty = 1;
  iput(running->fd[fd]->mptr);
  return nbytes;
}
