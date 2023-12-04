; this is a sample assembly file 
; that can be read by tests.
; the expected result will just be a regular run of the program
; that will be verified for correctness manually once
; generate expected result with:
; make buildtests
.org 0x100 ; comment
  nop
  ; full line is a comments
  halt ; comment
  stop
  di
  ei

  ld c, a
  ld b, [hl]
  adc a, c
  ld b, 6 + 2 ; comment after expression 
  jr nz, 2 + 2

  ld sp, 0x100 + 0x21A
  ld [hl+], a
  inc hl
  inc [hl]
  ld [hl], 1
  ld [1 + 2], sp
  jr -2
  add hl, bc

  ld a, [bc]
  ld a, [hl+]

  dec bc
  dec a

  ld c, 1+2

  ret nz

  ldh [1], a
  ldh a, [2]

  ldh [c], a
  ldh a, [c]

  ld [1], a
  ld a, [2]

  pop bc
  jp nz, 2

  jp 3

  call nc, 2

  push af

  add a, 1+2
  rst 0x00
  
  add sp, 2 + 2
  ld hl, sp+2

  ret 
  reti

  jp z, 2
  call 5

  rlc c
  rlc [hl]
l1:
  bit 0, d
  bit 0, [hl]

  ld bc, l1 + 1
@local:
  ld bc, @local
l2:
@local: ld bc, @local
  ld bc, $
  jr z, $ - l2
