Merge_DestMono_SrcMono_Src8_NoVolume_NoResample PROC ; 0

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop0:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit0

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit0

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop0

  ; Jump out point if end of dest is reached
  dest_end_exit0:
  inc   PTR_ESI

  ; Jump out point if end of src is reached
  src_end_exit0:

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_NoVolume_NoResample ENDP


Merge_DestStereo_SrcMono_Src8_NoVolume_NoResample PROC ; 1

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop1:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], eax
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit1

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit1

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop1

  ; Jump out point if end of dest is reached
  dest_end_exit1:
  inc   PTR_ESI

  ; Jump out point if end of src is reached
  src_end_exit1:

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_NoVolume_NoResample ENDP


Merge_DestMono_SrcMono_Src16_NoVolume_NoResample PROC ; 2

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop2:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit2

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit2

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop2

  ; Jump out point if end of dest is reached
  dest_end_exit2:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit2:

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_NoVolume_NoResample ENDP


Merge_DestStereo_SrcMono_Src16_NoVolume_NoResample PROC ; 3

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop3:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], eax
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit3

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit3

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop3

  ; Jump out point if end of dest is reached
  dest_end_exit3:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit3:

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_NoVolume_NoResample ENDP


Merge_DestMono_SrcStereo_Src8_NoVolume_NoResample PROC ; 8

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop8:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit8

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit8

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop8

  ; Jump out point if end of dest is reached
  dest_end_exit8:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit8:

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_NoVolume_NoResample ENDP


Merge_DestStereo_SrcStereo_Src8_NoVolume_NoResample PROC ; 9

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop9:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit9

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit9

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; End loop
  jmp   merge_loop9

  ; Jump out point if end of dest is reached
  dest_end_exit9:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit9:

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_NoVolume_NoResample ENDP


Merge_DestMono_SrcStereo_Src16_NoVolume_NoResample PROC ; 10

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop10:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit10

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit10

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop10

  ; Jump out point if end of dest is reached
  dest_end_exit10:
  add   PTR_ESI, 4

  ; Jump out point if end of src is reached
  src_end_exit10:

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_NoVolume_NoResample ENDP


Merge_DestStereo_SrcStereo_Src16_NoVolume_NoResample PROC ; 11

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop11:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit11

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit11

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; End loop
  jmp   merge_loop11

  ; Jump out point if end of dest is reached
  dest_end_exit11:
  add   PTR_ESI, 4

  ; Jump out point if end of src is reached
  src_end_exit11:

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_NoVolume_NoResample ENDP


Merge_DestMono_SrcMono_Src8_NoVolume_Resample PROC ; 16

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop16:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit16

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop16

  ; Jump out point if end of dest is reached
  dest_end_exit16:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit16:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_NoVolume_Resample ENDP


Merge_DestStereo_SrcMono_Src8_NoVolume_Resample PROC ; 17

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop17:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], eax
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit17

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit17

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop17

  ; Jump out point if end of dest is reached
  dest_end_exit17:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit17:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_NoVolume_Resample ENDP


Merge_DestMono_SrcMono_Src16_NoVolume_Resample PROC ; 18

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop18:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit18

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit18

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop18

  ; Jump out point if end of dest is reached
  dest_end_exit18:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit18:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_NoVolume_Resample ENDP


Merge_DestStereo_SrcMono_Src16_NoVolume_Resample PROC ; 19

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop19:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], eax
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit19

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit19

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop19

  ; Jump out point if end of dest is reached
  dest_end_exit19:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit19:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_NoVolume_Resample ENDP


Merge_DestMono_SrcMono_Src8_NoVolume_Filtered PROC ; 20

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcMono_Src8_NoVolume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue20
  test eax, 040000000h
  jnz  last_continue20

  ; Merge sample data loop
  ALIGN 16
  merge_loop20:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop20

  ; loop to load all of the full sample points
  whole_loop20:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit20
  whole_continue20:

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  add   ecx, eax
  sub   edx, 65536
  inc   PTR_ESI
  cmp   edx, 65536*2
  jae   whole_loop20

  skip_loop20:
  and   edx, 0ffffh
  last_continue20:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit20

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  inc   PTR_ESI
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop20

  ; Jump out point if end of dest is reached
  dest_end_exit20:
  jmp   src_save_value20

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit20:
  or   edx, 080000000h
  src_last_exit20:
  or   edx, 040000000h
  src_save_value20:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_NoVolume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop532:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit532

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop532

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit532

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop532

  ; Jump out point if end of dest is reached
  dest_end_exit532:
  inc   PTR_ESI
  add   edx, [step_fract]
  jc    skip_filter_adjust532

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  dec   PTR_ESI

  skip_filter_adjust532:

  ; Jump out point if end of src is reached
  src_end_exit532:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_NoVolume_Filtered ENDP


Merge_DestStereo_SrcMono_Src8_NoVolume_Filtered PROC ; 21

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcMono_Src8_NoVolume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue21
  test eax, 040000000h
  jnz  last_continue21

  ; Merge sample data loop
  ALIGN 16
  merge_loop21:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop21

  ; loop to load all of the full sample points
  whole_loop21:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit21
  whole_continue21:

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  add   ecx, eax
  sub   edx, 65536
  inc   PTR_ESI
  cmp   edx, 65536*2
  jae   whole_loop21

  skip_loop21:
  and   edx, 0ffffh
  last_continue21:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit21

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  inc   PTR_ESI
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], eax
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jb    merge_loop21

  ; Jump out point if end of dest is reached
  dest_end_exit21:
  jmp   src_save_value21

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit21:
  or   edx, 080000000h
  src_last_exit21:
  or   edx, 040000000h
  src_save_value21:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_NoVolume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop533:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], eax
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit533

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop533

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit533

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop533

  ; Jump out point if end of dest is reached
  dest_end_exit533:
  inc   PTR_ESI
  add   edx, [step_fract]
  jc    skip_filter_adjust533

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  dec   PTR_ESI

  skip_filter_adjust533:

  ; Jump out point if end of src is reached
  src_end_exit533:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_NoVolume_Filtered ENDP


Merge_DestMono_SrcMono_Src16_NoVolume_Filtered PROC ; 22

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcMono_Src16_NoVolume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue22
  test eax, 040000000h
  jnz  last_continue22

  ; Merge sample data loop
  ALIGN 16
  merge_loop22:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop22

  ; loop to load all of the full sample points
  whole_loop22:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit22
  whole_continue22:

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop22

  skip_loop22:
  and   edx, 0ffffh
  last_continue22:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit22

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   PTR_ESI, 2
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop22

  ; Jump out point if end of dest is reached
  dest_end_exit22:
  jmp   src_save_value22

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit22:
  or   edx, 080000000h
  src_last_exit22:
  or   edx, 040000000h
  src_save_value22:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_NoVolume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop534:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit534

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop534

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit534

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop534

  ; Jump out point if end of dest is reached
  dest_end_exit534:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust534

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust534:

  ; Jump out point if end of src is reached
  src_end_exit534:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_NoVolume_Filtered ENDP


Merge_DestStereo_SrcMono_Src16_NoVolume_Filtered PROC ; 23

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcMono_Src16_NoVolume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue23
  test eax, 040000000h
  jnz  last_continue23

  ; Merge sample data loop
  ALIGN 16
  merge_loop23:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop23

  ; loop to load all of the full sample points
  whole_loop23:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit23
  whole_continue23:

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop23

  skip_loop23:
  and   edx, 0ffffh
  last_continue23:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit23

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   PTR_ESI, 2
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], eax
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jb    merge_loop23

  ; Jump out point if end of dest is reached
  dest_end_exit23:
  jmp   src_save_value23

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit23:
  or   edx, 080000000h
  src_last_exit23:
  or   edx, 040000000h
  src_save_value23:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_NoVolume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop535:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], eax
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit535

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop535

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit535

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop535

  ; Jump out point if end of dest is reached
  dest_end_exit535:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust535

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust535:

  ; Jump out point if end of src is reached
  src_end_exit535:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_NoVolume_Filtered ENDP


