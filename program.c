#include <semaphore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>


const char* USAGESTR = "USAGE: ./myprogram -d <directoryName> -n <#ofthreads>\n";
const char* ERRORSTR = "ERROR: Invalid arguments.\n";

typedef struct data{
  // unique word to enter the array
  char* uniqueword;
  // files that contains this unique word
  char** files;
}data;

// words[] will hold data elements and will be filled by multiple threads
data* words;

int emptywordsindex =0;

// initial words[] size is consist of 8 elements.
int arraysize = 8;

// which folder is this program working on? this variable will be prepended to file names in each thread
char* folderName;

// all the txt file names in the directory (will act as a task queue)
char **textfiles;

// this counter will count the amount of txt files are found and will decrement when a txt file is obtained by a thread
int txtCounter = 0;

// this mutex will help us to distribute txt files to threads
pthread_mutex_t txtfilemutex= PTHREAD_MUTEX_INITIALIZER;

// this mutex will help us to write to words[] for each thread
pthread_mutex_t wordsarraymutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t filenamemutex = PTHREAD_MUTEX_INITIALIZER;
// isExist function returns index of the word if its exists on the array, if not returns -1
int isExist(char* word){
  for(int i =0; words[i].uniqueword != NULL ; i++){
    if(strcmp(words[i].uniqueword ,word) == 0){
      return i; 
    }
  }
  return -1;
}



