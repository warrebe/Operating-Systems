/*
    # Course: CS 344
    # Author: Benjamin Warren
    # Date: 4/28/2022
    # Assignment: Assignment 2 - archive
    # Description: - pack and unpack archive files
    # Requirements:
    Recursively store files and directories into a single archive file.
    When at least one FILE is specified, create a new archive.
    If FILE is a directory, add all of its contents to the archive file, recursively.     
    When no FILE argument is specified, unpack the ARCHIVE file.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

/**
 * Like mkdir, but creates parent paths as well
 *
 * @return 0, or -1 on error, with errno set
 * @see mkdir(2)
 */
int
mkpath(const char *pathname, mode_t mode)
{
  char *tmp = malloc(strlen(pathname) + 1);
  strcpy(tmp, pathname);
  for (char *p = tmp; *p != '\0'; ++p)
  {
    if (*p == '/')
    {
      *p = '\0';
      struct stat st;
      if (stat(tmp, &st))
      {
        if (mkdir(tmp, mode))
        {
          free(tmp);
          return -1;
        }
      }
      else if (!S_ISDIR(st.st_mode))
      {
        free(tmp);
        return -1;
      }
      *p = '/';
    }
  }
  free(tmp);
  return 0;
}

/**
 * Allocates a string containing the CWD
 *
 * @return allocated string
 */
char *
getcwd_a(void)
{
  char *pwd = NULL;
  for (size_t sz = 128;; sz *= 2)
  {
    pwd = realloc(pwd, sz);
    if (getcwd(pwd, sz)) break;
    if (errno != ERANGE) err(errno, "getcwd()");
  }
  return pwd;
}


/** 
 * Packs a single file or directory recursively
 *
 * @param fn The filename to pack
 * @param outfp The file to write encoded output to
 */
void
pack(char * const fn, FILE *outfp)
{
  struct stat st;
  stat(fn, &st);
  if (S_ISDIR(st.st_mode))
  {
    long int nameLen = strlen(fn);
    if(fn[strlen(fn) - 1]!= '/'){
      nameLen += 1;
      fprintf(stderr, "Recursing `%s/'\n", fn);
      int a = fprintf(outfp, "%zu:%s/", nameLen, fn);
      if(a < 0){
        fprintf(stderr, "Error writing to file");
        exit(1);
      }
    }
    else{
      fprintf(stderr, "Recursing `%s'\n", fn);
      int a = fprintf(outfp, "%zu:%s", nameLen, fn);
      if(a < 0){
        fprintf(stderr, "Error writing to file");
        exit(1);
      }
    }
    DIR* currDir;
    if((currDir = opendir(fn)) == NULL){
      fprintf(stderr, "Could not open directory");
      exit(1);
    }
    struct dirent *aDir;
    while((aDir = readdir(currDir)) != NULL){
        if((!strcmp(aDir->d_name, ".")) || (!strcmp(aDir->d_name, ".."))){
            continue;
        }
        else{
            char* cwd = getcwd_a();
            cwd = strcat(cwd, "/");
            cwd = strcat(cwd, fn);
            chdir(cwd);
            char* rec = aDir->d_name;
            pack(rec, outfp);
            free(cwd);
        }
    }
    closedir(currDir);
    chdir("..");
    fwrite("0:", sizeof(char), 2, outfp);
  }
  else if (S_ISREG(st.st_mode))
  {
    long int len = strlen(fn);
    fprintf(stderr, "Packing `%s'\n", fn);
    FILE* fn1;
    if((fn1 = fopen(fn, "r")) == NULL){
      fprintf(stderr, "Could not open file");
      exit(1);
    }
    char c[1];
    int index = 1;
    char *message = calloc(sizeof(char), 1);
    while(!feof(fn1)){
        size_t i = fread(c, sizeof(char), 1, fn1);
        if(i == 0){
          break;
        }
        message = realloc(message, (index+1)*sizeof(char));
        message[index - 1] = c[0];
        ++index;
    }
    message[index - 1] = 0;
    fclose(fn1);
    if (st.st_size == 0){
      int a = fprintf(outfp, "%zu:%s%ld:", len, fn, st.st_size);
      if(a < 0){
        fprintf(stderr, "Error writing to file");
        exit(1);
      }
    }
    else{
      int a = fprintf(outfp, "%zu:%s%ld:%s", len, fn, st.st_size, message);
      if(a < 0){
        fprintf(stderr, "Error writing to file");
        exit(1);
      }
    }
    free(message);
  }
  else
  {
    fprintf(stderr, "Skipping non-regular file `%s'.\n", fn);
  }
}

/**
 * Unpacks an entire archive
 *
 * @param fp The archive to unpack
 */