Merge_DestMono_SrcStereo_Src8_NoVolume_Resample PROC ; 24

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop24:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit24

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit24

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop24

  ; Jump out point if end of dest is reached
  dest_end_exit24:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit24:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_NoVolume_Resample ENDP


Merge_DestStereo_SrcStereo_Src8_NoVolume_Resample PROC ; 25

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop25:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit25

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit25

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; End loop
  jmp   merge_loop25

  ; Jump out point if end of dest is reached
  dest_end_exit25:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit25:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_NoVolume_Resample ENDP


Merge_DestMono_SrcStereo_Src16_NoVolume_Resample PROC ; 26

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop26:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit26

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit26

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Apply volume
  shl   eax, 11

  ; End loop
  jmp   merge_loop26

  ; Jump out point if end of dest is reached
  dest_end_exit26:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit26:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_NoVolume_Resample ENDP


Merge_DestStereo_SrcStereo_Src16_NoVolume_Resample PROC ; 27

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; Merge sample data loop
  ALIGN 16
  merge_loop27:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit27

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit27

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; End loop
  jmp   merge_loop27

  ; Jump out point if end of dest is reached
  dest_end_exit27:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit27:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_NoVolume_Resample ENDP


Merge_DestMono_SrcStereo_Src8_NoVolume_Filtered PROC ; 28

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcStereo_Src8_NoVolume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue28
  test eax, 040000000h
  jnz  last_continue28

  ; Merge sample data loop
  ALIGN 16
  merge_loop28:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop28

  ; loop to load all of the full sample points
  whole_loop28:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit28
  whole_continue28:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop28

  skip_loop28:
  and   edx, 0ffffh
  last_continue28:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit28

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx
  add   PTR_ESI, 2
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop28

  ; Jump out point if end of dest is reached
  dest_end_exit28:
  jmp   src_save_value28

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit28:
  or   edx, 080000000h
  src_last_exit28:
  or   edx, 040000000h
  src_save_value28:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_NoVolume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data loop
  ALIGN 16
  merge_loop540:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit540

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop540

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit540

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; End loop
  jmp   merge_loop540

  ; Jump out point if end of dest is reached
  dest_end_exit540:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust540

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust540:

  ; Jump out point if end of src is reached
  src_end_exit540:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_NoVolume_Filtered ENDP


Merge_DestStereo_SrcStereo_Src8_NoVolume_Filtered PROC ; 29

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcStereo_Src8_NoVolume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; Save registers
  push  PTR_EBP

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue29
  test eax, 040000000h
  jnz  last_continue29

  ; Merge sample data loop
  ALIGN 16
  merge_loop29:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  mov   eax, [cur_r]
  imul  ecx, ebx
  imul  eax, ebx
  sar   ecx, 16
  sar   eax, 16
  cmp   edx, 65536*2
  mov   [cur_r], eax
  jl    skip_loop29

  ; loop to load all of the full sample points
  whole_loop29:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit29
  whole_continue29:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8
  add   ecx, eax
  add   [cur_r], ebx
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop29

  skip_loop29:
  and   edx, 0ffffh
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit29
  last_continue29:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8
  add   PTR_ESI, 2
  mov   ebp, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebp
  mov   ebp, [cur_r]
  mov   [cur_r], ebx
  imul  ebx, edx
  sar   ebx, 16
  add   ebx, ebp
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jb    merge_loop29

  ; Jump out point if end of dest is reached
  dest_end_exit29:
  jmp   src_save_value29

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit29:
  or   edx, 080000000h
  src_last_exit29:
  or   edx, 040000000h
  src_save_value29:
  mov  [cur_l], ecx

  ; Restore registers
  pop   PTR_EBP

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_NoVolume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Save registers
  push  PTR_EBP

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop541:

  ; Upsample the data points
  mov   ebp, edx
  mov   eax, [cur_l]
  mov   ebx, [cur_r]
  shr   ebp, 17
  sub   eax, ecx
  sub   ebx, [cur_r2]
  imul  eax, ebp
  imul  ebx, ebp
  sar   eax, 15
  sar   ebx, 15
  add   eax, ecx
  add   ebx, [cur_r2]

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit541

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop541

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit541

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; End loop
  jmp   merge_loop541

  ; Jump out point if end of dest is reached
  dest_end_exit541:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust541

  ; rotate filter values
  mov   [cur_l], ecx
  mov   ecx, [cur_r2]
  mov   [cur_r], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust541:

  ; Jump out point if end of src is reached
  src_end_exit541:

  ; Restore registers
  pop   PTR_EBP

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_NoVolume_Filtered ENDP


Merge_DestMono_SrcStereo_Src16_NoVolume_Filtered PROC ; 30

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcStereo_Src16_NoVolume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue30
  test eax, 040000000h
  jnz  last_continue30

  ; Merge sample data loop
  ALIGN 16
  merge_loop30:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop30

  ; loop to load all of the full sample points
  whole_loop30:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit30
  whole_continue30:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 4
  cmp   edx, 65536*2
  jae   whole_loop30

  skip_loop30:
  and   edx, 0ffffh
  last_continue30:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit30

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx
  add   PTR_ESI, 4
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop30

  ; Jump out point if end of dest is reached
  dest_end_exit30:
  jmp   src_save_value30

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit30:
  or   edx, 080000000h
  src_last_exit30:
  or   edx, 040000000h
  src_save_value30:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_NoVolume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data loop
  ALIGN 16
  merge_loop542:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Apply volume
  shl   eax, 11

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit542

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop542

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit542

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; End loop
  jmp   merge_loop542

  ; Jump out point if end of dest is reached
  dest_end_exit542:
  add   PTR_ESI, 4
  add   edx, [step_fract]
  jc    skip_filter_adjust542

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 4

  skip_filter_adjust542:

  ; Jump out point if end of src is reached
  src_end_exit542:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_NoVolume_Filtered ENDP


Merge_DestStereo_SrcStereo_Src16_NoVolume_Filtered PROC ; 31

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcStereo_Src16_NoVolume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; Save registers
  push  PTR_EBP

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue31
  test eax, 040000000h
  jnz  last_continue31

  ; Merge sample data loop
  ALIGN 16
  merge_loop31:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  mov   eax, [cur_r]
  imul  ecx, ebx
  imul  eax, ebx
  sar   ecx, 16
  sar   eax, 16
  cmp   edx, 65536*2
  mov   [cur_r], eax
  jl    skip_loop31

  ; loop to load all of the full sample points
  whole_loop31:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit31
  whole_continue31:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16
  add   ecx, eax
  add   [cur_r], ebx
  sub   edx, 65536
  add   PTR_ESI, 4
  cmp   edx, 65536*2
  jae   whole_loop31

  skip_loop31:
  and   edx, 0ffffh
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit31
  last_continue31:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16
  add   PTR_ESI, 4
  mov   ebp, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebp
  mov   ebp, [cur_r]
  mov   [cur_r], ebx
  imul  ebx, edx
  sar   ebx, 16
  add   ebx, ebp
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jb    merge_loop31

  ; Jump out point if end of dest is reached
  dest_end_exit31:
  jmp   src_save_value31

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit31:
  or   edx, 080000000h
  src_last_exit31:
  or   edx, 040000000h
  src_save_value31:
  mov  [cur_l], ecx

  ; Restore registers
  pop   PTR_EBP

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_NoVolume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Save registers
  push  PTR_EBP

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop543:

  ; Upsample the data points
  mov   ebp, edx
  mov   eax, [cur_l]
  mov   ebx, [cur_r]
  shr   ebp, 17
  sub   eax, ecx
  sub   ebx, [cur_r2]
  imul  eax, ebp
  imul  ebx, ebp
  sar   eax, 15
  sar   ebx, 15
  add   eax, ecx
  add   ebx, [cur_r2]

  ; Apply volume
  shl   eax, 11
  shl   ebx, 11

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit543

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop543

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit543

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; End loop
  jmp   merge_loop543

  ; Jump out point if end of dest is reached
  dest_end_exit543:
  add   PTR_ESI, 4
  add   edx, [step_fract]
  jc    skip_filter_adjust543

  ; rotate filter values
  mov   [cur_l], ecx
  mov   ecx, [cur_r2]
  mov   [cur_r], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 4

  skip_filter_adjust543:

  ; Jump out point if end of src is reached
  src_end_exit543:

  ; Restore registers
  pop   PTR_EBP

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_NoVolume_Filtered ENDP


Merge_DestMono_SrcMono_Src8_Volume_NoResample PROC ; 32

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]

  ; Merge sample data loop
  ALIGN 16
  merge_loop32:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit32

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit32

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]

  ; End loop
  jmp   merge_loop32

  ; Jump out point if end of dest is reached
  dest_end_exit32:
  inc   PTR_ESI

  ; Jump out point if end of src is reached
  src_end_exit32:

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_Volume_NoResample ENDP


Merge_DestStereo_SrcMono_Src8_Volume_NoResample PROC ; 33

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data loop
  ALIGN 16
  merge_loop33:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit33

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit33

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; End loop
  jmp   merge_loop33

  ; Jump out point if end of dest is reached
  dest_end_exit33:
  inc   PTR_ESI

  ; Jump out point if end of src is reached
  src_end_exit33:

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_Volume_NoResample ENDP


Merge_DestMono_SrcMono_Src16_Volume_NoResample PROC ; 34

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  imul  eax, [scale_left]

  ; Merge sample data loop
  ALIGN 16
  merge_loop34:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit34

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit34

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  imul  eax, [scale_left]

  ; End loop
  jmp   merge_loop34

  ; Jump out point if end of dest is reached
  dest_end_exit34:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit34:

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_Volume_NoResample ENDP


Merge_DestStereo_SrcMono_Src16_Volume_NoResample PROC ; 35

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data loop
  ALIGN 16
  merge_loop35:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit35

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit35

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; End loop
  jmp   merge_loop35

  ; Jump out point if end of dest is reached
  dest_end_exit35:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit35:

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_Volume_NoResample ENDP


Merge_DestMono_SrcStereo_Src8_Volume_NoResample PROC ; 40

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data loop
  ALIGN 16
  merge_loop40:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit40

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit40

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; End loop
  jmp   merge_loop40

  ; Jump out point if end of dest is reached
  dest_end_exit40:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit40:

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_Volume_NoResample ENDP


Merge_DestStereo_SrcStereo_Src8_Volume_NoResample PROC ; 41

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data loop
  ALIGN 16
  merge_loop41:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit41

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit41

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; End loop
  jmp   merge_loop41

  ; Jump out point if end of dest is reached
  dest_end_exit41:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit41:

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_Volume_NoResample ENDP


Merge_DestMono_SrcStereo_Src16_Volume_NoResample PROC ; 42

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data loop
  ALIGN 16
  merge_loop42:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit42

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit42

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; End loop
  jmp   merge_loop42

  ; Jump out point if end of dest is reached
  dest_end_exit42:
  add   PTR_ESI, 4

  ; Jump out point if end of src is reached
  src_end_exit42:

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_Volume_NoResample ENDP


Merge_DestStereo_SrcStereo_Src16_Volume_NoResample PROC ; 43

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data loop
  ALIGN 16
  merge_loop43:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit43

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit43

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; End loop
  jmp   merge_loop43

  ; Jump out point if end of dest is reached
  dest_end_exit43:
  add   PTR_ESI, 4

  ; Jump out point if end of src is reached
  src_end_exit43:

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_Volume_NoResample ENDP


Merge_DestMono_SrcMono_Src8_Volume_Resample PROC ; 48

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]

  ; Merge sample data loop
  ALIGN 16
  merge_loop48:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit48

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit48

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]

  ; End loop
  jmp   merge_loop48

  ; Jump out point if end of dest is reached
  dest_end_exit48:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit48:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_Volume_Resample ENDP


Merge_DestStereo_SrcMono_Src8_Volume_Resample PROC ; 49

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data loop
  ALIGN 16
  merge_loop49:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit49

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit49

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; End loop
  jmp   merge_loop49

  ; Jump out point if end of dest is reached
  dest_end_exit49:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit49:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_Volume_Resample ENDP


Merge_DestMono_SrcMono_Src16_Volume_Resample PROC ; 50

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  imul  eax, [scale_left]

  ; Merge sample data loop
  ALIGN 16
  merge_loop50:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit50

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit50

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Apply volume
  imul  eax, [scale_left]

  ; End loop
  jmp   merge_loop50

  ; Jump out point if end of dest is reached
  dest_end_exit50:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit50:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_Volume_Resample ENDP


Merge_DestStereo_SrcMono_Src16_Volume_Resample PROC ; 51

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data loop
  ALIGN 16
  merge_loop51:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit51

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit51

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; End loop
  jmp   merge_loop51

  ; Jump out point if end of dest is reached
  dest_end_exit51:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit51:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_Volume_Resample ENDP


Merge_DestMono_SrcMono_Src8_Volume_Filtered PROC ; 52

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcMono_Src8_Volume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_left]
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue52
  test eax, 040000000h
  jnz  last_continue52

  ; Merge sample data loop
  ALIGN 16
  merge_loop52:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop52

  ; loop to load all of the full sample points
  whole_loop52:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit52
  whole_continue52:

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  add   ecx, eax
  sub   edx, 65536
  inc   PTR_ESI
  cmp   edx, 65536*2
  jae   whole_loop52

  skip_loop52:
  and   edx, 0ffffh
  last_continue52:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit52

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  inc   PTR_ESI
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop52

  ; Jump out point if end of dest is reached
  dest_end_exit52:
  jmp   src_save_value52

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit52:
  or   edx, 080000000h
  src_last_exit52:
  or   edx, 040000000h
  src_save_value52:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_Volume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop564:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Apply volume
  imul  eax, [scale_left]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit564

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop564

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit564

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop564

  ; Jump out point if end of dest is reached
  dest_end_exit564:
  inc   PTR_ESI
  add   edx, [step_fract]
  jc    skip_filter_adjust564

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  dec   PTR_ESI

  skip_filter_adjust564:

  ; Jump out point if end of src is reached
  src_end_exit564:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_Volume_Filtered ENDP


Merge_DestStereo_SrcMono_Src8_Volume_Filtered PROC ; 53

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcMono_Src8_Volume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_left]
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_right]
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue53
  test eax, 040000000h
  jnz  last_continue53

  ; Merge sample data loop
  ALIGN 16
  merge_loop53:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop53

  ; loop to load all of the full sample points
  whole_loop53:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit53
  whole_continue53:

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  add   ecx, eax
  sub   edx, 65536
  inc   PTR_ESI
  cmp   edx, 65536*2
  jae   whole_loop53

  skip_loop53:
  and   edx, 0ffffh
  last_continue53:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit53

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  inc   PTR_ESI
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx

  ; Duplicate the left channel into the right
  mov   ebx, eax
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jb    merge_loop53

  ; Jump out point if end of dest is reached
  dest_end_exit53:
  jmp   src_save_value53

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit53:
  or   edx, 080000000h
  src_last_exit53:
  or   edx, 040000000h
  src_save_value53:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_Volume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop565:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit565

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop565

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit565

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop565

  ; Jump out point if end of dest is reached
  dest_end_exit565:
  inc   PTR_ESI
  add   edx, [step_fract]
  jc    skip_filter_adjust565

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  dec   PTR_ESI

  skip_filter_adjust565:

  ; Jump out point if end of src is reached
  src_end_exit565:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_Volume_Filtered ENDP


Merge_DestMono_SrcMono_Src16_Volume_Filtered PROC ; 54

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcMono_Src16_Volume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_left]
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue54
  test eax, 040000000h
  jnz  last_continue54

  ; Merge sample data loop
  ALIGN 16
  merge_loop54:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop54

  ; loop to load all of the full sample points
  whole_loop54:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit54
  whole_continue54:

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop54

  skip_loop54:
  and   edx, 0ffffh
  last_continue54:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit54

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   PTR_ESI, 2
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop54

  ; Jump out point if end of dest is reached
  dest_end_exit54:
  jmp   src_save_value54

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit54:
  or   edx, 080000000h
  src_last_exit54:
  or   edx, 040000000h
  src_save_value54:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_Volume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop566:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Apply volume
  imul  eax, [scale_left]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit566

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop566

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit566

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop566

  ; Jump out point if end of dest is reached
  dest_end_exit566:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust566

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust566:

  ; Jump out point if end of src is reached
  src_end_exit566:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_Volume_Filtered ENDP


Merge_DestStereo_SrcMono_Src16_Volume_Filtered PROC ; 55

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcMono_Src16_Volume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_left]
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_right]
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue55
  test eax, 040000000h
  jnz  last_continue55

  ; Merge sample data loop
  ALIGN 16
  merge_loop55:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop55

  ; loop to load all of the full sample points
  whole_loop55:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit55
  whole_continue55:

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop55

  skip_loop55:
  and   edx, 0ffffh
  last_continue55:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit55

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   PTR_ESI, 2
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx

  ; Duplicate the left channel into the right
  mov   ebx, eax
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jb    merge_loop55

  ; Jump out point if end of dest is reached
  dest_end_exit55:
  jmp   src_save_value55

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit55:
  or   edx, 080000000h
  src_last_exit55:
  or   edx, 040000000h
  src_save_value55:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_Volume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop567:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Duplicate the left channel into the right
  mov   ebx, eax

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit567

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop567

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit567

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop567

  ; Jump out point if end of dest is reached
  dest_end_exit567:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust567

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust567:

  ; Jump out point if end of src is reached
  src_end_exit567:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_Volume_Filtered ENDP


Merge_DestMono_SrcStereo_Src8_Volume_Resample PROC ; 56

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data loop
  ALIGN 16
  merge_loop56:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit56

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit56

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; End loop
  jmp   merge_loop56

  ; Jump out point if end of dest is reached
  dest_end_exit56:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit56:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_Volume_Resample ENDP


Merge_DestStereo_SrcStereo_Src8_Volume_Resample PROC ; 57

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data loop
  ALIGN 16
  merge_loop57:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit57

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit57

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; End loop
  jmp   merge_loop57

  ; Jump out point if end of dest is reached
  dest_end_exit57:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit57:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_Volume_Resample ENDP


Merge_DestMono_SrcStereo_Src16_Volume_Resample PROC ; 58

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data loop
  ALIGN 16
  merge_loop58:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit58

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit58

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; End loop
  jmp   merge_loop58

  ; Jump out point if end of dest is reached
  dest_end_exit58:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit58:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_Volume_Resample ENDP


Merge_DestStereo_SrcStereo_Src16_Volume_Resample PROC ; 59

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data loop
  ALIGN 16
  merge_loop59:

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit59

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit59

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; End loop
  jmp   merge_loop59

  ; Jump out point if end of dest is reached
  dest_end_exit59:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit59:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_Volume_Resample ENDP


Merge_DestMono_SrcStereo_Src8_Volume_Filtered PROC ; 60

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcStereo_Src8_Volume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_left]
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_right]
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; Save registers
  push  PTR_EBP

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue60
  test eax, 040000000h
  jnz  last_continue60

  ; Merge sample data loop
  ALIGN 16
  merge_loop60:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  mov   eax, [cur_r]
  imul  ecx, ebx
  imul  eax, ebx
  sar   ecx, 16
  sar   eax, 16
  cmp   edx, 65536*2
  mov   [cur_r], eax
  jl    skip_loop60

  ; loop to load all of the full sample points
  whole_loop60:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit60
  whole_continue60:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8
  add   ecx, eax
  add   [cur_r], ebx
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop60

  skip_loop60:
  and   edx, 0ffffh
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit60
  last_continue60:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8
  add   PTR_ESI, 2
  mov   ebp, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebp
  mov   ebp, [cur_r]
  mov   [cur_r], ebx
  imul  ebx, edx
  sar   ebx, 16
  add   ebx, ebp
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop60

  ; Jump out point if end of dest is reached
  dest_end_exit60:
  jmp   src_save_value60

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit60:
  or   edx, 080000000h
  src_last_exit60:
  or   edx, 040000000h
  src_save_value60:
  mov  [cur_l], ecx

  ; Restore registers
  pop   PTR_EBP

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_Volume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Save registers
  push  PTR_EBP

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data loop
  ALIGN 16
  merge_loop572:

  ; Upsample the data points
  mov   ebp, edx
  mov   eax, [cur_l]
  mov   ebx, [cur_r]
  shr   ebp, 17
  sub   eax, ecx
  sub   ebx, [cur_r2]
  imul  eax, ebp
  imul  ebx, ebp
  sar   eax, 15
  sar   ebx, 15
  add   eax, ecx
  add   ebx, [cur_r2]

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit572

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop572

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit572

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; End loop
  jmp   merge_loop572

  ; Jump out point if end of dest is reached
  dest_end_exit572:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust572

  ; rotate filter values
  mov   [cur_l], ecx
  mov   ecx, [cur_r2]
  mov   [cur_r], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust572:

  ; Jump out point if end of src is reached
  src_end_exit572:

  ; Restore registers
  pop   PTR_EBP

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_Volume_Filtered ENDP


Merge_DestStereo_SrcStereo_Src8_Volume_Filtered PROC ; 61

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcStereo_Src8_Volume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_left]
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_right]
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; Save registers
  push  PTR_EBP

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue61
  test eax, 040000000h
  jnz  last_continue61

  ; Merge sample data loop
  ALIGN 16
  merge_loop61:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  mov   eax, [cur_r]
  imul  ecx, ebx
  imul  eax, ebx
  sar   ecx, 16
  sar   eax, 16
  cmp   edx, 65536*2
  mov   [cur_r], eax
  jl    skip_loop61

  ; loop to load all of the full sample points
  whole_loop61:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit61
  whole_continue61:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8
  add   ecx, eax
  add   [cur_r], ebx
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop61

  skip_loop61:
  and   edx, 0ffffh
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit61
  last_continue61:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8
  add   PTR_ESI, 2
  mov   ebp, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebp
  mov   ebp, [cur_r]
  mov   [cur_r], ebx
  imul  ebx, edx
  sar   ebx, 16
  add   ebx, ebp
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jb    merge_loop61

  ; Jump out point if end of dest is reached
  dest_end_exit61:
  jmp   src_save_value61

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit61:
  or   edx, 080000000h
  src_last_exit61:
  or   edx, 040000000h
  src_save_value61:
  mov  [cur_l], ecx

  ; Restore registers
  pop   PTR_EBP

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_Volume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Save registers
  push  PTR_EBP

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop573:

  ; Upsample the data points
  mov   ebp, edx
  mov   eax, [cur_l]
  mov   ebx, [cur_r]
  shr   ebp, 17
  sub   eax, ecx
  sub   ebx, [cur_r2]
  imul  eax, ebp
  imul  ebx, ebp
  sar   eax, 15
  sar   ebx, 15
  add   eax, ecx
  add   ebx, [cur_r2]

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit573

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop573

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit573

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; End loop
  jmp   merge_loop573

  ; Jump out point if end of dest is reached
  dest_end_exit573:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust573

  ; rotate filter values
  mov   [cur_l], ecx
  mov   ecx, [cur_r2]
  mov   [cur_r], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust573:

  ; Jump out point if end of src is reached
  src_end_exit573:

  ; Restore registers
  pop   PTR_EBP

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_Volume_Filtered ENDP


