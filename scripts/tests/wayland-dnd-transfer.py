#!/usr/bin/env python3
"""Run the production bounded pipe reader against real OS pipes."""
import pathlib, subprocess, sys, tempfile
s=(pathlib.Path(sys.argv[1])/'dlls/winewayland.drv/wayland_data_device.c').read_text()
limits=s[s.index('#define DROP_MAX_BYTES'):s.index('/* The event thread owns drag_offer')]
reader=s[s.index('static void *read_drop_data('):s.index('\nNTSTATUS wayland_read_drop_files(')]
code=r'''
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define WARN(...) ((void)0)
'''+limits+reader+r'''
int main(void)
{
    int fd[2], status;
    size_t size;
    char *data;
    pid_t child;
    struct timespec before, after;
    assert(!pipe(fd)); assert(write(fd[1], "file:///tmp/a.wav\r\n", 19) == 19); close(fd[1]);
    data = read_drop_data(fd[0], &size); close(fd[0]);
    assert(data && size == 19 && !memcmp(data, "file:///tmp/a.wav\r\n", 19)); free(data);
    assert(!pipe(fd));
    clock_gettime(CLOCK_MONOTONIC, &before);
    data = read_drop_data(fd[0], &size);
    clock_gettime(CLOCK_MONOTONIC, &after);
    assert(!data); assert(after.tv_sec - before.tv_sec < 5);
    close(fd[0]); close(fd[1]);
    assert(!pipe(fd)); child = fork(); assert(child >= 0);
    if (!child)
    {
        char chunk[4096] = {0}; size_t count;
        signal(SIGPIPE, SIG_IGN); close(fd[0]);
        for (count = 0; count < 1024*1024+4096; count += sizeof(chunk))
            if (write(fd[1], chunk, sizeof(chunk)) < 0) break;
        close(fd[1]); _exit(0);
    }
    close(fd[1]); data = read_drop_data(fd[0], &size); close(fd[0]);
    assert(!data); assert(waitpid(child, &status, 0) == child && WIFEXITED(status));
    puts("PASS: file pipe import, stalled source timeout, oversized source rejection");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=pathlib.Path(tmp)/'test.c'; exe=pathlib.Path(tmp)/'test'
    src.write_text(code)
    subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
