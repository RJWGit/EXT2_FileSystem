/************* cd_ls_pwd.c file **************/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, inode_start;
extern char line[256], cmd[32], pathname[256];
char *t1 = "xwrxwrxwr-------"; 
char *t2 = "----------------";

#define OWNER  000700
#define GROUP  000070
#define OTHER  000007

int change_dir()
{
  if(strcmp(pathname, "") == 0)
      running->cwd = root;

  else{
    int ino = getino(pathname);

    if(ino == 0){
      printf("Error: No ino found");
      return 0;
    }

    MINODE *mip = iget(dev, ino);

    if (!S_ISDIR(mip->INODE.i_mode)){
      printf("Not a directory\n");
      return 0;
    }

    iput(running->cwd);
    running->cwd = mip;
    iput(mip);
  }

}

int rmkdir(){
  int ino, blk, pino;
  MINODE *pmip;
  char *bname = basename(pathname);
  char *dname = dirname(pathname);

  //Check if user gave new directory name
  if (strcmp(bname,"") == 0){
    printf("No dir name specified\n");
    return 1;
  }

  //Parent INODE
  pino = getino(dname);
  pmip = iget(dev,pino);

  //Check if dirname is a directory
  if (!S_ISDIR(pmip->INODE.i_mode)){
    printf("Not a directory\n");
    return 1;
  }

  //Check if directory already exists in parent
  if(search(pmip,bname) != 0){
    printf("That directory name already exists!\n");
    return 1;
  }

  ino = ialloc(dev);
  blk = balloc(dev);

  if(ino == 0){
    printf("That ialloc failed!\n");
    return 1;
  }

  if(blk == 0){
    printf("That balloc failed!\n");
    return 1;
  }

  createInodeDir(ino,blk);
  createDataBlockStartEntries(ino,blk,pino);
  newDirEntry(pmip,ino,bname,'d');

  // pmip->dirty = 1;
  // iput(pmip);
  //printf("Bname: %s\nDname: %s\n",bname,dname);

}

int list_file(int ino, char *name){
  MINODE *mip = iget(dev,ino);
  INODE *ip = &(mip->INODE);
  char ftime[64]; 

  if ((ip->i_mode & 0xF000) == 0x8000) // if (S_ISREG()) 
    printf("%-8s ","Reg File "); 
  if ((ip->i_mode & 0xF000) == 0x4000) // if (S_ISDIR()) 
    printf("%-8s ","Directory");
  if ((ip->i_mode & 0xF000) == 0xA000) // if (S_ISLNK()) 
    printf("%-9s ","Link"); 

  for (int i=8; i >= 0; i--){
    if (ip->i_mode & (1 << i)) // print r|w|x 
    printf("%c", t1[i]); 
    else
    printf("%c", t2[i]);
  }


  printf(" Link count:%-8d ",ip->i_links_count); // link count 
  printf("GID:%-6d ",ip->i_gid); // gid 
  printf("UID:%-6d ",ip->i_uid); // uid 
  printf("File size:%-6d ",ip->i_size); // file size

  // print time
  strcpy(ftime, ctime(&ip->i_dtime)); // print time in calendar form 
  ftime[strlen(ftime)-1] = 0; // kill \n at end 
  printf(" %-32s ",ftime); 

  // print name 
  printf(" %-10s\n", name); // print file basename 
  iput(mip);
}

int list_dir(){

  int ino = 0;
  MINODE *mip;
  char sbuf[BLKSIZE];
  char *cp, temp[256]; 
  DIR *dp; 
  struct dirent *ptr;

  //Get minode from cwd if no pathname provided
  if(strcmp(pathname, "") == 0){
    mip = running->cwd;
    ino = mip->ino;
  }

  //Get minode from user provided path using iget
  else{
    ino = getino(pathname);
    mip = iget(dev, ino);
  }

  printf("\n-----------------------------------\n");
  for(int i = 0; i < 12; i++){
    if (mip->INODE.i_block[i] == 0){ 
      printf("\n-----------------------------------\n");
      return 0;}

    get_block(dev,mip->INODE.i_block[i],sbuf);
    dp = (DIR *)sbuf; 
    cp = sbuf; 

    while (cp < sbuf + BLKSIZE){ 
      strncpy(temp, dp->name, dp->name_len); 
      temp[dp->name_len] = 0; 
      //printf("%8d%8d%8u %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
       list_file(dp->inode,temp);

      //printf("Legnth of dir %d\n",dp->rec_len);
      cp += dp->rec_len; 
      dp = (DIR *)cp; 
      } 

  }
  iput(mip);

}