Merge_DestMono_SrcStereo_Src16_Volume_Filtered PROC ; 62

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcStereo_Src16_Volume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_left]
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_right]
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; Save registers
  push  PTR_EBP

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue62
  test eax, 040000000h
  jnz  last_continue62

  ; Merge sample data loop
  ALIGN 16
  merge_loop62:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  mov   eax, [cur_r]
  imul  ecx, ebx
  imul  eax, ebx
  sar   ecx, 16
  sar   eax, 16
  cmp   edx, 65536*2
  mov   [cur_r], eax
  jl    skip_loop62

  ; loop to load all of the full sample points
  whole_loop62:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit62
  whole_continue62:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16
  add   ecx, eax
  add   [cur_r], ebx
  sub   edx, 65536
  add   PTR_ESI, 4
  cmp   edx, 65536*2
  jae   whole_loop62

  skip_loop62:
  and   edx, 0ffffh
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit62
  last_continue62:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16
  add   PTR_ESI, 4
  mov   ebp, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebp
  mov   ebp, [cur_r]
  mov   [cur_r], ebx
  imul  ebx, edx
  sar   ebx, 16
  add   ebx, ebp
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop62

  ; Jump out point if end of dest is reached
  dest_end_exit62:
  jmp   src_save_value62

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit62:
  or   edx, 080000000h
  src_last_exit62:
  or   edx, 040000000h
  src_save_value62:
  mov  [cur_l], ecx

  ; Restore registers
  pop   PTR_EBP

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_Volume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Save registers
  push  PTR_EBP

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data loop
  ALIGN 16
  merge_loop574:

  ; Upsample the data points
  mov   ebp, edx
  mov   eax, [cur_l]
  mov   ebx, [cur_r]
  shr   ebp, 17
  sub   eax, ecx
  sub   ebx, [cur_r2]
  imul  eax, ebp
  imul  ebx, ebp
  sar   eax, 15
  sar   ebx, 15
  add   eax, ecx
  add   ebx, [cur_r2]

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit574

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop574

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit574

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; End loop
  jmp   merge_loop574

  ; Jump out point if end of dest is reached
  dest_end_exit574:
  add   PTR_ESI, 4
  add   edx, [step_fract]
  jc    skip_filter_adjust574

  ; rotate filter values
  mov   [cur_l], ecx
  mov   ecx, [cur_r2]
  mov   [cur_r], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 4

  skip_filter_adjust574:

  ; Jump out point if end of src is reached
  src_end_exit574:

  ; Restore registers
  pop   PTR_EBP

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_Volume_Filtered ENDP


Merge_DestStereo_SrcStereo_Src16_Volume_Filtered PROC ; 63

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcStereo_Src16_Volume_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_left]
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 65536  ; 100000000f/2048/32
  imul  [scale_right]
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; Save registers
  push  PTR_EBP

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue63
  test eax, 040000000h
  jnz  last_continue63

  ; Merge sample data loop
  ALIGN 16
  merge_loop63:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  mov   eax, [cur_r]
  imul  ecx, ebx
  imul  eax, ebx
  sar   ecx, 16
  sar   eax, 16
  cmp   edx, 65536*2
  mov   [cur_r], eax
  jl    skip_loop63

  ; loop to load all of the full sample points
  whole_loop63:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit63
  whole_continue63:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16
  add   ecx, eax
  add   [cur_r], ebx
  sub   edx, 65536
  add   PTR_ESI, 4
  cmp   edx, 65536*2
  jae   whole_loop63

  skip_loop63:
  and   edx, 0ffffh
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit63
  last_continue63:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16
  add   PTR_ESI, 4
  mov   ebp, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebp
  mov   ebp, [cur_r]
  mov   [cur_r], ebx
  imul  ebx, edx
  sar   ebx, 16
  add   ebx, ebp
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jb    merge_loop63

  ; Jump out point if end of dest is reached
  dest_end_exit63:
  jmp   src_save_value63

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit63:
  or   edx, 080000000h
  src_last_exit63:
  or   edx, 040000000h
  src_save_value63:
  mov  [cur_l], ecx

  ; Restore registers
  pop   PTR_EBP

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_Volume_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Save registers
  push  PTR_EBP

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop575:

  ; Upsample the data points
  mov   ebp, edx
  mov   eax, [cur_l]
  mov   ebx, [cur_r]
  shr   ebp, 17
  sub   eax, ecx
  sub   ebx, [cur_r2]
  imul  eax, ebp
  imul  ebx, ebp
  sar   eax, 15
  sar   ebx, 15
  add   eax, ecx
  add   ebx, [cur_r2]

  ; Apply volume
  imul  eax, [scale_left]
  imul  ebx, [scale_right]

  ; Merge sample data into output buffer
  add   [PTR_EDI], eax
  add   [PTR_EDI+4], ebx
  add   PTR_EDI, 8
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit575

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop575

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit575

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; End loop
  jmp   merge_loop575

  ; Jump out point if end of dest is reached
  dest_end_exit575:
  add   PTR_ESI, 4
  add   edx, [step_fract]
  jc    skip_filter_adjust575

  ; rotate filter values
  mov   [cur_l], ecx
  mov   ecx, [cur_r2]
  mov   [cur_r], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 4

  skip_filter_adjust575:

  ; Jump out point if end of src is reached
  src_end_exit575:

  ; Restore registers
  pop   PTR_EBP

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_Volume_Filtered ENDP


Merge_DestMono_SrcMono_Src8_Copy16_NoResample PROC ; 64

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Merge sample data loop
  ALIGN 16
  merge_loop64:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit64

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit64

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; End loop
  jmp   merge_loop64

  ; Jump out point if end of dest is reached
  dest_end_exit64:
  inc   PTR_ESI

  ; Jump out point if end of src is reached
  src_end_exit64:

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_Copy16_NoResample ENDP


Merge_DestStereo_SrcMono_Src8_Copy16_NoResample PROC ; 65

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Merge sample data loop
  ALIGN 16
  merge_loop65:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], ax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit65

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit65

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; End loop
  jmp   merge_loop65

  ; Jump out point if end of dest is reached
  dest_end_exit65:
  inc   PTR_ESI

  ; Jump out point if end of src is reached
  src_end_exit65:

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_Copy16_NoResample ENDP


Merge_DestMono_SrcMono_Src16_Copy16_NoResample PROC ; 66

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Merge sample data loop
  ALIGN 16
  merge_loop66:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit66

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit66

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; End loop
  jmp   merge_loop66

  ; Jump out point if end of dest is reached
  dest_end_exit66:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit66:

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_Copy16_NoResample ENDP


Merge_DestStereo_SrcMono_Src16_Copy16_NoResample PROC ; 67

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Merge sample data loop
  ALIGN 16
  merge_loop67:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], ax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit67

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit67

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; End loop
  jmp   merge_loop67

  ; Jump out point if end of dest is reached
  dest_end_exit67:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit67:

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_Copy16_NoResample ENDP


Merge_DestMono_SrcStereo_Src8_Copy16_NoResample PROC ; 72

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; Merge sample data loop
  ALIGN 16
  merge_loop72:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit72

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit72

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; End loop
  jmp   merge_loop72

  ; Jump out point if end of dest is reached
  dest_end_exit72:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit72:

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_Copy16_NoResample ENDP


Merge_DestStereo_SrcStereo_Src8_Copy16_NoResample PROC ; 73

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge sample data loop
  ALIGN 16
  merge_loop73:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], bx
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit73

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit73

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; End loop
  jmp   merge_loop73

  ; Jump out point if end of dest is reached
  dest_end_exit73:
  add   PTR_ESI, 2

  ; Jump out point if end of src is reached
  src_end_exit73:

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_Copy16_NoResample ENDP


Merge_DestMono_SrcStereo_Src16_Copy16_NoResample PROC ; 74

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; Merge sample data loop
  ALIGN 16
  merge_loop74:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit74

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit74

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; End loop
  jmp   merge_loop74

  ; Jump out point if end of dest is reached
  dest_end_exit74:
  add   PTR_ESI, 4

  ; Jump out point if end of src is reached
  src_end_exit74:

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_Copy16_NoResample ENDP