void processfile(char* filename){
  // tokenize the file (word by word)
  // add unique elements to the data array
  // if array size is not enough; reallocate it with double of its current size
  
  // notify the user about which file is handled by which thread
  fprintf(stdout,"MAIN THREAD: Assigned \"%s\" to worker thread %ld.\n",filename,pthread_self());
  
  // buffer that contains file's content
  char *buffer =0;
  long content_length;
  char* filePath = malloc(strlen(folderName)*sizeof(char*) +strlen(filename)*sizeof(char*)+1 );
  snprintf(filePath,strlen(folderName)*sizeof(char*) +strlen(filename)*sizeof(char*) ,"%s/%s",folderName,filename);
  // doing the tokenizations and assigning unique words 
  FILE* fp = fopen(filePath,"rb");
  free(filePath);
  if(fp == NULL){
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  fseek(fp,0,SEEK_END);
  // get the content length
  content_length =ftell(fp);
  fseek(fp,0,SEEK_SET);
  buffer = malloc(content_length +1);   // +1 for null termination 
  if (buffer){
    // succesfully allocated
    fread(buffer,1,content_length,fp);
    // null terminate the buffer
    buffer[content_length] = '\0';
  }
  fclose(fp);
  if(buffer){
    // savepointer for strtok_r
    char *savepointer;
    char *token;
    int index =0  ;
    for(token = strtok_r(buffer," \n",&savepointer) ; token != NULL ; token = strtok_r(NULL," \n",&savepointer)){
      // for each token ,check if its exists on the global array 
      index = isExist(token);
      if(index != -1){
        // the token already exists in the array
        // TODO: add file name to the data's file arary
        fprintf(stdout,"The word \"%s\" has already located at index %d.\n",token,index);
        pthread_mutex_lock(&filenamemutex);
        pthread_mutex_unlock(&filenamemutex);
          continue;
      }else{
        pthread_mutex_lock(&wordsarraymutex);
        // check if the global array is full    
        if(emptywordsindex == arraysize){
          // if so then reallocate the array's size
          arraysize *= 2;
          words = realloc(words,arraysize*(sizeof(data)));
          if(words == NULL){
            perror("error while re-allocating the words array");
            exit(EXIT_FAILURE);
          }
          fprintf(stdout,"THREAD %ld: Re-allocated array of %d pointers.\n",pthread_self(),arraysize);
        }
        words[emptywordsindex].uniqueword = malloc((strlen(token) + 1 )*sizeof(char*));
        words[emptywordsindex].uniqueword[strlen(token)] = '\0';
        strcpy(words[emptywordsindex].uniqueword, token);

        printf("THREAD %ld: Added the word \"%s\" at index %d.\n",pthread_self(),token,emptywordsindex);
        emptywordsindex++;

        pthread_mutex_unlock(&wordsarraymutex);
      }
    }
  }
  free(buffer);
}

// this function will wait and get task, it also waits for a thread to complete and assign it with new file if there is any
void* threadroutine(void* args){
  while(txtCounter > 0){
    char* filename;
    pthread_mutex_lock(&txtfilemutex);
    if(txtCounter == 0){
      return NULL;
    } 
    filename = malloc(100*sizeof(char*));
    strcpy(filename,textfiles[0]);
    int i;
    for(i = 0 ; i< txtCounter -1 ; i++ ){
      strcpy(textfiles[i],textfiles[i+1]);
    }
    txtCounter--;
    pthread_mutex_unlock(&txtfilemutex);
    processfile(filename);
  }
  return NULL;
}


int main(int argc, char *argv[]){
  // check if argument amount is correct
  if(argc != 5){
    fprintf(stderr,"%s",USAGESTR);
    return 1;
  }
  // check if thread amount is positive or not
  // first convert 5th argument to integer
  // also check if flags are in position
  int threadAmount;
  threadAmount = atoi(argv[4]);
  if(threadAmount < 1 || (strcmp(argv[1],"-d") != 0)  || (strcmp(argv[3],"-n")) != 0){
    fprintf(stderr,"%s",ERRORSTR);
    return 1;
  }

  // Open the directory
  DIR* dir = opendir(argv[2]);

  if(!dir){
    perror(NULL);
    return 1;
  }
  // directory exists
  folderName = malloc((strlen(argv[2])+1)*(sizeof(char*)));
  strcpy(folderName,argv[2]);
  // null terminate the folder name
  folderName[strlen(argv[2])] = '\0';
  
  // this struct will hold directory entries
  struct dirent *ent;

  // traverse the directory
  while((ent = readdir(dir)) != NULL){
    // check if the file has correct extension (.txt)
    if(strcmp(ent->d_name + strlen(ent->d_name)-4 , ".txt") == 0){
      // a txt file found
      txtCounter++;
    }
  }
  // check if there is no txt file under the dir
  if(txtCounter == 0){
    fprintf(stderr,"There is no txt file under %s\n",folderName);
    return 1;
  }
  // holder for printing purposes
  int totalFileAmount = txtCounter;
  // rewind the directory to iterate it again
  rewinddir(dir);

  textfiles = malloc(txtCounter*sizeof(char*));
  int i =0;
  while((ent = readdir(dir)) != NULL){
    if(strcmp(ent->d_name + strlen(ent->d_name)-4 , ".txt") == 0){
      textfiles[i] = malloc((strlen(ent->d_name)+1)*sizeof(char*));
      textfiles[i][strlen(ent->d_name)] = '\0';
      strcpy(textfiles[i],ent->d_name);
      i++;
    }
  }

  // allocate inital memory for the words array
  // make the words array null initialized
  words =(data*) calloc(arraysize,(sizeof(data)));
  fprintf(stdout,"MAIN THREAD: Allocated initial array of 8 pointers.\n");

  // create array of threads
  pthread_t workers[threadAmount];
  
  for(i = 0 ; i<threadAmount; i++){
    // creating joinable thread because we will wait for them to finish
    if(pthread_create(&workers[i],NULL,&threadroutine,NULL) != 0){
      // error occured while creating the thread
      perror("error while creating thread");
      return 1;
    }
  }

  // wait for threads to complete
  for(i = 0 ; i<threadAmount; i++){
   // creating joinable thread because we will wait for them to finish
   if(pthread_join(workers[i],NULL) != 0){
     // error occured while joining the thread
     perror("error while joining thread");
     return 1;
   }
  } 

  // program has come to the end print the message
  fprintf(stdout,"MAIN THREAD: All done (succesfully read %d words with %d threads from %d files).\n",emptywordsindex,atoi(argv[4]),totalFileAmount);

  // free allocated spaces
  free(textfiles);
  free(folderName);
  free(words);
}
