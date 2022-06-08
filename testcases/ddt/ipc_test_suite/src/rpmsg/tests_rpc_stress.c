/*
 * Copyright (c) 2013-2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * tests_rpc_stress.c
 *
 * Stress tests for rpmsg-rpc.
 *
 */

#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <semaphore.h>

#include <ctype.h>
#include <sys/ioctl.h>
#include "rpmsg_rpc.h"

typedef struct {
    int a;
} fxn_double_args;


typedef struct {
  void *next;
  int idx;
  char *name;
} function_info;

/* Note: Set bit 31 to indicate static function indicies:
 * This function order will be hardcoded on BIOS side, hence preconfigured:
 */
#define NUM_ITERATIONS               5000

#define max(a, b)  ((a) > (b) ? (a) : (b))

#define SYSFS_PATH "/sys/class/rpmsg_rpc/"
#define NUM_FILE "/num_funcs"
#define FUNC_BASE "/c_function"
#define NODE_BASE "rpc_example_"
#define DEV_NODE_BASE "/dev/"
#define PATH_MAX (100)
#define NODES_CT (10)
#define NODES_MAX_LENGTH (20)

static int test_status = 0;
static bool runTest = true;
char nodes[NODES_CT][NODES_MAX_LENGTH];

int get_nodes(void)
{
    int fd;
    int i;
    int count=0;
    char node_name[NODES_MAX_LENGTH];

    for (i=0; i< NODES_CT; i++) {
        sprintf(node_name, "%s%s%d", DEV_NODE_BASE, NODE_BASE, i + 1);
        fd = open(node_name, O_RDWR);
        if (fd > 0) {
            close(fd);
            sprintf(nodes[count], "%s%d", NODE_BASE, i + 1);
            count++;
        }
    }

    return count;
}

int get_functions_info(int core_id, function_info** func_arr)
{
    int fd,ffd;
    ssize_t b_read;
    long fsize;
    char *num_fns;
    int n_funcs;
    char file_path[PATH_MAX];
    char func_base[PATH_MAX];
    char *func_name;
    char sysfs_path[PATH_MAX];
    char current_f[8];
    function_info *func_inf = NULL;
    function_info *c_func;
    int idx;

    errno = 0;

    sprintf(sysfs_path, "%s%s", SYSFS_PATH, nodes[core_id]);
    sprintf(file_path, "%s%s", sysfs_path, NUM_FILE);
    sprintf(func_base, "%s%s", sysfs_path, FUNC_BASE);

    fd = open(file_path, O_RDONLY);
    if (fd > 0) {
        fsize = lseek(fd, 0L, SEEK_END);
        lseek(fd, 0L, SEEK_SET);
        num_fns = (char *)malloc(fsize);
        b_read = read(fd, num_fns, fsize);
        if(b_read < 1) {
            perror("Unable to read num_funcs");
            free(num_fns);
            return -1;
        }
        n_funcs = atoi(num_fns);
        free(num_fns);
        for (idx = 0; idx < n_funcs; idx++) {
            sprintf(current_f, "%d", idx + 1);
            func_name = (char *)calloc(sizeof(char),strlen(func_base)+strlen(current_f)+1);
            sprintf(func_name, "%s%s", func_base, current_f);
            ffd = open(func_name, O_RDONLY);
            if (ffd < 1) {
              goto func_problem;
            }
            fsize = lseek(ffd, 0L, SEEK_END);
            lseek(ffd, 0L, SEEK_SET);
            c_func = (function_info *)malloc(sizeof(function_info));
            c_func->idx = idx;
            c_func->name = (char *)malloc(fsize);
            read(ffd, c_func->name, fsize);
            c_func->next = func_inf;
            func_inf = c_func;
            close(ffd);
            free(func_name);
        }
    }

    *func_arr = func_inf;

    func_problem:
        close(fd);

      return errno;
}