int rpwd(MINODE *wd){
  char myname[256];
  int myino, parentino;

  if(wd == root)
    return 0;
    
  myino = wd->ino;
  //printf("RWPD INO:%d\n", myino);

  parentino = get_myino(wd);

  MINODE *pip = iget(dev,parentino);

  get_myname(pip,myino,&myname); 

  rpwd(pip);

  printf("/%s",myname);

  iput(pip);
}

int rcreat(){
  int ino, blk, pino;
  MINODE *mip, *pmip;
  char *bname = basename(pathname);
  char *dname = dirname(pathname);

  //Check if user gave new directory name
  if (strcmp(pathname,"") == 0){
    printf("No dir name specified\n");
    return 1;
  }

  //Parent INODE
  pino = getino(dname);
  pmip = iget(dev,pino);

  //Check if dirname is a directory
  if (!S_ISDIR(pmip->INODE.i_mode)){
    printf("Not a directory\n");
    return 1;
  }

  if(search(pmip,bname) != 0){
    printf("That name already exists in desired directory!\n");
    return 1;
  }

  ino = ialloc(dev);
  blk = balloc(dev);

  if(ino == 0){
    printf("That ialloc failed!\n");
    return 1;
  }

  if(blk == 0){
    printf("That balloc failed!\n");
    return 1;
  }

  createInodeFile(ino,blk);
  newDirEntry(pmip,ino,bname,'f');

  printf("Bname: %s\nDname: %s\n",bname,dname);
  iput(pmip);
}

int rrmdir(){
  int ino, blk, pino;
  MINODE *mip, *pmip;

  // char *pname;
  // strcpy(pname, pathname);
  char *bname = basename(pathname);
  char *dname = dirname(pathname);

  //Check if user gave new directory name
  if (strcmp(pathname,"") == 0){
    printf("No dir name specified\n");
    return 1;
  }

  //Parent INODE
  pino = getino(dname);
  pmip = iget(dev,pino);

  //Check if dirname is a directory
  if (!S_ISDIR(pmip->INODE.i_mode)){
    printf("Not a directory\n");
    return 1;
  }

  //Check if child exists and get child INODE number
  if((ino = search(pmip,bname)) == 0){
    printf("No such directory exists!\n");
    return 1;
  }

  //Get child INODE
  mip = iget(dev, ino);

  if (!S_ISDIR(mip->INODE.i_mode)){
    printf("Not a directory\n");
    return 1;
  }

  // if(mip->refCount != 1){
  //   printf("Minode is busy\n");
  //   return 1;
  // }

  if(isDirEmpty(mip) == 1){
    printf("Directory is not empty\n");
    return 1;
  }

  rmChild(pmip,bname);
  pmip->dirty = 1;
  iput(pmip);


  //Deallocate all blocks in INODE
  bdalloc(mip->dev,mip->INODE.i_block[0]);
  
  //Deallocate INODE
  idalloc(mip->dev,ino);

  // mip->dirty = 1;
  // iput(mip);
}

int pwd(MINODE *wd)
{
  if(wd == root)
    printf("/");

  else
    rpwd(wd);
    printf("\n");

  return 0;
}

