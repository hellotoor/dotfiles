set nocompatible
set smartindent
set backspace=indent,eol,start
"set statusline=%F%m%r%h%w\[POS=%l,%v][%p%%]\%{strftime(\"%d/%m/%y\-\ %H:%M\")}
set statusline=%F%m%r%h%w\[pos=%l,%v][%p%%]
set number
"set cursorline
set ruler
set et
set tabstop=2
"set softtabstop=4
set shiftwidth=2
set expandtab
filetype plugin indent on
set ignorecase smartcase
set noerrorbells
set showmatch
set hlsearch
set showcmd
set scrolloff=3
set noswapfile
"set incsearch
"set laststatus=2
"set autoindent
"set autochdir 
set cursorline
set term=xterm
set autoread


"Taglist
let Tlist_Ctags_Cmd="/usr/bin/ctags"
let Tlist_Show_One_File=1
let Tlist_Exit_OnlyWindow=1 
let Tlist_WinWidth=40
"let g:winManagerWindowLayout='FileExplorer|TagList'
"let g:winManagerWindowLayout='TagList'
let Tlist_GainFocus_On_ToggleOpen=0
let Tlist_Use_Right_Window=1
let Tlist_Auto_Update=1

call pathogen#runtime_append_all_bundles()
filetype plugin indent on
set completeopt=longest,menu
set mouse=n

syntax on
set background=dark
"colorscheme solarized 
colorscheme custom 
"colorscheme darkburn 

"let &termencoding=&encoding
set fileencodings=utf-8,gbk,euc-cn,big5,utf-16le


autocmd BufReadPost *
     \ if line("'\"") > 1 && line("'\"") <= line("$") |
     \   exe "normal! g`\"" |
     \ endif

if !exists(":DiffOrig")
      command DiffOrig vert new | set bt=nofile | r # | 0d_ | diffthis
              \ | wincmd p | diffthis
endif

"cscope
"set cscopequickfix=s-,c-,d-,i-,t-,e-
cs add ./cscope.out
cs add /usr/include/cscope.out /usr/include

"nmap: map in normal mod, imap:map in insert mod, vmap:map in vusual mod,
"noremap :no recursive map
nnoremap <C-h> <C-w>h
nnoremap <C-j> <C-w>j
nnoremap <C-k> <C-w>k
nnoremap <C-l> <C-w>l
nnoremap <C-s> :update<CR>
nmap <F1> :tabprevious<CR>
nmap <F2> :tabnext<CR>
nmap <F3> :vs<CR>
nmap <F4> :q<CR>
nmap <F5> :ConqueTermVSplit bash<CR>
nmap <F6> :cn<CR>
nmap <F7> :cp<CR>
nmap <F8> :cw<CR>
nmap <silent> <F12> :AV<CR>
nmap <C-n> :tab split<CR>
nmap wm :Tlist<CR>
nmap * *N



let g:SuperTabRetainCompletionType=2
let g:SuperTabDefaultCompletionType="<C-X><C-O>"
"let g:LookupFile_TagExpr = '"/home/wxf/work/allinone_s1_b20x/tags"' 
"let g:LookupFile_TagExpr = '"/home/wxf/work/allinone_s3/tags"' 

"inoremap [ []<Esc>i
"inoremap ( ()<Esc>i
"inoremap { {}<Esc>i
"inoremap < <><Esc>i
"inoremap > <><Esc>i
"inoremap ' ''<Esc>i
"inoremap " ""<Esc>i

"NERDTree
let NERDChristmasTree=1
let NERDTreeAutoCenter=1
let NERDTreeShowBookmarks=1
let NERDTreeShowLineNumbers=1
nnoremap wn  :NERDTreeTabsToggle<cr>

"NERD Commenter
let mapleader=","

"CtrlP
let g:ctrlp_cmd = 'CtrlP ./'
let g:ctrlp_map = '<c-p>'
let g:ctrlp_by_filename = 1
let g:ctrlp_match_window = 'results:50'


"neocomplete
"Note: This option must set it in .vimrc(_vimrc).  NOT IN .gvimrc(_gvimrc)!
" Disable AutoComplPop.
let g:acp_enableAtStartup = 0
" Use neocomplete.
let g:neocomplete#enable_at_startup = 1
" Use smartcase.
let g:neocomplete#enable_smart_case = 1
" Set minimum syntax keyword length.
let g:neocomplete#sources#syntax#min_keyword_length = 3
let g:neocomplete#lock_buffer_name_pattern = '\*ku\*'