int get_function(int core_id, char *f_name, function_info* result)
{
    function_info *f_list;
    function_info *iter;
    char func_name[PATH_MAX];
    char *cf_name;
    char *f_iter;

    errno = 0;

    sprintf(func_name, "%s", f_name);
    f_iter = func_name;
    for (; *f_iter; ++f_iter) {
        *f_iter = tolower(*f_iter);
    }

    if (get_functions_info(core_id, &f_list)) {
        printf("Unable to get function information from %d for %s\n", core_id, f_name);
        return errno;
    }

    result->idx = -1;
    iter=f_list;
    while (iter) {
        f_list=iter;
        cf_name = (char *)malloc(strlen(iter->name)+1);
        sprintf(cf_name, "%s", iter->name);
        f_iter = cf_name;
        for (; *f_iter; ++f_iter) {
            *f_iter=tolower(*f_iter);
        }
        if (strstr(cf_name, func_name)) {
            result->idx = iter->idx;
            result->name=(char *)malloc(strlen(iter->name)+1);
            sprintf(result->name, "%s", iter->name);
        }
        iter=iter->next;
        free(cf_name);
        free(f_list);
    }

    return errno;
}

int connect_to_proc(int core_id, char *name)
{
    char node_name[PATH_MAX];
    int fd;

    sprintf(node_name, "%s%s", DEV_NODE_BASE, nodes[core_id]);

    fd = open(node_name, O_RDWR);
    if (fd < 0) {
        perror("Can't open rpc_example device");
    }
    else {
        sprintf(name, "%s", nodes[core_id]);
    }

    return fd;
}

int exec_cmd(int fd, char *msg, size_t len, char *reply_msg, int *reply_len)
{
    int ret = 0;

    ret = write(fd, msg, len);
    if (ret < 0) {
        perror("Can't write to rpc_example instance");
        return -1;
    }

    /* Now, await normal function result from rpc_example service:
     * Note: len should be exact length of response expected.
     */
    ret = read(fd, reply_msg, sizeof(struct rppc_function_return));
    if (ret < 0) {
        perror("Can't read from rpc_example instance");
        return errno;
    }
    else {
        *reply_len = ret;
    }
    return 0;
}

int send_cmd(int fd, char *msg, int len)
{
    int ret = 0;

    ret = write(fd, msg, len);
    if (ret < 0) {
         perror("Can't write to rpc_example instance\n");
         return -1;
    }

    return(0);
}

int recv_cmd(int fd, int len, char *reply_msg, int *reply_len)
{
    int ret = 0;

    /* Now, await normal function result from rpc_example service: */
    // Note: len should be max length of response expected.
    ret = read(fd, reply_msg, len);
    if (ret < 0) {
         perror("Can't read from rpc_example instance\n");
         return errno;
    }
    else {
          *reply_len = ret;
    }
    return(0);
}

typedef struct test_exec_args {
    int fd;
    int start_num;
    int test_num;
    sem_t * sem;
    int thread_num;
} test_exec_args;

typedef struct test_read_args {
    int fd;
    int num_threads;
} test_read_args;