int link(){

  int ino,pino;
  MINODE *pmip, *oldmip;

 if (strcmp(pathname,"") == 0 || strcmp(arg2,"") == 0){
    printf("No dir name specified\n");
    return 1;
  }

  ino = getino(pathname);

  if(ino != 0)
    oldmip = iget(dev, ino);
  
  //Split arg into directory name and basename
  char *bname = basename(arg2);
  char *dname = dirname(arg2);

  //Chech if reg file or link file
  if ((oldmip->INODE.i_mode & 0xF000) != 0x8000 && (oldmip->INODE.i_mode & 0xF000) != 0xA000){
    printf("Not a link file or reg file\n");
    return 1;
  }

  //Parent INODE
  pino = getino(dname);
  if(ino != 0)
    pmip = iget(dev,pino);

  //Check if dirname is a directory
  if (!S_ISDIR(pmip->INODE.i_mode)){
    printf("Not a directory\n");
    return 1;
  }

  //Add file to new directory with same ino 
  newDirEntry(pmip,ino,bname,'r');

  //Update link count of the INODE file
  oldmip->INODE.i_links_count += 1;
  oldmip->dirty = 1;
  iput(oldmip);
  return 0;
}

int unlink(){

  MINODE *mip,*pmip;
  int ino, pino;

  if (strcmp(pathname,"") == 0){
    printf("No dir name specified\n");
    return 1;
  }

  ino = getino(pathname);
  if(ino != 0)
    mip = iget(dev, ino);
  
  //Split arg into directory name and basename
  char *bname = basename(pathname);
  char *dname = dirname(pathname);

  //Chech if reg file or link file
  if ((mip->INODE.i_mode & 0xF000) != 0x8000 && (mip->INODE.i_mode & 0xF000) != 0xA000){
    printf("Not a link file or reg file\n");
    return 1;
  }
  
  //Get parent directory
  pino = getino(dname);
  if(ino != 0)
    pmip = iget(dev,pino);

  //Remove file from parent directory
  rmChild(pmip,bname);

  //Update link count of the INODE file
  mip->INODE.i_links_count -= 1;

  //Delete INODE and deallocate blocks
  if(mip->INODE.i_links_count == 0){
    //Deallocate all blocks in INODE
    bdalloc(mip->dev,mip->INODE.i_block[0]);
    
    //Deallocate INODE
    idalloc(mip->dev,ino);
  }

  else{  
    mip->dirty = 1;
  } 
  iput(mip);
  return 0;
}

int symlink(){
  int ino, pino, fino, size;
  MINODE *pmip, *oldmip, *mip;
  char oldname[59];

 if (strcmp(pathname,"") == 0 || strcmp(arg2,"") == 0){
    printf("No dir name specified\n");
    return 1;
  }

  //Save oldname
  strcpy(oldname,pathname);

  //Ino of old file/dir
  ino = getino(pathname);

  if(ino != 0)
    oldmip = iget(dev, ino);
  
  //Split arg into directory name and basename
  char *bname = basename(arg2);
  char *dname = dirname(arg2);


  //Parent INODE
  pino = getino(dname);
  if(ino != 0)
    pmip = iget(dev,pino);

  //Check if dirname is a directory
  if (!S_ISDIR(pmip->INODE.i_mode)){
    printf("Not a directory\n");
    return 1;
  }

  //Update path with new file name and create new file
  strcpy(pathname,arg2);
  if(rcreat() == 1){
    return 1;
  }

  //Get MINODE of newly created file
  fino = getino(pathname);
  if(fino != 0)
   mip = iget(dev,fino);

  //Set regular file to link file and upate info
  mip->INODE.i_mode = 0xA000;
  mip->INODE.i_block[0] = balloc(mip->dev);
  mip->INODE.i_size = getSizeofString(oldname)+1;

  //Update block with name
  put_block(mip->dev,mip->INODE.i_block[0],oldname);

  //Update parent directory of new link file
  pmip->dirty = 1;
  iput(pmip);

  return 0;
}

int readlink(){
  int ino = getino(pathname);
  int filesize;
  char buffer[BLKSIZE];
  MINODE *mip;

  if(ino != 0)
    mip = iget(dev,ino);
  
  //Check if link file
  if(mip->INODE.i_mode != 0xA000){
    printf("Not a link file\n");
    return 1;
  }

  get_block(mip->dev, mip->INODE.i_block[0], buffer);
  filesize = mip->INODE.i_size;
  printf("Buffer:%s\n",buffer);
  printf("Size: %d\n", filesize);

  //return buffer;
  return 0;

}



