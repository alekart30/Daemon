#include <syslog.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
char config_name[256];
char path1[256];
char path2[256];

void sig_handler(int signo)
{
  if(signo == SIGTERM)
  {
    syslog(LOG_INFO, "SIGTERM has been caught! Exiting...");
    if(remove("run/daemon.pid") != 0)
    {
      syslog(LOG_ERR, "Failed to remove the pid file. Error number is %d", errno);
      exit(1);
    }
    exit(0);
  }
  
  if(signo == SIGHUP){
    syslog(LOG_INFO, "SIGHUP has been caught! Exiting...");
    if(read_config(path1, path2) == 0)
    {
      syslog(LOG_ERR, "Failed to read config. There is no config");
      exit(1);
    }
    
   }
}

void handle_signals()
{
  if(signal(SIGTERM, sig_handler) == SIG_ERR)
  {
    syslog(LOG_ERR, "Error! Can't catch SIGTERM");
    exit(1);
  }
  if(signal(SIGHUP, sig_handler) == SIG_ERR)
  {
    syslog(LOG_ERR, "Error! Can't catch SIGHUP");
    exit(1);
  }
}

void daemonise()
{
  pid_t pid, sid;
  FILE *pid_fp;

  syslog(LOG_INFO, "Starting daemonisation.");

  //First fork
  pid = fork();
  if(pid < 0)
  {
    syslog(LOG_ERR, "Error occured in the first fork while daemonising. Error number is %d", errno);
    exit(1);
  }

  if(pid > 0)
  {
    syslog(LOG_INFO, "First fork successful (Parent)");
    exit(0);
  }
  syslog(LOG_INFO, "First fork successful (Child)");

  //Create a new session
  sid = setsid();
  if(sid < 0) 
  {
    syslog(LOG_ERR, "Error occured in making a new session while daemonising. Error number is %d", errno);
    exit(1);
  }
  syslog(LOG_INFO, "New session was created successfuly!");

  //Second fork
  pid = fork();
  if(pid < 0)
  {
    syslog(LOG_ERR, "Error occured in the second fork while daemonising. Error number is %d", errno);
    exit(1);
  }

  if(pid > 0)
  {
    syslog(LOG_INFO, "Second fork successful (Parent)");
    exit(0);
  }
  syslog(LOG_INFO, "Second fork successful (Child)");

  pid = getpid();

  //Change working directory to Home directory
  if(chdir(getenv("HOME")) == -1)
  {
    syslog(LOG_ERR, "Failed to change working directory while daemonising. Error number is %d", errno);
    exit(1);
  }

  //Grant all permisions for all files and directories created by the daemon
  umask(0);

  //Redirect std IO
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  if(open("/dev/null",O_RDONLY) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stdin while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(open("/dev/null",O_WRONLY) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stdout while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(open("/dev/null",O_RDWR) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stderr while daemonising. Error number is %d", errno);
    exit(1);
  }

  //Create a pid file
  mkdir("run/", 0777);
  pid_fp = fopen("run/daemon.pid", "w");
  if(pid_fp == NULL)
  {
    syslog(LOG_ERR, "Failed to create a pid file while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(fprintf(pid_fp, "%d\n", pid) < 0)
  {
    syslog(LOG_ERR, "Failed to write pid to pid file while daemonising. Error number is %d, trying to remove file", errno);
    fclose(pid_fp);
    if(remove("run/daemon.pid") != 0)
    {
      syslog(LOG_ERR, "Failed to remove pid file. Error number is %d", errno);
    }
    exit(1);
  }
  fclose(pid_fp);
}

int read_config(){
  FILE* config_fp;
  char buf[256];
  config_fp = fopen(config_name, "r");
  if(config_fp == NULL)
    return 0;
  
  fgets(path1, 255, config_fp);
  fgets(path2, 255, config_fp);
  path1[strlen(path1)-1] = '\0';
  path2[strlen(path2)-1] = '\0';
  syslog(LOG_INFO, "Config was read");
  return 1;
}

void do_task(char *dirname1, char *dirname2)
{
  struct dirent *d;
  DIR *dir;
  char buf[256];
  int len;
  int fd1, fd2;
  struct stat stbuf;

  //Remove all files in dirname2
  dir = opendir(dirname2);
  while(d = readdir(dir))
  {
    sprintf(buf, "%s/%s", dirname2, d->d_name);
    remove(buf);
    syslog(LOG_INFO, "Remove file %s in %s", d->d_name, dirname2);
  }
  closedir(dir);

  //Copy files to dirname1
  dir = opendir(dirname1);

  while(d = readdir(dir))
  {
    //Check length of filename
    len = strlen(d->d_name);
    if(len <= 3)
      continue;

    if(d->d_name[len-1] == 'k' && d->d_name[len-2] == 'b' && d->d_name[len-3] == '.')//Check for extension
    {
      sprintf(buf, "%s/%s", dirname1, d->d_name);
      fd1 = open(buf, O_RDONLY);
      if(fd1 < 0)
      {
        syslog(LOG_INFO, "Problems with reading file %s", buf);
        continue;
      }
      sprintf(buf, "%s/%s", dirname2, d->d_name);
      fd2 = open(buf, O_CREAT | O_WRONLY, 0600);
      if(fd2 < 0)
      {
        syslog(LOG_INFO, "Problems with creating file %s", buf);
        continue;
      }
      fstat(fd1, &stbuf);
      sendfile(fd2, fd1, 0, stbuf.st_size);

      close(fd1);
      close(fd2);
      syslog(LOG_INFO, "Copy file %s from %s to %s", d->d_name, dirname1, dirname2);
    }
  }

  closedir(dir);
}


int main(int argc, char **argv)
{
  FILE *pid_fp, *config_fp;
  pid_t pid;
  char buf[256];
  
  

  //Check number of args
  if(argc != 2)
  {
    printf("Incorrcect command line args\n");
    return 0;
  }

  //Start daemon
  if(!strcmp(argv[1], "start"))
  { 
    strcat(strcpy(buf, getenv("HOME")), "/run/daemon.pid");
    pid_fp = fopen(buf, "r");
    //It is first start
    if(pid_fp == NULL)
      printf("Daemon starts\n");
    //It is restart
    else
    {
       fscanf(pid_fp, "%d", &pid);
       kill(pid, SIGTERM);
       printf("Daemon restarts\n");
       fclose(pid_fp);
    }
  }
  //Stop daemon
  else if(!strcmp(argv[1], "stop"))
       {
          strcat(strcpy(buf, getenv("HOME")), "/run/daemon.pid");
          pid_fp = fopen(buf, "r");
          //No working daemon
          if(pid_fp == NULL)
          {
            printf("There is no daemon\n");
            return 0;
          }
          //Kill daemon
          else
          {
            fscanf(pid_fp, "%d", &pid);
            kill(pid, SIGTERM);
            printf("Daemon is killed\n");
            fclose(pid_fp);
            return 0;
          }
        }
  
  else
  {
    printf("Incorrect comand line args\n");
    return 0;
  }
  

  //Save absolute path to config
  getcwd(config_name, 256);
  strcat(config_name, "/config");
  
  // Tries to read config
  if(!read_config())
  {
    printf("No config\n");
    return 0;
  }
  


  
  daemonise();
  handle_signals();

  while(1)
  {
    sleep(30);
    do_task(path1, path2);
  }
  
  return 0;
}