static pthread_t * clientThreads = NULL;
static sem_t * clientSems = NULL;
static char **clientPackets = NULL;
static bool *readMsg = NULL;
static int *fds;
static function_info func_inf;
static function_info fault_func_inf;
int flt_msg_num=NUM_ITERATIONS;
static char *reset_cmd = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void * test_exec_call(void * arg)
{
    int               i;
    int               packet_len;
    int               reply_len;
    char              packet_buf[512] = {0};
    char              return_buf[512] = {0};
    test_exec_args    *args = (test_exec_args *) arg;
    int               fd = args->fd;
    struct rppc_function *function;
    struct rppc_function_return *returned;

    for (i = args->start_num; i < args->start_num + NUM_ITERATIONS; i++) {
        if(reset_cmd != NULL && i-args->start_num >= flt_msg_num) {
            pthread_mutex_lock(&lock);
            if(runTest) {
                sleep(3);
                runTest = false;
                system(reset_cmd);
                sleep(3);
            }
            pthread_mutex_unlock(&lock);
            break;
        }
        function = (struct rppc_function *)packet_buf;
        function->fxn_id = i - args->start_num != flt_msg_num ? func_inf.idx : fault_func_inf.idx;
        function->num_params = 1; //hard coded at the moment, should be based on func_inf->name
        function->params[0].type = RPPC_PARAM_TYPE_ATOMIC;
        function->params[0].size = sizeof(int);
        function->params[0].data = i;
        function->num_translations = 0;

        returned = (struct rppc_function_return *)return_buf;

        /* Exec command: */
        packet_len = sizeof(struct rppc_function) +\
                     (function->num_translations * \
                      sizeof(struct rppc_param_translation));
        if (args->test_num == 1)  {
            if (exec_cmd(fd, (char *)packet_buf, packet_len,
                         (char *)return_buf, &reply_len)) {
                test_status = -1;
                break;
            }
        }
        else if (args->test_num == 2 || args->test_num == 3) {
            if (send_cmd(fd, (char *)packet_buf, packet_len)) {
                test_status = -1;
                break;
            }
            sem_wait(&clientSems[args->thread_num]);
            memcpy(return_buf, clientPackets[args->thread_num], 512);
            readMsg[args->thread_num] = true;
        }
       if (i * 3 != returned->status) {
           printf ("rpc_stress: "
                   "called fxn (%d), result = %d, expected %d\n",
                        function->params[0].data, returned->status, i * 3);
           test_status = -1;
           break;
       }
       else {
           /* printf ("rpc_stress: called fxnTriple(%d), result = %d\n",
                        function->params[0].data, returned->status); */
           printf (".");
       }
    }

    return NULL;
}

void * test_select_thread (void * arg)
{
    int fd = -1;
    int reply_len;
    char return_buf[512] = {0};
    struct rppc_function_return *rtn_packet =
                               (struct rppc_function_return *)return_buf;
    int n, i;
    fd_set rfd;
    int max_fd = -1;
    int ret;
    int sem_val;

    while (runTest) {
        FD_ZERO(&rfd);
        for (i = 0; i < (int)arg; i++) {
            FD_SET(fds[i], &rfd);
            max_fd = max(max_fd, fds[i]);
        }
        n = select(1 + max_fd, &rfd, NULL, NULL, NULL);
        switch (n) {
            case -1:
                perror("select");
                return NULL;
            default:
                for (i = 0; i < (int)arg; i++) {
                    if (FD_ISSET(fds[i], &rfd)) {
                        fd = fds[i];
                        break;
                    }
                }
                break;
        }

        ret = recv_cmd(fd, sizeof(*rtn_packet), (char *)rtn_packet, &reply_len);

        if(!ret && !rtn_packet->status) {
            for (i = 0; i < (int)arg; i++) {
                sem_getvalue(&clientSems[i], &sem_val);
                if(sem_val < 1)sem_post(&clientSems[i]);
            }
            continue;
        }

        if(ret == ENXIO)
        {
          for (i = 0; i < (int)arg; i++) {
            sem_post(&clientSems[i]);
          }
          break;
        }
        else if (ret) {
            test_status = -1;
            printf("test_select_thread: recv_cmd failed!");
            break;
        }

        if (runTest == false)
            break;

        /* Decode reply: */
        while (readMsg[i] == false) {
            sleep(1);
        }
        memcpy(clientPackets[i], rtn_packet, 512);
        readMsg[i] = false;
        sem_post(&clientSems[i]);
    }
    return NULL;
}