Merge_DestStereo_SrcStereo_Src16_Copy16_NoResample PROC ; 75

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge sample data loop
  ALIGN 16
  merge_loop75:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], bx
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit75

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit75

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; End loop
  jmp   merge_loop75

  ; Jump out point if end of dest is reached
  dest_end_exit75:
  add   PTR_ESI, 4

  ; Jump out point if end of src is reached
  src_end_exit75:

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_Copy16_NoResample ENDP


Merge_DestMono_SrcMono_Src8_Copy16_Resample PROC ; 80

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Merge sample data loop
  ALIGN 16
  merge_loop80:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit80

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit80

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; End loop
  jmp   merge_loop80

  ; Jump out point if end of dest is reached
  dest_end_exit80:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit80:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_Copy16_Resample ENDP


Merge_DestStereo_SrcMono_Src8_Copy16_Resample PROC ; 81

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; Merge sample data loop
  ALIGN 16
  merge_loop81:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], ax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit81

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit81

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; End loop
  jmp   merge_loop81

  ; Jump out point if end of dest is reached
  dest_end_exit81:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit81:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_Copy16_Resample ENDP


Merge_DestMono_SrcMono_Src16_Copy16_Resample PROC ; 82

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Merge sample data loop
  ALIGN 16
  merge_loop82:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit82

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit82

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; End loop
  jmp   merge_loop82

  ; Jump out point if end of dest is reached
  dest_end_exit82:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit82:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_Copy16_Resample ENDP


Merge_DestStereo_SrcMono_Src16_Copy16_Resample PROC ; 83

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; Merge sample data loop
  ALIGN 16
  merge_loop83:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], ax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit83

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit83

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; End loop
  jmp   merge_loop83

  ; Jump out point if end of dest is reached
  dest_end_exit83:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit83:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_Copy16_Resample ENDP


Merge_DestMono_SrcMono_Src8_Copy16_Filtered PROC ; 84

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcMono_Src8_Copy16_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue84
  test eax, 040000000h
  jnz  last_continue84

  ; Merge sample data loop
  ALIGN 16
  merge_loop84:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop84

  ; loop to load all of the full sample points
  whole_loop84:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit84
  whole_continue84:

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  add   ecx, eax
  sub   edx, 65536
  inc   PTR_ESI
  cmp   edx, 65536*2
  jae   whole_loop84

  skip_loop84:
  and   edx, 0ffffh
  last_continue84:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit84

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  inc   PTR_ESI
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Scale back down after divide
  sar   eax, 11

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jb    merge_loop84

  ; Jump out point if end of dest is reached
  dest_end_exit84:
  jmp   src_save_value84

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit84:
  or   edx, 080000000h
  src_last_exit84:
  or   edx, 040000000h
  src_save_value84:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_Copy16_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop596:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit596

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop596

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit596

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop596

  ; Jump out point if end of dest is reached
  dest_end_exit596:
  inc   PTR_ESI
  add   edx, [step_fract]
  jc    skip_filter_adjust596

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  dec   PTR_ESI

  skip_filter_adjust596:

  ; Jump out point if end of src is reached
  src_end_exit596:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src8_Copy16_Filtered ENDP


Merge_DestStereo_SrcMono_Src8_Copy16_Filtered PROC ; 85

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcMono_Src8_Copy16_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue85
  test eax, 040000000h
  jnz  last_continue85

  ; Merge sample data loop
  ALIGN 16
  merge_loop85:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop85

  ; loop to load all of the full sample points
  whole_loop85:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit85
  whole_continue85:

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  add   ecx, eax
  sub   edx, 65536
  inc   PTR_ESI
  cmp   edx, 65536*2
  jae   whole_loop85

  skip_loop85:
  and   edx, 0ffffh
  last_continue85:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit85

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8
  inc   PTR_ESI
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Scale back down after divide
  sar   eax, 11

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], ax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop85

  ; Jump out point if end of dest is reached
  dest_end_exit85:
  jmp   src_save_value85

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit85:
  or   edx, 080000000h
  src_last_exit85:
  or   edx, 040000000h
  src_save_value85:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_Copy16_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop597:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], ax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit597

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop597

  ; Move the source pointer
  inc   PTR_ESI
  cmp   PTR_ESI, src_end
  jae   src_end_exit597

  ; Load sample data
  xor   eax, eax
  mov   al, byte ptr [PTR_ESI]
  sub   eax, 080h
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop597

  ; Jump out point if end of dest is reached
  dest_end_exit597:
  inc   PTR_ESI
  add   edx, [step_fract]
  jc    skip_filter_adjust597

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  dec   PTR_ESI

  skip_filter_adjust597:

  ; Jump out point if end of src is reached
  src_end_exit597:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src8_Copy16_Filtered ENDP


Merge_DestMono_SrcMono_Src16_Copy16_Filtered PROC ; 86

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcMono_Src16_Copy16_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue86
  test eax, 040000000h
  jnz  last_continue86

  ; Merge sample data loop
  ALIGN 16
  merge_loop86:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop86

  ; loop to load all of the full sample points
  whole_loop86:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit86
  whole_continue86:

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop86

  skip_loop86:
  and   edx, 0ffffh
  last_continue86:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit86

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   PTR_ESI, 2
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Scale back down after divide
  sar   eax, 11

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jb    merge_loop86

  ; Jump out point if end of dest is reached
  dest_end_exit86:
  jmp   src_save_value86

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit86:
  or   edx, 080000000h
  src_last_exit86:
  or   edx, 040000000h
  src_save_value86:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_Copy16_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop598:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit598

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop598

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit598

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop598

  ; Jump out point if end of dest is reached
  dest_end_exit598:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust598

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust598:

  ; Jump out point if end of src is reached
  src_end_exit598:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcMono_Src16_Copy16_Filtered ENDP


Merge_DestStereo_SrcMono_Src16_Copy16_Filtered PROC ; 87

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcMono_Src16_Copy16_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue87
  test eax, 040000000h
  jnz  last_continue87

  ; Merge sample data loop
  ALIGN 16
  merge_loop87:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop87

  ; loop to load all of the full sample points
  whole_loop87:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit87
  whole_continue87:

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop87

  skip_loop87:
  and   edx, 0ffffh
  last_continue87:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit87

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]
  add   PTR_ESI, 2
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Scale back down after divide
  sar   eax, 11

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], ax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop87

  ; Jump out point if end of dest is reached
  dest_end_exit87:
  jmp   src_save_value87

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit87:
  or   edx, 080000000h
  src_last_exit87:
  or   edx, 040000000h
  src_save_value87:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_Copy16_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop599:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], ax
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit599

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop599

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit599

  ; Load sample data
  movsx eax, word ptr [PTR_ESI]

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop599

  ; Jump out point if end of dest is reached
  dest_end_exit599:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust599

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust599:

  ; Jump out point if end of src is reached
  src_end_exit599:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcMono_Src16_Copy16_Filtered ENDP


Merge_DestMono_SrcStereo_Src8_Copy16_Resample PROC ; 88

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; Merge sample data loop
  ALIGN 16
  merge_loop88:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit88

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit88

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; End loop
  jmp   merge_loop88

  ; Jump out point if end of dest is reached
  dest_end_exit88:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit88:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_Copy16_Resample ENDP


Merge_DestStereo_SrcStereo_Src8_Copy16_Resample PROC ; 89

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge sample data loop
  ALIGN 16
  merge_loop89:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], bx
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit89

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit89

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; End loop
  jmp   merge_loop89

  ; Jump out point if end of dest is reached
  dest_end_exit89:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit89:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_Copy16_Resample ENDP


Merge_DestMono_SrcStereo_Src16_Copy16_Resample PROC ; 90

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; Merge sample data loop
  ALIGN 16
  merge_loop90:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit90

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit90

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; End loop
  jmp   merge_loop90

  ; Jump out point if end of dest is reached
  dest_end_exit90:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit90:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_Copy16_Resample ENDP


