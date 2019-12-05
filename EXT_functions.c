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
  printf("RWPD INO:%d\n", myino);

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


int closeFile(int fd){
  //int fd = findFdByName(filename);
  if(fd == -1){
    printf("Which file to close? (give fd)\n");
    scanf("%d",&fd);
  }
  // //clear stdin buffer
  // while ((getchar()) != '\n'); 

  MINODE *mip;
  if ((fd>8)||(fd<0)){
    printf("Error: Invalid file descriptor \n");
    return 1;
  }

  if (running->fd[fd]==NULL)
	{
		printf("Error: File descriptor not found \n");
		return 1;
	}

  OFT* oftp = running->fd[fd];
  running->fd[fd] = 0;
  oftp->refCount--;
  
  if (oftp->refCount > 0)
		return 0;

  //No more refrences of minode
  mip = oftp->mptr;
  iput(mip);
  
  return 0;
}

//Print open file descriptor
int pfd(){
  int i;
  printf("\n---------------------------------------\n");
	printf("FD\tmode\toffset\tINODE\n");
  
	for(i = 0;i<8;i++)
	{
		if (running->fd[i]!= 0)
		{ 
			printf("%d\t",i);
			switch(running->fd[i]->mode)
			{
				case 0:
					printf("READ\t");
					break;
				case 1:
					printf("WRITE\t");
					break;
				case 2:
					printf("R/W\t");
					break;
				case 3:
					printf("APPEND\t");
					break;
				default:
					printf("Error\t");
					break;
			}
			printf("%d\t",running->fd[i]->offset);
      printf("[%d, %d]\n",running->fd[i]->mptr->dev, running->fd[i]->mptr->ino);

		}
	}
	return 0;
}

int openFile(char *filename, int mode){
  int dev, ino, index;
  OFT *fp;
  MINODE *mip;

  if(mode == -1){
    pfd();
    printf("What mode to open file with? |0(R)|1(W)|2(RW)|3(Append)|\n");
    scanf("%d",&mode);
  }

  // //Clear stdin buffer
  // while ((getchar()) != '\n'); 

  printf("mode: %d\n", mode);
  printf("Letter:%c\n",filename[0]);

  if(filename[0] == '\0'){
    printf("No filename given\n");
    return 1;
  }


  if(filename[0] == '/')
    dev = root->dev;
  else
    dev = running->cwd->dev;

  ino = getino(filename);


  //TODO: if ino is 0 then create new file with given pathname and mode with
  if(ino == 0){
    printf("Error: Could not find inode of given name\n");
    return 1;
  }
  
  mip = iget(dev,ino);

  if (!S_ISREG(mip->INODE.i_mode)){
    printf("Not a regular file\n");
    return 1;
  }


  if(isFileOpen(mip) == 1)
    return 1;

  index = falloc();

  if(index == -1){
    printf("No aviable spots in process\n");
    return 1;
  }

  fp = malloc(sizeof(OFT));

  fp->mode = mode;
  fp->refCount = 1;
  fp->mptr = mip;
  
  switch (mode)
  {
    case 0: fp->offset = 0; //Read
            mip->INODE.i_atime = time(0L);
            break;
    case 1: truncate(mip); //Write
            fp->offset = 0; 
            mip->INODE.i_mtime = time(0L);
            break;
    case 2: fp->offset = 0; //ReadWrite
            mip->INODE.i_mtime = time(0L);
            break;
    case 3: fp->offset = mip->INODE.i_size; //Append
            mip->INODE.i_mtime = time(0L);
            break;
    default: printf("Invalid mode\n");
             return 1;
  }

  running->fd[index] = fp;
  mip->dirty = 1;
  iput(mip);
  return index;
} 

int read_file(int fd ,char *buf){
  // int nbytes, fd;

  // printf("Enter fd\n");
  // scanf("&d", &fd);

  // printf("Enter bytes to be readd\n");
  // scanf("&d", &nbytes);

  int nbytes = BLKSIZE;

  if ((fd>7)||(fd<0))
	{
		printf("Invalid file descriptor\n");
		return 1;
	}

  if(running->fd[fd]->mode != 0 &&running->fd[fd]->mode!=2){
   	printf("File is not open for read\n");
		return 1; 
  }

  return(myread(fd, buf, nbytes));

}

int write_file(){
  int fd, size;
  char string[255];

  //Print open FDs
  pfd();
  printf("Enter fd\n");
  scanf("%d", &fd);

  while ((getchar()) != '\n'); 

  //TODO: Why is this printing "seg" sometimes
  printf("Enter text to write to file\n");
  // scanf("%s", string);
  fgets(string, 5, stdin);

  printf("FD NUMBER: %d\n", fd);
  if ((fd>7)||(fd<0))
	{
		printf("Invalid file descriptor\n");
		return 1;
	}

  if(running->fd[fd]->mode != 1 &&running->fd[fd]->mode!=2){
   	printf("File is not open for write\n");
		return 1; 
  }

  size = strlen(string);
  printf("Size num: %d\n", size);
  printf("Size num: %s\n", string);

  // printf("%s\n",string);
  return(mywrite(fd, string, size));
} 

int cat(char *filename){
  char buf[255];
  int n;
  int fd;

  
  //Check if filename is empty
  if(filename[0] == 0){
    printf("Enter name of file\n");
    scanf("%s", filename);

    if(filename[0] == 0){
      printf("Filename can not be empty\n");
      return 1;
    }

  }

  fd = openFile(filename, 0);

  while(n = read_file(fd, buf)){
    buf[n] = 0;
    printf("\nOutput: %s\n", buf);

    // for(int i = 0; i<n; i++){
    //   printf("%c", globalreadbuf[i]);
    // }
  }
  closeFile(fd);
  return 0;
}