" Define dictionary.
let g:neocomplete#sources#dictionary#dictionaries = {
    \ 'default' : '',
    \ 'vimshell' : $HOME.'/.vimshell_hist',
    \ 'scheme' : $HOME.'/.gosh_completions'
        \ }

" Define keyword.
if !exists('g:neocomplete#keyword_patterns')
    let g:neocomplete#keyword_patterns = {}
endif
let g:neocomplete#keyword_patterns['default'] = '\h\w*'

" Plugin key-mappings.
inoremap <expr><C-g>     neocomplete#undo_completion()
inoremap <expr><C-l>     neocomplete#complete_common_string()

" Recommended key-mappings.
" <CR>: close popup and save indent.
inoremap <silent> <CR> <C-r>=<SID>my_cr_function()<CR>
function! s:my_cr_function()
  return neocomplete#close_popup() . "\<CR>"
  " For no inserting <CR> key.
  "return pumvisible() ? neocomplete#close_popup() : "\<CR>"
endfunction

" <TAB>: completion.
inoremap <expr><TAB>  pumvisible() ? "\<C-n>" : "\<TAB>"
"inoremap <expr><TAB>   neocomplete#start_manual_complete()

" <C-h>, <BS>: close popup and delete backword char.
inoremap <expr><C-h> neocomplete#smart_close_popup()."\<C-h>"
inoremap <expr><BS> neocomplete#smart_close_popup()."\<C-h>"
inoremap <expr><C-y>  neocomplete#close_popup()
inoremap <expr><C-e>  neocomplete#cancel_popup()

" Close popup by <Space>.
"inoremap <expr><Space> pumvisible() ? neocomplete#close_popup() : "\<Space>"
inoremap <expr><Esc> pumvisible() ? neocomplete#close_popup()."\<Esc>" : "\<Esc>"

" For cursor moving in insert mode(Not recommended)
"inoremap <expr><Left>  neocomplete#close_popup() . "\<Left>"
"inoremap <expr><Right> neocomplete#close_popup() . "\<Right>"
"inoremap <expr><Up>    neocomplete#close_popup() . "\<Up>"
"inoremap <expr><Down>  neocomplete#close_popup() . "\<Down>"
" Or set this.
"let g:neocomplete#enable_cursor_hold_i = 1
" Or set this.
"let g:neocomplete#enable_insert_char_pre = 1

" AutoComplPop like behavior.
"let g:neocomplete#enable_auto_select = 1

" Shell like behavior(not recommended).
"set completeopt+=longest
"let g:neocomplete#enable_auto_select = 1
let g:neocomplete#disable_auto_complete = 0
"inoremap <expr><TAB>  pumvisible() ? "\<Down>" : "\<C-x>\<C-u>"

" Enable omni completion.
autocmd FileType css setlocal omnifunc=csscomplete#CompleteCSS
autocmd FileType html,markdown setlocal omnifunc=htmlcomplete#CompleteTags
autocmd FileType javascript setlocal omnifunc=javascriptcomplete#CompleteJS
autocmd FileType python setlocal omnifunc=pythoncomplete#Complete
autocmd FileType xml setlocal omnifunc=xmlcomplete#CompleteTags

" Enable heavy omni completion.
if !exists('g:neocomplete#sources#omni#input_patterns')
  let g:neocomplete#sources#omni#input_patterns = {}
endif
"let g:neocomplete#sources#omni#input_patterns.php = '[^. \t]->\h\w*\|\h\w*::'
let g:neocomplete#sources#omni#input_patterns.c = '[^.[:digit:] *\t]\%(\.\|->\)'
"let g:neocomplete#sources#omni#input_patterns.cpp = '[^.[:digit:] *\t]\%(\.\|->\)\|\h\w*::'

" For perlomni.vim setting.
" https://github.com/c9s/perlomni.vim
let g:neocomplete#sources#omni#input_patterns.perl = '\h\w*->\h\w*\|\h\w*::'

"supertab
"let g:SuperTabDefaultCompletionType = "<c-x><c-u>"


highlight DiffAdd cterm=bold ctermfg=black ctermbg=darkblue gui=none guifg=bg guibg=Red
highlight DiffDelete cterm=bold ctermfg=black ctermbg=darkcyan gui=none guifg=bg guibg=Red
highlight DiffChange cterm=bold ctermfg=black ctermbg=darkcyan gui=none guifg=bg guibg=Red
highlight DiffText cterm=bold ctermfg=black ctermbg=darkcyan gui=none guifg=bg guibg=Red
