#!/usr/bin/env python3
"""Exercise real URI-list aggregation and DROPFILES layout.

Only the Wine NT DOS-path lookup is replaced; UTF-8 conversion uses libc and
expected UTF-16 payloads are literal, independent fixtures.
"""
import pathlib, subprocess, sys, tempfile
s=(pathlib.Path(sys.argv[1])/'dlls/winewayland.drv/wayland_data_device.c').read_text()
parser=s[s.index('static int uri_hex('):s.index('#define DROP_MAX_BYTES')]
code=r'''
#include <assert.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
typedef uint16_t WCHAR;
struct drop_files {uint32_t pFiles; struct {int32_t x, y;} pt; int32_t fNC, fWide;};
#define TRUE 1
#define FILE_OPEN 1
static size_t lstrlenW(const WCHAR *s) {const WCHAR *p=s; while (*p) ++p; return p-s;}
static void ntdll_get_dos_file_name(const char *path, WCHAR **dos, unsigned disposition)
{
    wchar_t wide[1024]; size_t count, i;
    assert(disposition == FILE_OPEN); *dos = NULL;
    if (!strcmp(path, "/unmapped/file.wav")) return;
    count = mbstowcs(wide, path, 1024); assert(count != (size_t)-1 && count < 1024);
    *dos = malloc((count+3)*sizeof(WCHAR)); (*dos)[0]='Z'; (*dos)[1]=':';
    for (i=0; i<=count; ++i) (*dos)[i+2] = wide[i]=='/' ? '\\' : wide[i];
}
'''+parser+r'''
int main(void)
{
    const char uris[]="# native file manager list\r\nfile:///tmp/kick%20one.wav\r\n\r\n"
                      "https://example.invalid/no.wav\nfile://localhost/tmp/%E9%9F%B3.wav\n"
                      "file:///unmapped/file.wav\nfile:///tmp/bass.wav";
    const WCHAR expected[]={
        'Z',':','\\','t','m','p','\\','k','i','c','k',' ','o','n','e','.','w','a','v',0,
        'Z',':','\\','t','m','p','\\',0x97f3,'.','w','a','v',0,
        'Z',':','\\','t','m','p','\\','b','a','s','s','.','w','a','v',0,0};
    const char invalid[]="#comment\r\nhttps://bad/a\nfile://remote/a\nfile:///unmapped/file.wav\n";
    struct drop_files *drop; size_t size=0;
    assert(setlocale(LC_ALL,"C.UTF-8"));
    drop=import_uri_list((void *)uris, sizeof(uris)-1, &size);
    assert(drop && drop->pFiles==20 && drop->fWide && !drop->fNC && !drop->pt.x && !drop->pt.y);
    assert(size==20+sizeof(expected));
    assert(!memcmp((char *)drop+drop->pFiles,expected,sizeof(expected)));
    free(drop);
    assert(!import_uri_list((void *)invalid,sizeof(invalid)-1,&size));
    assert(!import_uri_list((void *)"",0,&size));
    puts("PASS: mixed CRLF/LF/comments, multi-file UTF16 DROPFILES and double NUL");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=pathlib.Path(tmp)/'test.c'; exe=pathlib.Path(tmp)/'test'
    src.write_text(code)
    subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