Merge_DestStereo_SrcStereo_Src16_Copy16_Resample PROC ; 91

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge sample data loop
  ALIGN 16
  merge_loop91:

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], bx
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit91

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]
  cmp   PTR_ESI, src_end
  jae   src_end_exit91

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; End loop
  jmp   merge_loop91

  ; Jump out point if end of dest is reached
  dest_end_exit91:

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  sbb   PTR_EAX, PTR_EAX
  add   PTR_ESI, [STEPTBL_PTR][PTR_SIZE+PTR_EAX*PTR_SIZE]

  ; Jump out point if end of src is reached
  src_end_exit91:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_Copy16_Resample ENDP


Merge_DestMono_SrcStereo_Src8_Copy16_Filtered PROC ; 92

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcStereo_Src8_Copy16_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue92
  test eax, 040000000h
  jnz  last_continue92

  ; Merge sample data loop
  ALIGN 16
  merge_loop92:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop92

  ; loop to load all of the full sample points
  whole_loop92:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit92
  whole_continue92:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop92

  skip_loop92:
  and   edx, 0ffffh
  last_continue92:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit92

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1
  add   PTR_ESI, 2
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Scale back down after divide
  sar   eax, 11

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jb    merge_loop92

  ; Jump out point if end of dest is reached
  dest_end_exit92:
  jmp   src_save_value92

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit92:
  or   edx, 080000000h
  src_last_exit92:
  or   edx, 040000000h
  src_save_value92:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_Copy16_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop604:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit604

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop604

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit604

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop604

  ; Jump out point if end of dest is reached
  dest_end_exit604:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust604

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust604:

  ; Jump out point if end of src is reached
  src_end_exit604:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src8_Copy16_Filtered ENDP


Merge_DestStereo_SrcStereo_Src8_Copy16_Filtered PROC ; 93

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcStereo_Src8_Copy16_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; Save registers
  push  PTR_EBP

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue93
  test eax, 040000000h
  jnz  last_continue93

  ; Merge sample data loop
  ALIGN 16
  merge_loop93:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  mov   eax, [cur_r]
  imul  ecx, ebx
  imul  eax, ebx
  sar   ecx, 16
  sar   eax, 16
  cmp   edx, 65536*2
  mov   [cur_r], eax
  jl    skip_loop93

  ; loop to load all of the full sample points
  whole_loop93:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit93
  whole_continue93:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8
  add   ecx, eax
  add   [cur_r], ebx
  sub   edx, 65536
  add   PTR_ESI, 2
  cmp   edx, 65536*2
  jae   whole_loop93

  skip_loop93:
  and   edx, 0ffffh
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit93
  last_continue93:

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8
  add   PTR_ESI, 2
  mov   ebp, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebp
  mov   ebp, [cur_r]
  mov   [cur_r], ebx
  imul  ebx, edx
  sar   ebx, 16
  add   ebx, ebp
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Scale back down after divide
  sar   eax, 11
  sar   ebx, 11

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], bx
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop93

  ; Jump out point if end of dest is reached
  dest_end_exit93:
  jmp   src_save_value93

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit93:
  or   edx, 080000000h
  src_last_exit93:
  or   edx, 040000000h
  src_save_value93:
  mov  [cur_l], ecx

  ; Restore registers
  pop   PTR_EBP

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_Copy16_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Save registers
  push  PTR_EBP

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop605:

  ; Upsample the data points
  mov   ebp, edx
  mov   eax, [cur_l]
  mov   ebx, [cur_r]
  shr   ebp, 17
  sub   eax, ecx
  sub   ebx, [cur_r2]
  imul  eax, ebp
  imul  ebx, ebp
  sar   eax, 15
  sar   ebx, 15
  add   eax, ecx
  add   ebx, [cur_r2]

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], bx
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit605

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop605

  ; Move the source pointer
  add   PTR_ESI, 2
  cmp   PTR_ESI, src_end
  jae   src_end_exit605

  ; Load sample data
  xor   ebx, ebx
  xor   eax, eax
  mov   ax, word ptr [PTR_ESI]
  mov   bl, ah
  and   eax, 0ffh
  sub   ebx, 080h
  sub   eax, 080h
  shl   ebx, 8
  shl   eax, 8

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; End loop
  jmp   merge_loop605

  ; Jump out point if end of dest is reached
  dest_end_exit605:
  add   PTR_ESI, 2
  add   edx, [step_fract]
  jc    skip_filter_adjust605

  ; rotate filter values
  mov   [cur_l], ecx
  mov   ecx, [cur_r2]
  mov   [cur_r], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 2

  skip_filter_adjust605:

  ; Jump out point if end of src is reached
  src_end_exit605:

  ; Restore registers
  pop   PTR_EBP

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src8_Copy16_Filtered ENDP


Merge_DestMono_SrcStereo_Src16_Copy16_Filtered PROC ; 94

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestMono_SrcStereo_Src16_Copy16_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue94
  test eax, 040000000h
  jnz  last_continue94

  ; Merge sample data loop
  ALIGN 16
  merge_loop94:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  imul  ecx, ebx
  sar   ecx, 16
  cmp   edx, 65536*2
  jl    skip_loop94

  ; loop to load all of the full sample points
  whole_loop94:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit94
  whole_continue94:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1
  add   ecx, eax
  sub   edx, 65536
  add   PTR_ESI, 4
  cmp   edx, 65536*2
  jae   whole_loop94

  skip_loop94:
  and   edx, 0ffffh
  last_continue94:
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit94

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1
  add   PTR_ESI, 4
  mov   ebx, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebx
  imul  eax, [divider_l]

  ; Scale back down after divide
  sar   eax, 11

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jb    merge_loop94

  ; Jump out point if end of dest is reached
  dest_end_exit94:
  jmp   src_save_value94

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit94:
  or   edx, 080000000h
  src_last_exit94:
  or   edx, 040000000h
  src_save_value94:
  mov  [cur_l], ecx

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_Copy16_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop606:

  ; Upsample the data points
  mov   ebx, edx
  mov   eax, [cur_l]
  shr   ebx, 17
  sub   eax, ecx
  imul  eax, ebx
  sar   eax, 15
  add   eax, ecx

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  add   PTR_EDI, 2
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit606

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop606

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit606

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; Merge left and right channels for mono dest
  add   eax, ebx

  ; Shift right to avoid clipping on direct copies
  sar   eax, 1

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax

  ; End loop
  jmp   merge_loop606

  ; Jump out point if end of dest is reached
  dest_end_exit606:
  add   PTR_ESI, 4
  add   edx, [step_fract]
  jc    skip_filter_adjust606

  ; rotate filter values
  mov   [cur_l], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 4

  skip_filter_adjust606:

  ; Jump out point if end of src is reached
  src_end_exit606:

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestMono_SrcStereo_Src16_Copy16_Filtered ENDP


