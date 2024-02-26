.org 0x0
  nop 
  halt 
  stop 
  di 
  ei 
  ld c, a
  ld b, [hl]
  adc a, c
  ld b, 0x8
  jr nz, 0x4
  ld sp, 0x31a
  ld [hl+], a
  inc hl
  inc [hl]
  ld [hl], 0x1
  ld [0x3], sp
  jr 0xfe
  add hl, bc
  ld a, [bc]
  ld a, [hl+]
  dec bc
  dec a
  ld c, 0x3
  ret nz
  ldh [0x1], a
  ldh a, [0x2]
  ldh [c], a
  ldh a, [c]
  ld [0x1], a
  ld a, [0x2]
  pop bc
  jp nz, 0x2
  jp 0x3
  call nc, 0x2
  push af
  add a, 0x3
  rst 0x00
  add sp, 0x4
  ld hl, sp+0x2
  ret 
  reti 
  jp z, 0x2
  call 0x5
  rlc b
  ld bc, 0x6cb
  rlc b
  ld b, d
  rlc b
  ld b, [hl]
  ld bc, 0x14d
  ld bc, 0x153
  ld bc, 0x156
  ld bc, 0x159
  jr z, 0x6
  ld a, 0x3
  ld a, 0x4
  ld bc, 0x302
  ld bc, 0x101
  ld bc, 0x101
  ld bc, 0x101
  ld bc, 0x101
  ld bc, 0x101
  ld bc, 0x101
  ld bc, 0x101
  ld bc, 0x101
  ld bc, 0x101
  nop 
.db 0x74
  ld h, l
.db 0x73
.db 0x74
  ld sp, 0x6574
.db 0x73
.db 0x74
  ld [hl-], a
  xor a, d
  cp a, e
  call z, 0xdd
  halt 
  nop 
  jp 0x195
  jp 0x195
  add a, h
  ld [hl], a
  ld bc, 0x9b3e
  call 0x1a1
  swap a
  ld [0x1], a
  inc de
  ld [bc], a
  ld a, 0x21
  ld a, 0x23
  jp hl
  ld a, 0x2
  ld bc, 0x6261
  ld h, e
  ld h, [hl]
  ld h, d
  ld h, e
  ld bc, 0x302
  dec a
  ld e, e
  ld a, 0x0
  ld a, 0x2
  ld a, 0x4
  ld a, 0x0
  ld a, 0x1
  ld a, 0x2
  ld a, 0x3
  ld a, 0x4
  ld a, 0x5
  inc a
  dec a
  ld a, 0x3f
.db 0x74
  ld h, l
.db 0x73
.db 0x74
  dec b
  inc b
.db 0x6