void * test_read_thread (void * arg)
{
    test_read_args *read_info = (test_read_args *)arg;
    int fd = read_info->fd;
    int reply_len;
    char  return_buf[512] = {0};
    int i;
    struct rppc_function_return *rtn_packet =
                               (struct rppc_function_return *)return_buf;
    int               packet_id;
    int ret;
    int sem_val;

    while (runTest) {
        ret = recv_cmd(fd, sizeof(*rtn_packet), (char *)rtn_packet, &reply_len);

        if(!ret && !rtn_packet->status) {
            for (i = 0; i< read_info->num_threads; i++) {
                sem_getvalue(&clientSems[i], &sem_val);
                if(sem_val < 1)sem_post(&clientSems[i]);
            }
            continue;
        }

        if (ret == ENXIO) {
            for (i = 0; i< read_info->num_threads; i++) {
                sem_post(&clientSems[i]);
            }
            break;
        }
        else if (ret) {
            test_status = -1;
            printf("test_read_tread: recv_cmd failed!");
            break;
        }

        if (runTest == false)
            break;

        /* Decode reply: */
        packet_id = ((rtn_packet->status / 3) - 1) / NUM_ITERATIONS;
        while (readMsg[packet_id] == false) {
            sleep(1);
        }
        memcpy(clientPackets[packet_id], rtn_packet, 512);
        readMsg[packet_id] = false;
        sem_post(&clientSems[packet_id]);
    }
    return NULL;
}