Merge_DestStereo_SrcStereo_Src16_Copy16_Filtered PROC ; 95

  ; check to see if we have to call the upsampling version
  cmp   [playback_ratio], 10000h
  jle   Merge_DestStereo_SrcStereo_Src16_Copy16_UpFiltered

  ; build average dividers
  mov   ecx, edx
  mov   ebx, [playback_ratio]
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_l], eax
  xor   edx, edx
  mov   eax, 08000000h  ; 100000000f/32
  div   ebx
  dec   eax
  mov   [divider_r], eax
  mov   edx, ecx

  ; load initial sample
  mov   ecx, [cur_l]

  ; Save registers
  push  PTR_EBP

  ; handle start up loop management
  mov  eax, edx
  and  edx, 03fffffffh
  test eax, 080000000h
  jnz  whole_continue95
  test eax, 040000000h
  jnz  last_continue95

  ; Merge sample data loop
  ALIGN 16
  merge_loop95:
  mov   ebx, 65536
  sub   ebx, edx
  add   edx, [playback_ratio]

  ; weight the initial sample
  mov   eax, [cur_r]
  imul  ecx, ebx
  imul  eax, ebx
  sar   ecx, 16
  sar   eax, 16
  cmp   edx, 65536*2
  mov   [cur_r], eax
  jl    skip_loop95

  ; loop to load all of the full sample points
  whole_loop95:
  cmp   PTR_ESI, [src_end]
  jae   src_whole_exit95
  whole_continue95:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16
  add   ecx, eax
  add   [cur_r], ebx
  sub   edx, 65536
  add   PTR_ESI, 4
  cmp   edx, 65536*2
  jae   whole_loop95

  skip_loop95:
  and   edx, 0ffffh
  cmp   PTR_ESI, [src_end]
  jae   src_last_exit95
  last_continue95:

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16
  add   PTR_ESI, 4
  mov   ebp, ecx
  mov   ecx, eax

  ; weight the final sample
  imul  eax, edx
  sar   eax, 16
  add   eax, ebp
  mov   ebp, [cur_r]
  mov   [cur_r], ebx
  imul  ebx, edx
  sar   ebx, 16
  add   ebx, ebp
  imul  eax, [divider_l]
  imul  ebx, [divider_r]

  ; Scale back down after divide
  sar   eax, 11
  sar   ebx, 11

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], bx
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jb    merge_loop95

  ; Jump out point if end of dest is reached
  dest_end_exit95:
  jmp   src_save_value95

  ; jump out when src is exceed, but save our current loop position
  src_whole_exit95:
  or   edx, 080000000h
  src_last_exit95:
  or   edx, 040000000h
  src_save_value95:
  mov  [cur_l], ecx

  ; Restore registers
  pop   PTR_EBP

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_Copy16_UpFiltered:

  ; adjust fractional
  shl   edx, 16

  ; Save registers
  push  PTR_EBP

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; Merge sample data loop
  ALIGN 16
  merge_loop607:

  ; Upsample the data points
  mov   ebp, edx
  mov   eax, [cur_l]
  mov   ebx, [cur_r]
  shr   ebp, 17
  sub   eax, ecx
  sub   ebx, [cur_r2]
  imul  eax, ebp
  imul  ebx, ebp
  sar   eax, 15
  sar   ebx, 15
  add   eax, ecx
  add   ebx, [cur_r2]

  ; Copy sample data into output buffer
  mov   [PTR_EDI], ax
  mov   [PTR_EDI+2], bx
  add   PTR_EDI, 4
  cmp   PTR_EDI, dest_end
  jae   dest_end_exit607

  ; Add to accumulator and advance the source correctly
  add   edx, [step_fract]
  jnc   merge_loop607

  ; Move the source pointer
  add   PTR_ESI, 4
  cmp   PTR_ESI, src_end
  jae   src_end_exit607

  ; Load sample data
  mov   ebx, dword ptr [PTR_ESI]
  movsx eax, bx
  sar   ebx, 16

  ; rotate filter values
  mov   ecx, [cur_l]
  mov   [cur_l], eax
  mov   eax, [cur_r]
  mov   [cur_r], ebx
  mov   [cur_r2], eax

  ; End loop
  jmp   merge_loop607

  ; Jump out point if end of dest is reached
  dest_end_exit607:
  add   PTR_ESI, 4
  add   edx, [step_fract]
  jc    skip_filter_adjust607

  ; rotate filter values
  mov   [cur_l], ecx
  mov   ecx, [cur_r2]
  mov   [cur_r], ecx

  ; un-increment the source to skip the early source adjustment
  sub   PTR_ESI, 4

  skip_filter_adjust607:

  ; Jump out point if end of src is reached
  src_end_exit607:

  ; Restore registers
  pop   PTR_EBP

  ; adjust fractional
  shr   edx, 16

  ; Routine end
  ret

Merge_DestStereo_SrcStereo_Src16_Copy16_Filtered ENDP


vector_table    LABEL PTR_LABEL
  PTR_TAG Merge_DestMono_SrcMono_Src8_NoVolume_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_NoVolume_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src16_NoVolume_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_NoVolume_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src8_NoVolume_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_NoVolume_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src16_NoVolume_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_NoVolume_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_NoVolume_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_NoVolume_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_NoVolume_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_NoVolume_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_NoVolume_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_NoVolume_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_NoVolume_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_NoVolume_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src8_NoVolume_Resample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_NoVolume_Resample
  PTR_TAG Merge_DestMono_SrcMono_Src16_NoVolume_Resample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_NoVolume_Resample
  PTR_TAG Merge_DestMono_SrcMono_Src8_NoVolume_Filtered
  PTR_TAG Merge_DestStereo_SrcMono_Src8_NoVolume_Filtered
  PTR_TAG Merge_DestMono_SrcMono_Src16_NoVolume_Filtered
  PTR_TAG Merge_DestStereo_SrcMono_Src16_NoVolume_Filtered
  PTR_TAG Merge_DestMono_SrcStereo_Src8_NoVolume_Resample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_NoVolume_Resample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_NoVolume_Resample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_NoVolume_Resample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_NoVolume_Filtered
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_NoVolume_Filtered
  PTR_TAG Merge_DestMono_SrcStereo_Src16_NoVolume_Filtered
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_NoVolume_Filtered
  PTR_TAG Merge_DestMono_SrcMono_Src8_Volume_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_Volume_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src16_Volume_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_Volume_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src8_Volume_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_Volume_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src16_Volume_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_Volume_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_Volume_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_Volume_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_Volume_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_Volume_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_Volume_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_Volume_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_Volume_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_Volume_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src8_Volume_Resample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_Volume_Resample
  PTR_TAG Merge_DestMono_SrcMono_Src16_Volume_Resample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_Volume_Resample
  PTR_TAG Merge_DestMono_SrcMono_Src8_Volume_Filtered
  PTR_TAG Merge_DestStereo_SrcMono_Src8_Volume_Filtered
  PTR_TAG Merge_DestMono_SrcMono_Src16_Volume_Filtered
  PTR_TAG Merge_DestStereo_SrcMono_Src16_Volume_Filtered
  PTR_TAG Merge_DestMono_SrcStereo_Src8_Volume_Resample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_Volume_Resample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_Volume_Resample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_Volume_Resample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_Volume_Filtered
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_Volume_Filtered
  PTR_TAG Merge_DestMono_SrcStereo_Src16_Volume_Filtered
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_Volume_Filtered
  PTR_TAG Merge_DestMono_SrcMono_Src8_Copy16_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_Copy16_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src16_Copy16_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_Copy16_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src8_Copy16_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_Copy16_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src16_Copy16_NoResample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_Copy16_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_Copy16_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_Copy16_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_Copy16_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_Copy16_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_Copy16_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_Copy16_NoResample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_Copy16_NoResample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_Copy16_NoResample
  PTR_TAG Merge_DestMono_SrcMono_Src8_Copy16_Resample
  PTR_TAG Merge_DestStereo_SrcMono_Src8_Copy16_Resample
  PTR_TAG Merge_DestMono_SrcMono_Src16_Copy16_Resample
  PTR_TAG Merge_DestStereo_SrcMono_Src16_Copy16_Resample
  PTR_TAG Merge_DestMono_SrcMono_Src8_Copy16_Filtered
  PTR_TAG Merge_DestStereo_SrcMono_Src8_Copy16_Filtered
  PTR_TAG Merge_DestMono_SrcMono_Src16_Copy16_Filtered
  PTR_TAG Merge_DestStereo_SrcMono_Src16_Copy16_Filtered
  PTR_TAG Merge_DestMono_SrcStereo_Src8_Copy16_Resample
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_Copy16_Resample
  PTR_TAG Merge_DestMono_SrcStereo_Src16_Copy16_Resample
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_Copy16_Resample
  PTR_TAG Merge_DestMono_SrcStereo_Src8_Copy16_Filtered
  PTR_TAG Merge_DestStereo_SrcStereo_Src8_Copy16_Filtered
  PTR_TAG Merge_DestMono_SrcStereo_Src16_Copy16_Filtered
  PTR_TAG Merge_DestStereo_SrcStereo_Src16_Copy16_Filtered
