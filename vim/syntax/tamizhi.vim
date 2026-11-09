" Vim syntax file
" Language: Tamizhi (Tanglish & C options)
if exists('b:current_syntax') | finish | endif

" Comments start with '#'
syn keyword tzTodo contained TODO FIXME XXX NOTE
syn match   tzComment "#.*$" contains=tzTodo,@Spell

" Control flow: @(c) aga irundha { } ^(c) aga irundha { } illana { }
" and C-style: if else while for do switch case default
syn match   tzCondMark "[@^]\ze\s*("
syn keyword tzConditional aga irundha illana if else switch case default therivu nilai muthal
syn keyword tzRepeat      sutri while for do padigal sei in ul
syn keyword tzFlow        udai thodar break continue defer pinbu
syn keyword tzReturn      kudu return

" OOP Keywords
syn keyword tzOOP         this suya super thai moolam new pudhiya delete azhi
syn keyword tzClassKw     vaguppu class nextgroup=tzClassName skipwhite
syn match   tzClassName   contained "\h\w*"

" Declarations
syn keyword tzDecl        edu var let
syn keyword tzStorage     const static maradha nilaiyana typedef sizeof alavu cast mathu
syn keyword tzImport      serku veli import extern
syn keyword tzFuncKw      seyal fn func function nextgroup=tzFuncName skipwhite
syn keyword tzStructKw    kattamaipu struct union ondrippu enum enumaripu nextgroup=tzStructName skipwhite
syn match   tzFuncName    contained "\h\w*"
syn match   tzStructName  contained "\h\w*"

" Builtin output
syn keyword tzBuiltin pathi print println

" Types and literals
syn keyword tzType int float double char short long void bool string
syn keyword tzType unsigned signed uchar ushort uint ulong
syn keyword tzType int8 int16 int32 int64 int8_t int16_t int32_t int64_t
syn keyword tzType uint8 uint16 uint32 uint64 uint8_t uint16_t uint32_t uint64_t
syn keyword tzType size_t ssize_t intptr_t uintptr_t ptrdiff_t
syn keyword tzBoolean true false
syn keyword tzConstant null maru

syn match tzNumber "\<0[xX][0-9a-fA-F]\+\>"
syn match tzNumber "\<0[bB][01]\+\>"
syn match tzNumber "\<0[oO][0-7]\+\>"
syn match tzNumber "\<\d\+\>"
syn match tzFloat  "\<\d\+\.\d\+\([eE][+-]\?\d\+\)\?\>"
syn match tzFloat  "\<\d\+[eE][+-]\?\d\+\>"

" Strings, escapes and printf-style format specifiers
syn match  tzEscape contained "\\[nrt0\\\"']"
syn match  tzFormat contained "%[-+ 0#]*\d*\(\.\d\+\)\?\(ll\|l\)\?[dfsc%]"
syn region tzString start=+"+ skip=+\\"+ end=+"+ oneline contains=tzEscape,tzFormat
syn region tzChar   start=+'+ skip=+\\'+ end=+'+ oneline contains=tzEscape

" Operators
syn match tzOperator "[-+*/%=<>!]=\?"
syn match tzOperator "&&\|||"
syn match tzOperator "++\|--"
syn match tzOperator "->\|\.\|::"
syn match tzOperator "[&|^~?:]"
syn match tzOperator "<<\|>>"
syn match tzOperator "<<=\|>>="
syn match tzDelim    "[(){}\[\];,]"

" Function calls: name followed by '('
syn match tzCall "\<\h\w*\ze\s*("  contains=tzKeywordGuard
syn match tzKeywordGuard contained "\<\%(aga\|irundha\|sutri\|pathi\|if\|else\|while\|for\|switch\|case\|default\|print\|println\)\>"

hi def link tzComment     Comment
hi def link tzTodo        Todo
hi def link tzCondMark    Conditional
hi def link tzConditional Conditional
hi def link tzRepeat      Repeat
hi def link tzFlow        Statement
hi def link tzReturn      Statement
hi def link tzOOP         Special
hi def link tzClassKw     Structure
hi def link tzClassName   Type
hi def link tzDecl        StorageClass
hi def link tzStorage     StorageClass
hi def link tzImport      Include
hi def link tzFuncKw      Keyword
hi def link tzStructKw    Structure
hi def link tzFuncName    Function
hi def link tzStructName  Type
hi def link tzBuiltin     Function
hi def link tzType        Type
hi def link tzBoolean     Boolean
hi def link tzConstant    Constant
hi def link tzNumber      Number
hi def link tzFloat       Float
hi def link tzString      String
hi def link tzChar        Character
hi def link tzEscape      SpecialChar
hi def link tzFormat      Special
hi def link tzOperator    Operator
hi def link tzDelim       Delimiter
hi def link tzCall        Function
hi def link tzKeywordGuard NONE

let b:current_syntax = 'tamizhi'
