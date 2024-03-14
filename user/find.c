#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int path_match(char *path, char *match)
{
  char *p;
  for(p = path + strlen(path); p >= path && *p != '/'; p--) ;
  p++;
  return strcmp(p, match) == 0 ? 1 : 0;
}

void find(char *path, char *target)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
      fprintf(2, "find: cannot open %s\n", path);
      return;
  }

  if(fstat(fd, &st) < 0){
      fprintf(2, "find: cannot stat %s\n", path);
      close(fd);
      return;
  }

  switch(st.type) {
    case T_DEVICE:
    case T_FILE:
      if (path_match(path, target))
        fprintf(1, "%s\n", path, target);
      break;
    case T_DIR:
      if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
          printf("ls: path too long\n");
          break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while(read(fd, &de, sizeof(de)) == sizeof(de)) {
        if(de.inum == 0)
          continue;

        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
          continue;

        int namelen = strlen(de.name);
        memmove(p, de.name, namelen);
        p[namelen] = 0;

        find(buf, target); 
      }
      break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
    if (argc == 2)
    {
      find(".", argv[1]);
      exit(0);
    } else if (argc == 3)
    {
      find(argv[1], argv[2]);
      exit(0);
    } else {
      fprintf(2, "usage: find [path] [filename]");
      exit(1);
    }
    
}