int
unpack(FILE *fp, const char* fn)
{
  /* If file is 0 - indicates EOD */
  if(!strcmp(fn, "0")){
    chdir("..");
    char* temp = calloc(sizeof(char), 1);
    int i = 1;
    char c[1] = {'a'};
    while(c[0] != ':'){
      temp = realloc(temp, i*sizeof(char));
      size_t j = fread(c, sizeof(char), 1, fp);
      if(j < sizeof(char)){
          // END OF FILE
          free(temp);
          return 0;
      }
      temp[i - 1] = c[0];
      ++i;
    }
    temp[i - 2] = 0;
    int fileNameLength = atoi(temp);
    if(fileNameLength == 0){
        unpack(fp, "0");
    }
    else{
        char fileName[fileNameLength];
        size_t j = fread(fileName, sizeof(char), fileNameLength, fp);
        if(j < sizeof(char)){
          // END OF FILE
          free(temp);
          return 0;
        }
        fileName[fileNameLength] = 0;
        unpack(fp, fileName);
    }
    free(temp);
  }
  else if (fn[strlen(fn)-1] == '/')
  {
    /* If file ends with '/' indicates it is a directory, mkpath() */
    if (mkpath(fn, 0700)) err(errno, "mkpath()");
    fprintf(stderr, "Recursing into `%s'\n", fn);
    char* temp = calloc(sizeof(char), 1);
    int i = 1;
    char c[1] = {'a'};
    while(c[0] != ':'){
      temp = realloc(temp, i*sizeof(char));
      size_t j = fread(c, sizeof(char), 1, fp);
      if(j < sizeof(char)){
          // END OF FILE
          free(temp);
          return 0;
      }
      temp[i - 1] = c[0];
      ++i;
    }
    temp[i - 2] = 0;                               
    int fileNameLength = atoi(temp);
    if(fileNameLength == 0){
        unpack(fp, "0");
    }
    else{
        char fileName[fileNameLength];
        size_t j = fread(fileName, sizeof(char), fileNameLength, fp);
        if(j < sizeof(char)){
          // END OF FILE
          free(temp);
          return 0;
        }
        fileName[fileNameLength] = 0;
        chdir(fn);
        unpack(fp, fileName);
    }
    free(temp);
  }
  else
  {
    /* Regular file for unpacking */
    fprintf(stderr, "Unpacking file %s\n", fn);
    FILE* fileRead;
    if((fileRead = fopen(fn, "r"))){
      /* If the file is the argument passed to the executable aka the first file */
      fclose(fileRead);
      char* temp = calloc(sizeof(char), 1);
      int i = 1;
      char c[1] = {'0'};
      while(c[0] != ':'){
        temp = realloc(temp, i*sizeof(char));
        size_t j = fread(c, sizeof(char), 1, fp);
        if(j < sizeof(char)){
          // END OF FILE
          free(temp);
          return 0;
        }
        temp[i - 1] = c[0];
        ++i;
      }
      temp[i - 2] = 0;                                           
      int fileNameLength = atoi(temp);
      free(temp);
      char fileName[fileNameLength];
      size_t j = fread(fileName, sizeof(char), fileNameLength, fp);
      if(j < sizeof(char)){
          // END OF FILE
          return 0;
      }
      fileName[fileNameLength] = 0;
      unpack(fp, fileName);
    }
    else{
      /* Not the first file, create regular file */
      char* temp = calloc(sizeof(char), 1);
      int i = 1;
      char c[1] = {'0'};
      while(c[0] != ':'){
        temp = realloc(temp, i*sizeof(char));
        size_t j = fread(c, sizeof(char), 1, fp);
        if(j < sizeof(char)){
            // END OF FILE
            free(temp);
            return 0;
        }
        temp[i - 1] = c[0];
        ++i;
      }
      temp[i - 2] = 0;                                          
      int fileLength = atoi(temp);
      if(fileLength == 0){
          // File has no content
          FILE* newFile = fopen(fn, "w");
          if(newFile == NULL){
            fprintf(stderr, "Could not create new file");
            exit(1);
          }
          fclose(newFile);
      }
      else{
        char fileMessage[fileLength];
        size_t j = fread(fileMessage, sizeof(char), fileLength, fp);
        if(j < sizeof(char)){
            // END OF FILE
            free(temp);
            return 0;
        }
        fileMessage[fileLength] = 0;
        FILE* newFile = fopen(fn, "w");
        if(newFile == NULL){
          fprintf(stderr, "Could not create new file");
          exit(1);
        }
        fwrite(fileMessage, sizeof(char), fileLength, newFile);
        fclose(newFile);
      }
      free(temp);
      temp = calloc(sizeof(char), 1);
      i = 1;
      c[0] = 'a';
      while(c[0] != ':' && !(feof(fp))){
        temp = realloc(temp, i*sizeof(char));
        size_t j = fread(c, sizeof(char), 1, fp);
        if(j < sizeof(char)){
            // END OF FILE
            free(temp);
            return 0;
        }
        temp[i - 1] = c[0];
        ++i;
      }
      temp[i - 2] = 0;                              
      int fileNameLength = atoi(temp);
      if(temp == NULL){
        // END
      }
      else if(fileNameLength == 0){
          // Pass 0 argument in recursion indicating End of Directory
          unpack(fp, "0");
      }
      else{
          // Recursively pass next file to be unpacked
          char fileName[fileNameLength];
          size_t j = fread(fileName, sizeof(char), fileNameLength, fp);
          if(j < sizeof(char)){
              // END OF FILE
              free(temp);
              return 0;
          }
          fileName[fileNameLength] = 0;
          unpack(fp, fileName);
      }
      free(temp);
    }
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s FILE... OUTFILE\n"
                    "       %s INFILE\n", argv[0], argv[0]);
    exit(1);
  }
  char *fn = argv[argc-1];
  if (argc > 2)
  { /* Packing files */
    FILE *fp = fopen(fn, "w");
    if(fp == NULL){
      fprintf(stderr, "Could not create file for packing");
      exit(1);
    }
    for (int argind = 1; argind < argc - 1; ++argind)
    {
        pack(argv[argind], fp);
    }
    fclose(fp);
  }
  else
  { /* Unpacking an archive file */
    FILE *fp = fopen(fn, "r");
    if(fp == NULL){
      fprintf(stderr, "Unable to open file to unpack\n");
      exit(1);
    }
    unpack(fp, fn);
    fclose(fp);
  }
}