int test_rpc_stress_select(int core_id, int num_comps)
{
    int ret = 0;
    int i = 0, j = 0;
    struct rppc_create_instance connreq;
    struct rppc_function *function;
    pthread_t select_thread;
    int               packet_len;
    char              packet_buf[512] = {0};
    test_exec_args args[num_comps];

    fds = malloc (sizeof(int) * num_comps);
    if (!fds) {
        return -1;
    }
    for (i = 0; i < num_comps; i++) {
        /* Connect to the rpc_example ServiceMgr on the specified core: */
        fds[i] = connect_to_proc(core_id, connreq.name);
        if (fds[i] < 0) {
            perror("Can't open rpc_example device");
            ret = -1;
            break;
        }

        /* Create an rpc_example server instance, and rebind its address to this
        * file descriptor.
        */
        ret = ioctl(fds[i], RPPC_IOC_CREATE, &connreq);
        if (ret < 0) {
            perror("Can't connect to rpc_example instance");
            close(fds[i]);
            break;
        }
        printf("rpc_sample: Connected to %s\n", connreq.name);
    }
    if (i != num_comps) {
        /* cleanup */
        for (j = 0; j < i; j++) {
            ret = close(fds[j]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        free(fds);
        return -1;
    }

    clientSems = malloc(sizeof(sem_t) * num_comps);
    if (!clientSems) {
        free(clientThreads);
        for (i = 0; i < num_comps; i++) {
            ret = close(fds[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    readMsg = malloc(sizeof(bool) * num_comps);
    if (!readMsg) {
        free(clientSems);
        free(clientThreads);
        for (i = 0; i < num_comps; i++) {
            ret = close(fds[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    clientPackets = malloc(sizeof(char *) * num_comps);
    if (!clientPackets) {
        free(readMsg);
        free(clientSems);
        free(clientThreads);
        for (i = 0; i < num_comps; i++) {
            ret = close(fds[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    for (i = 0; i < num_comps; i++) {
        clientPackets[i] = malloc(512 * sizeof(char));
        if (!clientPackets[i]) {
            for (j = 0; j < i; j++) {
                free(clientPackets[j]);
            }
            free(clientPackets);
            free(readMsg);
            free(clientSems);
            free(clientThreads);
            for (i = 0; i < num_comps; i++) {
                ret = close(fds[i]);
                if (ret < 0) {
                    perror("Can't close rpc_example fd ??");
                }
            }
            return -1;
        }
    }

    ret = pthread_create(&select_thread, NULL, test_select_thread,
                         (void *)num_comps);
    if (ret < 0) {
        perror("Can't create thread");
        ret = -1;
    }

    clientThreads = malloc(sizeof(pthread_t) * num_comps);
    if (!clientThreads) {
        for (i = 0; i < num_comps; i++) {
            ret = close(fds[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        free(fds);
        return -1;
    }

    for ( i = 0; i < num_comps; i++) {
        ret = sem_init(&clientSems[i], 0, 0);
        args[i].fd = fds[i];
        args[i].start_num = 1;
        args[i].test_num = 3;
        args[i].sem = &clientSems[i];
        args[i].thread_num = i;
        readMsg[i] = true;
        ret = pthread_create(&clientThreads[i], NULL, test_exec_call,
                             (void *)&args[i]);
        if (ret < 0) {
            perror("Can't create thread");
            ret = -1;
            break;
        }
        printf("Created thread %d\n", i);
    }

    for (j = 0; j < i; j++) {
        printf("Join thread %d\n", j);
        pthread_join(clientThreads[j], NULL);
    }

    free(clientThreads);

    function = (struct rppc_function *)packet_buf;
    function->fxn_id = func_inf.idx;
    function->num_params = 1;
    function->params[0].type = RPPC_PARAM_TYPE_ATOMIC;
    function->params[0].size = sizeof(int);
    function->params[0].data = i;
    function->num_translations = 0;

    /* Exec command: */
    packet_len = sizeof(struct rppc_function) +\
                 (function->num_translations *\
                  sizeof(struct rppc_param_translation));

    runTest = false;
    if (send_cmd(fds[0], (char *)packet_buf, packet_len)) {
        test_status = -1;
    }

    pthread_join(select_thread, NULL);

    for (i = 0; i < num_comps; i++) {
        free(clientPackets[i]);
    }
    free(clientPackets);
    free(readMsg);
    free(clientSems);

    for (i = 0; i < num_comps; i++) {
        /* Terminate connection and destroy rpc_example instance */
        ret = close(fds[i]);
        if (ret < 0) {
            perror("Can't close rpc_example fd ??");
            ret = -1;
        }
        printf("rpc_sample: Closed connection to %s!\n", connreq.name);
    }

    free(fds);
    return ret;
}

int test_rpc_stress_multi_threads(int core_id, int num_threads)
{
    int ret = 0;
    int i = 0, j = 0;
    int fd;
    int packet_len;
    char packet_buf[512] = {0};
    pthread_t read_thread;
    struct rppc_create_instance connreq;
    struct rppc_function *function;
    test_exec_args args[num_threads];
    test_read_args read_args;

    /* Connect to the rpc_example ServiceMgr on the specified core: */
    fd = connect_to_proc(core_id, connreq.name);
    if (fd < 0) {
        perror("Can't open rpc_example device");
        return -1;
    }
    /* Create an rpc_example server instance, and rebind its address to this
    * file descriptor.
    */
    ret = ioctl(fd, RPPC_IOC_CREATE, &connreq);
    if (ret < 0) {
        perror("Can't connect to rpc_example instance");
        close(fd);
        return -1;
    }
    printf("rpc_sample: Connected to %s\n", connreq.name);

    clientThreads = malloc(sizeof(pthread_t) * num_threads);
    if (!clientThreads) {
        ret = close(fd);
        return -1;
    }
    clientSems = malloc(sizeof(sem_t) * num_threads);
    if (!clientSems) {
        free(clientThreads);
        ret = close(fd);
        return -1;
    }

    readMsg = malloc(sizeof(bool) * num_threads);
    if (!readMsg) {
        free(clientSems);
        free(clientThreads);
        ret = close(fd);
        return -1;
    }

    clientPackets = malloc(sizeof(char *) * num_threads);
    if (!clientPackets) {
        free(readMsg);
        free(clientSems);
        free(clientThreads);
        ret = close(fd);
        return -1;
    }

    for (i = 0; i < num_threads; i++) {
        clientPackets[i] = malloc(512 * sizeof(char));
        if (!clientPackets[i]) {
            for (j = 0; j < i; j++) {
                free(clientPackets[j]);
            }
            free(clientPackets);
            free(readMsg);
            free(clientSems);
            free(clientThreads);
            close(fd);
            return -1;
        }
    }
    read_args.fd = fd;
    read_args.num_threads = num_threads;
    ret = pthread_create(&read_thread, NULL, test_read_thread, (void *)&read_args);
    if (ret < 0) {
        perror("Can't create thread");
        for (i = 0; i < num_threads; i++) {
            free(clientPackets[i]);
        }
        free(clientPackets);
        free(readMsg);
        free(clientSems);
        free(clientThreads);
        close(fd);
        return -1;
    }

    for ( i = 0; i < num_threads; i++) {
        ret = sem_init(&clientSems[i], 0, 0);
        args[i].fd = fd;
        args[i].start_num = (i * NUM_ITERATIONS) + 1;
        args[i].test_num = 2;
        args[i].sem = &clientSems[i];
        args[i].thread_num = i;
        readMsg[i] = true;
        ret = pthread_create(&clientThreads[i], NULL, test_exec_call,
                             (void *)&args[i]);
        if (ret < 0) {
            perror("Can't create thread");
            ret = -1;
            break;
        }
        printf("Created thread %d\n", i);
    }

    for (j = 0; j < i; j++) {
        printf("Join thread %d\n", j);
        pthread_join(clientThreads[j], NULL);
        sem_destroy(&clientSems[j]);
    }

    function = (struct rppc_function *)packet_buf;
    function->fxn_id = func_inf.idx;
    function->num_params = 1;
    function->params[0].type = RPPC_PARAM_TYPE_ATOMIC;
    function->params[0].size = sizeof(int);
    function->params[0].data = i;
    function->num_translations = 0;

    /* Exec command: */
    packet_len = sizeof(struct rppc_function) +\
                 (function->num_translations *\
                  sizeof(struct rppc_param_translation));

    runTest = false;
    if (send_cmd(fd, (char *)packet_buf, packet_len)) {
        test_status = -1;
    }

    pthread_join(read_thread, NULL);

    for (i = 0; i < num_threads; i++) {
        free(clientPackets[i]);
    }
    free(clientPackets);
    free(readMsg);
    free(clientSems);
    free(clientThreads);

    /* Terminate connection and destroy rpc_example instance */
    ret = close(fd);
    if (ret < 0) {
        perror("Can't close rpc_example fd ??");
        ret = -1;
    }
    printf("rpc_sample: Closed connection to %s!\n", connreq.name);

    return ret;
}

int test_rpc_stress_multi_srvmgr(int core_id, int num_comps)
{
    int ret = 0;
    int i = 0, j = 0;
    int fd[num_comps];
    struct rppc_create_instance connreq;
    test_exec_args args[num_comps];

    for (i = 0; i < num_comps; i++) {
        /* Connect to the rpc_example ServiceMgr on the specified core: */
        fd[i] = connect_to_proc(core_id, connreq.name);
        if (fd[i] < 0) {
            perror("Can't open rpc_example device");
            ret = -1;
            break;
        }
        /* Create an rpc_example server instance, and rebind its address to this
        * file descriptor.
        */
        ret = ioctl(fd[i], RPPC_IOC_CREATE, &connreq);
        if (ret < 0) {
            perror("Can't connect to rpc_example instance");
            close(fd[i]);
            break;
        }
        printf("rpc_sample: Connected to %s\n", connreq.name);
    }
    if (i != num_comps) {
        /* cleanup */
        for (j = 0; j < i; j++) {
            ret = close(fd[j]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    clientThreads = malloc(sizeof(pthread_t) * num_comps);
    if (!clientThreads) {
        for (i = 0; i < num_comps; i++) {
            ret = close(fd[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    for ( i = 0; i < num_comps; i++) {
        args[i].fd = fd[i];
        args[i].start_num = 1;
        args[i].test_num = 1;
        args[i].sem = NULL;
        args[i].thread_num = i;
        ret = pthread_create(&clientThreads[i], NULL, test_exec_call,
                             (void *)&args[i]);
        if (ret < 0) {
            perror("Can't create thread");
            ret = -1;
            break;
        }
        printf("Created thread %d\n", i);
    }

    for (j = 0; j < i; j++) {
        printf("Join thread %d\n", j);
        pthread_join(clientThreads[j], NULL);
    }

    free(clientThreads);

    for (i = 0; i < num_comps; i++) {
        /* Terminate connection and destroy rpc_example instance */
        ret = close(fd[i]);
        if (ret < 0) {
            perror("Can't close rpc_example fd ??");
            ret = -1;
        }
        printf("rpc_sample: Closed connection to %s!\n", connreq.name);
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int ret;
    int test_id = -1;
    int core_id = 0;
    int num_comps = 1;
    int num_threads = 1;
    char *f_name = "_triple";
    char *n_iter;
    int c;

    while (1)
    {
        c = getopt (argc, argv, "t:c:x:l:f:m:r:");
        if (c == -1)
            break;

        switch (c)
        {
        case 't':
            test_id = atoi(optarg);
            break;
        case 'c':
            core_id = atoi(optarg);
            break;
        case 'x':
            num_comps = atoi(optarg);
            break;
        case 'l':
            num_threads = atoi(optarg);
            break;
        case 'f':
            f_name = optarg;
            break;
        case 'r':
            reset_cmd = optarg;
            break;
        case 'm':
            switch(atoi(optarg)){
              case 0:
                flt_msg_num = 0;
                break;
              case 1:
                flt_msg_num = NUM_ITERATIONS/2;
                break;
              case 2:
                flt_msg_num = NUM_ITERATIONS - 1;
                break;
              default:
                printf("Warning: Unsupported -m value %s, MMU fault will not be triggered", optarg);
                break;
            }
            break;
        default:
            printf ("Unrecognized argument\n");
        }
    }

    if (test_id < 1 || test_id > 3) {
        printf("Invalid test id\n");
        return 1;
    }

    if (get_nodes() < 1) {
        printf("No rpmsg nodes found\n");
        return 1;
    }

    if (get_function(core_id, f_name, &func_inf) ||
        func_inf.idx == -1){
        printf("Function matching %s not found for core %d\n", f_name, core_id);
        return 1;
    }

    if (flt_msg_num > -1 && (get_function(core_id, "fault", &fault_func_inf) ||
        fault_func_inf.idx == -1)){
        printf("Unable to get MMU fault function information for core %d\n", core_id);
        return 1;
    }

    printf("Using function:\n%s\n for the test\n", func_inf.name);

    n_iter = func_inf.name;
    for ( ; *n_iter; ++n_iter)*n_iter = tolower(*n_iter);

    switch (test_id) {
        case 1:
            /* multiple threads each with an RPMSG-RPC ServiceMgr instance */
            if (core_id < 0 || core_id > 3) {
                printf("Invalid core id\n");
                return 1;
            }
            if (num_comps < 0) {
                printf("Invalid num comps id\n");
                return 1;
            }
            ret = test_rpc_stress_multi_srvmgr(core_id, num_comps);
            break;
        case 2:
            /* Multiple threads, 1 RPMSG-RPC ServiceMgr instances */
            if (core_id < 0 || core_id > 3) {
                printf("Invalid core id\n");
                return 1;
            }
            if (num_threads < 0) {
                printf("Invalid num threads\n");
                return 1;
            }
            ret = test_rpc_stress_multi_threads(core_id, num_threads);
            break;
        case 3:
            /* 1 thread using multiple RPMSG-RPC ServiceMgr instances */
            if (core_id < 0 || core_id > 3) {
                printf("Invalid core id\n");
                return 1;
            }
            if (num_comps < 0) {
                printf("Invalid num comps id\n");
                return 1;
            }
            ret = test_rpc_stress_select(core_id, num_comps);
            break;
        default:
            ret = -1;
            break;
    }

    if (ret < 0 || test_status < 0) {
        printf ("TEST STATUS: FAILED.\n");
    }
    else {
        printf ("TEST STATUS: PASSED.\n");
    }

    return 0;
}
