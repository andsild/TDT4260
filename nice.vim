let SessionLoad = 1
let s:so_save = &so | let s:siso_save = &siso | set so=0 siso=0
let v:this_session=expand("<sfile>:p")
silent only
cd /mnt/virtbox/prefetcher
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
set shortmess=aoO
badd +294 prefetcher.cc
badd +1 test_prefetcher.cc
badd +64 interface.hh
badd +41 ~/Descargas/05_v1_sample_prefetcher.cpp
badd +234 ~/Descargas/PREF_KIT/src/prefetch/sample_prefetcher.h
badd +0 ~/Descargas/PREF_KIT/src/prefetch/interface.h
badd +2 \[unite]\ -\ tag
argglobal
silent! argdel *
argadd prefetcher.cc
edit prefetcher.cc
set splitbelow splitright
wincmd _ | wincmd |
split
1wincmd k
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd _ | wincmd |
split
1wincmd k
wincmd w
wincmd w
wincmd w
wincmd t
set winheight=1 winwidth=1
exe '1resize ' . ((&lines * 39 + 26) / 53)
exe 'vert 1resize ' . ((&columns * 95 + 95) / 191)
exe '2resize ' . ((&lines * 3 + 26) / 53)
exe 'vert 2resize ' . ((&columns * 95 + 95) / 191)
exe '3resize ' . ((&lines * 43 + 26) / 53)
exe 'vert 3resize ' . ((&columns * 95 + 95) / 191)
exe '4resize ' . ((&lines * 5 + 26) / 53)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=7
setlocal fml=1
setlocal fdn=20
setlocal fen
277
normal! zo
294
normal! zo
298
normal! zo
302
normal! zo
357
normal! zo
357
normal! zc
383
normal! zo
399
normal! zo
let s:l = 75 - ((8 * winheight(0) + 19) / 39)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
75
normal! 06|
wincmd w
argglobal
enew
file ClangDiagnostics@1
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal nofen
lcd /mnt/virtbox/prefetcher
wincmd w
argglobal
edit /mnt/virtbox/prefetcher/prefetcher.cc
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal fen
357
normal! zo
let s:l = 45 - ((28 * winheight(0) + 21) / 43)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
45
normal! 0
wincmd w
argglobal
enew
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal nofen
lcd /mnt/virtbox/prefetcher
wincmd w
exe '1resize ' . ((&lines * 39 + 26) / 53)
exe 'vert 1resize ' . ((&columns * 95 + 95) / 191)
exe '2resize ' . ((&lines * 3 + 26) / 53)
exe 'vert 2resize ' . ((&columns * 95 + 95) / 191)
exe '3resize ' . ((&lines * 43 + 26) / 53)
exe 'vert 3resize ' . ((&columns * 95 + 95) / 191)
exe '4resize ' . ((&lines * 5 + 26) / 53)
tabedit /mnt/virtbox/prefetcher/interface.hh
set splitbelow splitright
wincmd t
set winheight=1 winwidth=1
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 67 - ((21 * winheight(0) + 24) / 49)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
67
normal! 02|
tabedit ~/Descargas/05_v1_sample_prefetcher.cpp
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
wincmd _ | wincmd |
split
1wincmd k
wincmd w
wincmd t
set winheight=1 winwidth=1
exe 'vert 1resize ' . ((&columns * 95 + 95) / 191)
exe '2resize ' . ((&lines * 43 + 26) / 53)
exe 'vert 2resize ' . ((&columns * 95 + 95) / 191)
exe '3resize ' . ((&lines * 5 + 26) / 53)
exe 'vert 3resize ' . ((&columns * 95 + 95) / 191)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 3 - ((2 * winheight(0) + 24) / 49)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
3
normal! 0
wincmd w
argglobal
edit ~/Descargas/PREF_KIT/src/prefetch/interface.h
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 1 - ((0 * winheight(0) + 21) / 43)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
1
normal! 0
lcd /mnt/virtbox/prefetcher
wincmd w
argglobal
enew
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal nofen
lcd /mnt/virtbox/prefetcher
wincmd w
exe 'vert 1resize ' . ((&columns * 95 + 95) / 191)
exe '2resize ' . ((&lines * 43 + 26) / 53)
exe 'vert 2resize ' . ((&columns * 95 + 95) / 191)
exe '3resize ' . ((&lines * 5 + 26) / 53)
exe 'vert 3resize ' . ((&columns * 95 + 95) / 191)
tabnext 1
if exists('s:wipebuf') && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=1 shortmess=aTI
let s:sx = expand("<sfile>:p:r")."x.vim"
if file_readable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &so = s:so_save | let &siso = s:siso_save
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :
