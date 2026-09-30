#!/usr/bin/env python3
"""Compile the driver's local URI decoder and exercise hostile/native file names.

Changing percent decoding, local-host filtering, or bounded input handling must
fail these hand-written fixtures. No compositor or Wine installation is needed.
"""
import pathlib, subprocess, sys, tempfile
source = pathlib.Path(sys.argv[1]) / 'dlls/winewayland.drv/wayland_data_device.c'
s = source.read_text()
start = s.find('static int uri_hex(')
end = s.find('\nstatic void *import_uri_list(', start)
assert start >= 0 and end > start, 'incoming local-file URI decoder is not implemented'
code = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
'''+s[start:end]+r'''
static void check(const char *uri, const char *expected)
{
    char *got = local_uri_path(uri, strlen(uri));
    if (expected) { assert(got); assert(!strcmp(got, expected)); }
    else assert(!got);
    free(got);
}
int main(void)
{
    check("file:///tmp/drum%20one.wav", "/tmp/drum one.wav");
    check("file:/tmp/hat.wav", "/tmp/hat.wav");
    check("file://localhost/tmp/%E9%9F%B3.wav", "/tmp/\xe9\x9f\xb3.wav");
    check("file:///tmp/a%23b%25c.wav", "/tmp/a#b%c.wav");
    check("file://remote.invalid/tmp/a.wav", NULL);
    check("https://example.com/a.wav", NULL);
    check("file:///tmp/a%00.wav", NULL);
    check("file:///tmp/a%GG.wav", NULL);
    check("file:///tmp/a%2", NULL);
    check("file:///tmp/a?query", NULL);
    check("file:///tmp/a#fragment", NULL);
    check("file:relative.wav", NULL);
    check("# comment", NULL);
    assert(!local_uri_path("file:///tmp/a\0evil", 18));
    { char uri[512], host[256]; assert(!gethostname(host, sizeof(host)));
      snprintf(uri, sizeof(uri), "file://%s/tmp/a.wav", host);
      check(uri, "/tmp/a.wav"); }
    puts("PASS: local-file URI decoder");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = pathlib.Path(tmp)/'test.c'; exe = pathlib.Path(tmp)/'test'
    src.write_text(code)
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
