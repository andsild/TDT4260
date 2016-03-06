let SessionLoad = 1
let s:so_save = &so | let s:siso_save = &siso | set so=0 siso=0
let v:this_session=expand("<sfile>:p")
silent only
cd /mnt/virtbox/prefetcher
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
set shortmess=aoO
badd +75 prefetcher.cc
badd +1 test_prefetcher.cc
badd +64 interface.hh
badd +1 ~/Descargas/05_v1_sample_prefetcher.cpp
badd +234 ~/Descargas/PREF_KIT/src/prefetch/sample_prefetcher.h
badd +1 ~/Descargas/PREF_KIT/src/prefetch/interface.h
badd +2 \[unite]\ -\ tag
badd +0 ClangDiagnostics@1
argglobal
silent! argdel *
argadd prefetcher.cc
edit prefetcher.cc
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
wincmd t
set winheight=1 winwidth=1
exe 'vert 1resize ' . ((&columns * 79 + 79) / 159)
exe 'vert 2resize ' . ((&columns * 79 + 79) / 159)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=7
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 390 - ((19 * winheight(0) + 19) / 39)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
390
normal! 0
wincmd w
argglobal
edit prefetcher.cc
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal fen
326
normal! zo
333
normal! zo
342
normal! zo
let s:l = 45 - ((31 * winheight(0) + 19) / 39)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
45
normal! 0
lcd /mnt/virtbox/prefetcher
wincmd w
exe 'vert 1resize ' . ((&columns * 79 + 79) / 159)
exe 'vert 2resize ' . ((&columns * 79 + 79) / 159)
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
let s:l = 67 - ((17 * winheight(0) + 19) / 39)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
67
normal! 02|
lcd /mnt/virtbox/prefetcher
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
exe 'vert 1resize ' . ((&columns * 79 + 79) / 159)
exe '2resize ' . ((&lines * 34 + 21) / 43)
exe 'vert 2resize ' . ((&columns * 79 + 79) / 159)
exe '3resize ' . ((&lines * 4 + 21) / 43)
exe 'vert 3resize ' . ((&columns * 79 + 79) / 159)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 3 - ((2 * winheight(0) + 19) / 39)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
3
normal! 0
lcd /mnt/virtbox/prefetcher
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
let s:l = 1 - ((0 * winheight(0) + 17) / 34)
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
exe 'vert 1resize ' . ((&columns * 79 + 79) / 159)
exe '2resize ' . ((&lines * 34 + 21) / 43)
exe 'vert 2resize ' . ((&columns * 79 + 79) / 159)
exe '3resize ' . ((&lines * 4 + 21) / 43)
exe 'vert 3resize ' . ((&columns * 79 + 79) / 159)
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
