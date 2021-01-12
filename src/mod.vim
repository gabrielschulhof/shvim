:set tabstop=2
:set expandtab
:set whichwrap+=<,>,[,],h,l
:set nohlsearch
:set list
:set selection=exclusive

" Figure out whether we're on the last character by jumping forward and back
" and checking whether that causes a line change.
function! IsLastChar()
  let result = 0
  let currentLine = line(".")
  execute "normal! l"
  if !(line(".") == currentLine)
    let result = 1
  endif
  execute "normal! h"
  return result
endfunction

"  get character at cursor:
"  :echo matchstr(getline('.'), '\%' . col('.') . 'c.')

function! GetPosInfo()
  return { 'first': (col('.') ==# 1), 'last': IsLastChar() }
endfunction

function! NSNextChar()
  let pos = GetPosInfo()

  if !pos.first
    execute "normal! l"
  endif

  if pos.last && !pos.first
    execute "normal! mbvho"
  else
    execute "normal! mbvl"
  endif
endfunction

function! NSPrevChar()
  let pos = GetPosInfo()

  if pos.last
    execute "normal! mbvlo"
  elseif !pos.first
    execute "normal! lmbvh"
  else
    execute "normal! mbvh"
  endif
endfunction

function! NSPrevWord()
  let pos = GetPosInfo()

  if pos.first
    execute "normal! mbvb"
  elseif pos.last
    execute "normal! mbvlob"
  else
    execute "normal! lmbvb"
  endif
endfunction

function! NSNextWord()
  let pos = GetPosInfo()

  if !pos.first
    execute "normal! l"
  endif

  if pos.last
    if !pos.first
      execute "normal! mbvhow"
    else
      execute "normal! mbvw"
    endif
  else
    execute "normal! mbvw"
  endif
endfunction

function! NSNextLine()
  let pos = GetPosInfo()

  if pos.last && !pos.first
    execute "normal! mbvjolo"
    if !IsLastChar()
      execute "normal! l"
    endif
  elseif pos.first
    execute "normal! mbvj"
  else
    execute "normal! lmbvj"
  endif
endfunction

function! NSNextPar()
  if !GetPosInfo().first
    execute "normal! l"
  endif
  execute "normal! mbv}"
endfunction

function! NSPrevLine()
  if GetPosInfo().first
    execute "normal! mbvk"
  else
    execute "normal! lmbvk"
  endif
endfunction

function! NSPrevPar()
  let pos = GetPosInfo()

  if pos.first && pos.last
    execute "normal! hmbvlo{"
  else
    execute "normal! lmbv{"
  endif
endfunction

function! NSEnd()
  if !GetPosInfo().first
    execute "normal! l"
  endif
  execute "normal! mbv$"
endfunction

function! NSHome()
  let pos = GetPosInfo()

  if pos.last
    if !pos.first
      execute "normal! mbvlo0"
    else
      execute "normal! mbv"
    endif
  else
    execute "normal! lmbv0"
  endif
endfunction

function! NSEnd()
  if GetPosInfo().first
    execute "normal! mbv$"
  else
    execute "normal! lmbv$"
  endif
endfunction

function! NSEndDoc()
  if GetPosInfo().first
    execute "normal! mbvG$"
  else
    execute "normal! lmbvG$"
  endif
endfunction

function! NSHomeDoc()
  if !GetPosInfo().first
    execute "normal! l"
  endif

  execute "normal! mbv1G0"
endfunction

function! NPaste()
  let pos = GetPosInfo()

  if !pos.first && !pos.last
    execute "normal! l"
  endif
  if pos.last
    execute "normal! gp"
  else
    execute "normal! gP"
  endif
endfunction

function! NPasteOver()
  if GetPosInfo().last
    execute "normal! gp"
  else
    execute "normal! gP"
  endif
endfunction

inoremap <S-right> <Esc>:call NSNextChar()<CR>
inoremap <S-C-right> <Esc>:call NSNextWord()<CR>
vnoremap <S-right> l
vnoremap <S-C-right> w
vnoremap <right> <Esc>li
vnoremap <C-right> <Esc>wi

inoremap <S-left> <Esc>:call NSPrevChar()<CR>
inoremap <S-C-left> <Esc>:call NSPrevWord()<CR>
vnoremap <S-left> h
vnoremap <S-C-left> b
vnoremap <left> <Esc>hi
vnoremap <C-left> <Esc>bi

inoremap <S-down> <Esc>:call NSNextLine()<CR>
inoremap <S-C-down> <Esc>:call NSNextPar()<CR>
vnoremap <S-down> j
vnoremap <S-C-down> }
vnoremap <down> <Esc>ji
vnoremap <C-down> <Esc>}i
inoremap <C-down> <C-o>}

inoremap <S-up> <Esc>:call NSPrevLine()<CR>
inoremap <S-C-up> <Esc>:call NSPrevPar()<CR>
vnoremap <S-up> k
vnoremap <S-C-up> {
vnoremap <up> <Esc>ki
vnoremap <C-up> <Esc>{i
inoremap <C-up> <C-o>{

inoremap <S-end> <Esc>:call NSEnd()<CR>
inoremap <S-C-end> <Esc>:call NSEndDoc()<CR>
vnoremap <S-end> $
vnoremap <end> <Esc>$i
vnoremap <C-end> <Esc>G$i

inoremap <S-home> <Esc>:call NSHome()<CR>
inoremap <S-C-home> <Esc>:call NSHomeDoc()<CR>
vnoremap <S-home> 0
vnoremap <home> <Esc>0i
vnoremap <C-home> <Esc>1G0i

vnoremap <Backspace> "_di
vnoremap <Delete> "_di

inoremap <C-a> <Esc>:execute "normal! 1G0mbvG$"<CR>
vnoremap <C-c> mbomey`bv`eo
inoremap <C-f> <Esc>l/
vnoremap <C-f> <Esc>l/
vnoremap <C-g> <Esc>nmbgn
inoremap <C-l> <Esc>:

inoremap <C-v> <C-o>:call NPaste()<CR>
vnoremap <C-v> "_d:call NPasteOver()<CR>i
vnoremap <C-x> di
inoremap <C-z> <C-o>u
nnoremap <C-z> <Esc>ui
inoremap <C-y> <C-o><C-r>
